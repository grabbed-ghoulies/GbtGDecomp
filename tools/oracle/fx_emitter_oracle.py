"""Call-sequence differential check for the FX anim emitter (type 0xD) and the actor queries it uses:
0x00109DB0..0x0010A6D0 (sizes, constructor, destructor, message, launch, flight, update, player
scan, collection, visibility and draw), 0x00069590 burst parameters, 0x0003D860 view radius,
0x00040550 matrix reach, 0x00041CB0 sound check, 0x00042560/0x000425D0 shape walks, 0x001205F0
cylinder test, 0x00159AD0 frustum test and the intro notice pair 0x0013FF00/0x0013FF70.

Callees outside the batch are logging stubs on both sides, fed from the same per-case queues.
Usage: fx_emitter_oracle.py [seed] [count] [entry]"""
import os, random, struct, subprocess, sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_XMM0, UC_X86_REG_XMM1)
from retail_cpu import ROOT, STOP, STACK, HEAP, w32, r32, harness
from imp_strategy_oracle import SECTIONS, r16, w16, w8, r8, fb, cstr

STUBS = 0x00F20000
EMIT, PARAMS, RESOURCE, SPRITE, TABLE_ANIM, CTRL = HEAP, HEAP + 0x1000, HEAP + 0x1400, HEAP + 0x1500, HEAP + 0x1600, HEAP + 0x1700
MODEL, MODELCELL, NODES, BLOCK5, BLOCK4 = HEAP + 0x1800, HEAP + 0x7000, HEAP + 0x1840, HEAP + 0x1900, HEAP + 0x1A00
RECORDS = HEAP + 0x1C00
PLAYERS = [HEAP + 0x3000, HEAP + 0x6000]
VIEW = HEAP + 0x9000
MESSAGE, EVENT = HEAP + 0x9800, HEAP + 0x9900
CHALLENGE, SCENEFLAG, STORY, CASEBITS = HEAP + 0xA000, HEAP + 0xA100, HEAP + 0xA200, HEAP + 0xA300
ANCHORS = HEAP + 0xA400
ORIGIN = 0x0050DD80
SCRATCH = STUBS + 0xF00
CALLEES = {
    0x000420D0: 'frame', 0x000412A0: 'position', 0x00041730: 'setposition', 0x0003BE20: 'records',
    0x0003CFD0: 'remove', 0x00035B60: 'acquire', 0x00035C00: 'release', 0x0003AAA0: 'find',
    0x0003A9E0: 'start', 0x000307C0: 'releasectl', 0x0003EF70: 'base', 0x0003DB90: 'height',
    0x0003DB30: 'bound', 0x000414F0: 'sqdist', 0x0012A970: 'soundactive', 0x00112CC0: 'bindtree',
    0x00056330: 'sphere', 0x00123E70: 'award', 0x000411A0: 'params', 0x00107520: 'create',
    0x0003E850: 'unregister', 0x0003C770: 'probe', 0x0003D0A0: 'effects', 0x0011BDA0: 'anchor',
    0x0005C1A0: 'billboard', 0x0003AEB0: 'joint', 0x0003C9E0: 'draw',
}
X87 = ('height', 'bound', 'sphere')
QUEUES = ['acquire', 'find', 'height', 'bound', 'sqdist', 'soundactive', 'sphere', 'joint', 'draw']
ENTRIES = ['sizes', 'ctor', 'dtor', 'message', 'launch', 'flight', 'update', 'scan', 'collect', 'visible', 'draw',
    'burst', 'viewradius', 'reach', 'sound', 'shapes', 'cylinders', 'frustum', 'intro']


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
        self.u, self.queues, self.log = u, {k: list(v) for k, v in queues.items()}, []
        for address, name in CALLEES.items():
            if name in X87:
                # x87 results: the stub loads the value its hook leaves in a scratch word.
                u.mem_write(address, b'\xd9\x05' + struct.pack('<I', SCRATCH + 4 * list(X87).index(name)) + b'\xc3')
            else:
                u.mem_write(address, b'\xc3')
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

    def x87(self, name, value):
        w32(self.u, SCRATCH + 4 * list(X87).index(name), fb(value))

    def who(self, a):
        names = {EMIT: 'emitter', PLAYERS[0]: 'p0', PLAYERS[1]: 'p1', PLAYERS[0] + 0x1100: 'b0', PLAYERS[1] + 0x1100: 'b1',
            PLAYERS[0] + 0x2000: 'c0', PLAYERS[1] + 0x2000: 'c1', 0: 'none'}
        return names.get(a, '%x' % a)

    def dispatch(self, uc, address, size, name):
        u, log = self.u, self.log
        if name == 'frame':
            log.append('frame %s' % self.who(self.arg(0)))
        elif name == 'position':
            obj, out = self.reg(UC_X86_REG_ESI), self.reg(UC_X86_REG_EDI)
            u.mem_write(out, bytes(u.mem_read(obj + 0x7F0, 12)))
        elif name == 'setposition':
            obj, point = self.reg(UC_X86_REG_EDI), self.reg(UC_X86_REG_ESI)
            log.append('setposition %s %08x %08x %08x' % (self.who(obj), r32(u, point), r32(u, point + 4), r32(u, point + 8)))
            u.mem_write(obj + 0x7F0, bytes(u.mem_read(point, 12)))
        elif name == 'records':
            record = self.reg(UC_X86_REG_EAX)
            log.append('records %s %x %x %x %s %x' % ({RESOURCE: 'resource', PLAYERS[0] + 0x900: 'r0', PLAYERS[1] + 0x900: 'r1',
                0: 'none'}.get(record, '%x' % record), self.arg(0), self.arg(1), self.arg(2), self.who(self.arg(3)),
                (self.arg(4) - EMIT) if self.arg(4) else 0))
        elif name == 'remove':
            log.append('remove %x' % (self.reg(UC_X86_REG_ECX) & 0xffff))
        elif name == 'acquire':
            v = self.pop('acquire')
            log.append('acquire %s' % cstr(u, self.reg(UC_X86_REG_EAX)))
            self.ret(SPRITE if v else 0)
        elif name == 'release':
            log.append('release %s' % ('sprite' if self.reg(UC_X86_REG_EBX) == SPRITE else '%x' % self.reg(UC_X86_REG_EBX)))
        elif name == 'find':
            v = self.pop('find')
            log.append('find %s' % cstr(u, self.reg(UC_X86_REG_ECX)))
            self.ret(TABLE_ANIM + 0x40 if v else 0)
        elif name == 'start':
            log.append('start %x' % (self.reg(UC_X86_REG_ECX) - TABLE_ANIM))
        elif name == 'releasectl':
            log.append('releasectl %d' % self.arg(0))
        elif name == 'base':
            log.append('base %x' % r32(u, self.arg(1)))
        elif name == 'height':
            v = self.pop('height')
            log.append('height %s' % self.who(self.reg(UC_X86_REG_EDI)))
            self.x87('height', [0.5, 3.0][v % 2])
        elif name == 'bound':
            v = self.pop('bound')
            log.append('bound %s' % self.who(self.reg(UC_X86_REG_EDI)))
            self.x87('bound', [0.25, 2.0][v % 2])
        elif name == 'sqdist':
            v = self.pop('sqdist')
            log.append('sqdist %s %s' % (self.who(self.reg(UC_X86_REG_EAX)), self.who(self.arg(0))))
            u.reg_write(UC_X86_REG_XMM0, fb([100.0, 2000.0, 900.0][v % 3]))
        elif name == 'soundactive':
            v = self.pop('soundactive')
            log.append('soundactive %x' % (self.reg(UC_X86_REG_EAX) - EMIT))
            self.ret(v)
        elif name == 'bindtree':
            log.append('bindtree')
        elif name == 'sphere':
            v = self.pop('sphere')
            record, center = self.arg(0), self.arg(2)
            log.append('sphere %x %d' % (record - BLOCK4 if record else 0, {0: 0, 0x1234: 1, 0x5678: 2}.get(self.arg(1), 9)))
            for i in range(3):
                w32(u, center + 4 * i, fb([0.5 * (v + 1), [1.0, -6.0, 9.0][v % 3], -2.0 * (v + 1)][i]))
            self.x87('sphere', [0.3, 1.5, 4.0][v % 3])
        elif name == 'award':
            log.append('award %x %s %x %08x %x' % (self.reg(UC_X86_REG_EDX), self.who(self.arg(0)), self.arg(1), self.arg(2), self.arg(3)))
        elif name == 'params':
            out = self.reg(UC_X86_REG_EAX)
            log.append('params')
            u.mem_write(out, b'\x5a' * 0x354)
            self.ret(out)
        elif name == 'create':
            p = self.arg(0)
            words = [r16(u, p), r16(u, p + 2), r16(u, p + 4), r32(u, p + 0x24), r32(u, p + 0x334) - EMIT, r32(u, p + 0x338),
                r8(u, p + 0x33C), r32(u, p + 0x340), r32(u, p + 0x344), r32(u, p + 0x348), r32(u, p + 0x34C), r32(u, p + 0x350),
                r8(u, p + 0x10)]
            log.append('create ' + ' '.join('%x' % w for w in words))
        elif name == 'unregister':
            log.append('unregister')
        elif name == 'probe':
            log.append('probe %x' % r32(u, EMIT + 0x118))
        elif name == 'effects':
            log.append('effects')
        elif name == 'anchor':
            log.append('anchor %x' % (self.reg(UC_X86_REG_ESI) - ANCHORS))
        elif name == 'billboard':
            pos, size = self.reg(UC_X86_REG_ECX), self.reg(UC_X86_REG_EDX)
            log.append('billboard %s %08x %x %08x %08x %08x %08x %08x %08x' % ('sprite' if self.arg(0) == SPRITE else '?', self.arg(1),
                self.arg(2), r32(u, pos), r32(u, pos + 4), r32(u, pos + 8), r32(u, size), r32(u, size + 4),
                u.reg_read(UC_X86_REG_XMM0) & 0xffffffff))
        elif name == 'joint':
            v = self.pop('joint')
            out = self.arg(1)
            log.append('joint %s %x %s' % (self.who(self.reg(UC_X86_REG_EDX)), self.arg(0), 'origin' if self.reg(UC_X86_REG_EBX) == ORIGIN else '?'))
            for i in range(3):
                w32(u, out + 4 * i, fb([1.0, -1.0, 2.5][i] * v))
        elif name == 'draw':
            v = self.pop('draw')
            log.append('draw %s %x' % (self.who(self.arg(0)), self.arg(2)))
            self.ret(v)


