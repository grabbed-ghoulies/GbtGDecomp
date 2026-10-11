"""Call-sequence differential check for the player mind message slot 0x0007F1E0 with 0x000357B0,
0x00049DB0 and 0x00061960. Every other callee is a logging stub on both sides with scripted
returns. Needs cache/oracle/mem_00011000.bin and mem_00210000.bin."""
import os, random, struct, subprocess, sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_XMM0)
from retail_cpu import ROOT, STOP, STACK, HEAP, w32, r32, harness

def r16(u, a): return struct.unpack('<H', u.mem_read(a, 2))[0]
def w16(u, a, v): u.mem_write(a, struct.pack('<H', v & 0xffff))
def fb(v): return struct.unpack('<I', struct.pack('<f', v))[0]
def cstr(u, a): return bytes(u.mem_read(a, 0x40)).split(b'\0')[0].decode('latin1')

MIND, ACTOR, BODY, OTHER, SOURCE, MESSAGE, NAMES, VTABLE = (HEAP + 0x1000, HEAP + 0x2000, HEAP + 0x4000,
    HEAP + 0x5000, HEAP + 0x6000, HEAP + 0x7000, HEAP + 0x8000, HEAP + 0x9000)
ACTOR_MESSAGE = 0x00F20010
POSITION = [fb(1.5), fb(-2.25), fb(3.0)]
EULER = [fb(0.125), fb(-0.5), fb(2.0)]
DURATION = fb(2.75)
NAME_TEXT = [b'', b'aid_camerashake_hit\0', b'aidx\0', b'xyz\0']
PULSE = [0x0050447C, 0x00504480, 0x00504484, 0x00504488, 0x00504490, 0x00504494, 0x0050449C, 0x005044A0]
CALLEES = {
    0x0002A2E0: 'base', 0x0007F870: 'register', 0x0007DB20: 'reset', 0x0007DC40: 'impact',
    0x0007E420: 'event', 0x00116810: 'queue', 0x0007DD30: 'callout', 0x0007F920: 'turn',
    0x0007DAE0: 'release', 0x0007E5C0: 'distance', 0x0007DEE0: 'track', 0x0007DC80: 'activation',
    0x0007E470: 'shake', 0x0007F8C0: 'boost', 0x0007DBE0: 'contact', 0x0007DCC0: 'source',
    0x0007DCE0: 'target', 0x0007DAC0: 'clearbody', 0x00140A40: 'reference', 0x00136700: 'navigation',
    0x000412A0: 'position', 0x00041820: 'euler', 0x0003B1D0: 'duration', 0x00123E50: 'special',
    0x00028D50: 'resume', 0x00028CA0: 'active', 0x00049D10: 'sceneend', 0x0007F100: 'record',
    0x0001EFF0: 'bit', ACTOR_MESSAGE: 'actormsg',
}
QUEUES = ['special', 'resume', 'active', 'sceneend']
IDS = [0x23, 0x29, 0x2A, 0x2B, 0x2D, 0x31, 0x34, 0x37, 0x3D, 0x3E, 0x40, 0x41, 0x42, 0x44, 0x46, 0x48, 0x49,
       0x5A, 0x50, 0x5B, 0x6E, 0x6E, 0x70, 0x75, 0x7C, 0x7D, 0x86, 0x8D, 0x90, 0x90, 0x94, 0xB2, 0x24, 0x10, 0xC0]

