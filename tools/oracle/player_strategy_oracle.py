"""Call-sequence differential check for the player strategy batch: 0x000BEE10, 0x000BEF40,
0x000BF2B0, 0x000BEDB0, 0x000BF4D0, 0x0013A490, 0x000BF600, 0x000BF6A0, 0x000D0690,
0x0013FF30/0x0013FF70, 0x0012D090/0x0012D4A0/0x0012D4E0/0x0012D510 and the axis writers
0x00108FC0/0x001090A0/0x00109180. Other callees, including the nearest-actor query 0x00128DF0, are
logging stubs on both sides with scripted returns. Needs cache/oracle/mem_00011000.bin and
mem_00210000.bin."""
import os, random, struct, subprocess, sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP)
from retail_cpu import ROOT, STOP, STACK, HEAP, w32, r32, harness

def r16(u, a): return struct.unpack('<H', u.mem_read(a, 2))[0]
def w16(u, a, v): u.mem_write(a, struct.pack('<H', v & 0xffff))
def fb(v): return struct.unpack('<I', struct.pack('<f', v))[0]
def cstr(u, a): return bytes(u.mem_read(a, 0x40)).split(b'\0')[0].decode('latin1')

CALLEES = {
    0x0002BC10: 'base', 0x0002C130: 'baseupdate', 0x0002BAF0: 'baseenter', 0x0002B8D0: 'state',
    0x00108270: 'input', 0x001083F0: 'edge', 0x001085F0: 'stick', 0x001088D0: 'stick2', 0x0004B4D0: 'chapter',
    0x00028210: 'drain', 0x00029D90: 'request', 0x0002A8E0: 'goal', 0x00117400: 'pause', 0x00039510: 'sound',
    0x0003C040: 'callout', 0x00130710: 'health', 0x0012FE10: 'hearts', 0x0012F990: 'digits',
    0x00142C20: 'widget', 0x00035E90: 'resource', 0x00032700: 'error', 0x00024C20: 'idle', 0x00042360: 'timer',
    0x00128DF0: 'nearest', 0x00F20010: 'actormsg',
}
QUEUES = ['edge', 'stick', 'chapter', 'idle', 'timer', 'resource', 'nearest']
ACTOR, BODY, MIND, STRAT, DEVICE, BITS = HEAP, HEAP + 0x2000, HEAP + 0x3000, HEAP + 0x3800, HEAP + 0x4000, HEAP + 0x4800
CHILDREN, KEYBOARD, VTABLE = HEAP + 0x5000, 0x00507C98, HEAP + 0x5800
SCRIPT, MESSAGE, TEXTURE, FOUND = HEAP + 0xC000, HEAP + 0xD000, HEAP + 0xE000, HEAP + 0xF000
STORY, UNLOCK, SCENE, CASE_ACTOR, CASE_BODY, CASE_BITS, WIDGET = (HEAP + 0xE800, HEAP + 0xE900, HEAP + 0xEA00,
    HEAP + 0x10000, HEAP + 0x11000, HEAP + 0x11800, HEAP + 0x12000)
NAMES = {0x312D40: 'brew', 0x312DC0: 'cross', 0x312E40: 'tick', 0x3213A4: 'idle',
         0x3033A8: 'right', 0x303428: 'wrong'}
ENTRIES = ['message', 'enter', 'update', 'update', 'update', 'available', 'scripted', 'knockout', 'idle', 'move',
           'intro', 'brew', 'close', 'accept', 'axes']