def setup(r, rng, line):
    u = r.u
    def put(v):
        line.append(v & 0xffffffff)
        return v
    # Emitter actor.
    for i in range(3):
        w32(u, EMIT + 0x7F0 + 4 * i, put(fb(rng.uniform(-20, 20))))
    for o in (0x30, 0x34, 0x38, 0x200, 0x1C0):
        w32(u, EMIT + o, put(fb(rng.choice([0.5, 1.0, 2.0]))))
    w32(u, EMIT + 0x1D0, put(fb(rng.choice([0.0, 1.0, 9.0]))))
    for i in range(3):
        w32(u, EMIT + 0x3C + 4 * i, put(fb(rng.uniform(-5, 20))))
    w32(u, EMIT + 0x48, TABLE_ANIM if put(rng.choice([0, 1, 1])) else 0)
    w32(u, EMIT + 0x50, CTRL if put(rng.choice([0, 1])) else 0)
    w32(u, EMIT + 0x8C, RESOURCE)
    w8(u, EMIT + 0x19B, put(rng.getrandbits(8)))
    w32(u, EMIT + 0x92C, put(fb(rng.choice([1.0, 2.0]))))
    w32(u, EMIT + 0x930, put(fb(rng.choice([0.0, 0.0, 1.0]))))
    w32(u, EMIT + 0x118, 7)
    for i in range(8):
        w16(u, EMIT + 0x334 + 0xB8 * i + 8, put(rng.choice([0xFFFF, 3])))
    # Model with a kind-5 block and a kind-4 shape block.
    model = put(rng.choice([0, 1, 1]))
    w32(u, EMIT + 0x204, MODELCELL if model else 0)
    w32(u, MODELCELL, MODEL)
    w32(u, MODEL, NODES); w32(u, MODEL + 4, 2)
    w32(u, NODES, put(rng.choice([4, 5]))); w32(u, NODES + 4, BLOCK5)
    w32(u, NODES + 8, 4); w32(u, NODES + 12, BLOCK4)
    w32(u, BLOCK5, put(fb(rng.choice([1.0, -3.0, 0.5]))))
    shapes = put(rng.randrange(5))
    for k in range(shapes):
        rec = BLOCK4 + 8 + 0x24 * k
        w8(u, rec, put(rng.choice([0, 0, 1])))
        w8(u, rec + 1, 0x24 if k + 1 < shapes else 0)
        w16(u, rec + 2, put(rng.choice([2, 7])))
        w32(u, rec + 4, put(rng.choice([0x10, 0x20, 0x30])))
    # Matrix records.
    count = put(rng.randrange(4))
    w32(u, EMIT + 0x288, RECORDS + 0x300); w32(u, EMIT + 0x28C, count)
    w32(u, EMIT + 0x2A0, put(rng.choice([0, 0, 1])))
    w32(u, EMIT + 0x29C, put(fb(rng.choice([0.0, 5.0]))))
    for k in range(count):
        rec = RECORDS + 0x300 + 0x48 * k
        for i in range(3):
            w32(u, rec + 4 + 4 * i, put(fb(rng.uniform(-30, 30))))
        w32(u, rec + 0x44, put(rng.choice([0, 1, 1])))
    # Emitter fields.
    w32(u, EMIT + 0x93C, put(rng.choice([0, 1, 1, 2])))
    w32(u, EMIT + 0x940, put(rng.choice([1, 2, 5, 0x10])))
    w32(u, EMIT + 0x944, put(fb(rng.choice([1.0, 3.0]))))
    w32(u, EMIT + 0x948, put(rng.getrandbits(8)))
    w32(u, EMIT + 0x94C, put(rng.getrandbits(8)))
    tokens = put(rng.randrange(4))
    w32(u, EMIT + 0x950, tokens)
    for k in range(tokens):
        w32(u, EMIT + 0x954 + 4 * k, put(rng.getrandbits(16)))
    for o in (0x974, 0x978, 0x97C, 0x980, 0x984, 0x988):
        w32(u, EMIT + o, put(fb(rng.uniform(-10, 10))))
    w32(u, EMIT + 0x98C, put(fb(rng.choice([0.0, 0.01, 0.4, 0.75]))))
    w32(u, EMIT + 0x990, put(rng.choice([0, 0, 1, 2])))
    w32(u, EMIT + 0x994, SPRITE if put(rng.choice([0, 1])) else 0)
    for o in (0x998, 0x99C):
        w32(u, EMIT + o, put(fb(rng.uniform(0.5, 1.5))))
    w32(u, EMIT + 0x9A0, put(fb(rng.uniform(0, 1.2))))
    w32(u, EMIT + 0x9A4, put(fb(rng.uniform(0, 2.0))))
    w32(u, EMIT + 0x9A8, put(rng.choice([0, 0x40, 0xC0])))
    w32(u, EMIT + 0x9AC, put(fb(rng.choice([0.0, 0.004, 0.5]))))
    for k in range(8):
        w8(u, EMIT + 0x184 + k, put(rng.randrange(6)))
    anchors = put(rng.randrange(3))
    for k in range(anchors):
        w32(u, ANCHORS + 0x40 * k + 4, ANCHORS + 0x40 * (k + 1) if k + 1 < anchors else 0)
    w32(u, EMIT + 0x1F8, ANCHORS if anchors else 0)
    # Players.
    players = put(rng.randrange(3))
    for index in range(2):
        actor = PLAYERS[index]
        body = actor + 0x1100
        body_actor = actor + 0x1800
        case = actor + 0x2000
        w32(u, actor + 0x93C + 4, PLAYERS[index + 1] + 0x93C if index + 1 < players else 0)
        w32(u, actor + 0xCE0, put(rng.choice([0, 1, 1])))
        w16(u, actor + 0xCDC, put(rng.choice([0, 2])))
        w32(u, actor + 0x93C, put(rng.choice([0, 1, 1])))
        w32(u, actor + 0x95C, body); w32(u, actor + 0x8C, actor + 0x900)
        w16(u, body + 0x94, put(rng.choice([0, 3, 3])))
        w32(u, body + 0x884, put(fb(rng.choice([1.0, 2.0]))))
        w32(u, body + 0x888, put(fb(rng.choice([1.0, 0.5]))))
        w32(u, body + 8, body_actor)
        # The intro gate 0x0013FF70 reads the first player's body bit block.
        w32(u, body + 0x15C, body + 0x200); w32(u, body + 0x208, body + 0x210)
        w8(u, body + 0x210 + 3, put(rng.choice([0, 8])))
        for i in range(3):
            w32(u, body_actor + 0x7F0 + 4 * i, put(fb(rng.uniform(-20, 20))))
        w32(u, body_actor + 0x34, put(fb(rng.choice([1.0, 3.0]))))
        w32(u, body_actor + 0x38, put(fb(rng.choice([0.5, 2.0]))))
        w32(u, body + 0x48, case if put(rng.choice([0, 1])) else 0)
        w32(u, case + 0x204, MODELCELL if put(rng.choice([0, 1, 1])) else 0)
        w32(u, MODELCELL + 0x168, put(rng.choice([0, 0x1234])))
        w32(u, MODELCELL + 0x164, put(rng.choice([0, 0x5678])))
    w32(u, 0x00511B40, PLAYERS[0] + 0x93C if players else 0)
    # Globals.
    w32(u, 0x00506C54, CHALLENGE + 0x80 - 8); w32(u, CHALLENGE + 0x80, CHALLENGE)
    for k in range(4):
        w8(u, CHALLENGE + k, put(rng.getrandbits(8)))
    w32(u, 0x00506C74, SCENEFLAG + 0x80 - 8); w32(u, SCENEFLAG + 0x80, SCENEFLAG)
    w8(u, SCENEFLAG, put(rng.choice([0, 0, 8])))
    w32(u, 0x0050EA9C, put(rng.choice([0, 0, 0x200000, 0x4000])))
    w32(u, 0x0054DD7C, put(rng.getrandbits(32)))
    w32(u, 0x004312DC, put(fb(rng.choice([0.0, 0.0, 1.0]))))
    w32(u, 0x005096C0, put(fb(rng.choice([0.25, 0.016]))))
    w32(u, 0x004312C4, put(fb(rng.choice([1.0, 2.0]))))
    w32(u, 0x0050E764, put(rng.choice([0, 0, 1]))); w32(u, 0x0050E768, put(rng.choice([0, 0, 1])))
    w32(u, 0x0051248C, 0); w8(u, 0x0030F144, put(rng.choice([0, 1, 1])))
    w32(u, 0x00506C6C, STORY + 0x80 - 8); w32(u, STORY + 0x80, STORY)
    w8(u, STORY, put(rng.choice([0, 0x80, 0x01])))
    w32(u, 0x00545B38, put(rng.choice([0, 0, 1])))
    for o in range(3):
        w32(u, ORIGIN + 4 * o, put(fb(rng.choice([0.0, 1.0]))))
    w32(u, 0x005077A8, VIEW)
    for i in range(3):
        w32(u, VIEW + 4 * i, put(fb(rng.uniform(-200, 200))))
    for k in range(6):
        for i in range(4):
            w32(u, VIEW + 0x138 + 0x48 + 0x10 * k + 4 * i, put(0x7FC00000 if rng.random() < 0.03 else fb(rng.uniform(-1, 1) if i < 3 else rng.uniform(-40, 40))))
    w32(u, 0x00511B2C, 0)
    tail = put(rng.choice([0, 1]))
    w32(u, 0x00511B30, HEAP + 0xA600 if tail else 0)
    w32(u, HEAP + 0xA604, 0)
    # Parameters.
    w32(u, PARAMS + 0x334, put(rng.choice([1, 2, 0x10])))
    w32(u, PARAMS + 0x338, put(rng.getrandbits(32)))
    w32(u, PARAMS + 0x33C, put(rng.getrandbits(32)))
    w32(u, PARAMS + 0x3C0, put(rng.randrange(32)))
    u.mem_write(PARAMS + 0x340, b'aid_tex_spark\0' if put(rng.choice([0, 1])) else b'none\0')
    w32(u, 0x00546208, 0x0031CB4C)


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


