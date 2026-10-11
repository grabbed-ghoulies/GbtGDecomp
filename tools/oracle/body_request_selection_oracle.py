"""Check request bit selection and target chain length against retail instructions.

Includes padding bits, retained output words, NaNs, infinities and signed zero.
No retail callees are substituted.
"""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator, bits
from retail_cpu import ROOT, STOP, STACK, w32, r32, harness
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EBX, UC_X86_REG_ECX


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 3000
    cpu = emulator()
    body, owner, model, storage, flags = [0x10001000 + i * 0x2000 for i in range(5)]
    table, result = 0x1000B000, 0x1000F000
    lines, expected = [], []
    for case in range(count):
        def call(address, eax=0, edx=0, args=()):
            w32(cpu, STACK, STOP)
            for index, value in enumerate(args):
                w32(cpu, STACK + 4 + index * 4, value)
            cpu.reg_write(UC_X86_REG_ESP, STACK)
            cpu.reg_write(UC_X86_REG_EAX, eax)
            cpu.reg_write(UC_X86_REG_EDX, edx)
            cpu.emu_start(address, STOP, count=100000)
            return cpu.reg_read(UC_X86_REG_EAX)
        if case % 3 == 0:
            scalar = lambda: bits(rng.uniform(-10, 10))
            words = [scalar() for _ in range(4)]
            words += [rng.randrange(4), rng.randrange(8), rng.randrange(3),
                      rng.choice([0, scalar()]), rng.randrange(65536), rng.randrange(65536), rng.randrange(32)]
            words += [scalar() for _ in range(4)]
            words += [rng.randrange(4), rng.randrange(3), rng.randrange(2), rng.randrange(2), scalar(), scalar(),
                      rng.choice([0, 1, 2, 3, 0xFFFFFFFF]), rng.randrange(8)]
            if case % 9 == 0:
                words[rng.choice([0, 1, 2, 3, 7, 11, 12, 13, 14, 19, 20])] = rng.choice([0x7FC00001, 0x7F800000, 0xFF800000, 0x80000000])
            cpu.mem_write(owner + 8, struct.pack('<3I', *words[:3]))
            w32(cpu, owner + 0x38, words[3])
            w32(cpu, body + 8, owner)
            cpu.mem_write(body + 0x2D0, bytes([words[16], 0, words[17]]))
            row, target, shape = storage, flags, model
            cpu.mem_write(row + 6, bytes(words[4:7]))
            w32(cpu, row + 0xC, words[7])
            cpu.mem_write(row + 0x10, struct.pack('<2H', *words[8:10]))
            w32(cpu, row + 0x20, words[10])
            cpu.mem_write(target + 0x20, struct.pack('<3I', *words[11:14]))
            w32(cpu, target + 0x38, words[14])
            cpu.mem_write(target + 0x16, bytes([words[15]]))
            w32(cpu, shape + 8, words[19])
            w32(cpu, shape + 0x14, words[20])
            cpu.reg_write(UC_X86_REG_EBX, row)
            cpu.reg_write(UC_X86_REG_ECX, body)
            expected.append([call(0x1BA20, eax=words[21], args=[target, shape if words[18] else 0, words[22]])])
            lines.append('requestmatch ' + ' '.join(map(str, words)))
        elif case % 3 == 1:
            size = rng.randrange(17)
            logical = rng.randrange(size * 8 + 1)
            words = [logical, size, rng.getrandbits(32), rng.getrandbits(32)]
            words += [rng.getrandbits(32) if case % 7 else 0 for _ in range(4)]
            cpu.mem_write(storage, struct.pack('<3I', logical, size, flags))
            cpu.mem_write(flags, struct.pack('<4I', *words[4:]))
            w32(cpu, result, words[3])
            w32(cpu, 0x54DD7C, words[2])
            population = call(0x66180, edx=storage)
            success = call(0x661C0, eax=storage, args=[result])
            expected.append([population, success, r32(cpu, result), r32(cpu, 0x54DD7C)])
            lines.append('requestbits ' + ' '.join(map(str, words)))
        else:
            words = [rng.randrange(2), rng.randrange(8), rng.randrange(2),
                     bits(rng.choice([0, -1, .5, 1, 2, rng.uniform(-4, 4)])),
                     rng.getrandbits(32), rng.randrange(256), rng.randrange(256),
                     bits(rng.uniform(-20, 20)), bits(rng.uniform(-20, 20)), rng.randrange(256)]
            if case % 9 == 0:
                words[rng.choice([3, 7, 8])] = rng.choice([0x7FC00001, 0x7F800000, 0xFF800000, 0x80000000])
            if case % 11 == 0:
                words[6] = words[5]
            w32(cpu, body + 8, owner)
            w32(cpu, body + 0x268, model if words[0] else 0)
            cpu.mem_write(owner + 0xCF4, struct.pack('<H', words[1]))
            cpu.mem_write(model + 0x1A4, bytes([words[2]]))
            w32(cpu, body + 0x16C, words[3])
            w32(cpu, body + 0x15C, storage)
            w32(cpu, storage + 8, flags)
            w32(cpu, flags, words[4])
            cpu.mem_write(0x433D5C, bytes(words[5:7]))
            w32(cpu, 0x433D60, words[7])
            w32(cpu, 0x433D64, words[8])
            w32(cpu, 0x545AC4, table)
            if words[1]:
                cpu.mem_write(table + words[1] * 0x87 - 0x38, bytes([words[9]]))
            expected.append([call(0x243C0, edx=body)])
            lines.append('requestlength ' + ' '.join(map(str, words)))
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    process = subprocess.run([harness('body_joint_harness')], input='\n'.join(lines) + '\n',
                             text=True, capture_output=True, check=True, env=environment)
    actual = process.stdout.splitlines()
    if len(actual) != count:
        raise RuntimeError('wrong result count')
    failures = 0
    for index, (line, wanted) in enumerate(zip(actual, expected)):
        got = list(map(int, line.split()))
        if got != wanted:
            if failures < 8:
                print(index, lines[index], 'expected', wanted, 'actual', got)
            failures += 1
    print(f'body request selection primitives: {count} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    raise SystemExit(main())