WRITERS = [0x00109180, 0x00108FC0, 0x001090A0]

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
        u.mem_map(0x00F20000, 0x1000); u.mem_write(0x00F20000, b'\xc3' * 0x1000)
        u.mem_map(HEAP, 0x100000)
        u.mem_map(STACK - 0x10000, 0x20000)
        self.u, self.queues, self.log = u, {k: list(v) for k, v in queues.items()}, []
        for address, name in CALLEES.items():
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

    def dispatch(self, uc, address, size, name):
        u, log, reg = self.u, self.log, self.reg
        if name == 'base':
            log.append('base %x %d' % (r32(u, self.arg(1)), self.arg(0) == STRAT))
        elif name == 'baseupdate':
            log.append('baseupdate %d' % (self.arg(0) == STRAT))
        elif name == 'baseenter':
            log.append('baseenter %x %d' % (self.arg(1) & 0xffff, self.arg(0) == STRAT))
        elif name == 'state':
            log.append('state %x %d' % (reg(UC_X86_REG_EDI) & 0xffff, reg(UC_X86_REG_ESI) == STRAT))
        elif name == 'input':
            log.append('input %d' % (self.arg(0) == DEVICE))
        elif name == 'edge':
            v = self.pop('edge'); log.append('edge %x %d' % (self.arg(0), reg(UC_X86_REG_EAX) == DEVICE)); self.ret(v)
        elif name in ('stick', 'stick2'):
            v = self.pop('stick')
            out = reg(UC_X86_REG_EDI)
            x, y = [(0.05, 0.025), (0.2, 0.1), (0.1, 0.05)][v]
            w32(u, out, fb(x)); w32(u, out + 4, fb(y))
            log.append('%s %d' % (name, reg(UC_X86_REG_EAX) == DEVICE))
        elif name == 'chapter':
            v = self.pop('chapter'); log.append('chapter %d' % (reg(UC_X86_REG_EAX) == 0x510E58)); self.ret(v * 9)
        elif name == 'drain':
            log.append('drain')
        elif name == 'request':
            log.append('request %x %d' % (reg(UC_X86_REG_EBX), self.arg(0) == MIND)); self.ret(HEAP + 0x13000)
        elif name == 'goal':
            log.append('goal %s %d' % (NAMES.get(reg(UC_X86_REG_EAX), '?'), self.arg(0) == MIND)); self.ret(1)
        elif name == 'pause':
            log.append('pause %x %d' % (self.arg(0) & 0xff, self.arg(1)))
        elif name == 'sound':
            log.append('sound %s %d %08x' % (NAMES.get(self.arg(0), '?'), self.arg(1), self.arg(2)))
        elif name == 'callout':
            log.append('callout %x %d %d' % (reg(UC_X86_REG_EDX), self.arg(0), reg(UC_X86_REG_ECX) == ACTOR))
        elif name == 'health':
            log.append('health %d %d' % (reg(UC_X86_REG_EDI), self.arg(0)))
        elif name == 'hearts':
            log.append('hearts')
        elif name == 'digits':
            counter = reg(UC_X86_REG_ECX)
            log.append('digits %d %d %d %d' % (r32(u, counter), r32(u, counter + 4), counter == 0x5459A8,
                                              reg(UC_X86_REG_ESI) == 0x312A1C))
        elif name == 'widget':
            log.append('widget %d %d' % (reg(UC_X86_REG_EAX), reg(UC_X86_REG_ESI) == WIDGET))
        elif name == 'resource':
            v = self.pop('resource')
            w32(u, self.arg(1), TEXTURE if v else 0)
            log.append('resource %s' % NAMES.get(self.arg(0), '?'))
        elif name == 'error':
            log.append('error %d' % self.arg(0))
        elif name == 'idle':
            v = self.pop('idle'); log.append('idle %d' % (reg(UC_X86_REG_EAX) == BODY)); self.ret(v)
        elif name == 'timer':
            v = self.pop('timer')
            log.append('timer %08x %d' % (r32(u, reg(UC_X86_REG_EAX)), reg(UC_X86_REG_ECX) == ACTOR)); self.ret(v)
        elif name == 'nearest':
            v = self.pop('nearest')
            log.append('nearest %x %d %x %08x %d %x %x' % (reg(UC_X86_REG_EAX), self.arg(0) == ACTOR,
                       self.arg(1) & 0xffff, self.arg(2), self.arg(3) != 0, self.arg(4), self.arg(5)))
            self.ret(FOUND if v else 0)
        elif name == 'actormsg':
            message = self.arg(1)
            log.append('actormsg %d ' % (self.arg(0) == ACTOR) + ' '.join('%x' % r32(u, message + 4 * i) for i in range(9)))

