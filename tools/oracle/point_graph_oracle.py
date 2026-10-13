"""Point-graph solver, rotation and D3DX matrix helpers: retail machine code against the native translation.

Maps every section of the local default.xbe (never committed) into Unicorn, runs each case on retail code
and on cache/oracle/point_graph_harness, and compares the whole test memory image and the returned word.
Covers 0x00102500, 0x00101C80, 0x00101DD0, 0x00101F10, 0x00102030 (with 0x00102280), 0x00052040,
0x0010BEB0 (with 0x00153A00 and D3DXVec3Normalize), 0x0010C340 (with 0x0010C100 and 0x00103400),
0x0010C8D0 and D3DX 0x001850CA / 0x00184B7B / 0x0018462D.
"""
import os
import random
import struct
import subprocess
import sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UcError
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESI,
    UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_XMM1)

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
XBE = os.path.join(ROOT, 'OriginalFiles', 'default.xbe')
BASE = 0x40000000
SIZE = 0x8000
FIELDS, POINTS, EDGES, COORDS, FLAGS, CONTACTS, VERTICES, SCRATCH = (0, 0x1000, 0x2000, 0x3000, 0x3800, 0x4000,
    0x5000, 0x6000)
STACK = 0x50000000
STOP = 0x00F00000


def load_xbe(cpu):
    data = open(XBE, 'rb').read()
    base = struct.unpack_from('<I', data, 0x104)[0]
    count = struct.unpack_from('<I', data, 0x11C)[0]
    headers = struct.unpack_from('<I', data, 0x120)[0] - base
    for index in range(count):
        flags, vaddr, vsize, raddr, rsize, name = struct.unpack_from('<6I', data, headers + index * 0x38)
        start = vaddr & ~0xFFF
        end = (vaddr + vsize + 0xFFF) & ~0xFFF
        for page in range(start, end, 0x1000):
            try:
                cpu.mem_map(page, 0x1000)
            except UcError:
                pass
        cpu.mem_write(vaddr, data[raddr:raddr + min(rsize, vsize)])


def new_cpu():
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    load_xbe(cpu)
    cpu.mem_map(BASE, SIZE)
    cpu.mem_map(STACK - 0x10000, 0x20000)
    cpu.mem_map(STOP, 0x1000)
    cpu.mem_write(STOP, b'\xf4')
    return cpu


def f32(value):
    return struct.pack('<f', value)


def run(cpu, address, registers, stack_words, xmm1=None):
    esp = STACK - 0x100
    cpu.mem_write(esp, struct.pack('<I', STOP) + b''.join(struct.pack('<I', w & 0xFFFFFFFF) for w in stack_words))
    cpu.reg_write(UC_X86_REG_ESP, esp)
    for register, value in registers.items():
        cpu.reg_write(register, value & 0xFFFFFFFF)
    if xmm1 is not None:
        cpu.reg_write(UC_X86_REG_XMM1, struct.unpack('<I', f32(xmm1))[0])
    cpu.emu_start(address, STOP, count=20000000)
    return cpu.reg_read(UC_X86_REG_EAX)


def word(value):
    return struct.unpack('<I', f32(value))[0]