class Retail:
    def __init__(self, queues):
        u = Uc(UC_ARCH_X86, UC_MODE_32)
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
            log.append('base %x %d' % (r32(u, self.arg(1)), self.arg(0) == MIND))
        elif name == 'register':
            log.append('register %d' % (reg(UC_X86_REG_EAX) == ACTOR))
        elif name == 'reset':
            log.append('reset %d' % (self.arg(0) == MIND))
        elif name == 'navigation':
            log.append('navigation %d %x' % (self.arg(0) == MIND, r32(u, ACTOR + 0xCEC)))
        elif name in ('impact', 'release', 'activation', 'clearbody'):
            log.append('%s %d' % (name, reg(UC_X86_REG_EAX) == MIND))
        elif name == 'event':
            log.append('event %d %d' % (self.arg(0), reg(UC_X86_REG_EAX) == MIND))
        elif name == 'queue':
            notice = reg(UC_X86_REG_EDI)
            log.append('queue ' + ' '.join('%x' % r32(u, notice + 4 * i) for i in range(4)))
        elif name == 'callout':
            log.append('callout %x %d' % (r32(u, self.arg(0) + 4), reg(UC_X86_REG_EDX) == MIND))
        elif name == 'turn':
            point = reg(UC_X86_REG_EAX) + 4
            log.append('turn %x %x %x %d' % (r32(u, point), r32(u, point + 4), r32(u, point + 8), reg(UC_X86_REG_ECX) == MIND))
        elif name == 'distance':
            log.append('distance %x %d' % (reg(UC_X86_REG_XMM0) & 0xffffffff, reg(UC_X86_REG_EAX) == MIND))
        elif name == 'track':
            log.append('track %x %x %d' % (reg(UC_X86_REG_ECX), reg(UC_X86_REG_ESI), reg(UC_X86_REG_EAX) == MIND))
        elif name == 'shake':
            log.append('shake %s %d' % (cstr(u, self.arg(0)), reg(UC_X86_REG_EAX) == MIND))
        elif name == 'boost':
            log.append('boost %d' % (reg(UC_X86_REG_EAX) == MIND))
        elif name == 'contact':
            log.append('contact %x %x %d' % (reg(UC_X86_REG_ECX), self.arg(0), reg(UC_X86_REG_EAX) == MIND))
        elif name in ('source', 'target'):
            log.append('%s %x %d' % (name, r32(u, reg(UC_X86_REG_EAX)), self.arg(0) == MIND))
        elif name == 'reference':
            log.append('reference %x %x' % (reg(UC_X86_REG_EAX) - MIND, r32(u, reg(UC_X86_REG_ECX) + 4)))
        elif name == 'position':
            for i in range(3):
                w32(u, reg(UC_X86_REG_EDI) + 4 * i, POSITION[i])
            log.append('position %d' % (reg(UC_X86_REG_ESI) == ACTOR))
        elif name == 'euler':
            for i in range(3):
                w32(u, reg(UC_X86_REG_EDI) + 4 * i, EULER[i])
            log.append('euler %d' % (reg(UC_X86_REG_ESI) == ACTOR))
        elif name == 'duration':
            u.reg_write(UC_X86_REG_XMM0, DURATION)
            log.append('duration %d %d' % (reg(UC_X86_REG_EAX) == ACTOR, reg(UC_X86_REG_ECX)))
        elif name == 'special':
            v = self.pop('special'); log.append('special %x' % reg(UC_X86_REG_EAX)); self.ret(v)
        elif name == 'resume':
            v = self.pop('resume'); log.append('resume %d' % (reg(UC_X86_REG_EAX) == ACTOR)); self.ret(v)
        elif name == 'active':
            v = self.pop('active'); log.append('active %d' % (reg(UC_X86_REG_EAX) == ACTOR)); self.ret(v)
        elif name == 'sceneend':
            v = self.pop('sceneend'); log.append('sceneend'); self.ret(v)
        elif name == 'record':
            position = self.arg(1)
            log.append('record %x %x %x %x %d' % (self.arg(0), r32(u, position), r32(u, position + 4), r32(u, position + 8), reg(UC_X86_REG_EBX) == MIND))
        elif name == 'bit':
            log.append('bit %x %d %d' % (reg(UC_X86_REG_ECX), reg(UC_X86_REG_EDX), reg(UC_X86_REG_EAX) == BODY))
        elif name == 'actormsg':
            message = self.arg(1)
            log.append('actormsg %d ' % (self.arg(0) == ACTOR) + ' '.join('%x' % r32(u, message + 4 * i) for i in range(9)))