def snapshot(r):
    u = r.u
    words = [r32(u, EMIT + o) for o in range(0x93C, 0x9B0, 4) if o not in (0x994,)]
    words += [1 if r32(u, EMIT + 0x994) else 0]
    words += [r32(u, EMIT + o) for o in (0x3C, 0x40, 0x44)] + [r32(u, EMIT + o) for o in (0x7F0, 0x7F4, 0x7F8)]
    words += [r32(u, EMIT + 0x50) and 1, r32(u, EMIT + 0x29C), r32(u, EMIT + 0x2A0), r32(u, 0x0054DD7C)]
    words += [r8(u, EMIT + 0x184 + k) for k in range(8)]
    words += [r8(u, STORY), r32(u, 0x00545B38), cstr(u, 0x00545B44) or '-']
    link = r32(u, 0x00511B30)
    words += [{0: 'n', EMIT + 0x934: 'e', HEAP + 0xA600: 't'}.get(link, '?'), {0: 'n', EMIT + 0x934: 'e'}.get(r32(u, HEAP + 0xA604), '?')]
    return ' '.join(str(w) for w in words)


def main():
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 400
    only = sys.argv[3] if len(sys.argv) > 3 else None
    rng = random.Random(seed)
    names = [only] if only else ENTRIES
    lines, expected = [], []
    for case in range(count):
        entry = names[case % len(names)]
        queues = {q: [rng.choice([0, 1, 1, 2]) for _ in range(12)] for q in QUEUES}
        line = [ENTRIES.index(entry)]
        for q in QUEUES:
            line += queues[q]
        r = Retail(queues)
        setup(r, rng, line)
        u = r.u
        ident = rng.choice([7, 7, 0x23, 0x96, 0x41])
        event_kind = rng.choice([0x1C, 0x1E, 0x20, 0x21, 0x2F, 0x22, 0x30])
        landing = [fb(rng.uniform(-10, 10)) for _ in range(3)]
        w32(u, EVENT + 0xC, event_kind)
        msg = [ident, landing[0], landing[1], landing[2]] if ident == 0x96 else [ident, 0, 0, EVENT]
        for i, w in enumerate(msg):
            w32(u, MESSAGE + 4 * i, w)
        line += [ident, event_kind] + landing
        floats = [fb(rng.uniform(-5, 5)) for _ in range(4)]
        line += floats
        out = '-'
        if entry == 'sizes':
            call(r, 0x00109DB0, {}, (HEAP + 0xF000,))
            out = '%x %x' % (r32(u, HEAP + 0xF000), r32(u, HEAP + 0xF004))
        elif entry == 'ctor':
            empty = rng.choice([0, 0, 0, 1])
            line.append(empty)
            out = '%d' % call(r, 0x00109FC0, {}, (EMIT, 0 if empty else PARAMS))
        elif entry == 'dtor':
            call(r, 0x0010A120, {}, (EMIT,))
        elif entry == 'message':
            call(r, 0x0010A180, {}, (EMIT, MESSAGE))
        elif entry == 'launch':
            call(r, 0x00109D10, {}, (EMIT, MESSAGE + 4))
        elif entry == 'flight':
            call(r, 0x001099C0, {}, (EMIT,))
        elif entry == 'update':
            call(r, 0x0010A2E0, {}, (EMIT,))
        elif entry == 'scan':
            call(r, 0x0010A590, {UC_X86_REG_EAX: EMIT})
        elif entry == 'collect':
            bounds = HEAP + 0xF100
            for i, o in enumerate((4, 8, 0xC, 0x14, 0x1C)):
                w32(u, bounds + o, floats[i % 4] if o != 0x1C else fb(abs(struct.unpack('<f', struct.pack('<I', floats[3]))[0])))
            which = rng.choice([0, 1])
            line.append(which)
            out = '%d' % call(r, 0x0010A6D0, {}, (EMIT, bounds, PLAYERS[which]))
        elif entry == 'visible':
            a, b = rng.randrange(2), rng.randrange(4)
            line += [a, b]
            out = '%d' % call(r, 0x00109E30, {}, (EMIT, a, b))
        elif entry == 'draw':
            kind = rng.choice([0, 2, 2, 1])
            line.append(kind)
            out = '%d' % call(r, 0x00109EE0, {}, (EMIT, 0x77, kind))
        elif entry == 'burst':
            p = HEAP + 0xE000
            call(r, 0x00069590, {UC_X86_REG_EAX: p, UC_X86_REG_ESI: EMIT}, (floats[0],))
            out = ' '.join('%08x' % r32(u, p + o) for o in (0, 4, 0x24, 0x334, 0x338, 0x33C, 0x340, 0x344, 0x348, 0x34C, 0x350))
        elif entry == 'viewradius':
            call(r, 0x0003D860, {UC_X86_REG_EDI: EMIT})
            out = '%08x' % (u.reg_read(UC_X86_REG_XMM0) & 0xffffffff)
        elif entry == 'reach':
            call(r, 0x00040550, {UC_X86_REG_EAX: EMIT})
            out = '%08x' % (u.reg_read(UC_X86_REG_XMM0) & 0xffffffff)
        elif entry == 'sound':
            out = '%d' % call(r, 0x00041CB0, {UC_X86_REG_EAX: EMIT})
        elif entry == 'shapes':
            mask, kind, ident2 = rng.choice([0x10, 0x20]), rng.choice([0, 1]), rng.choice([-1, 2, 7])
            line += [mask, kind, ident2 & 0xffff]
            found = call(r, 0x00042560, {UC_X86_REG_EAX: EMIT, UC_X86_REG_EBX: kind, UC_X86_REG_EDI: ident2 & 0xffff}, (mask,))
            seq = []
            guard = 0
            while found and guard < 8:
                seq.append('%x' % (found - BLOCK4))
                found = call(r, 0x000425D0, {UC_X86_REG_ECX: EMIT, UC_X86_REG_EAX: found, UC_X86_REG_EBX: kind,
                    UC_X86_REG_EDI: ident2 & 0xffff}, (mask,))
                guard += 1
            out = ' '.join(seq) or '-'
        elif entry == 'cylinders':
            a, b = HEAP + 0xF200, HEAP + 0xF300
            exact = rng.random() < 0.3
            for k, base in enumerate((a, b)):
                for o in (4, 8, 0xC, 0x14, 0x1C):
                    if exact:
                        value = {4: [0.0, 3.0], 8: [0.0, 0.5], 0xC: [0.0, 4.0], 0x14: [2.0, 2.0], 0x1C: [2.5, 2.5]}[o][k]
                    else:
                        value = rng.choice([rng.uniform(-3, 3), 0.0, 1.0])
                    w32(u, base + o, fb(value))
                    line.append(r32(u, base + o))
            out = '%d' % call(r, 0x001205F0, {UC_X86_REG_ECX: a, UC_X86_REG_EAX: b})
        elif entry == 'frustum':
            plane = HEAP + 0xF400
            start = rng.randrange(6)
            w8(u, plane, start)
            line.append(start)
            # Optionally pull every plane far behind the sphere and poison one normal with NaN.
            override = rng.choice([0, 1])
            poisoned = rng.randrange(6)
            line += [override, poisoned]
            if override:
                for k in range(6):
                    w32(u, VIEW + 0x138 + 0x48 + 0x10 * k + 0xC, fb(1000.0))
                w32(u, VIEW + 0x138 + 0x48 + 0x10 * poisoned, 0x7FC00000)
            for i in range(3):
                w32(u, HEAP + 0xF500 + 4 * i, fb(struct.unpack('<f', struct.pack('<I', floats[i + 1]))[0] * 10.0))
            result = call(r, 0x00159AD0, {}, (VIEW + 0x138, HEAP + 0xF500, floats[0] & 0x7fffffff, plane))
            out = '%d %d' % (result, r8(u, plane))
        if entry == 'intro':
            u.mem_write(HEAP + 0xF600, b'intros_test\0')
            out = '%d' % call(r, 0x0013FF00, {UC_X86_REG_ESI: HEAP + 0xF600})
        expected.append(out + ' | ' + snapshot(r) + ' | ' + ' ; '.join(r.log))
        lines.append(' '.join('%u' % (v & 0xffffffff) for v in line))
    text = str(count) + '\n' + '\n'.join(lines) + '\n'
    open(os.path.join(ROOT, 'cache', 'oracle', 'fx_emitter_input.txt'), 'w').write(text)
    result = subprocess.run([harness('fx_emitter_harness')], input=text, capture_output=True, text=True)
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


if __name__ == '__main__':
    main()
