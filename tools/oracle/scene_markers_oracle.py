"""Differential check for scene marker loading and dispatch: 0x0011CB80/0x000FFE20, 0x00140020 with
0x001401B0/0x001402B0/0x00140450/0x0011EEC0, 0x0011F1C0, 0x00127FF0, and the marker handlers for kinds 2
(0x000FFEC0, 0x00100110, 0x000FFFD0), 3 (0x000FF680), 4 (0x000FF4F0, 0x001349E0, 0x00069F30), 6
(0x001001A0), 8/0x14 (0x00100210/0x001002B0, 0x001246A0), 0xB (0x000FFAD0, 0x0006A010), 0x10
(0x000FFDC0, 0x0006E110) and 0x13 (0x000FF880). 0x000411A0, 0x001347C0, 0x0011F2E0, 0x000FBFB0 and
0x000662A0 run for real; factory creation/cloning, the player spawn, the end gate, animation starts,
object messages and resource calls are logging stubs. Needs mem_00011000.bin, mem_00210000.bin and the
retail XBE (OriginalFiles/default.xbe) for the actor tag table at 0x0042A854."""
import os, random, struct, subprocess, sys, zlib
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP)
from retail_cpu import ROOT, STOP, STACK, w32, r32, harness

def r16(u, a): return struct.unpack('<H', u.mem_read(a, 2))[0]
def w16(u, a, v): u.mem_write(a, struct.pack('<H', v & 0xffff))
def fb(v): return struct.unpack('<I', struct.pack('<f', v))[0]
def cstr(u, a, n=0x80): return bytes(u.mem_read(a, n)).split(b'\0')[0].decode('latin1')

def xbe_reader():
    data = open(os.path.join(ROOT, 'OriginalFiles', 'default.xbe'), 'rb').read()
    base = struct.unpack_from('<I', data, 0x104)[0]
    count, table = struct.unpack_from('<II', data, 0x11C)
    sections = []
    for i in range(count):
        flags, va, vsize, raw, rsize = struct.unpack_from('<IIIII', data, table - base + i * 0x38)
        sections.append((va, vsize, raw, rsize))
    def read(va, n):
        for sva, vs, raw, rs in sections:
            if sva <= va < sva + vs:
                off = va - sva
                return bytes(data[raw + off:raw + min(off + n, rs)]).ljust(n, b'\0')
        return bytes(n)
    return read

XBE = xbe_reader()
HEAP, ARENA = 0x10000000, 0x10400000
RECORDS, OBJECTS, VTABLE, RESOURCE = 0x10000000, 0x10100000, 0x10200000, 0x10300000
DEFAULT_NAME = 0x00546208
MESSAGE_STUB = 0x00F20010
SIZES = {2: 0x30, 3: 0x1A8, 4: 0x434, 6: 0xA8, 8: 0x80, 0xB: 0x350, 0x10: 0x3C, 0x13: 0x2C, 0x14: 0x84, 1: 0x2C,
         0x1A: 0x2C}
NAMES = [b'aid_objparams_test_alpha', b'aid_objparams_test_beta', b'aid_model_test_gamma', b'notaid_name',
         b'aid_anim_test_delta', b'aid_callout_test_eps']
TEMPLATES = {b'aid_objparams_test_alpha': 0x63C, b'aid_objparams_test_beta': 0x3C4}
CALLEES = {0x00107520: 'create', 0x00107720: 'clone', 0x00134920: 'player', 0x00123B00: 'gate', 0x0003ADA0: 'anim',
           MESSAGE_STUB: 'message', 0x00035E90: 'acquire', 0x00035C00: 'release', 0x00032700: 'error',
           0x000F72D0: 'alloc', 0x000F73A0: 'realloc', 0x000F7100: 'heapindex', 0x0014D360: 'heapfree',
           0x000663E0: 'free'}
QUEUES = ['create', 'player', 'gate']

def checksum(data):
    return zlib.crc32(bytes(data)) & 0xffffffff

def template_bytes(name):
    size = TEMPLATES[name]
    data = bytearray(size)
    struct.pack_into('<HHH', data, 0, size, 0x6E, 5 if size == 0x63C else 0xD)
    for i in range(0x40, size, 4):
        struct.pack_into('<I', data, i, (i * 2654435761) & 0xffffffff)
    return bytes(data)

