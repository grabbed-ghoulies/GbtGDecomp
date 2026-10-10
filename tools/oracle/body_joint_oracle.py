"""Compare 0x00073C30 and 0x00101280 with retail machine code and genuine math callees.

Run body_joint_oracle.py [seed] [cases]. Requires the existing cache/oracle memory
exports and body_joint_harness executable. No retail files are changed.
"""
import math
import os
import random
import struct
import subprocess
import sys

from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_MEM_INVALID
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ESP, UC_X86_REG_XMM0, UC_X86_REG_EIP
from retail_cpu import ROOT, STOP, STACK, w32, r32, harness

BODY, ACTOR, JOINT, RESULT = 0x10001000, 0x10003000, 0x10005000, 0x10006000


def bits(value):
    return struct.unpack('<I', struct.pack('<f', value))[0]


def number(word):
    return struct.unpack('<f', struct.pack('<I', word))[0]


def write(data, offset, word):
    struct.pack_into('<I', data, offset, word)


def equivalent(first, second):
    a, b = number(first), number(second)
    if math.isnan(a) and math.isnan(b):
        return True
    if first == second:
        return True
    return math.isfinite(a) and math.isfinite(b) and math.isclose(a, b, rel_tol=2e-6, abs_tol=2e-7)


def emulator():
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0x10000, 0x175000)
    cpu.mem_map(0x200000, 0x10000)
    cpu.mem_map(0x210000, 0x130000)
    cpu.mem_map(0x400000, 0x200000)
    cpu.mem_map(0x10000000, 0x10000)
    cpu.mem_map(STOP, 0x1000)
    cpu.mem_map(STACK - 0x10000, 0x20000)
    def invalid(cpu, access, address, size, value, user):
        print(f'unmapped access at PC {cpu.reg_read(UC_X86_REG_EIP):08x}: {address:08x}', flush=True)
        return False
    cpu.hook_add(UC_HOOK_MEM_INVALID, invalid)
    for base, name in [(0x11000, 'mem_00011000.bin'), (0x160000, 'mem_00160000.bin'),
                       (0x210000, 'mem_00210000.bin'), (0x200000, 'mem_00200000.bin'),
                       (0x203000, 'mem_00203000.bin')]:
        with open(os.path.join(ROOT, 'cache', 'oracle', name), 'rb') as source:
            cpu.mem_write(base, source.read())
    return cpu