def build_case(rng, mode):
    memory = bytearray(SIZE)
    words = [0] * 16
    points = rng.randrange(2, 24)
    edges = rng.randrange(1, 40)
    scale = rng.choice([1., 10., 100.])
    struct.pack_into('<I', memory, FIELDS, points)
    struct.pack_into('<I', memory, FIELDS + 4, BASE + POINTS)
    struct.pack_into('<I', memory, FIELDS + 8, edges)
    struct.pack_into('<I', memory, FIELDS + 0x10, rng.randrange(0, edges + 1))
    struct.pack_into('<I', memory, FIELDS + 0x14, BASE + EDGES)
    memory[FIELDS + 0x18] = rng.randrange(2)
    memory[FIELDS + 0x19] = rng.randrange(2)
    struct.pack_into('<f', memory, FIELDS + 0x1C, rng.choice([0., .01, .1, .5]))
    struct.pack_into('<I', memory, FIELDS + 0x24, BASE + COORDS)
    struct.pack_into('<I', memory, FIELDS + 0x2C, BASE + FLAGS)
    for index in range(points):
        row = POINTS + index * 0x30
        for offset in range(0, 0x24, 4):
            struct.pack_into('<f', memory, row + offset, rng.uniform(-scale, scale))
        struct.pack_into('<f', memory, row + 0x24, rng.choice([0., .5, 1., 2.]))
        memory[row + 0x28] = 1 if rng.random() < .2 else 0
        for offset in range(0, 0xC, 4):
            struct.pack_into('<f', memory, COORDS + index * 0xC + offset, rng.uniform(-scale, scale))
    for index in range(edges):
        row = EDGES + index * 0x1C
        a = rng.randrange(points)
        b = rng.randrange(points)
        struct.pack_into('<II', memory, row, a, b)
        struct.pack_into('<II', memory, row + 8, BASE + POINTS + a * 0x30, BASE + POINTS + b * 0x30)
        length = rng.uniform(.1, scale)
        struct.pack_into('<fff', memory, row + 0x10, length, rng.choice([1. / (length * length), .5, 1.]),
            length * length)
        memory[FLAGS + index] = rng.randrange(2)
    for offset in range(0x70, 0x1B0, 4):
        struct.pack_into('<f', memory, FIELDS + offset, rng.uniform(-2, 2))
    for offset in (0x1BC, 0x1C0, 0x1C4):
        struct.pack_into('<I', memory, FIELDS + offset, rng.randrange(edges))
    struct.pack_into('<I', memory, FIELDS + 0x1C8, rng.randrange(12))
    reals = [rng.uniform(-scale, scale) for _ in range(16)]
    if mode in (1, 2, 3):
        words[1] = rng.randrange(points)
        words[2] = rng.randrange(points)
        words[3] = word(rng.choice([rng.uniform(.1, scale), .5, 1.]))
    elif mode == 0:
        words[0] = word(rng.choice([1. / 60, 1. / 30, .1, 0.]))
    elif mode == 5:
        count = rng.randrange(0, 12)
        struct.pack_into('<I', memory, SCRATCH, count)
        for index in range(count):
            row = SCRATCH + 4 + index * 0x28
            for offset in range(0, 0x24, 4):
                struct.pack_into('<f', memory, row + offset, rng.uniform(-scale, scale))
            struct.pack_into('<f', memory, row + 0x24, rng.uniform(0, scale * 2))
        for index in range(4, 7):
            words[index] = word(reals[index])
    elif mode == 6:
        for index in range(4, 10):
            words[index] = word(reals[index])
        if rng.random() < .2:
            for index in range(7, 10):
                words[index] = words[index - 3]
    elif mode == 7:
        count = rng.randrange(0, 6)
        words[1] = count
        for index in range(18):
            for offset in range(0, 0xC, 4):
                struct.pack_into('<f', memory, VERTICES + index * 0xC + offset, rng.uniform(-scale, scale))
        for index in range(count):
            row = CONTACTS + index * 0x28
            corners = [BASE + VERTICES + rng.randrange(18) * 0xC for _ in range(3)]
            struct.pack_into('<III', memory, row, *corners)
            for offset in range(0xC, 0x28, 4):
                struct.pack_into('<f', memory, row + offset, rng.uniform(-1, 1))
    elif mode == 10:
        words[0] = word(rng.choice([1. / 60, .1, .05]))
        memory[FIELDS + 0x1D4] = rng.randrange(0, 4)
        asset, block, grid, triangles, cells, lists = 0x6800, 0x6880, 0x6900, 0x7000, 0x7400, 0x7600
        struct.pack_into('<I', memory, asset + 0x10, BASE + block)
        struct.pack_into('<I', memory, block + 0x14, BASE + grid)
        for index in range(18):
            for offset in range(0, 0xC, 4):
                struct.pack_into('<f', memory, VERTICES + index * 0xC + offset, rng.uniform(-scale, scale))
        struct.pack_into('<II', memory, grid + 8, BASE + VERTICES, BASE + triangles)
        struct.pack_into('<III', memory, grid + 0x18, 2, 2, 2)
        struct.pack_into('<ffff', memory, grid + 0x28, scale, -scale, -scale, -scale)
        struct.pack_into('<I', memory, grid + 0x44, BASE + cells)
        for index in range(16):
            struct.pack_into('<III', memory, triangles + index * 0xC, *[rng.randrange(18) for _ in range(3)])
        for cell in range(8):
            count = rng.randrange(0, 16)
            struct.pack_into('<II', memory, cells + cell * 8, count, BASE + lists + cell * 0x40)
            for entry in range(count):
                struct.pack_into('<I', memory, lists + cell * 0x40 + entry * 4, rng.randrange(16))
    elif mode == 9:
        for offset in range(0, 0xC0, 4):
            struct.pack_into('<f', memory, SCRATCH + offset, rng.uniform(-scale, scale))
        for index in range(4, 10):
            words[index] = word(reals[index])
    return memory, words