class Retail:
    def __init__(self, queues):
        u = Uc(UC_ARCH_X86, UC_MODE_32)
        u.mem_map(0, 0x1000)
        u.mem_map(0x00010000, 0x150000)
        u.mem_write(0x00011000, open(os.path.join(ROOT, 'cache', 'oracle', 'mem_00011000.bin'), 'rb').read())
        u.mem_map(0x00210000, 0x130000)
        u.mem_write(0x00210000, open(os.path.join(ROOT, 'cache', 'oracle', 'mem_00210000.bin'), 'rb').read())
        u.mem_map(0x00400000, 0x200000)
        u.mem_write(0x0042A000, XBE(0x0042A000, 0x1000))
        u.mem_map(STOP, 0x1000); u.mem_write(STOP, b'\xf4')
        u.mem_map(0x00F20000, 0x1000); u.mem_write(0x00F20000, b'\xc3' * 0x1000)
        u.mem_map(HEAP, 0x800000)
        u.mem_map(STACK - 0x10000, 0x20000)
        self.u, self.queues, self.log = u, {k: list(v) for k, v in queues.items()}, []
        self.next = ARENA
        self.sizes = {}
        self.objects = 0
        w32(u, VTABLE + 0xC, MESSAGE_STUB)
        for address, name in CALLEES.items():
            u.mem_write(address, b'\xc3')
            u.hook_add(UC_HOOK_CODE, self.dispatch, begin=address, end=address, user_data=name)

    def pop(self, name):
        q = self.queues[name]
        return q.pop(0) if q else 0

    def arg(self, k):
        return r32(self.u, self.u.reg_read(UC_X86_REG_ESP) + 4 + 4 * k)

    def ret(self, value):
        self.u.reg_write(UC_X86_REG_EAX, value & 0xffffffff)

    def allocate(self, size):
        address = self.next
        self.next += (size + 0xF) & ~0xF
        self.sizes[address] = size
        return address

    def new_object(self):
        address = OBJECTS + 0x1000 * self.objects
        self.objects += 1
        w32(self.u, address, VTABLE)
        return address

    def dispatch(self, uc, address, size, name):
        u, log, reg = self.u, self.log, self.u.reg_read
        if name == 'alloc':
            self.ret(self.allocate(self.arg(0)))
        elif name == 'realloc':
            old, new = reg(UC_X86_REG_EAX), self.arg(0)
            fresh = self.allocate(new)
            if old:
                u.mem_write(fresh, bytes(u.mem_read(old, min(self.sizes.get(old, 0), new))))
            self.ret(fresh)
        elif name == 'heapindex':
            self.ret(0)
        elif name in ('heapfree', 'free'):
            pass
        elif name == 'create':
            params = self.arg(0)
            n = r16(u, params)
            log.append('create %x %x %08x' % (r16(u, params + 4), n, checksum(u.mem_read(params, n))))
            self.ret(self.new_object() if self.pop('create') else 0)
        elif name == 'clone':
            text = cstr(u, reg(UC_X86_REG_EAX)).encode()
            log.append('clone %s' % text.decode())
            if text in TEMPLATES:
                data = template_bytes(text)
                block = self.allocate(len(data))
                u.mem_write(block, data)
                self.ret(block)
            else:
                self.ret(0)
        elif name == 'player':
            p, e = reg(UC_X86_REG_EBX), self.arg(0)
            log.append('player %d %s' % (reg(UC_X86_REG_EAX), ' '.join('%08x' % r32(u, a + 4 * i) for a in (p, e) for i in range(3))))
            self.ret(self.new_object() if self.pop('player') else 0)
        elif name == 'gate':
            v = self.pop('gate'); log.append('gate'); self.ret(v)
        elif name == 'anim':
            mix = reg(UC_X86_REG_EDI)
            log.append('anim %x %d %s %s %s' % ((reg(UC_X86_REG_EBX) - OBJECTS) // 0x1000, self.arg(0), cstr(u, self.arg(1)),
                       ' '.join('%08x' % r32(u, mix + o) for o in (4, 8, 0xC, 0x10, 0x14, 0x1C, 0x20)) + ' %x' % r16(u, mix + 0x18),
                       cstr(u, mix + 0x24)))
        elif name == 'message':
            obj, words = self.arg(0), self.arg(1)
            first = r32(u, words)
            n = 0x42 if first == 0xA8 else 9
            log.append('message %x %x %08x' % ((obj - OBJECTS) // 0x1000, first, checksum(u.mem_read(words, 4 * n))))
        elif name == 'acquire':
            text = cstr(u, self.arg(0))
            log.append('acquire %s' % text)
            w32(u, self.arg(1), RESOURCE if text == self.resource_name else 0)
        elif name == 'release':
            log.append('release')
        elif name == 'error':
            log.append('error %d' % self.arg(0))

def build_records(rng, line, kinds_allowed):
    count = rng.randrange(0, 9)
    kinds = [rng.choice(kinds_allowed) for _ in range(count)]
    ids = rng.sample(range(1, 40), count)
    rows = []
    for kind, rid in zip(kinds, ids):
        size = SIZES[kind]
        data = bytearray(rng.getrandbits(8) for _ in range(size))
        struct.pack_into('<IHH', data, 0, size, kind, rid)
        struct.pack_into('<H', data, 8, rng.choice([0, 0, 0] + ids + [99]))
        for off in range(0xC, 0x24, 4):
            struct.pack_into('<f', data, off, rng.uniform(-50, 50))
        struct.pack_into('<f', data, 0x24, rng.choice([1.0, 0.5]))
        for off in (0x28, 0x4C, 0xA8, 0xCC, 0x128, 0x1A8, 0x30, 0x34):
            if off + 0x30 <= size and rng.random() < 0.7:
                text = rng.choice(NAMES)
                data[off:off + len(text) + 1] = text + b'\0'
        if kind == 2:
            for off in (0x28, 0x2A):
                struct.pack_into('<H', data, off, rng.choice([0, 0, 1]))
            struct.pack_into('<I', data, 0x2C, rng.choice([0, 1]))
        if kind == 0x13:
            struct.pack_into('<I', data, 0x28, rng.choice([0, 0, 1]))
        if kind in (8, 0x14):
            struct.pack_into('<ii', data, 0x2C, rng.randrange(-20, 60), rng.randrange(-20, 60))
            if kind == 8:
                data[0x30:0x40] = rng.choice([b'objTag_Actor\0', b'zzz\0', b'objTag_Actor_Imp\0']).ljust(16, b'\0')
            else:
                data[0x34:0x44] = rng.choice([b'objTag_Actor\0', b'zzz\0']).ljust(16, b'\0')
        if kind == 4:
            struct.pack_into('<II', data, 0x228, rng.choice([0, 0, 3]), rng.randrange(0x10000))
        if kind == 0xB:
            struct.pack_into('<ii', data, 0x40, rng.choice([0, 0, 25]), rng.choice([0, 5]))
        rows.append(bytes(data))
    block = bytearray(struct.pack('<II', len(rows), 8 + sum(len(r) for r in rows)))
    for r in rows:
        block += r
    line += [len(block)] + list(block)
    return bytes(block)

def run_case(rng, entry):
    queues = {q: [rng.choice([0, 1, 1]) for _ in range(40)] for q in QUEUES}
    line = [entry]
    for q in QUEUES:
        line += queues[q]
    r = Retail(queues)
    u = r.u
    r.resource_name = ''
    kinds = [2, 3, 4, 6, 8, 0xB, 0x10, 0x13, 0x14, 1, 0x1A]
    block = build_records(rng, line, kinds)
    u.mem_write(RECORDS, block)
    w32(u, 0x0051108C, RECORDS)
    u.mem_write(DEFAULT_NAME, b'\0')
    # Randomizer tables.
    swap_count = rng.randrange(3)
    ids = [struct.unpack_from('<H', block, 8 + 6)[0]] if len(block) > 8 else [0]
    swaps = [rng.randrange(1, 40) for _ in range(2 * swap_count)]
    groups = rng.randrange(3)
    group_ids = [rng.choice([0, rng.randrange(1, 40)]) for _ in range(10 * groups)]
    line += [swap_count] + swaps + [groups] + group_ids
    w32(u, 0x00545CC8, swap_count)
    for i, v in enumerate(swaps):
        w16(u, 0x00545C48 + 2 * i, v)
    w32(u, 0x00545D1C, groups)
    for i, v in enumerate(group_ids):
        w16(u, 0x00545CCC + 2 * i, v)
    seed = rng.getrandbits(32)
    w32(u, 0x0054DD7C, seed)
    # Player-start gates.
    gates = [rng.choice([0, 1]), rng.choice([1, 1, 0]), rng.choice([0, 0, 8]), rng.choice([0, 0, 0x200000])]
    line += [seed] + gates
    w32(u, 0x005462FC, gates[0]); w32(u, 0x00430D0C, gates[1])
    w32(u, 0x00506C74, HEAP + 0x7F0000); w32(u, HEAP + 0x7F0008, HEAP + 0x7F0010); u.mem_write(HEAP + 0x7F0010, bytes([gates[2]]))
    w32(u, 0x0050EA9C, gates[3])
    w32(u, 0x00512344, 0); w32(u, 0x00512348, 0)
    for a in range(0x00545AD0, 0x00545AEC, 4):
        w32(u, a, 0xA5A5A5A5)
    argument = 0
    w32(u, STACK, STOP)
    u.reg_write(UC_X86_REG_ESP, STACK)
    if entry == 0:
        u.emu_start(0x00140020, STOP, count=5000000)
    elif entry == 1:
        argument = rng.randrange(0, 40)
        kind = rng.choice([0, 2, 4, 8])
        wrap = rng.choice([0, 1])
        line += [argument, kind, wrap]
        w32(u, STACK + 4, kind); w32(u, STACK + 8, wrap)
        u.reg_write(UC_X86_REG_EAX, argument)
        u.emu_start(0x0011F1C0, STOP, count=100000)
    elif entry == 2:
        u.emu_start(0x00127FF0, STOP, count=5000000)
    else:
        name = rng.choice(['aid_script_ghoulies_scene_x', 'aid_s_y', 'aid_scriptq_ghoulies_z'])
        r.resource_name = 'aid_marker_' + name.split('_', 2)[2]
        present = rng.choice([0, 1, 1])
        if not present:
            r.resource_name = '-'
        resource = build_records(rng, line, [3, 8])[8:] + struct.pack('<IHH', 0x10, 0, 0) + bytes(8)
        u.mem_write(RESOURCE, resource)
        line += [present] + list(name.encode()) + [0]
        scene = HEAP + 0x7E0000
        u.mem_write(scene, name.encode() + b'\0')
        u.reg_write(UC_X86_REG_ECX, scene)
        u.emu_start(0x0011CB80, STOP, count=1000000)
    result = u.reg_read(UC_X86_REG_EAX)
    out = []
    if entry == 1:
        out.append('%x' % (result & 0xffff))
    if entry == 3:
        out.append('%x %x %08x' % (r32(u, result), r32(u, result + 4), checksum(u.mem_read(result, r32(u, result + 4)))))
    pool = r32(u, 0x00512344)
    occupancy = []
    if pool:
        begin, cursor = r32(u, pool + 4), r32(u, pool + 8)
        occupancy = ['%08x' % checksum(u.mem_read(begin, cursor - begin)), '%x' % ((cursor - begin) // 0x38)]
    camera = ['%08x' % r32(u, a) for a in range(0x00545AD0, 0x00545AEC, 4)]
    out.append('%08x' % r32(u, 0x0054DD7C))
    out.append('%x' % r32(u, 0x00512348))
    expected = ' '.join(out) + ' O ' + ' '.join(occupancy) + ' C ' + ' '.join(camera) + ' | ' + ' ; '.join(r.log)
    return line, expected

def main():
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 2000
    rng = random.Random(seed)
    lines, expected = [], []
    for case in range(count):
        line, want = run_case(rng, case % 4)
        lines.append(' '.join('%u' % (v & 0xffffffff) for v in line))
        expected.append(want)
    text = str(count) + '\n' + '\n'.join(lines) + '\n'
    open(os.path.join(ROOT, 'cache', 'oracle', 'scene_markers_input.txt'), 'w').write(text)
    result = subprocess.run([harness('scene_markers_harness')], input=text, capture_output=True, text=True)
    if result.returncode != 0:
        print('harness exit', result.returncode, result.stderr[-600:])
    got = result.stdout.splitlines()
    bad = abs(len(expected) - len(got))
    for i, (want, have) in enumerate(zip(expected, got)):
        if want.split() != have.split():
            bad += 1
            if bad <= 5:
                print('MISMATCH', i, i % 4, '\n exp', want[:1500], '\n got', have[:1500])
    print('scene markers: cases', len(expected), len(got), 'mismatches', bad)
    return int(bad != 0)

if __name__ == '__main__':
    raise SystemExit(main())
