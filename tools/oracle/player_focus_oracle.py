"""Compare angular actor query, near selection and focus maintenance with retail.

All callees execute; navigation grids and held models are absent in this fixture.
The empty cone-query local is supplied as retained caller-stack storage.
"""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator, bits, equivalent
from retail_cpu import ROOT, STOP, STACK, w32, r32, harness
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EDI, UC_X86_REG_EBX


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 3000
    expected, lines = [], []
    owner, actor, mind = 0x10001000, 0x10003000, 0x10005000
    body, other, storage, flags, result = 0x10006000, 0x10007000, 0x10008100, 0x10008000, 0x10009000
    for index in range(count):
        scalar = lambda low, high: bits(rng.uniform(low, high))
        words = [index % 3, rng.randrange(2)]
        words += [scalar(-10, 10) for _ in range(3)]
        words += [scalar(-25, 25) for _ in range(3)]
        words += [scalar(-7, 7), scalar(0, 4), scalar(-10, 100), rng.choice([0, 4, 0x20000, 0x4000])]
        words += [rng.randrange(2), rng.randrange(2), rng.choice([0, 8, 32, 40]), rng.choice([0, 16])]
        words += [scalar(0, 3), scalar(0, 3), scalar(0, 8), scalar(0, 8), scalar(0, 8)]
        words += [rng.getrandbits(32), scalar(0, 3), scalar(0, 3), rng.getrandbits(32)]
        if index % 10 == 0:
            words[rng.choice([*range(2, 11), *range(16, 21), 22, 23])] = rng.choice([0x7FC00000, 0x7F800000, 0xFF800000, 0x80000000])
        cpu = emulator()
        cpu.mem_map(0, 0x1000)
        cpu.mem_write(owner + 8, struct.pack('<3I', *words[2:5]))
        cpu.mem_write(actor + 8, struct.pack('<3I', *words[5:8]))
        w32(cpu, owner + 0x18, words[8])
        w32(cpu, owner + 0x38, words[16])
        w32(cpu, actor + 0x38, words[17])
        w32(cpu, owner + 0x95C, body)
        w32(cpu, actor + 0x95C, other)
        cpu.mem_write(actor + 0xCDC, b'\x01\x00')
        w32(cpu, body + 8, owner)
        w32(cpu, other + 8, actor)
        w32(cpu, body + 0x888, words[18])
        w32(cpu, other + 0x888, words[19])
        w32(cpu, other + 0x78, words[20])
        w32(cpu, other + 0x15C, storage)
        w32(cpu, storage + 8, flags)
        cpu.mem_write(flags + 3, bytes([words[14]]))
        cpu.mem_write(flags + 7, bytes([words[15]]))
        w32(cpu, mind + 8, owner)
        w32(cpu, mind + 0xC, actor if words[12] else 0)
        cpu.mem_write(mind + 0x20, bytes([words[13]]))
        w32(cpu, mind + 0x34, words[22])
        w32(cpu, mind + 0x38, words[23])
        w32(cpu, 0x511B38, actor + 0x934 if words[1] else 0)
        w32(cpu, result, words[24])
        w32(cpu, result + 4, words[21])
        w32(cpu, STACK, STOP)
        if words[0] == 0:
            address = 0x128F70
            args = [owner, *words[8:11], result + 4, result, words[11], 0]
        elif words[0] == 1:
            address, args = 0x7E600, []
            w32(cpu, STACK - 8, words[21])
            cpu.reg_write(UC_X86_REG_EDI, owner)
            cpu.reg_write(UC_X86_REG_EBX, result)
        else:
            address, args = 0x7E6B0, [mind]
            w32(cpu, STACK - 52, words[21])
        for item, value in enumerate(args):
            w32(cpu, STACK + 4 + item * 4, value)
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.emu_start(address, STOP, count=1000000)
        from unicorn.x86_const import UC_X86_REG_EAX
        selected = cpu.reg_read(UC_X86_REG_EAX)
        expected.append([selected, r32(cpu, result + 4), r32(cpu, result), cpu.mem_read(mind + 0x20, 1)[0], r32(cpu, mind + 0x34)])
        lines.append('playerfocus ' + ' '.join(map(str, words)))
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    process = subprocess.run([harness('body_joint_harness')], input='\n'.join(lines) + '\n', text=True, capture_output=True, check=True, env=environment)
    actual = process.stdout.splitlines()
    if len(actual) != count:
        raise RuntimeError('wrong result count')
    failures = 0
    for index, (line, wanted) in enumerate(zip(actual, expected)):
        got = list(map(int, line.split()))
        if len(got) != len(wanted) or any(not equivalent(a, b) if item in (1, 2, 4) else a != b for item, (a, b) in enumerate(zip(got, wanted))):
            if failures < 8:
                print(index, lines[index], 'expected', wanted, 'actual', got)
            failures += 1
    print(f'player focus: {count} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    raise SystemExit(main())
