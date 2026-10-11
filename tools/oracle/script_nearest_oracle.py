"""Differential check for the nearest script actor query 0x00128DF0 and its path gate 0x001295E0, with
the real filter 0x00129420 (flags limited to 0 and 0x4000, so only the requester is filtered) and the
real square root 0x00152B30. Positions 0x000412A0, the path walk 0x0012CC50 and the predicate are
logging stubs with scripted results. Needs cache/oracle/mem_00011000.bin and mem_00210000.bin."""
import os, random, struct, subprocess, sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP
from retail_cpu import ROOT, STOP, STACK, HEAP, w32, r32, harness

def w16(u, a, v): u.mem_write(a, struct.pack('<H', v & 0xffff))
def fb(v): return struct.unpack('<I', struct.pack('<f', v))[0]

CANDIDATES, REQUESTER, POINT, OUT, MINDS = HEAP, HEAP + 0x8000, HEAP + 0x9000, HEAP + 0x9100, HEAP + 0xA000
PREDICATE = 0x00F20020
QUEUES = ['path', 'predicate']

class Retail:
    def __init__(self, queues, objects):
        u = Uc(UC_ARCH_X86, UC_MODE_32)
        u.mem_map(0x00010000, 0x150000)
        u.mem_write(0x00011000, open(os.path.join(ROOT, 'cache', 'oracle', 'mem_00011000.bin'), 'rb').read())
        u.mem_map(0x00200000, 0x10000)
        u.mem_write(0x00203000, open(os.path.join(ROOT, 'cache', 'oracle', 'mem_00203000.bin'), 'rb').read()[:0xD000])
        u.mem_map(0x00210000, 0x130000)
        u.mem_write(0x00210000, open(os.path.join(ROOT, 'cache', 'oracle', 'mem_00210000.bin'), 'rb').read())
        u.mem_map(0x00400000, 0x200000)
        u.mem_map(STOP, 0x1000); u.mem_write(STOP, b'\xf4')
        u.mem_map(0x00F20000, 0x1000); u.mem_write(0x00F20000, b'\xc3' * 0x1000)
        u.mem_map(HEAP, 0x100000)
        u.mem_map(STACK - 0x10000, 0x20000)
        self.u, self.queues, self.log, self.objects = u, {k: list(v) for k, v in queues.items()}, [], objects
        for address, name in ((0x000412A0, 'position'), (0x0012CC50, 'path'), (PREDICATE, 'predicate')):
            u.mem_write(address, b'\xc3')
            u.hook_add(UC_HOOK_CODE, self.dispatch, begin=address, end=address, user_data=name)

    def pop(self, name):
        q = self.queues[name]
        return q.pop(0) if q else 0

    def arg(self, k):
        return r32(self.u, self.u.reg_read(UC_X86_REG_ESP) + 4 + 4 * k)

    def name(self, address):
        return self.objects.get(address, ('?', None))[0]

    def dispatch(self, uc, address, size, name):
        u = self.u
        if name == 'position':
            actor, out = u.reg_read(UC_X86_REG_ESI), u.reg_read(UC_X86_REG_EDI)
            for i, value in enumerate(self.objects[actor][1]):
                w32(u, out + 4 * i, value)
            self.log.append('position %s' % self.name(actor))
        elif name == 'path':
            v = self.pop('path')
            source, destination = u.reg_read(UC_X86_REG_EAX), self.arg(0)
            words = [r32(u, a + 4 * i) for a in (source, destination) for i in range(3)]
            self.log.append('path ' + ' '.join('%08x' % w for w in words) + ' %x %x %x %x %x %x %d' % (
                self.arg(1), self.arg(2), self.arg(3), self.arg(4), self.arg(5), self.arg(7) & 0xffff, self.arg(6) != 0))
            u.reg_write(UC_X86_REG_EAX, v)
        else:
            v = self.pop('predicate')
            self.log.append('predicate %s %s' % (self.name(self.arg(0)), self.name(self.arg(1))))
            u.reg_write(UC_X86_REG_EAX, v)