def main():
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 3000
    rng = random.Random(seed)
    lines, expected = [], []
    for case in range(count):
        kind = IDS[case % len(IDS)]
        queues = {q: [rng.choice([0, 1]) for _ in range(4)] for q in QUEUES}
        if kind == 0x90:
            queues['resume'][0] = queues['active'][0] = rng.choice([0, 1, 1, 1])
        r = Retail(queues)
        u = r.u
        line = []
        for q in QUEUES:
            line += queues[q]
        message = [kind] + [rng.choice([0, 1, 2, ACTOR, OTHER, rng.getrandbits(32)]) for _ in range(11)]
        if kind == 0x50:
            message[1] = rng.choice([0, NAMES, NAMES + 0x40, NAMES + 0x80])
        if kind in (0x37, 0x94, 0x40):
            message[1:5] = [fb(rng.uniform(-50, 50)) for _ in range(4)]
            if kind == 0x94:
                message[1] = rng.choice([0, OTHER, 7])
        if kind == 0x2D:
            message[2] = OTHER
        alert = rng.choice([0.0, 9.0, 10.0, 12.5, -3.0])
        mind = [fb(alert), rng.choice([0, 3]), rng.choice([0, SOURCE, OTHER]), rng.randrange(5)]
        health = rng.choice([0, 1, 2, 3, 5, 6, 10, 11, 40] if kind != 0x90 else [2, 3, 5, 6, 10, 11])
        floor, other_key, source_key = rng.randrange(4), rng.choice([0x12, 0x8001]), rng.choice([ACTOR, OTHER, 1])
        chapter, flags = rng.choice([0, 1, 1, 2]), rng.choice([0, 0x8000, 0x7FFF, 0x10000])
        line += message + mind + [health, floor, other_key, source_key, chapter, flags]
        lines.append(' '.join('%u' % (v & 0xffffffff) for v in line))
        # Retail state.
        w32(u, MIND + 8, ACTOR)
        w32(u, MIND + 0x50, mind[0]); w32(u, MIND + 0x7C, mind[1]); w32(u, MIND + 0x88, mind[2]); w32(u, MIND + 0x150, mind[3])
        w32(u, ACTOR, VTABLE); w32(u, VTABLE + 0xC, ACTOR_MESSAGE)
        w32(u, ACTOR + 0x95C, BODY); w32(u, ACTOR + 0xCEC, floor)
        w16(u, BODY + 0x94, health)
        w16(u, OTHER + 6, other_key)
        w32(u, SOURCE + 0x10, source_key)
        for i, text in enumerate(NAME_TEXT):
            u.mem_write(NAMES + 0x40 * (i - 1) if i else NAMES + 0xC0, text or b'\0')
        w32(u, 0x00430D0C, chapter); w32(u, 0x0050EA9C, flags)
        for address in PULSE:
            w32(u, address, 0xA5A5A5A5)
        for i, word in enumerate(message):
            w32(u, MESSAGE + 4 * i, word)
        w32(u, STACK, STOP); w32(u, STACK + 4, MIND); w32(u, STACK + 8, MESSAGE)
        u.reg_write(UC_X86_REG_ESP, STACK)
        u.emu_start(0x0007F1E0, STOP, count=100000)
        state = [r32(u, MIND + o) for o in (0x50, 0x7C, 0x88, 0x150)] + [r32(u, a) for a in PULSE]
        expected.append(' '.join('%x' % w for w in state) + ' | ' + ' ; '.join(r.log))
    text = str(count) + '\n' + '\n'.join(lines) + '\n'
    open(os.path.join(ROOT, 'cache', 'oracle', 'player_mind_messages_input.txt'), 'w').write(text)
    result = subprocess.run([harness('player_mind_messages_harness')], input=text, capture_output=True, text=True)
    if result.returncode != 0:
        print('harness exit', result.returncode, result.stderr[-400:])
    got = result.stdout.splitlines()
    bad = abs(len(expected) - len(got))
    for i, (want, have) in enumerate(zip(expected, got)):
        if want.split() != have.split():
            bad += 1
            if bad <= 6:
                print('MISMATCH', i, hex(IDS[i % len(IDS)]), '\n exp', want, '\n got', have)
    print('player mind messages: cases', len(expected), len(got), 'mismatches', bad)
    return int(bad != 0)

if __name__ == '__main__':
    raise SystemExit(main())
