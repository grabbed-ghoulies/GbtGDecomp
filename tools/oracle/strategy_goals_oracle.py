"""Call-sequence differential check for the strategy goal batch (0x0002C130, 0x00138A30, 0x00139010,
0x00138F60, 0x001391D0, 0x00135790, 0x00135540, 0x00135AB0, 0x00135B40, 0x00135420, 0x00139E90,
0x0013A0F0, 0x0013D400, 0x0002A080, 0x000285B0, 0x00028500, 0x00028580, 0x00124800, 0x0002B540,
0x00028150, 0x0013E6B0, 0x0003AC90).

Every callee outside the batch is replaced by a logging stub on both sides and returns scripted
values from the same per-case queues. The retail functions run under Unicorn; the host harness
links player_strategy_goals.cpp with matching stubs. Both print the call log and the touched
actor, strategy, mind and body words.
Needs cache/oracle/mem_00011000.bin and mem_00210000.bin."""
import os, random, struct, subprocess, sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP)
from retail_cpu import ROOT, STOP, STACK, HEAP, w32, r32, harness

def r16(u, a): return struct.unpack('<H', u.mem_read(a, 2))[0]
def w16(u, a, v): u.mem_write(a, struct.pack('<H', v & 0xffff))
def rf(u, a): return struct.unpack('<f', u.mem_read(a, 4))[0]
def wf(u, a, v): u.mem_write(a, struct.pack('<f', v))
def fb(v): return struct.unpack('<I', struct.pack('<f', v))[0]
def cstr(u, a): return bytes(u.mem_read(a, 0x40)).split(b'\0')[0].decode('latin1')

STUBS = 0x00F20000
GOAL_NAMES = {0x321808: 'ghouly_intro', 0x321828: 'drone_waitforscare', 0x31B548: 'celebrate'}
# Callee address -> name. Every one becomes a RET with a logging hook.
CALLEES = {
    0x00027650: 'select', 0x00027510: 'lookup', 0x00029E80: 'clear', 0x0003AE00: 'pose',
    0x0001FD20: 'unlatch', 0x0001FC40: 'latch', 0x000307C0: 'release', 0x0002A190: 'event',
    0x000E9210: 'drop', 0x00053170: 'effect', 0x0012A260: 'sound', 0x0001EF10: 'bit',
    0x00129640: 'target', 0x00124640: 'sphere', 0x001244D0: 'box', 0x0002A030: 'settarget',
    0x00111420: 'spawn', 0x0012CC50: 'path', 0x00029D90: 'request', 0x00145FF0: 'classify',
    0x001460C0: 'range', 0x0002A8E0: 'goal', 0x00028240: 'remove', 0x00112CC0: 'bind',
    0x0002B7F0: 'decay', 0x001414C0: 'controlled', 0x00135700: 'finish', 0x00108270: 'input',
    0x0002B8D0: 'state', 0x00141760: 'differs', 0x0013A2A0: 'saved', 0x00028D50: 'resume',
    0x00042360: 'timer', 0x000412A0: 'position', 0x00041820: 'euler',
    0x00028360: 'mask', 0x00028210: 'drain', 0x00112A20: 'detach', 0x00028D00: 'player',
    0x000283D0: 'latchkind', 0x000F72D0: 'alloc',
}
QUEUES = ['select', 'lookup', 'effect', 'bit', 'target', 'sphere', 'box', 'spawn', 'path', 'classify',
          'range', 'decay', 'controlled', 'differs', 'saved', 'resume', 'timer', 'key', 'mask', 'player', 'latchkind']

OTHER1, OTHER2, TARGET2, CARRIED, MESSAGE = HEAP + 0xD000, HEAP + 0xE000, HEAP + 0x8800, HEAP + 0x8C00, HEAP + 0xF800
ACTOR, BODY, MIND, STRAT, RING, ROWS, BITS, ATTACH, TABLE = (HEAP + 0x0000, HEAP + 0x2000, HEAP + 0x3000,
    HEAP + 0x3800, HEAP + 0x3900, HEAP + 0x3A00, HEAP + 0x3C00, HEAP + 0x4000, HEAP + 0x5000)
LATCH, TARGET, POOL, RECORDS, SCRIPT, EVENTS, REQUESTS = (HEAP + 0x6000, HEAP + 0x8000, HEAP + 0x9000,
    HEAP + 0x9100, HEAP + 0x9800, HEAP + 0xA000, HEAP + 0xB000)