def main():
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 3000
    rng = random.Random(seed)
    lines, expected = [], []
    for case in range(count):
        queues = {q: [rng.choice([0, 1, 1]) for _ in range(8)] for q in QUEUES}
        line = []
        for q in QUEUES:
            line += queues[q]
        n = rng.randrange(5)
        candidates = []
        for i in range(n):
            tag = rng.choice([0, 3, 5])
            position = [fb(rng.choice([rng.uniform(-150, 150), 10.0])) for _ in range(3)]
            candidates.append((tag, position))
            line += [tag] + position
        requester = rng.choice([-1, -1] + list(range(n)))
        requester_class = rng.choice([0x12, 0x8003])
        requester_position = [fb(rng.uniform(-150, 150)) for _ in range(3)]
        point = rng.choice([None, [fb(rng.uniform(-150, 150)) for _ in range(3)]])
        tag = rng.choice([0, 0, 3, 5])
        radius = rng.choice([0.0, -1.0, 50.0, 150.0, 400.0])
        flags = rng.choice([0, 0x4000])
        predicate = rng.choice([0, 1])
        distance = rng.choice([0, 1, 1])
        line += [requester & 0xffffffff, requester_class] + requester_position
        line += [0 if point is None else 1] + (point or [0, 0, 0])
        line += [tag, fb(radius), flags, predicate, distance]
        lines.append(' '.join('%u' % (v & 0xffffffff) for v in line))
        objects = {}
        for i, (candidate_tag, position) in enumerate(candidates):
            objects[CANDIDATES + 0x1000 * i] = ('c%d' % i, position)
        requester_address = REQUESTER if requester < 0 else CANDIDATES + 0x1000 * requester
        if requester < 0:
            objects[REQUESTER] = ('r', requester_position)
        r = Retail(queues, objects)
        u = r.u
        links = []
        for i, (candidate_tag, position) in enumerate(candidates):
            obj = CANDIDATES + 0x1000 * i
            w16(u, obj + 6, candidate_tag)
            w32(u, obj + 0x95C, MINDS + 0x100 * i)
            links.append(obj + 0x934)
        for i, link in enumerate(links):
            w32(u, link + 4, links[i + 1] if i + 1 < len(links) else 0)
        w32(u, 0x00511B38, links[0] if links else 0)
        if requester < 0:
            w16(u, REQUESTER + 6, requester_class)
        if point is not None:
            for i, value in enumerate(point):
                w32(u, POINT + 4 * i, value)
        w32(u, OUT, 0xA5A5A5A5)
        args = (requester_address, tag, fb(radius), OUT if distance else 0, flags, PREDICATE if predicate else 0)
        w32(u, STACK, STOP)
        for i, a in enumerate(args):
            w32(u, STACK + 4 + 4 * i, a)
        u.reg_write(UC_X86_REG_ESP, STACK)
        u.reg_write(UC_X86_REG_EAX, 0 if point is None else POINT)
        u.emu_start(0x00128DF0, STOP, count=1000000)
        result = u.reg_read(UC_X86_REG_EAX)
        found = r.name(result) if result else '-'
        expected.append('%s %08x | %s' % (found, r32(u, OUT), ' ; '.join(r.log)))
    text = str(count) + '\n' + '\n'.join(lines) + '\n'
    open(os.path.join(ROOT, 'cache', 'oracle', 'script_nearest_input.txt'), 'w').write(text)
    result = subprocess.run([harness('script_nearest_harness')], input=text, capture_output=True, text=True)
    if result.returncode != 0:
        print('harness exit', result.returncode, result.stderr[-400:])
    got = result.stdout.splitlines()
    bad = abs(len(expected) - len(got))
    for i, (want, have) in enumerate(zip(expected, got)):
        if want.split() != have.split():
            bad += 1
            if bad <= 5:
                print('MISMATCH', i, lines[i], '\n exp', want, '\n got', have)
    print('script nearest: cases', len(expected), len(got), 'mismatches', bad)
    return int(bad != 0)

if __name__ == '__main__':
    raise SystemExit(main())
