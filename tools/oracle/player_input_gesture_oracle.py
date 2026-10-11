"""Execute both retail gesture state methods with their real math callees."""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator, bits
from retail_cpu import ROOT, STOP, STACK, w32, harness
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_XMM1


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 1000
    cpu = emulator()
    tracking, axes = 0x10001000, 0x10003000
    lines, expected = [], []
    specials = [0, 0x80000000, 0x7fc00001, 0x7f800000, 0xff800000,
                bits(.1), bits(.11), bits(.2)]
    for operation in range(2):
        for case in range(count):
            words = [operation] + [bits(rng.uniform(-1, 1)) for _ in range(12)]
            words[1] = rng.randrange(2)
            words[2] = rng.randrange(4)
            words[8] = rng.randrange(5)
            words[11] = bits(rng.uniform(0, .1))
            words[12] = rng.randrange(3)
            if case % 3 == 0:
                words[rng.choice([3, 4, 5, 6, 7, 9, 10, 11])] = rng.choice(specials)
            lines.append('inputgesture ' + ' '.join(map(str, words)))
            cpu.mem_write(tracking, struct.pack('<8I', *words[1:9]))
            cpu.mem_write(axes, struct.pack('<2I', *words[9:11]))
            w32(cpu, STACK, STOP)
            w32(cpu, STACK + 4, words[11])
            w32(cpu, STACK + 8, words[12])
            cpu.reg_write(UC_X86_REG_ESP, STACK)
            cpu.reg_write(UC_X86_REG_EAX, tracking)
            cpu.reg_write(UC_X86_REG_ESI, tracking)
            cpu.reg_write(UC_X86_REG_EDI, axes)
            cpu.reg_write(UC_X86_REG_XMM1, words[9])
            cpu.emu_start([0x7d5a0, 0x7d460][operation], STOP, count=100000)
            if cpu.reg_read(UC_X86_REG_ESP) != STACK + 4:
                raise RuntimeError('retail gesture did not return')
            expected.append(list(struct.unpack('<8I', cpu.mem_read(tracking, 32))))
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
    print(f'player input gesture: {len(expected)} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    sys.exit(main())