def setup(r, rng, line):
    u = r.u
    def word(v):
        line.append(v & 0xffffffff)
        return v
    def flt(v):
        line.append(fb(v))
        return fb(v)
    # Strategy.
    w32(u, STRAT + 8, ACTOR)
    w16(u, STRAT + 0xE, word(rng.choice([0x3F1, 0x3F1, 0x3F2, 0x3FC, 0x407, 0x408, 0x408, 0x3F5, 1000])))
    for off in (0x12, 0x14, 0x16, 0x18):
        w16(u, STRAT + off, word(rng.choice([0x3E9, 0x3F2])))
    u.mem_write(STRAT + 0x1C, bytes([word(rng.choice([0, 1, 2]))]))
    for off in (0x38, 0x3C, 0x40, 0x48):
        w32(u, STRAT + off, word(rng.choice([0, 0, 1])))
    w32(u, STRAT + 0x4C, flt(rng.choice([0.0, 29.5, 29.9, 31.0])))
    w32(u, STRAT + 0x50, flt(rng.choice([0.0, 2.5, 4.2, 6.0, -3.0])))
    # Actor, body and mind.
    w32(u, ACTOR, VTABLE); w32(u, VTABLE + 0xC, 0x00F20010)
    w32(u, ACTOR + 0x930, flt(rng.choice([0.0, 0.0, 0.5])))
    w32(u, ACTOR + 0x92C, flt(rng.choice([1.0, 0.5])))
    w32(u, ACTOR + 0x958, MIND); w32(u, ACTOR + 0x95C, BODY); w32(u, ACTOR + 0x960, DEVICE)
    w32(u, BODY + 0x15C, BITS); w32(u, BITS + 8, BITS + 0x10)
    u.mem_write(BITS + 0x10, bytes([word(rng.getrandbits(8)) for _ in range(8)]))
    w16(u, BODY + 0x30, word(rng.choice([0, 3])))
    w16(u, BODY + 0x94, word(rng.choice([0, 0, 3, 7])))
    w16(u, BODY + 0x96, word(rng.choice([0, 5])))
    w32(u, MIND + 0x58, HEAP + 0x14000 if word(rng.choice([0, 0, 1])) else 0)
    # Input device and children.
    w32(u, DEVICE, word(rng.choice([1, 2, 3, 4, 5])))
    for c in range(4):
        child = CHILDREN + 0x100 * c
        w32(u, DEVICE + 8 + 4 * c, child)
        w32(u, child, word(rng.choice([1, 2, 3, 5])))
        w32(u, child + 8, word(rng.choice([0, 0xFFFFFFFF])))
    # Globals.
    w32(u, 0x004E1C34, word(rng.choice([0xFFFFFFFF, 0xFFFFFFFF, 0])))
    w32(u, 0x004EE1EC, word(rng.choice([0, 0, 1])))
    w32(u, 0x005064D8, word(rng.choice([0, 1, 2, 3])))
    w32(u, 0x00430D0C, word(rng.choice([1, 1, 0])))
    w32(u, 0x00506C70, UNLOCK); w32(u, UNLOCK + 8, UNLOCK + 0x10); u.mem_write(UNLOCK + 0x10, bytes([word(rng.choice([0, 8]))]))
    w32(u, 0x00506C74, SCENE); w32(u, SCENE + 8, SCENE + 0x10); u.mem_write(SCENE + 0x10, bytes([word(rng.choice([0, 0, 8]))]))
    w32(u, 0x00506C6C, STORY); w32(u, STORY + 8, STORY + 0x10); u.mem_write(STORY + 0x14, bytes([word(rng.choice([0, 4]))]))
    w32(u, 0x0050EA9C, word(rng.choice([0, 0, 0x4000, 0x200000])))
    w32(u, 0x00506C8C, word(rng.randrange(4)))
    w32(u, 0x00430D9C, word(rng.randrange(4)))
    w32(u, 0x00545B38, word(rng.choice([0, 0, 1])))
    case = word(rng.choice([0, 1, 1]))
    w32(u, 0x00511B40, CASE_ACTOR + 0x93C if case else 0)
    w32(u, CASE_ACTOR + 0x95C, CASE_BODY); w32(u, CASE_BODY + 0x15C, CASE_BITS); w32(u, CASE_BITS + 8, CASE_BITS + 0x10)
    u.mem_write(CASE_BITS + 0x13, bytes([word(rng.choice([0, 8]))]))
    w32(u, 0x00510CEC, word(rng.randrange(3)))
    w32(u, 0x00506C88, word(rng.randrange(3)))
    w32(u, 0x005459F4, WIDGET if word(rng.choice([0, 1])) else 0)
    w32(u, WIDGET + 0x950, 0x1234)
    for off in (0x5459DC, 0x5459E0, 0x5459E4):
        w32(u, off, TEXTURE if word(rng.choice([0, 0, 1])) else 0)
    w16(u, TEXTURE + 8, word(rng.choice([12, 33])))
    w32(u, 0x004312DC, flt(rng.choice([0.0, 0.0, 1.0])))
    w32(u, 0x005096C0, flt(0.25))
    w32(u, 0x004312C4, flt(2.0))
    # Script records for 0x000BF4D0.
    # Distinct keys whose follow-ups only point forward (or past the table), so chains end.
    keys = rng.sample([1, 2, 3, 4], rng.randrange(4))
    rows = [(key, rng.choice([0, key + 1, key + 2])) for key in keys]
    word(len(rows))
    w32(u, SCRIPT, len(rows))
    cursor = SCRIPT + 8
    for key, following in rows:
        values = [fb(rng.uniform(-9, 9)) for _ in range(6)]
        u.mem_write(cursor, struct.pack('<IHHH', 0x24, 0, key, following) + bytes(2) + struct.pack('<6I', *values))
        line += [key, following] + values
        cursor += 0x24
    w32(u, 0x0051108C, SCRIPT)

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
    words = [r16(u, STRAT + o) for o in (0xE, 0x12, 0x14, 0x16, 0x18)] + [u.mem_read(STRAT + 0x1C, 1)[0]]
    words += [r32(u, STRAT + o) for o in (0x38, 0x3C, 0x40, 0x48, 0x4C, 0x50)]
    words += [r16(u, BODY + 0x30), r16(u, BODY + 0x94)]
    words += [r32(u, a) for a in (0x00510CEC, 0x00506C88, 0x00430D98, 0x00545990, 0x00545998, 0x005459A8, 0x005459AC)]
    words += [int(r32(u, a) == TEXTURE) for a in (0x005459DC, 0x005459E0, 0x005459E4)]
    words += [r32(u, a) for a in (0x005459E8, 0x005459EC, 0x005459F0, 0x00433C14, 0x00545B38, WIDGET + 0x950)]
    words += [u.mem_read(STORY + 0x14, 1)[0]]
    name = cstr(u, 0x00545B44) or '-'
    device = [r32(u, DEVICE + o) for o in range(8, 0x60, 4)]
    device += [r32(u, CHILDREN + 0x100 * c + o) for c in range(4) for o in range(8, 0x60, 4)]
    device += [r32(u, KEYBOARD + o) for o in range(0, 0x60, 4)]
    return ' '.join('%x' % w for w in words) + ' ' + name + ' D ' + ' '.join('%x' % w for w in device)

