"""Call-sequence differential check for the imp fire strategy (type 0x46) and its shared helpers:
strategy target queue 0x0002B910/0x00112970/0x00112A70/0x00112B40/0x0002CC30, target selection
0x00029AC0/0x00029BA0/0x00139AA0/0x00139C70, target checks and reactions 0x00138410/0x00141450/
0x00141470/0x000196B0/0x000196F0/0x00129280/0x001409C0/0x000F4870/0x000F4E10/0x0013A430/0x0013A4F0/
0x00138700/0x00138740/0x00138790/0x001384F0/0x001395C0/0x001398A0/0x001399A0/0x001383A0, roaming
0x00138310/0x00139FA0/0x0013A0A0, the class methods 0x000B0B90..0x000B4F50 and 0x000C2D90, angle
helpers 0x00100B80/0x00100D00 and model lookup 0x000409D0.

Callees outside the batch are logging stubs on both sides fed from the same per-case queues.
The retail code runs under Unicorn from the user's own default.xbe; imp_strategy_harness links
the translated modules with matching stubs. Both print the result, the touched state and the log.
Usage: imp_strategy_oracle.py [seed] [count] [entry]"""
import os, random, struct, subprocess, sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_XMM0, UC_X86_REG_XMM1)
from retail_cpu import ROOT, STOP, STACK, HEAP, w32, r32, harness

def r16(u, a): return struct.unpack('<H', u.mem_read(a, 2))[0]
def w16(u, a, v): u.mem_write(a, struct.pack('<H', v & 0xffff))
def w8(u, a, v): u.mem_write(a, bytes([v & 0xff]))
def r8(u, a): return u.mem_read(a, 1)[0]
def rf(u, a): return struct.unpack('<f', u.mem_read(a, 4))[0]
def fb(v): return struct.unpack('<I', struct.pack('<f', v))[0]
def cstr(u, a): return bytes(u.mem_read(a, 0x40)).split(b'\0')[0].decode('latin1')

def xbe_sections():
    data = open(os.path.join(ROOT, 'OriginalFiles', 'default.xbe'), 'rb').read()
    base = struct.unpack_from('<I', data, 0x104)[0]
    count, table = struct.unpack_from('<II', data, 0x11C)
    for i in range(count):
        flags, va, vsize, raw, rsize = struct.unpack_from('<IIIII', data, table - base + i * 0x38)
        yield va, data[raw:raw + min(vsize, rsize)]

SECTIONS = list(xbe_sections())
STUBS = 0x00F20000
ACTOR, BODY, MIND, STRAT, RING, ROWS, BITS, ATTR = (HEAP + 0x0000, HEAP + 0x1000, HEAP + 0x2000, HEAP + 0x2400,
    HEAP + 0x2500, HEAP + 0x2600, HEAP + 0x2700, HEAP + 0x2800)
EVENTS, CTRL, SLOTS, PLAYER, MESSAGE = HEAP + 0x2900, HEAP + 0x2C00, HEAP + 0x2D00, HEAP + 0x2E00, HEAP + 0x2F80
O1, O2 = HEAP + 0x3000, HEAP + 0x5000
T0, T1 = HEAP + 0x7000, HEAP + 0x7100
ALLOWED, OCC = HEAP + 0x7180, HEAP + 0x71C0
MODEL, NODES, MDATA = HEAP + 0x7200, HEAP + 0x7220, HEAP + 0x7300
LINKS = HEAP + 0x7800
ACTIVE = HEAP + 0x8000
OUT = HEAP + 0x9000
OBJ = HEAP + 0xA000
SEEN = HEAP + 0xA800
VT_ACTOR, VT_OBJ = STUBS + 0x800, STUBS + 0x900
NOTICE, OBJMSG, LEAVING, ENTERING = STUBS + 0x400, STUBS + 0x410, STUBS + 0x420, STUBS + 0x430
VT_STRAT, DISTANCE = STUBS + 0xA00, STUBS + 0xF00
TABLE = 0x0050E284
PARAMS = 0x004287BC
EFFECT_NAME = 0x00303058

CALLEES = {
    0x00129640: 'target', 0x00029C80: 'eligible', 0x0002A0C0: 'refresh', 0x0002A190: 'event',
    0x000E9210: 'drop', 0x0002B8D0: 'state', 0x0002BC10: 'base', 0x0002C130: 'baseupdate',
    0x0002BAF0: 'baseenter', 0x0013A490: 'knockout', 0x00028D50: 'resume', 0x0002A8E0: 'goal',
    0x00029D90: 'request', 0x0013AA50: 'search', 0x00100C70: 'randomize', 0x000412A0: 'position',
    0x00105F90: 'direction', 0x0003DF40: 'create', 0x00031240: 'clearslot', 0x0001EF10: 'bit',
    0x001460C0: 'unavailable', 0x00041490: 'distance', 0x0013A5B0: 'surface', 0x00019200: 'chance',
}
QUEUES = ['target', 'eligible', 'knockout', 'resume', 'search', 'direction', 'create', 'unavailable', 'surface',
    'chance', 'dist', 'randomize']