def retail(cpu, mode, memory, words):
    cpu.mem_write(BASE, bytes(memory))
    fields = BASE + FIELDS
    reals = [struct.unpack('<f', struct.pack('<I', w))[0] for w in words]
    result = 0
    if mode == 0:
        run(cpu, 0x102500, {UC_X86_REG_ESI: fields}, [], xmm1=reals[0])
    elif mode == 1:
        run(cpu, 0x101C80, {UC_X86_REG_ECX: words[1], UC_X86_REG_EDX: words[2]}, [fields, words[3]])
    elif mode == 2:
        run(cpu, 0x101DD0, {UC_X86_REG_ECX: words[1], UC_X86_REG_EDX: words[2]}, [fields, words[3]])
    elif mode == 3:
        run(cpu, 0x101F10, {UC_X86_REG_EAX: words[1], UC_X86_REG_ESI: words[2], UC_X86_REG_EDX: fields},
            [words[3]])
    elif mode == 4:
        run(cpu, 0x102030, {UC_X86_REG_EAX: fields}, [])
    elif mode == 5:
        cpu.mem_write(BASE + SCRATCH + 0x700, struct.pack('<III', words[4], words[5], words[6]))
        result = run(cpu, 0x52040, {UC_X86_REG_EDX: BASE + SCRATCH + 0x700}, [BASE + SCRATCH])
        cpu.mem_write(BASE + SCRATCH + 0x700, bytes(12))
    elif mode == 6:
        vectors = BASE + SCRATCH + 0x700
        cpu.mem_write(vectors, struct.pack('<6I', *words[4:10]))
        result = run(cpu, 0x10BEB0, {UC_X86_REG_ESI: BASE + SCRATCH, UC_X86_REG_EAX: vectors}, [vectors + 0xC])
        cpu.mem_write(vectors, bytes(0x18))
    elif mode == 7:
        result = run(cpu, 0x10C340, {UC_X86_REG_ESI: fields}, [BASE + CONTACTS, words[1]])
    elif mode == 8:
        run(cpu, 0x10C8D0, {UC_X86_REG_EBX: fields}, [])
    elif mode == 10:
        run(cpu, 0x10CFE0, {}, [fields, BASE + 0x6800], xmm1=reals[0])
    elif mode == 9:
        out = BASE + SCRATCH
        run(cpu, 0x1850CA, {}, [out, out + 0x40, out + 0x80])
        run(cpu, 0x184B7B, {}, [out + 0xC0, words[4], words[5], words[6]])
        cpu.mem_write(BASE + SCRATCH + 0x700, struct.pack('<III', words[7], words[8], words[9]))
        run(cpu, 0x18462D, {}, [out + 0x100, BASE + SCRATCH + 0x700, out + 0x40])
        cpu.mem_write(BASE + SCRATCH + 0x700, bytes(12))
    return bytes(cpu.mem_read(BASE, SIZE)), result & 0xFFFFFFFF


def main():
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 401
    cases = int(sys.argv[2]) if len(sys.argv) > 2 else 1000
    only = int(sys.argv[3]) if len(sys.argv) > 3 else None
    rng = random.Random(seed)
    cpu = new_cpu()
    batch = []
    expected = []
    for case in range(cases):
        mode = only if only is not None else case % 11
        memory, words = build_case(rng, mode)
        image, result = retail(cpu, mode, memory, words)
        batch.append(struct.pack('<II', mode, 0) + bytes(memory) + struct.pack('<16I', *words))
        expected.append((mode, image, result))
    harness = os.path.join(ROOT, 'cache', 'oracle', 'point_graph_harness' + ('.exe' if os.name == 'nt' else ''))
    output = subprocess.run([harness], input=b''.join(batch), capture_output=True, check=True).stdout
    mismatches = 0
    step = SIZE + 4
    for index, (mode, image, result) in enumerate(expected):
        native = output[index * step:(index + 1) * step]
        if native[:SIZE] != image or struct.unpack('<I', native[SIZE:])[0] != result:
            mismatches += 1
            if mismatches <= 8:
                first = next((offset for offset in range(SIZE) if native[offset] != image[offset]), None)
                print('mismatch case', index, 'mode', mode, 'first byte', None if first is None else hex(first),
                    'result', hex(result), hex(struct.unpack('<I', native[SIZE:])[0]))
    print('cases', cases, 'mismatches', mismatches)
    return 1 if mismatches else 0


if __name__ == '__main__':
    sys.exit(main())
