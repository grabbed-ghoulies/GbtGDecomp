"""Compare the complete retail input tracking routine, including its real sqrt."""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator, bits
from retail_cpu import ROOT, STOP, STACK, w32, harness
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_ESI, UC_X86_REG_EDI


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 2000
    cpu = emulator()
    tracking, axes, preset = 0x10001000, 0x10003000, 0x10005000
    lines, expected = [], []
    specials = [0, 0x80000000, 0x7fc00001, 0x7f800000, 0xff800000, bits(1)]
    for case in range(count):
        words = [bits(rng.uniform(-2, 2)) for _ in range(16)]
        words[5] = rng.randrange(4)
        words[12] = bits(rng.choice([0, -1, 1]))
        words[13] = bits(rng.uniform(0, .1))
        words[14] = bits(rng.uniform(0, 2))
        words[15] = rng.randrange(3)
        if case % 3 == 0:
            words[rng.choice([0, 1, 2, 3, 4, 6, 7, 8, 9, 10, 11, 12, 13, 14])] = rng.choice(specials)
        if case % 4 == 0:
            words[2] = bits(1 if words[5] == 1 else 0)
        lines.append('inputtracking ' + ' '.join(map(str, words)))
        cpu.mem_write(tracking, struct.pack('<6I', *words[:6]))
        cpu.mem_write(axes, struct.pack('<2I', *words[6:8]))
        cpu.mem_write(preset, struct.pack('<4I', *words[8:12]))
        w32(cpu, 0x4312dc, words[12])
        w32(cpu, 0x5096c0, words[13])
        w32(cpu, 0x4312c4, words[14])
        w32(cpu, STACK, STOP)
        w32(cpu, STACK + 4, words[15])
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.reg_write(UC_X86_REG_EAX, axes)
        cpu.reg_write(UC_X86_REG_ESI, tracking)
        cpu.reg_write(UC_X86_REG_EDI, preset)
        cpu.emu_start(0x109810, STOP, count=100000)
        if cpu.reg_read(UC_X86_REG_ESP) != STACK + 4:
            raise RuntimeError('retail tracking did not return')
        expected.append(list(struct.unpack('<6I', cpu.mem_read(tracking, 24))))
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    result = subprocess.run([harness('body_joint_harness')], input='\n'.join(lines) + '\n',
                            text=True, capture_output=True, check=True, env=environment)
    actual = result.stdout.splitlines()
    if len(actual) != len(expected):
        raise RuntimeError('wrong result count')
    failures = 0
    for case, (line, retail) in enumerate(zip(actual, expected)):
        native = list(map(int, line.split()))
        if native != retail:
            failures += 1
            if failures <= 8:
                print(f'case={case} native={native} retail={retail}')
                print(lines[case])
    print(f'player input tracking: {count} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    sys.exit(main())