ENTRIES = ['join', 'select', 'hostile', 'abandon', 'react', 'contact', 'cower', 'startle', 'notice', 'roam',
    'reached', 'update', 'message', 'enter', 'leave', 'ctor', 'dtor', 'angles', 'heading', 'model']

def code(value, table):
    for i, v in enumerate(table):
        if v == value:
            return i
    return 0xFF

class Retail:
    def __init__(self, queues):
        u = Uc(UC_ARCH_X86, UC_MODE_32)
        u.mem_map(0, 0x1000)
        u.mem_map(0x00010000, 0x5F0000)
        for va, data in SECTIONS:
            if va + len(data) <= 0x00600000:
                u.mem_write(va, data)
        u.mem_map(STOP, 0x1000); u.mem_write(STOP, b'\xf4')
        u.mem_map(STUBS, 0x1000); u.mem_write(STUBS, b'\xc3' * 0x1000)
        u.mem_map(HEAP, 0x100000)
        u.mem_map(STACK - 0x10000, 0x20000)
        self.u, self.queues, self.log, self.requests = u, {k: list(v) for k, v in queues.items()}, [], []
        self.next_block = HEAP + 0x80000
        self.events = []
        for address, name in CALLEES.items():
            u.mem_write(address, b'\xc3')
            u.hook_add(UC_HOOK_CODE, self.dispatch, begin=address, end=address, user_data=name)
        # 0x00041490 returns in ST0: its stub loads the value the hook stores at DISTANCE.
        u.mem_write(0x00041490, b'\xd9\x05' + struct.pack('<I', DISTANCE) + b'\xc3')
        u.mem_write(0x00029E80, b'\xc3')
        for address, name in ((NOTICE, 'notice'), (OBJMSG, 'object'), (LEAVING, 'leaving'), (ENTERING, 'entering')):
            u.hook_add(UC_HOOK_CODE, self.dispatch, begin=address, end=address, user_data=name)

    def pop(self, name):
        q = self.queues[name]
        return q.pop(0) if q else 0

    def arg(self, k):
        return r32(self.u, self.u.reg_read(UC_X86_REG_ESP) + 4 + 4 * k)

    def reg(self, r):
        return self.u.reg_read(r)

    def xmm(self, r):
        return self.u.reg_read(r) & 0xffffffff

    def ret(self, value):
        self.u.reg_write(UC_X86_REG_EAX, value & 0xffffffff)

    def block(self, n):
        b = self.next_block
        self.next_block += (n + 0xF) & ~0xF
        self.u.mem_write(b, b'\x00' * n)
        return b

    def actor_name(self, a):
        return {ACTOR: 'actor', O1: 'o1', O2: 'o2', 0: 'none'}.get(a, '%x' % a)

    def target_name(self, a):
        return {T0: 't0', T1: 't1', 0: 'none'}.get(a, '%x' % a)

    def dispatch(self, uc, address, size, name):
        u, log = self.u, self.log
        if name == 'target':
            v = self.pop('target')
            d = self.pop('dist')
            log.append('target %s %s %x %x %x %x' % (self.actor_name(self.arg(0)), {ALLOWED: 'allowed', 0: 'none'}.get(self.arg(1), '?'),
                self.arg(2) & 0xffff, self.arg(3) & 0xffff, self.arg(5), self.reg(UC_X86_REG_EAX)))
            w32(u, self.arg(4), fb([2.0, 40.0, 100.0][d % 3]))
            self.ret([0, T0, T1][v % 3])
        elif name == 'eligible':
            v = self.pop('eligible')
            log.append('eligible %s %d' % (self.target_name(self.reg(UC_X86_REG_EAX)), self.reg(UC_X86_REG_EDX))); self.ret(v)
        elif name == 'refresh':
            log.append('refresh')
        elif name == 'event':
            node = self.block(0x40)
            self.events.append(node)
            log.append('event %x %08x' % (self.arg(0), self.arg(1))); self.ret(node)
        elif name == 'drop':
            log.append('drop %d' % ((self.reg(UC_X86_REG_EAX) - EVENTS) // 0x40))
        elif name == 'state':
            log.append('state %x' % (self.reg(UC_X86_REG_EDI) & 0xffff))
            w16(u, self.reg(UC_X86_REG_ESI) + 0xE, self.reg(UC_X86_REG_EDI))
        elif name == 'base':
            log.append('base %x' % r32(u, self.arg(1)))
        elif name == 'baseupdate':
            log.append('baseupdate')
        elif name == 'baseenter':
            log.append('baseenter %x' % (self.arg(1) & 0xffff))
        elif name == 'knockout':
            v = self.pop('knockout'); log.append('knockout %d' % v); self.ret(v)
        elif name == 'resume':
            v = self.pop('resume'); log.append('resume %d' % v); self.ret(v)
        elif name == 'goal':
            log.append('goal %s' % cstr(u, self.reg(UC_X86_REG_EAX)))
        elif name == 'request':
            kind = self.reg(UC_X86_REG_EBX) & 0xffff
            b = self.block(0x40)
            self.requests.append(b)
            log.append('request %x' % kind); self.ret(b)
        elif name == 'search':
            v = self.pop('search')
            origin = self.reg(UC_X86_REG_EAX)
            words = [self.arg(i) for i in range(12)]
            log.append('search %08x %08x %08x | %08x %d %08x %d %08x %08x %s %s %d %x %x' % (r32(u, origin), r32(u, origin + 4),
                r32(u, origin + 8), words[0], words[1], words[2], words[3], words[4], words[5],
                {0: 'none', ALLOWED: 'allowed', OCC: 'occ'}.get(words[6], '?'), {0: 'none', ALLOWED: 'allowed', OCC: 'occ'}.get(words[7], '?'),
                words[9], words[10], words[11] & 0xffff))
            out = words[8]
            if v:
                for i in range(3):
                    w32(u, out + 4 * i, fb(1.5 + i))
            self.ret(v)
        elif name == 'randomize':
            v = self.pop('randomize')
            log.append('randomize %08x %08x' % (self.arg(0), self.xmm(UC_X86_REG_XMM0)))
            self.u.reg_write(UC_X86_REG_XMM0, fb([0.25, 3.0, 6.0][v % 3]))
        elif name == 'position':
            obj = self.reg(UC_X86_REG_ESI)
            out = self.reg(UC_X86_REG_EDI)
            u.mem_write(out, bytes(u.mem_read(obj + 0x7F0, 12)))
            self.ret(out)
        elif name == 'direction':
            v = self.pop('direction')
            d = self.reg(UC_X86_REG_EAX)
            out = self.reg(UC_X86_REG_EBX)
            log.append('direction %08x %08x %08x' % (r32(u, d), r32(u, d + 4), r32(u, d + 8)))
            if v:
                w32(u, out, fb(0.75 * v))
            self.ret(v)
        elif name == 'create':
            v = self.pop('create')
            log.append('create %s %s' % (cstr(u, self.reg(UC_X86_REG_EAX)), self.actor_name(self.arg(0))))
            self.ret(OBJ if v else 0)
        elif name == 'clearslot':
            log.append('clearslot %d %d' % (self.reg(UC_X86_REG_EAX), self.arg(0)))
        elif name == 'bit':
            index = self.reg(UC_X86_REG_ECX)
            bits = r32(u, r32(u, self.reg(UC_X86_REG_EAX) + 0x15C) + 8)
            v = (r8(u, bits + (index >> 3)) >> (index & 7)) & 1
            log.append('bit %x %d' % (index, v)); self.ret(v)
        elif name == 'unavailable':
            v = self.pop('unavailable')
            log.append('unavailable %s %d' % (self.actor_name(self.reg(UC_X86_REG_EAX)), v)); self.ret(v)
        elif name == 'distance':
            first, second = self.reg(UC_X86_REG_EAX), self.arg(0)
            v = {O1: 3.0, O2: 7.0, ACTOR: 5.0}.get(second, 9.0)
            log.append('distance %s %s' % (self.actor_name(first), self.actor_name(second)))
            w32(u, DISTANCE, fb(v))
        elif name == 'surface':
            v = self.pop('surface')
            log.append('surface %s' % self.target_name(self.reg(UC_X86_REG_EBX)))
            self.u.reg_write(UC_X86_REG_XMM0, fb([10.0, 80.0][v % 2]))
        elif name == 'chance':
            v = self.pop('chance')
            log.append('chance %08x' % self.xmm(UC_X86_REG_XMM1)); self.ret(v)
        elif name == 'notice':
            m = self.arg(1)
            log.append('notice %x %x' % (r32(u, m), r32(u, m + 4)))
        elif name == 'object':
            m = self.arg(1)
            log.append('object %x %x' % (r32(u, m), r32(u, m + 4)))
        elif name == 'entering':
            log.append('state %x' % (self.arg(1) & 0xffff))


def word(v):
    return v & 0xffffffff

def setup(r, rng, line, entry):
    u = r.u
    def put(v):
        line.append(word(v))
        return v
    # Actor.
    w32(u, ACTOR, VT_ACTOR); w32(u, VT_ACTOR + 0xC, NOTICE)
    w16(u, ACTOR + 6, put(rng.choice([2, 5, 9])))
    w32(u, ACTOR + 0x18, put(fb(rng.uniform(-3.5, 3.5))))
    for i in range(3):
        w32(u, ACTOR + 0x7F0 + 4 * i, put(fb(rng.uniform(-30, 30))))
    for o in (0x969, 0x96C, 0x96D, 0x96E, 0x96F, 0x970):
        w8(u, ACTOR + o, put(rng.choice([0, 0, 1])))
    w32(u, ACTOR + 0x974, put(fb(rng.choice([0.0, 0.0, 1.0]))))
    w32(u, ACTOR + 0x978, put(fb(rng.choice([0.0, 0.0, 1.0]))))
    w16(u, ACTOR + 0x97C, put(rng.choice([0, 1, 4])))
    w16(u, ACTOR + 0x97E, put(rng.choice([0, 1, 5])))
    w16(u, ACTOR + 0xCDC, put(rng.choice([0, 1, 2])))
    w32(u, ACTOR + 0x93C, put(rng.choice([0, 0, 1])))
    w32(u, ACTOR + 0xCA4, ALLOWED if put(rng.choice([0, 1])) else 0)
    w32(u, ACTOR + 0xCA0, OCC if put(rng.choice([0, 1])) else 0)
    w32(u, ACTOR + 0xCEC, put(rng.choice([0, 3])))
    attributes = put(rng.choice([0, 1, 1]))
    w32(u, ACTOR + 0x964, ATTR if attributes else 0)
    w32(u, ATTR + 8, put(fb(rng.choice([0.0, 0.5, 1.0]))))
    w32(u, ATTR + 0x18, put(rng.choice([0, 1, 3])))
    w32(u, ATTR + 0x2C, put(rng.choice([0, 1])))
    w32(u, ACTOR + 0x958, MIND); w32(u, ACTOR + 0x95C, BODY)
    w16(u, ALLOWED + 0x1C, 0); w16(u, OCC + 0x1C, put(rng.choice([0, 2, 5])))
    # Strategy.
    w32(u, STRAT, VT_STRAT); w32(u, VT_STRAT + 0x14, ENTERING); w32(u, VT_STRAT + 0x18, LEAVING)
    w32(u, STRAT + 8, ACTOR)
    w16(u, STRAT + 0xE, put(rng.choice([0x3EE, 0x3EF, 0x401, 0x40B, 0x40C, 0x416, 0x3F4, 0x3FC, 0x3FF, 0x415, 0x3E8])))
    w16(u, STRAT + 0x18, put(rng.choice([0x3E8, 0x40C])))
    w8(u, STRAT + 0x1A, put(rng.choice([0, 1])))
    w8(u, STRAT + 0x1C, put(rng.choice([0, 0, 1, 2])))
    w32(u, STRAT + 0x38, put(fb(rng.choice([0.0, 0.0, 0.01, 2.0]))))
    held = put(rng.choice([0, 0, 1, 2]))
    w32(u, STRAT + 0x3C, [0, O1 + 0x1000, O2 + 0x1000][held])
    w32(u, STRAT + 0x40, put(rng.choice([0, 1, 2, 2])))
    w32(u, STRAT + 0x44, OBJ if put(rng.choice([0, 1])) else 0)
    w32(u, STRAT + 0x48, put(rng.choice([0, 0, 1])))
    w32(u, STRAT + 0x4C, PARAMS)
    w32(u, OBJ, VT_OBJ); w32(u, VT_OBJ + 0xC, OBJMSG)
    # Mind.
    w32(u, MIND + 8, ACTOR)
    w32(u, MIND + 0xC, put(rng.choice([0, 0x77])))
    w32(u, MIND + 0x10, [0, T0, T1][put(rng.choice([0, 1, 1, 2]))])
    w32(u, MIND + 0x58, put(rng.choice([0, 0, 1])))
    w32(u, MIND + 0x64, put(rng.getrandbits(8)))
    w32(u, MIND + 0x68, put(rng.getrandbits(8)))
    w32(u, MIND + 0x6C, put(fb(rng.choice([0.0, 0.0, 30.0]))))
    events = rng.choice([[], [3], [1, 3], [2]])
    put(len(events))
    for i, kind in enumerate(events):
        node = EVENTS + 0x40 * i
        w16(u, node + 8, put(kind))
        w32(u, node + 4, EVENTS + 0x40 * (i + 1) if i + 1 < len(events) else 0)
    w32(u, MIND + 0x28, EVENTS if events else 0)
    # Body.
    w32(u, BODY + 8, ACTOR)
    w16(u, BODY + 0x30, put(rng.choice([0, 0, 0, 3])))
    w32(u, BODY + 0x34, put(fb(rng.choice([0.0, 0.0, 0.0, 1.0]))))
    w8(u, BODY + 0x6A, put(rng.choice([0, 0, 0, 1])))
    w8(u, BODY + 0x6B, put(rng.choice([0, 0, 0, 1])))
    w32(u, BODY + 0x70, put(fb(rng.choice([0.0, 0.0, 1.0]))))
    w16(u, BODY + 0x94, put(rng.choice([0, 3])))
    w16(u, BODY + 0x96, put(rng.choice([0, 4])))
    w8(u, BODY + 0x328, 0x55)
    w32(u, BODY + 0x15C, BITS); w32(u, BITS + 8, BITS + 0x10)
    for i in range(8):
        w8(u, BITS + 0x10 + i, put(rng.choice([0, 0, rng.getrandbits(8)])))
    # Ring.
    if put(rng.choice([0, 1, 1])):
        empty = put(rng.choice([0, 1]))
        kind = put(rng.choice([0, 0, 7]))
        u.mem_write(RING, struct.pack('<HHHHI', 8, 4, 1, 4 if empty else 2, ROWS))
        w16(u, ROWS + 8, kind)
        w32(u, ACTOR + 0x980, RING)
    else:
        w32(u, ACTOR + 0x980, 0)
    w16(u, 0, put(rng.choice([0, 0, 9])))
    # Targets.
    for t in (T0, T1):
        w32(u, t + 0x10, [O1, O2][put(rng.choice([0, 1]))])
        w8(u, t + 0x14, put(rng.choice([0, 0, 1])))
        w8(u, t + 0x19, put(rng.choice([0, 2])))
        for i in range(3):
            w32(u, t + 0x20 + 4 * i, put(fb(rng.uniform(-30, 30))))
    # Other actors and their bodies, occupancy and models.
    for index, other in enumerate((O1, O2)):
        w16(u, other + 6, put(rng.choice([2, 5, 9])))
        for i in range(3):
            w32(u, other + 0x7F0 + 4 * i, put(fb(rng.uniform(-30, 30))))
        body = other + 0x1000
        w32(u, other + 0x95C, body); w32(u, body + 8, other)
        w32(u, body + 0x48, ACTOR if put(rng.choice([0, 0, 1])) else 0)
        w32(u, body + 0x50, [0, ACTOR, O1][put(rng.choice([0, 0, 1, 2]))])
        w32(u, body + 0x58, put(rng.choice([0, 1, 1])))
        w32(u, body + 0x15C, other + 0x1400); w32(u, other + 0x1408, other + 0x1410)
        for i in range(8):
            w8(u, other + 0x1410 + i, put(rng.choice([0, 0, rng.getrandbits(8)])))
        w32(u, other + 0xCA8, [0, ALLOWED, OCC][put(rng.choice([0, 1, 2]))])
        # Actor +0x204 names a cell that holds the node table header.
        w32(u, other + 0x204, MODEL + 0x80 + 4 * index if put(rng.choice([0, 1, 1])) else 0)
        w32(u, MODEL + 0x80 + 4 * index, MODEL + 0x10 * index)
        nodes = put(rng.choice([0, 1, 2]))
        w32(u, MODEL + 0x10 * index, NODES + 0x40 * index if nodes else 0)
        w32(u, MODEL + 0x10 * index + 4, nodes)
        for n in range(nodes):
            w32(u, NODES + 0x40 * index + 8 * n, put(rng.choice([1, 2, 2])))
            data = MDATA + 0x200 * index
            w32(u, NODES + 0x40 * index + 8 * n + 4, data if put(rng.choice([0, 1, 1])) else 0)
        count = put(rng.randrange(5))
        w32(u, MDATA + 0x200 * index, count)
        for k in range(count):
            w16(u, MDATA + 0x200 * index + 4 + 0x34 * k, put(rng.choice([1, 4, 4])))
    # Target queue slots: up to three used slots with up to two other links each.
    for i in range(20):
        u.mem_write(TABLE + 0xC * i, b'\0' * 12)
    owners = [put(rng.choice([0, 1, 2])) for _ in range(3)]
    link_index = 0
    for s, owner in enumerate(owners):
        slot = TABLE + 0xC * s
        w32(u, slot + 8, [0, T0, T1][owner])
        previous = slot
        for k in range(put(rng.choice([0, 1, 2]) if owner else 0)):
            link = LINKS + 0x20 * link_index
            link_index += 1
            w32(u, link, previous); w32(u, previous + 4, link); w32(u, link + 4, 0)
            w32(u, link + 8, [O1, O2][put(rng.choice([0, 1]))])
            w32(u, link + 0xC, put(fb(rng.choice([1.0, 500.0, 3000.0]))))
            w32(u, link + 0x10, put(rng.choice([0, 0, 1])))
            previous = link
    queued = put(rng.choice([0, 0, 1, 2, 3]))
    link = STRAT + 0x20
    u.mem_write(link, b'\0' * 0x14)
    if queued and owners[queued - 1]:
        slot = TABLE + 0xC * (queued - 1)
        last = slot
        while r32(u, last + 4):
            last = r32(u, last + 4)
        w32(u, last + 4, link); w32(u, link, last); w32(u, link + 8, ACTOR)
    w32(u, 0x0050E570, put(rng.choice([1, 2, 5])))
    for k in (2, 5, 9):
        w32(u, 0x0050E660 + 4 * k, put(rng.choice([0, 1, 2])))
    # Active actor list.
    active = put(rng.randrange(4))
    for i in range(active):
        node = ACTIVE + 0x100 * i
        w32(u, node + 4, ACTIVE + 0x100 * (i + 1) if i + 1 < active else 0)
        w32(u, node + 0x18, node + 0x80)
        w16(u, node + 0x80 + 0x94, put(rng.choice([0, 2])))
        ring = put(rng.choice([0, 1, 2]))
        w32(u, node + 0x3C, node + 0x40 if ring else 0)
        u.mem_write(node + 0x40, struct.pack('<HHHHI', 8, 4, 0, 4 if ring == 1 else 1, node + 0x60))
        w16(u, node + 0x60, put(rng.choice([0, 3])))
    w32(u, 0x00511B48, ACTIVE if active else 0)
    # Animation controller.
    controller = put(rng.choice([0, 1, 1]))
    w32(u, ACTOR + 0x50, CTRL if controller else 0)
    w32(u, CTRL + 0x58, SLOTS); w8(u, CTRL + 0x5C, 0)
    w32(u, SLOTS + 0x28, PLAYER if put(rng.choice([0, 1, 1])) else 0)
    w32(u, PLAYER + 0xB0, put(rng.choice([0, 1, 1])))
    u.mem_write(PLAYER + 0x2C, b'aid_fire\0' if put(rng.choice([0, 1, 1])) else b'xyz\0')
    # Globals.
    w32(u, 0x0050E764, put(rng.choice([0, 0, 1]))); w32(u, 0x0050E768, put(rng.choice([0, 0, 1])))
    w32(u, 0x0051248C, 0); w8(u, 0x0030F144, put(rng.choice([0, 1, 1])))
    w32(u, 0x005064D8, put(rng.choice([0, 3])))
    w32(u, 0x0054DD7C, put(rng.getrandbits(32)))
    w32(u, 0x00428684, put(rng.choice([0, 1, 7])))
    w32(u, 0x004312DC, put(fb(rng.choice([0.0, 0.0, 1.0]))))
    w32(u, 0x005096C0, put(fb(0.25))); w32(u, 0x004312C4, put(fb(2.0)))
    w32(u, ACTOR + 0x92C, put(fb(rng.choice([1.0, 3.0]))))
    w32(u, ACTOR + 0x930, put(fb(rng.choice([0.0, 0.0, 1.0]))))
    w32(u, 0x00506C6C, SEEN - 8 + 0x100); w32(u, SEEN + 0x100, SEEN)
    w8(u, SEEN + 2, put(rng.choice([0, 0, 1])))
    w32(u, 0x00506E28, put(rng.choice([0, 1, 1])))
    w32(u, 0x00506DFC, [ACTOR, O1, O2][put(rng.choice([0, 1, 2]))])
    w32(u, 0x00506E00, put(rng.choice([0x10, 7])))
    queued_notices = put(rng.randrange(5))
    for i in range(queued_notices):
        w32(u, 0x00506E04 + 8 * i, [ACTOR, O1, O2][put(rng.choice([0, 1, 2]))])
        w32(u, 0x00506E08 + 8 * i, put(rng.choice([0x10, 3])))
    w32(u, 0x00506E24, queued_notices)
    w32(u, 0x00511B40, [0, 0x93C, O1 + 0x93C][put(rng.choice([0, 1, 2]))])

def message_words(rng):
    ident = rng.choice([0x14, 0x26, 0x27, 0x31, 0x36, 0x3B, 0x43, 0x44, 0x52, 0x6E, 0x6E, 0x6E, 0x7C, 0x80, 0x82,
        0x84, 0x55, 0x73, 0x74, 0x99, 0x10])
    words = [ident, rng.choice([0, ACTOR, O1, O2, OBJ]), rng.choice([0, ACTOR, O1, O2]), 0, rng.choice([0, 1])]
    words += [0] * 4
    return words

def snapshot(r):
    u = r.u
    def link_code(a):
        if a == 0:
            return 'n'
        if TABLE <= a < TABLE + 0xF0:
            return 's%d' % ((a - TABLE) // 0xC)
        if a == STRAT + 0x20:
            return 'me'
        return 'l%d' % ((a - LINKS) // 0x20)
    words = [r16(u, STRAT + 0xE), r8(u, STRAT + 0x1A), r8(u, STRAT + 0x1C), r32(u, STRAT + 0x38),
        {0: 0, O1 + 0x1000: 1, O2 + 0x1000: 2}.get(r32(u, STRAT + 0x3C), 9), r32(u, STRAT + 0x40),
        {0: 0, OBJ: 1}.get(r32(u, STRAT + 0x44), 9), r32(u, STRAT + 0x48), r32(u, STRAT + 0x4C)]
    me = STRAT + 0x20
    words += [link_code(r32(u, me)), link_code(r32(u, me + 4)), {0: 'n', ACTOR: 'a'}.get(r32(u, me + 8), '?'),
        '%x' % r32(u, me + 0xC), r32(u, me + 0x10)]
    words += [r32(u, MIND + 0xC), {0: 0, T0: 1, T1: 2}.get(r32(u, MIND + 0x10), 9), r32(u, BODY + 0x70),
        r8(u, BODY + 0x328), r32(u, ACTOR + 0xCA0) and 1]
    for other in (O1, O2):
        words.append({0: 0, ACTOR: 1, O1: 2}.get(r32(u, other + 0x1000 + 0x50), 9))
    slots = []
    for s in range(20):
        slot = TABLE + 0xC * s
        owner = {0: 0, T0: 1, T1: 2}.get(r32(u, slot + 8), 9)
        chain = []
        a = r32(u, slot + 4)
        guard = 0
        while a and guard < 10:
            chain.append(link_code(a) + ':' + link_code(r32(u, a)) + ':%x:%d' % (r32(u, a + 0xC), r32(u, a + 0x10)))
            a = r32(u, a + 4); guard += 1
        if owner or chain:
            slots.append('%d=%d[%s]' % (s, owner, ','.join(chain)))
    notices = [r32(u, 0x00506E28), {ACTOR: 0, O1: 1, O2: 2}.get(r32(u, 0x00506DFC), 9), r32(u, 0x00506E00),
        r32(u, 0x00506E24)] + [{ACTOR: 0, O1: 1, O2: 2}.get(r32(u, 0x00506E04 + 8 * i), 9) for i in range(r32(u, 0x00506E24))]
    notices += [r32(u, 0x00506E08 + 8 * i) for i in range(r32(u, 0x00506E24))]
    words += [r32(u, 0x0054DD7C), r8(u, CTRL + 0x5C)]
    reqs = ['|'.join('%08x' % r32(u, b + o) for o in range(0x10, 0x2C, 4)) for b in r.requests]
    events = ['%08x' % r32(u, e + 0x20) for e in r.events]
    return (' '.join(str(w) for w in words) + ' S ' + ' '.join(slots) + ' N ' + ' '.join('%x' % n for n in notices)
        + ' R ' + ' '.join(reqs) + ' E ' + ' '.join(events))

def call(r, entry, regs, args=()):
    u = r.u
    w32(u, STACK, STOP)
    for i, a in enumerate(args):
        w32(u, STACK + 4 + 4 * i, a)
    u.reg_write(UC_X86_REG_ESP, STACK)
    for reg, value in regs.items():
        u.reg_write(reg, value)
    u.emu_start(entry, STOP, count=2000000)
    return u.reg_read(UC_X86_REG_EAX)

def main():
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 400
    only = sys.argv[3] if len(sys.argv) > 3 else None
    rng = random.Random(seed)
    names = [only] if only else ENTRIES
    lines, expected = [], []
    for case in range(count):
        entry = names[case % len(names)]
        queues = {q: [rng.choice([0, 1, 1]) for _ in range(12)] for q in QUEUES}
        queues['target'] = [rng.choice([0, 1, 2]) for _ in range(12)]
        queues['dist'] = [rng.choice([0, 1, 2]) for _ in range(12)]
        queues['direction'] = [rng.choice([0, 1, 2]) for _ in range(12)]
        queues['randomize'] = [rng.choice([0, 1, 2]) for _ in range(12)]
        line = [ENTRIES.index(entry)]
        for q in QUEUES:
            line += queues[q]
        r = Retail(queues)
        setup(r, rng, line, entry)
        u = r.u
        message = message_words(rng)
        for i, w in enumerate(message):
            w32(u, MESSAGE + 4 * i, w)
        line += [code(w, [0, ACTOR, O1, O2, OBJ]) if i in (1, 2) else w for i, w in enumerate(message)]
        argument = rng.choice([0x3EE, 0x3EF, 0x401, 0x40B, 0x40C, 0x416, 0x3F4, 0x3FC, 0x3FF, 0x415, 0x3E8])
        argument2 = rng.choice([0x3EE, 0x3EF, 0x40B, 0x40C, 0x3E8])
        floats = [fb(rng.choice([rng.uniform(-12, 12), 0.0, 3.1415927, -3.1415927, 6.2831855])) for _ in range(3)]
        line += [argument, argument2] + floats
        out = '-'
        if entry == 'join':
            record = rng.choice([0, T0, T1])
            line.append(code(record, [0, T0, T1]))
            result = call(r, 0x0002B910, {UC_X86_REG_EBX: STRAT, UC_X86_REG_EDI: record})
            out = '%d' % result
        elif entry in ('select', 'hostile'):
            force = rng.choice([0, 0, 1])
            line.append(force)
            if entry == 'select':
                result = call(r, 0x00139AA0, {UC_X86_REG_EAX: STRAT}, (OUT, force))
            else:
                result = call(r, 0x00139C70, {UC_X86_REG_EAX: STRAT, UC_X86_REG_EDX: force}, (OUT,))
            out = '%d %d' % (result & 0xff, code(r32(u, OUT), [0, T0, T1]))
        elif entry == 'abandon':
            target = rng.choice([T0, T1])
            line.append(code(target, [0, T0, T1]))
            out = '%d' % call(r, 0x00138410, {UC_X86_REG_EAX: STRAT, UC_X86_REG_EDX: target})
        elif entry == 'react':
            out = '%d' % call(r, 0x001384F0, {UC_X86_REG_EAX: STRAT, UC_X86_REG_ECX: MESSAGE})
        elif entry == 'contact':
            u.reg_write(UC_X86_REG_XMM1, floats[0])
            out = '%d' % call(r, 0x001395C0, {UC_X86_REG_EAX: STRAT, UC_X86_REG_EDX: MESSAGE})
        elif entry == 'cower':
            out = '%d' % call(r, 0x001398A0, {UC_X86_REG_EAX: STRAT})
        elif entry == 'startle':
            out = '%d' % call(r, 0x001399A0, {UC_X86_REG_EAX: STRAT})
        elif entry == 'notice':
            out = '%d' % call(r, 0x0013A4F0, {UC_X86_REG_ESI: STRAT}, (0x10,))
        elif entry == 'roam':
            result = call(r, 0x0013A0A0, {UC_X86_REG_EAX: PARAMS, UC_X86_REG_EBX: ACTOR})
            out = '%d' % (1 if result else 0)
        elif entry == 'reached':
            out = '%d' % call(r, 0x001383A0, {UC_X86_REG_EAX: STRAT})
        elif entry == 'update':
            w16(u, STRAT + 0xE, argument)
            call(r, 0x000B45D0, {}, (STRAT,))
        elif entry == 'message':
            call(r, 0x000B46B0, {}, (STRAT, MESSAGE))
        elif entry == 'enter':
            call(r, 0x000B4940, {}, (STRAT, argument, argument2))
        elif entry == 'leave':
            call(r, 0x000B4A60, {}, (STRAT, argument, argument2))
        elif entry == 'ctor':
            call(r, 0x000B4570, {}, (STRAT,))
        elif entry == 'dtor':
            call(r, 0x000B45B0, {}, (STRAT,))
        elif entry == 'angles':
            a = call(r, 0x00100D00, {}, (floats[0], floats[1]))
            first = r.u.reg_read(UC_X86_REG_XMM0) & 0xffffffff
            call(r, 0x00100B80, {}, (floats[0], floats[1], floats[2]))
            second = r.u.reg_read(UC_X86_REG_XMM0) & 0xffffffff
            call(r, 0x000B40E0, {}, (fb(rng_fraction(floats[2])), floats[1]))
            third = r.u.reg_read(UC_X86_REG_XMM0) & 0xffffffff
            out = '%08x %08x %08x' % (first, second, third)
        elif entry == 'heading':
            target = rng.choice([T0, T1])
            line.append(code(target, [0, T0, T1]))
            # 0x000B42D0 runs 0x000B4160 with EBX parameters as its argument slot.
            call(r, 0x000B42D0, {UC_X86_REG_EAX: ACTOR, UC_X86_REG_EBX: PARAMS}, (target,))
        elif entry == 'model':
            index = rng.choice([0, 0, 1, 2])
            which = rng.choice([O1, O2])
            line += [index, code(which, [0, ACTOR, O1, O2])]
            result = call(r, 0x000409D0, {UC_X86_REG_EAX: which}, (4, index))
            base = MDATA + (0 if which == O1 else 0x200)
            out = '%d' % (-1 if result == 0 else (result - base - 4) // 0x34)
        line += [r32(u, PARAMS + 4 * i) for i in range(16)]
        expected.append(out + ' | ' + snapshot(r) + ' | ' + ' ; '.join(r.log))
        lines.append(' '.join('%u' % (v & 0xffffffff) for v in line))
    text = str(count) + '\n' + '\n'.join(lines) + '\n'
    open(os.path.join(ROOT, 'cache', 'oracle', 'imp_strategy_input.txt'), 'w').write(text)
    result = subprocess.run([harness('imp_strategy_harness')], input=text, capture_output=True, text=True)
    if result.returncode != 0:
        print('harness exit', result.returncode, result.stderr[-600:])
    got = result.stdout.splitlines()
    bad = 0
    for i, (want, have) in enumerate(zip(expected, got)):
        if want.split() != have.split():
            bad += 1
            if bad <= 6:
                print('MISMATCH', i, names[i % len(names)], '\n exp', want[:1500], '\n got', have[:1500])
    bad += abs(len(expected) - len(got))
    print('cases', len(expected), len(got), 'mismatches', bad)

def rng_fraction(bits):
    value = struct.unpack('<f', struct.pack('<I', bits))[0]
    return abs(value) / 12.0

if __name__ == '__main__':
    main()