def call(cpu, address, args, eax=0, ebx=0, xmm=0):
    w32(cpu, STACK, STOP)
    for index, arg in enumerate(args):
        w32(cpu, STACK + 4 + index * 4, arg)
    cpu.reg_write(UC_X86_REG_ESP, STACK)
    cpu.reg_write(UC_X86_REG_EAX, eax)
    cpu.reg_write(UC_X86_REG_EBX, ebx)
    cpu.reg_write(UC_X86_REG_XMM0, xmm)
    cpu.emu_start(address, STOP, count=100000)
    if cpu.reg_read(UC_X86_REG_ESP) != STACK + 4:
        raise RuntimeError('retail function did not return')
    return cpu.reg_read(UC_X86_REG_XMM0) & 0xffffffff


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 800
    cpu = emulator()
    lines, expected, cases = [], [], []
    exceptional = [0x7fc00001, 0x7f800000, 0xff800000, 0x80000000]
    for case in range(count):
        body = bytearray([0xa5] * 0x924)
        slot = case % 6
        row = 0x8b0 + slot * 0x14
        body[row] = case % 8
        body[0x308] = rng.choice([0, 1, 2])
        for index in range(4):
            body[0x309 + index] = rng.choice([0, 0, 1])
        for offset in [0x2fc, 0x300, 0x304, 0x8a8, row + 4, row + 8, row + 12]:
            value = bits(rng.uniform(-3, 3))
            if case % 13 == 0:
                value = rng.choice(exceptional)
            write(body, offset, value)
        words = [bits(rng.uniform(-3, 3)) for _ in range(3)]
        words += [bits(rng.choice([0, 1, -1, 0.5])), bits(rng.choice([0, 0, 1, -1])),
                  bits(rng.choice([0, 0, 1, -1])), bits(rng.uniform(0, 0.5)), bits(rng.choice([1, -1, 2]))]
        words += [bits(rng.uniform(-2, 2)) for _ in range(3)]
        cpu.mem_write(BODY, bytes(body))
        cpu.mem_write(ACTOR, bytes(0xd00))
        for index, word in enumerate(words[:3]):
            w32(cpu, ACTOR + 0x14 + index * 4, word)
        w32(cpu, ACTOR + 0x92c, words[3])
        w32(cpu, ACTOR + 0x930, words[4])
        for address, word in zip([0x4312dc, 0x5096c0, 0x4312c4], words[5:8]):
            w32(cpu, address, word)
        for index, word in enumerate(words[8:]):
            w32(cpu, JOINT + index * 4, word)
        result = call(cpu, 0x73c30, [BODY, JOINT, RESULT], eax=ACTOR, ebx=slot)
        expected.append((result, r32(cpu, RESULT), bytes(cpu.mem_read(BODY, len(body)))))
        lines.append('joint ' + ' '.join(map(str, list(body) + words + [slot])))
        cases.append(('joint', slot))
    for case in range(count):
        words = [bits(rng.uniform(-10, 10)), bits(rng.uniform(-10, 10)),
                 bits(rng.uniform(-1, 1)), bits(rng.uniform(-3, 3)),
                 bits(rng.uniform(-3, 3)), bits(rng.uniform(-1, 1))]
        if case % 17 == 0:
            words[case % 6] = rng.choice(exceptional)
        expected.append((call(cpu, 0x101280, words[1:], xmm=words[0]),))
        lines.append('angle ' + ' '.join(map(str, words)))
        cases.append(('angle',))
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    process = subprocess.run([harness('body_joint_harness')], input='\n'.join(lines) + '\n',
                             text=True, capture_output=True, check=True, env=environment)
    actual = process.stdout.splitlines()
    failures = 0
    if len(actual) != len(expected):
        raise RuntimeError('host result count mismatch')
    for index, (wanted, received) in enumerate(zip(expected, actual)):
        values = list(map(int, received.split()))
        okay = equivalent(wanted[0], values[0])
        if cases[index][0] == 'joint':
            okay = okay and wanted[1] == values[1]
            native = bytes(values[2:])
            slot = cases[index][1]
            float_offsets = {0x8b4 + slot * 0x14, 0x8b8 + slot * 0x14, 0x8bc + slot * 0x14}
            for offset in range(len(native)):
                if any(start <= offset < start + 4 for start in float_offsets):
                    continue
                okay = okay and wanted[2][offset] == native[offset]
            for offset in float_offsets:
                okay = okay and equivalent(struct.unpack_from('<I', wanted[2], offset)[0],
                                          struct.unpack_from('<I', native, offset)[0])
        if not okay:
            failures += 1
            if failures <= 5:
                print('MISMATCH', index, cases[index], 'weight', hex(wanted[0]), hex(values[0]))
                if cases[index][0] == 'joint':
                    row = 0x8b0 + cases[index][1] * 0x14
                    print('tracking', wanted[1], values[1], 'retail/host row',
                          wanted[2][row:row + 16].hex(), bytes(values[2:])[row:row + 16].hex())
                    original = list(map(int, lines[index].split()[1:]))
                    raw = bytes(original[:0x924])
                    print('target', [hex(struct.unpack_from('<I', raw, offset)[0]) for offset in [0x2fc, 0x300, 0x304]],
                          'actor/joint', [hex(word) for word in original[0x924:]])
    print('cases', len(expected), 'mismatches', failures)
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())