VT_STRAT, VT_BODY, VT_LATCH = STUBS + 0x800, STUBS + 0x900, STUBS + 0xA00
LEAVING, ENTERING, SLOT, LATCH_MSG = STUBS + 0x400, STUBS + 0x410, STUBS + 0x420, STUBS + 0x430
NOTICE, VT_ACTOR = STUBS + 0x440, STUBS + 0xB00
ACTOR_NAMES = {ACTOR: 'actor', OTHER1: 'other1', OTHER2: 'other2', 0: 'none'}

class Retail:
    def __init__(self, queues):
        u = Uc(UC_ARCH_X86, UC_MODE_32)
        u.mem_map(0, 0x1000)
        u.mem_map(0x00010000, 0x150000)
        u.mem_write(0x00011000, open(os.path.join(ROOT, 'cache', 'oracle', 'mem_00011000.bin'), 'rb').read())
        u.mem_map(0x00210000, 0x130000)
        u.mem_write(0x00210000, open(os.path.join(ROOT, 'cache', 'oracle', 'mem_00210000.bin'), 'rb').read())
        u.mem_map(0x00400000, 0x200000)
        u.mem_map(STOP, 0x1000); u.mem_write(STOP, b'\xf4')
        u.mem_map(STUBS, 0x1000); u.mem_write(STUBS, b'\xc3' * 0x1000)
        u.mem_map(HEAP, 0x100000)
        u.mem_map(STACK - 0x10000, 0x20000)
        self.u, self.queues, self.log = u, {k: list(v) for k, v in queues.items()}, []
        self.next_block = HEAP + 0x80000
        for address, name in CALLEES.items():
            u.mem_write(address, b'\xc3')
            u.hook_add(UC_HOOK_CODE, self.dispatch, begin=address, end=address, user_data=name)
        for address, name in ((LEAVING, 'leaving'), (ENTERING, 'entering'), (SLOT, 'slot'), (LATCH_MSG, 'message'),
                (NOTICE, 'notice')):
            u.hook_add(UC_HOOK_CODE, self.dispatch, begin=address, end=address, user_data=name)

    def pop(self, name):
        q = self.queues[name]
        return q.pop(0) if q else 0

    def arg(self, k):
        return r32(self.u, self.u.reg_read(UC_X86_REG_ESP) + 4 + 4 * k)

    def reg(self, r):
        return self.u.reg_read(r)

    def ret(self, value):
        self.u.reg_write(UC_X86_REG_EAX, value & 0xffffffff)

    def block(self, n):
        b = self.next_block
        self.next_block += (n + 0xF) & ~0xF
        self.u.mem_write(b, b'\x00' * n)
        return b

    def dispatch(self, uc, address, size, name):
        u, log = self.u, self.log
        if name == 'select':
            key = self.reg(UC_X86_REG_EAX) & 0xffff
            v = self.pop('select'); log.append('select %x %d' % (key, v)); self.ret(v)
        elif name == 'lookup':
            key = self.reg(UC_X86_REG_EDI) & 0xffff
            v = self.pop('lookup'); log.append('lookup %x %d' % (key, v)); self.ret(TABLE + 0x100 if v else 0)
        elif name == 'clear':
            log.append('clear')
        elif name == 'pose':
            log.append('pose %s' % ('actor' if self.reg(UC_X86_REG_ESI) == ACTOR else 'attached' if self.reg(UC_X86_REG_ESI) == ATTACH else '?'))
        elif name in ('latch', 'unlatch'):
            log.append(name)
        elif name == 'release':
            log.append('release %d' % self.arg(0));
        elif name == 'event':
            kind, duration = self.arg(0), self.arg(1)
            node = self.block(0x40)
            log.append('event %x %08x' % (kind, duration)); self.ret(node)
            self.last_event = node
        elif name == 'drop':
            node = self.reg(UC_X86_REG_EAX)
            log.append('drop %d' % ((node - EVENTS) // 0x40))
        elif name == 'effect':
            name_ptr, options = self.arg(0), self.arg(1)
            words = [r32(u, options + 4 * i) for i in range(7)]
            v = self.pop('effect')
            log.append('effect %s %s %d' % (cstr(u, name_ptr), ' '.join('%08x' % w for w in words), v)); self.ret(v)
        elif name == 'sound':
            log.append('sound %s %x %x %08x %d' % (cstr(u, self.reg(UC_X86_REG_EAX)), self.reg(UC_X86_REG_EBX) - ACTOR,
                self.arg(1) == ACTOR, self.arg(5), self.arg(0)))
        elif name == 'bit':
            index = self.reg(UC_X86_REG_ECX)
            bits = r32(u, r32(u, self.reg(UC_X86_REG_EAX) + 0x15C) + 8)
            v = (u.mem_read(bits + (index >> 3), 1)[0] >> (index & 7)) & 1
            log.append('bit %x %d' % (index, v)); self.ret(v)
        elif name == 'target':
            v = self.pop('target')
            log.append('target %08x %08x %08x %x %x' % (self.reg(UC_X86_REG_EAX), self.arg(1) - ACTOR, self.arg(5), self.arg(2) & 0xffff, self.arg(3) & 0xffff))
            wf(u, self.arg(4), 3.5)
            self.ret(TARGET if v else 0)
        elif name in ('sphere', 'box'):
            v = self.pop(name)
            point = self.reg(UC_X86_REG_ECX) if name == 'sphere' else self.reg(UC_X86_REG_EAX)
            distance = self.arg(0) if name == 'sphere' else self.reg(UC_X86_REG_EBX)
            log.append('%s %08x %08x %08x %08x' % (name, r32(u, point), r32(u, point + 4), r32(u, point + 8), r32(u, distance)))
            self.ret(v)
        elif name == 'settarget':
            log.append('settarget %08x' % self.reg(UC_X86_REG_EAX))
            w32(u, MIND + 0x10, self.reg(UC_X86_REG_EAX))
        elif name == 'spawn':
            v = self.pop('spawn')
            out = self.reg(UC_X86_REG_ESI)
            center = self.arg(1)
            log.append('spawn %x %08x %08x %08x %x %08x %08x %08x %x' % (self.reg(UC_X86_REG_EAX), r32(u, center), r32(u, center + 4),
                r32(u, center + 8), self.arg(2), self.arg(3), self.arg(4), self.arg(6) - ACTOR, self.arg(7) & 0xffff))
            if v:
                for i, value in enumerate((fb(11.0), fb(12.0), fb(13.0))):
                    w32(u, out + 4 * i, value)
            self.ret(v)
        elif name == 'path':
            v = self.pop('path')
            source, destination = self.reg(UC_X86_REG_EAX), self.arg(0)
            log.append('path %08x %08x %08x %08x %08x %08x %d %d %d %d %x %x' % tuple([r32(u, source + 4 * i) for i in range(3)] +
                [r32(u, destination + 4 * i) for i in range(3)] + [self.arg(1), self.arg(2), self.arg(3), self.arg(4),
                self.arg(5) - ACTOR, self.arg(7) & 0xffff]))
            self.ret(v)
        elif name == 'request':
            kind = self.reg(UC_X86_REG_EBX)
            block = self.block(0x40)
            self.requests.append(block)
            log.append('request %x' % kind); self.ret(block)
        elif name == 'classify':
            v = self.pop('classify'); log.append('classify %x' % (self.reg(UC_X86_REG_EBX) & 0xffff)); self.ret(v)
        elif name == 'range':
            v = self.pop('range'); log.append('range'); self.ret(v)
        elif name == 'goal':
            log.append('goal %s' % GOAL_NAMES.get(self.reg(UC_X86_REG_EAX), '?')); self.ret(1)
        elif name == 'remove':
            log.append('remove')
            self.pop_ring()
        elif name == 'bind':
            log.append('bind')
        elif name == 'decay':
            v = self.pop('decay'); log.append('decay %d' % (self.reg(UC_X86_REG_EAX) == STRAT)); self.ret(v)
        elif name == 'controlled':
            v = self.pop('controlled'); log.append('controlled %d' % (self.reg(UC_X86_REG_EAX) == TARGET)); self.ret(v)
        elif name == 'finish':
            log.append('finish %d' % self.arg(0))
        elif name == 'input':
            log.append('input %x' % self.arg(0))
        elif name == 'state':
            log.append('state %x' % (self.reg(UC_X86_REG_EDI) & 0xffff))
        elif name == 'differs':
            v = self.pop('differs')
            log.append('differs %d %x %x' % (self.reg(UC_X86_REG_EAX) == TARGET, self.reg(UC_X86_REG_ECX) & 0xffff, self.arg(0) - ACTOR)); self.ret(v)
        elif name == 'saved':
            v = self.pop('saved'); log.append('saved'); self.ret(v)
        elif name == 'resume':
            v = self.pop('resume'); log.append('resume %d' % (self.reg(UC_X86_REG_EAX) == ACTOR)); self.ret(v)
        elif name == 'timer':
            v = self.pop('timer')
            timer = self.reg(UC_X86_REG_EAX)
            log.append('timer %08x' % r32(u, timer))
            w32(u, timer, fb(0.0) if v else fb(0.5))
        elif name == 'position':
            source, out = self.reg(UC_X86_REG_ESI), self.reg(UC_X86_REG_EDI)
            for i in range(3):
                w32(u, out + 4 * i, r32(u, source + 8 + 4 * i))
        elif name == 'euler':
            source, out = self.reg(UC_X86_REG_ESI), self.reg(UC_X86_REG_EDI)
            for i in range(3):
                w32(u, out + 4 * i, r32(u, source + 0x14 + 4 * i))
        elif name in ('leaving', 'entering'):
            log.append('%s %x %x' % (name, self.arg(1) & 0xffff, self.arg(2) & 0xffff))
        elif name == 'slot':
            raise RuntimeError('slot')
        elif name == 'mask':
            v = self.pop('mask'); log.append('mask %d' % (self.reg(UC_X86_REG_EAX) == ACTOR)); self.ret(v)
        elif name == 'drain':
            log.append('drain'); w32(u, ACTOR + 0x980, 0)
        elif name == 'detach':
            log.append('detach %d' % (self.reg(UC_X86_REG_EAX) == STRAT + 0x20))
        elif name == 'player':
            v = self.pop('player'); log.append('player %s' % ACTOR_NAMES.get(self.reg(UC_X86_REG_EAX), '?')); self.ret(v)
        elif name == 'latchkind':
            v = self.pop('latchkind'); log.append('latchkind %x' % (self.reg(UC_X86_REG_EAX) & 0xffff)); self.ret(v)
        elif name == 'alloc':
            log.append('alloc'); self.ret(self.block(self.arg(0)))
        elif name == 'notice':
            log.append('notice %x' % r32(u, self.arg(1)))
        elif name == 'message':
            m = self.arg(1)
            log.append('message %x %08x %08x %08x %08x %08x %08x %08x %08x' % tuple(r32(u, m + 4 * i) for i in range(9)))

    def pop_ring(self):
        u = self.u
        ring = r32(u, ACTOR + 0x980)
        if ring == 0:
            return
        head, tail, capacity = r16(u, ring + 4), r16(u, ring + 6), r16(u, ring + 2)
        if tail == capacity or head == tail:
            w32(u, ACTOR + 0x980, 0)
        else:
            w16(u, ring + 4, (head + 1) % capacity)

def body_slot_hooks(r, keys):
    u = r.u
    for offset in (0x24, 0x38, 0x3C, 0x50, 0x54, 0x58):
        stub = STUBS + 0x600 + offset
        w32(u, VT_BODY + offset, stub)
        def hook(uc, address, size, off):
            k = r.pop('key')
            r.log.append('key %x %x' % (off, k))
            r.ret(k)
        u.hook_add(UC_HOOK_CODE, hook, begin=stub, end=stub, user_data=offset)

def setup(r, rng, line):
    u = r.u
    r.requests = []
    def word(v):
        line.append(v & 0xffffffff)
        return v
    # Strategy.
    w32(u, STRAT, VT_STRAT); w32(u, VT_STRAT + 0x14, ENTERING); w32(u, VT_STRAT + 0x18, LEAVING)
    w32(u, STRAT + 8, ACTOR)
    for off in (0x10, 0x12, 0x18):
        w16(u, STRAT + off, word(rng.choice([0x3E9, 0x3F2, 0x3FF, 0x415])))
    wf(u, STRAT + 0x34, rng.choice([0.0, 0.5, -1.0])); line.append(r32(u, STRAT + 0x34))
    # Actor.
    w16(u, ACTOR + 6, word(rng.choice([0, 3, 5])))
    for i in range(6):
        wf(u, ACTOR + 8 + 4 * i, rng.uniform(-50, 50)); line.append(r32(u, ACTOR + 8 + 4 * i))
    w32(u, ACTOR + 0x50, word(rng.choice([0, 1])))
    w16(u, ACTOR + 0x1FC, word(rng.choice([0, 2, 2, 7])))
    wf(u, ACTOR + 0x92C, 1.0); wf(u, ACTOR + 0x930, 0.0)
    w32(u, ACTOR + 0x958, MIND); w32(u, ACTOR + 0x95C, BODY); w32(u, ACTOR + 0x960, word(0x7000))
    w32(u, ACTOR + 0x984, word(rng.choice([0, 1])))
    for off in (0x988, 0x989, 0x98A):
        u.mem_write(ACTOR + off, bytes([word(rng.choice([0, 0, 1]))]))
    w32(u, ACTOR + 0x98C, word(rng.choice([0, 0, RECORDS])))
    w16(u, ACTOR + 0x990, word(rng.randrange(0x18)))
    wf(u, ACTOR + 0x994, rng.choice([0.0, 0.5, 2.0])); line.append(r32(u, ACTOR + 0x994))
    names = rng.choice([[], ['aid_fx_a'], ['aid_fx_a', 'plain', 'aid_fx_c']])
    for i, n in enumerate(names):
        u.mem_write(ACTOR + 0x998 + 0x80 * i, n.encode() + b'\0')
    line.append(len(names))
    sound = rng.choice(['', 'snd_win'])
    u.mem_write(ACTOR + 0xB18, sound.encode() + b'\0'); line.append(1 if sound else 0)
    w32(u, ACTOR + 0xCA4, ACTOR + 0x40)
    w32(u, ACTOR + 0xCA8, word(rng.choice([0, 1])) and ACTOR + 0x48)
    for off in (0xCAC, 0xCB0, 0xCB4):
        wf(u, ACTOR + off, rng.uniform(-10, 10)); line.append(r32(u, ACTOR + off))
    w32(u, ACTOR + 0xCB8, word(rng.choice([0, 1])))
    w16(u, ACTOR + 0xCDE, word(rng.choice([0, 1])))
    w32(u, ACTOR + 0xCEC, word(rng.choice([0, 1])))
    # Goal ring.
    if rng.random() < 0.85:
        capacity = 4
        head = rng.randrange(4)
        count = rng.choice([0, 1, 1, 2, 3])
        tail = capacity if count == 0 else (head + count - 1) % capacity
        u.mem_write(RING, struct.pack('<HHHHI', 8, capacity, head, tail, ROWS))
        kinds = [rng.choice([0, 1, 2, 3, 3, 3, 6, 7, 9, 0x12, 0x15, 0x1A, 0x1C, 0x23, 0x25, rng.randrange(0x26)]) for _ in range(4)]
        for i, k in enumerate(kinds):
            w16(u, ROWS + 8 * i, k)
        w32(u, ACTOR + 0x980, RING)
        line += [1, head, tail] + kinds
    else:
        w32(u, ACTOR + 0x980, 0); line.append(0)
    w16(u, 0, word(rng.choice([0, 9, 0x15])))  # Word read through an empty ring.
    # Body.
    w32(u, BODY, VT_BODY); w32(u, BODY + 8, ACTOR); w32(u, BODY + 0x15C, BITS); w32(u, BITS + 8, BITS + 0x10)
    bits = [rng.getrandbits(8) for _ in range(8)]
    u.mem_write(BITS + 0x10, bytes(bits)); line += bits
    attached = rng.choice([0, 0, 1])
    w32(u, BODY + 0x48, ATTACH if attached else 0); line.append(attached)
    u.mem_write(ATTACH + 0x93D, bytes([word(rng.choice([0, 1, 4]))]))
    u.mem_write(BODY + 0x6A, bytes([word(rng.choice([0, 0, 1]))]))
    w32(u, BODY + 0x73C, word(rng.choice([0, 1])) and TABLE)
    w16(u, BODY + 0x748, word(rng.choice([0xBE, 0xBF, 0xC0, 0])))
    wf(u, BODY + 0x884, 2.0); wf(u, BODY + 0x888, 3.0)
    # Mind.
    w32(u, MIND + 8, ACTOR)
    wf(u, ACTOR + 0x34, 1.5); wf(u, ACTOR + 0x38, 2.5)
    w32(u, MIND + 0x10, word(rng.choice([0, 0, TARGET])))
    w32(u, MIND + 0x58, word(rng.choice([0, 0, 1])))
    w32(u, MIND + 0x64, word(rng.getrandbits(8)))
    w32(u, MIND + 0x68, word(rng.getrandbits(8)))
    w32(u, MIND + 0xC, word(0x77))
    events = rng.choice([[], [3], [1, 3], [2]])
    for i, kind in enumerate(events):
        node = EVENTS + 0x40 * i
        w16(u, node + 8, kind)
        w32(u, node + 4, EVENTS + 0x40 * (i + 1) if i + 1 < len(events) else 0)
    w32(u, MIND + 0x28, EVENTS if events else 0); line += [len(events)] + events
    # Target.
    for i in range(3):
        wf(u, TARGET + 0x20 + 4 * i, rng.uniform(-5, 5)); line.append(r32(u, TARGET + 0x20 + 4 * i))
    u.mem_write(TARGET + 0x14, bytes([word(rng.choice([0, 1]))]))
    w32(u, TARGET + 0x10, word(0x1234))
    # Volume record.
    w16(u, RECORDS + 8, word(rng.choice([0, 1, 2])))
    # Latch list.
    nodes = rng.choice([[], [6], [3, 6], [3]])
    for i, tag in enumerate(nodes):
        obj = LATCH + 0x1000 * i
        w32(u, obj, VT_LATCH); w16(u, obj + 6, tag)
        w32(u, obj + 0x904, LATCH + 0x1000 * (i + 1) + 0x900 if i + 1 < len(nodes) else 0)
    w32(u, VT_LATCH + 0xC, LATCH_MSG)
    w32(u, 0x00511AA4, LATCH + 0x900 if nodes else 0); line += [len(nodes)] + nodes
    # Globals.
    w32(u, 0x0050EA9C, word(rng.getrandbits(3)))
    # Occupancy pool and script records for 0x00124800.
    n = rng.randrange(4)
    u.mem_write(POOL, struct.pack('<HHIII', 0x38, 4, RECORDS + 0x38, RECORDS + 0x38 * (n + 1), RECORDS + 0x38 * (n + 1)))
    ids = [rng.randrange(2) for _ in range(n)]
    for i, value in enumerate(ids):
        w16(u, RECORDS + 0x38 * (i + 1) + 0xA, value)
    w32(u, 0x00512344, POOL if rng.random() < 0.9 else 0); line += [1 if r32(u, 0x00512344) else 0, n] + ids
    rows = [(rng.randrange(2), rng.choice([0, 2, 2, 7])) for _ in range(rng.randrange(4))]
    w32(u, SCRIPT, len(rows))
    cursor = SCRIPT + 8
    for key, value in rows:
        u.mem_write(cursor, struct.pack('<IHHH', 0xC, 0, key, value)); cursor += 0xC
    w32(u, 0x0051108C, SCRIPT); line += [len(rows)] + [v for row in rows for v in row]
    w32(u, 0x0042A434, word(rng.choice([0xFFFFFFFF, 2])))
    w32(u, 0x00506930 + 8, HEAP + 0xC000)
    # Message actors, targets and the carried object.
    w32(u, ACTOR, VT_ACTOR); w32(u, VT_ACTOR + 0xC, NOTICE)
    w16(u, ACTOR + 0x1FE, 0)
    for other in (OTHER1, OTHER2):
        w16(u, other + 0x1FC, word(rng.choice([0, 2, 7])))
        w16(u, other + 0x1FE, word(rng.choice([0, 2, 7])))
        w32(u, other + 0x8F8, rng.choice([0, TARGET, TARGET2]))
        line.append({0: 0, TARGET: 1, TARGET2: 2}[r32(u, other + 0x8F8)])
        w32(u, other + 0x95C, other + 0x800)
        w32(u, other + 0x800 + 0x274, rng.choice([0, OTHER1, OTHER2]))
        line.append({0: 0, OTHER1: 1, OTHER2: 2}[r32(u, other + 0x800 + 0x274)])
    u.mem_write(TARGET + 0x18, bytes([word(rng.choice([0, 0, 1]))]))
    u.mem_write(TARGET2 + 0x18, bytes([word(rng.choice([0, 1]))]))
    u.mem_write(TARGET2 + 0x14, bytes([word(rng.choice([0, 1]))]))
    w32(u, TARGET2 + 0x10, 0x5678)
    carried = word(rng.choice([0, 1, 2]))
    w32(u, BODY + 0x268, CARRIED if carried else 0)
    u.mem_write(CARRIED + 0x199, bytes([1 if carried == 2 else 0]))
    u.mem_write(STRAT + 0x1C, bytes([word(rng.choice([0, 0, 1]))]))
    w16(u, STRAT + 0xC, word(0x3E8)); w16(u, STRAT + 0x14, word(0x3F4)); w16(u, STRAT + 0x16, word(0x418))
    w16(u, ACTOR + 0xCDC, word(rng.choice([0, 1, 1, 2])))
    w32(u, ACTOR + 0x93C, word(rng.choice([0, 0, 1])))
    u.mem_write(ACTOR + 0x969, bytes([word(rng.choice([0, 0, 1]))]))
    wf(u, ACTOR + 0x978, rng.choice([0.0, 0.0, 1.0])); line.append(r32(u, ACTOR + 0x978))
    w16(u, ACTOR + 0x97C, word(rng.choice([0, 1, 4]))); w16(u, ACTOR + 0x97E, word(rng.choice([0, 1, 5])))
    refs = [word(rng.choice([0, 0, 1])), word(rng.choice([0, 0, 1]))]
    w32(u, 0x0050E764, refs[0]); w32(u, 0x0050E768, refs[1])
    w32(u, 0x0051248C, 0); u.mem_write(0x0030F144, bytes([word(rng.choice([0, 1, 1]))]))
    # Clock for 0x000285B0.
    wf(u, 0x004312DC, 0.0); wf(u, 0x005096C0, 0.25); wf(u, 0x004312C4, 2.0)

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

ENTRIES = {
    'base': (0x0002C130, lambda r: ({}, (STRAT,))),
    'kind': (0x00135790, lambda r: ({UC_X86_REG_ECX: ACTOR}, (HEAP + 0xF000,))),
    'mode': (0x00135540, lambda r: ({UC_X86_REG_EAX: ACTOR, UC_X86_REG_ESI: HEAP + 0xF000}, ())),
    'scripted': (0x00124800, lambda r: ({}, ())),
    'effects': (0x00028150, lambda r: ({UC_X86_REG_EAX: ACTOR}, ())),
    'goal': (0x00138A30, lambda r: ({UC_X86_REG_EBX: STRAT}, ())),
    'step': (0x00139010, lambda r: ({UC_X86_REG_EAX: ACTOR, UC_X86_REG_ECX: STRAT}, ())),
    'begin': (0x00138F60, lambda r: ({UC_X86_REG_EDI: STRAT}, (ACTOR,))),
    'carry': (0x00135AB0, lambda r: ({UC_X86_REG_ESI: BODY}, ())),
    'message': (0x0002BC10, lambda r: ({}, (STRAT, MESSAGE))),
}

def snapshot(r):
    u = r.u
    ring = r32(u, ACTOR + 0x980)
    ring_words = [1 if ring else 0]
    if ring:
        stride, capacity, head, tail = (r16(u, ring + 2 * i) for i in range(4))
        ring_words += [stride, capacity, head, tail]
        if tail != capacity:
            row = r32(u, ring + 8) + stride * tail
            ring_words += [r16(u, row)] + [r32(u, row + o) for o in range(4, 0x20, 4)]
    words = ring_words + [r32(u, ACTOR + o) for o in (0x984, 0x98C, 0x994, 0xCA0, 0xCAC, 0xCB0, 0xCB4, 0xCB8)]
    words += [u.mem_read(ACTOR + o, 1)[0] for o in (0x988, 0x989, 0x98A)]
    words += [r16(u, STRAT + 0xE), r32(u, STRAT + 0x34), r32(u, MIND + 0x10), r32(u, MIND + 0xC)]
    words += [r32(u, MIND + o) for o in (0x70, 0x74, 0x78)]
    words += [r32(u, 0x0042A434), u.mem_read(HEAP + 0xC000 + 0x18, 1)[0], r32(u, ACTOR + 0x50)]
    reqs = ['|'.join('%08x' % r32(u, b + o) for o in range(0x10, 0x2C, 4)) for b in r.requests]
    events = ['%08x' % r32(u, r.last_event + 0x20)] if hasattr(r, 'last_event') else []
    return ' '.join('%x' % w for w in words) + ' R ' + ' '.join(reqs) + ' E ' + ' '.join(events)

def main():
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 300
    only = sys.argv[3] if len(sys.argv) > 3 else None
    rng = random.Random(seed)
    lines, expected = [], []
    names = [only] if only else list(ENTRIES)
    for case in range(count):
        entry = names[case % len(names)]
        queues = {q: [rng.choice([0, 1, 1]) for _ in range(12)] for q in QUEUES}
        queues['key'] = [rng.choice([0, 0x31, 0x77]) for _ in range(12)]
        queues['effect'] = [rng.choice([0, 1]) for _ in range(12)]
        line = [list(ENTRIES).index(entry)]
        for q in QUEUES:
            line += queues[q]
        r = Retail(queues)
        body_slot_hooks(r, queues['key'])
        setup(r, rng, line)
        argument = 0
        if entry == 'base':
            state = rng.choice([1000, 0x3E9, 0x3F4, 0x3F2, 0x418, 0x3F3, 0x3FB, 0x3FB, 0x3FB, 0x3FF, 0x3FF, 0x415, 0x400])
            w16(r.u, STRAT + 0xE, state); argument = state
        elif entry == 'kind':
            argument = rng.randrange(0x28)
        elif entry == 'scripted':
            argument = rng.choice([0, 2, 7, 3])
        elif entry == 'message':
            argument = rng.choice([1, 0x24, 0x26, 0x27, 0x32, 0x4B, 0x4D, 0x4D, 0x5D, 0x64, 0x65, 0x6E, 0x70, 0x70,
                0x72, 0x72, 0x74, 0x74, 0x7D, 0x8F, 0x8F, 2, 0x90])
            words = [argument, rng.choice([0, 1, ACTOR, OTHER1, OTHER2, 0x3F2]), rng.choice([0, OTHER1, OTHER2])]
            words += [fb(rng.uniform(-40, 40)) for _ in range(2)] + [fb(rng.uniform(-5, 5)) for _ in range(3)]
            words += [rng.choice([0, 0x4444])]
            for i, w in enumerate(words):
                w32(r.u, MESSAGE + 4 * i, w)
            words[4] = words[4] if argument != 0x6E else rng.choice([0, 1])
            w32(r.u, MESSAGE + 0x10, words[4])
            line += [{0: 0, ACTOR: 1, OTHER1: 2, OTHER2: 3}.get(w, 4 + w) if i in (1, 2) else w for i, w in enumerate(words)]
        line.append(argument)
        address, regs_args = ENTRIES[entry]
        regs, args = regs_args(r)
        if entry == 'kind':
            regs[UC_X86_REG_EAX] = argument
        if entry == 'scripted':
            args = (argument,)
        result = call(r, address, regs, args)
        out = '-' if entry in ('base', 'effects', 'message') else '%x' % result
        if entry == 'scripted':
            out = '%d' % (-1 if result == 0 else (result - RECORDS) // 0x38)
        if entry in ('kind', 'mode'):
            out += ' %x' % r32(r.u, HEAP + 0xF000 if entry == 'kind' else HEAP + 0xF000)
        expected.append(out + ' | ' + snapshot(r) + ' | ' + ' ; '.join(r.log))
        lines.append(' '.join('%u' % (v & 0xffffffff) for v in line))
    text = str(count) + '\n' + '\n'.join(lines) + '\n'
    open(os.path.join(ROOT, 'cache', 'oracle', 'strategy_input.txt'), 'w').write(text)
    result = subprocess.run([harness('strategy_goals_harness')], input=text, capture_output=True, text=True)
    if result.returncode != 0:
        print('harness exit', result.returncode, result.stderr[-400:])
    got = result.stdout.splitlines()
    bad = 0
    for i, (want, have) in enumerate(zip(expected, got)):
        if want.split() != have.split():
            bad += 1
            if bad <= 5:
                print('MISMATCH', i, names[i % len(names)], '\n exp', want[:1200], '\n got', have[:1200])
    bad += abs(len(expected) - len(got))
    print('cases', len(expected), len(got), 'mismatches', bad)

if __name__ == '__main__':
    main()