def main():
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 3000
    only = sys.argv[3] if len(sys.argv) > 3 else None
    rng = random.Random(seed)
    lines, expected, names = [], [], []
    for case in range(count):
        entry = only or ENTRIES[case % len(ENTRIES)]
        names.append(entry)
        queues = {q: [rng.choice([0, 1, 2] if q == 'stick' else [0, 1, 1]) for _ in range(12)] for q in QUEUES}
        line = [ENTRIES.index(entry)]
        for q in QUEUES:
            line += queues[q]
        r = Retail(queues)
        setup(r, rng, line)
        argument = 0
        regs, args = {}, ()
        if entry == 'message':
            argument = rng.choice([0x27, 0x2C, 0x82, 0x6E, 0x7D, 0x7D, 0xB1, 0xB1, 0x10])
            w32(r.u, MESSAGE, argument); w32(r.u, MESSAGE + 4, rng.choice([0, 1, 2, 3]))
            line.append(r32(r.u, MESSAGE + 4))
            address, args = 0x000BEE10, (STRAT, MESSAGE)
        elif entry == 'enter':
            argument = rng.choice([0x3F1, 0x3F2, 0x3FC, 0x407, 0x408, 0x3F5])
            address, args = 0x000BF2B0, (STRAT, argument, 0x3E9)
        elif entry == 'update':
            address, args = 0x000BEF40, (STRAT,)
        elif entry == 'available':
            address = 0x000BEDB0
        elif entry == 'scripted':
            w32(r.u, MESSAGE + 4, rng.choice([0, 1, 2, 3])); line.append(r32(r.u, MESSAGE + 4))
            address, regs = 0x000BF4D0, {UC_X86_REG_EAX: STRAT, UC_X86_REG_ECX: MESSAGE}
        elif entry == 'knockout':
            address, regs = 0x0013A490, {UC_X86_REG_EBX: STRAT}
        elif entry == 'idle':
            address, regs = 0x000BF600, {UC_X86_REG_EAX: BODY, UC_X86_REG_EDI: STRAT, UC_X86_REG_ESI: ACTOR}
        elif entry == 'move':
            address, regs = 0x000BF6A0, {UC_X86_REG_EBX: ACTOR}
        elif entry == 'intro':
            address = 0x0013FF30
        elif entry == 'brew':
            argument = rng.choice([0, 3, -2])
            address, regs = 0x0012D4A0, {UC_X86_REG_EAX: argument & 0xffffffff}
        elif entry == 'close':
            address = 0x0012D4E0
        elif entry == 'accept':
            address = 0x0012D510
        else:
            w32(r.u, MESSAGE, fb(0.25)); w32(r.u, MESSAGE + 4, fb(-0.75))
            argument = rng.randrange(3)
            address, regs = WRITERS[argument], {UC_X86_REG_EAX: DEVICE, UC_X86_REG_EBX: MESSAGE}
        line.append(argument & 0xffffffff)
        result = call(r, address, regs, args)
        out = '%x' % result if entry in ('available', 'knockout', 'idle', 'move') else '-'
        expected.append(out + ' | ' + snapshot(r) + ' | ' + ' ; '.join(r.log))
        lines.append(' '.join('%u' % (v & 0xffffffff) for v in line))
    text = str(count) + '\n' + '\n'.join(lines) + '\n'
    open(os.path.join(ROOT, 'cache', 'oracle', 'player_strategy_input.txt'), 'w').write(text)
    result = subprocess.run([harness('player_strategy_harness')], input=text, capture_output=True, text=True)
    if result.returncode != 0:
        print('harness exit', result.returncode, result.stderr[-400:])
    got = result.stdout.splitlines()
    bad = abs(len(expected) - len(got))
    for i, (want, have) in enumerate(zip(expected, got)):
        if want.split() != have.split():
            bad += 1
            if bad <= 5:
                print('MISMATCH', i, names[i], '\n exp', want[:1500], '\n got', have[:1500])
    print('player strategy: cases', len(expected), len(got), 'mismatches', bad)
    return int(bad != 0)

if __name__ == '__main__':
    raise SystemExit(main())
