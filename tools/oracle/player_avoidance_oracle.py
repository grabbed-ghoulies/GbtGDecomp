"""Check heading wrapping and empty-grid heading search with real retail callees.

Empty-grid cases do not establish populated navigation or live avoidance.
"""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator, bits, equivalent
from retail_cpu import ROOT, STOP, STACK, w32, r32, harness
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_XMM0, UC_X86_REG_XMM1


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 3000
    cpu = emulator()
    origin, output = 0x10001000, 0x10002000
    lines, expected = [], []
    for case in range(count):
        words = [case % 3, bits(rng.uniform(-100, 100)), bits(rng.uniform(-6, 6)),
                 rng.choice([0, 1, 10, 0xFFFFFFFF, 0x7FFFFFFF]), rng.getrandbits(32)]
        words += [bits(rng.uniform(-20, 20)) for _ in range(3)]
        words += [rng.randrange(2), rng.randrange(2), bits(rng.uniform(-10, 10)), bits(rng.uniform(-10, 10))]
        cpu.mem_write(origin, struct.pack('<3I', *words[5:8]))
        cpu.mem_write(output, struct.pack('<3I', *[0xA5A5A5A5] * 3))
        w32(cpu, 0x50E240, 0)
        w32(cpu, 0x54DD7C, words[4])
        cpu.reg_write(UC_X86_REG_EDI, origin)
        cpu.reg_write(UC_X86_REG_EAX, output if words[8] else 0)
        cpu.reg_write(UC_X86_REG_XMM0, words[11])
        cpu.reg_write(UC_X86_REG_XMM1, words[10])
        if words[0] == 0:
            cpu.reg_write(UC_X86_REG_XMM0, words[2])
            args, address = [words[1]], 0x100C70
        elif words[0] == 1:
            args, address = [words[1], 0, 0, 0, words[9], 9, 0], 0x13AC80
        else:
            cpu.reg_write(UC_X86_REG_EAX, origin)
            args, address = [words[1], 0, words[2], words[3], words[10], words[11], 0, 0,
                             output if words[8] else 0, words[9], 9, 0], 0x13AA50
        w32(cpu, STACK, STOP)
        for index, word in enumerate(args):
            w32(cpu, STACK + 4 + index * 4, word)
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.emu_start(address, STOP, count=100000)
        result = cpu.reg_read(UC_X86_REG_XMM0) & 0xFFFFFFFF if words[0] == 0 else cpu.reg_read(UC_X86_REG_EAX)
        expected.append([result, r32(cpu, 0x54DD7C), *struct.unpack('<3I', cpu.mem_read(output, 12))])
        lines.append('avoidanceheading ' + ' '.join(map(str, words)))
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
        first = equivalent(got[0], wanted[0]) if index % 3 == 0 else got[0] == wanted[0]
        if not first or got[1:] != wanted[1:]:
            if failures < 8:
                print(index, lines[index], 'expected', wanted, 'actual', got)
            failures += 1
    print(f'player avoidance headings (empty grids): {count} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    raise SystemExit(main())
