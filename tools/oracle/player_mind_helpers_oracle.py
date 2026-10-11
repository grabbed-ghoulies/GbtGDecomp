"""Compare alert, distance and position helpers with real retail code, no callee bridges."""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator, bits
from retail_cpu import ROOT, STOP, STACK, w32, r32, harness
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_XMM0


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 400
    cpu = emulator()
    mind, actor, position = 0x10001000, 0x10003000, 0x10005000
    lines, expected = [], []
    specials = [0, 0x80000000, 0x7fc00001, 0x7f800000, 0xff800000,
                bits(5), bits(10), bits(50)]
    for operation in range(3):
        for case in range(count):
            words = [rng.randrange(2)] + [bits(rng.uniform(-100, 100)) for _ in range(11)]
            if case % 5 == 0:
                words[rng.randrange(1, 12)] = rng.choice(specials)
            if operation == 2:
                words[11] = rng.randrange(1 << 32)
            lines.append('mindhelper ' + ' '.join(map(str, [operation, *words])))
            cpu.mem_write(mind, bytes(0x1a4))
            cpu.mem_write(actor, bytes(0xDB4))
            cpu.mem_write(actor + 8, struct.pack('<III', *words[5:8]))
            cpu.mem_write(position, struct.pack('<III', *words[8:11]))
            w32(cpu, mind + 8, actor)
            w32(cpu, mind + 0x124, words[0])
            w32(cpu, mind + 0x120, words[1])
            w32(cpu, mind + 0x134, words[0])
            cpu.mem_write(mind + 0x138, struct.pack('<III', *words[2:5]))
            w32(cpu, STACK, STOP)
            cpu.reg_write(UC_X86_REG_ESP, STACK)
            cpu.reg_write(UC_X86_REG_EAX, mind if operation == 1 else mind + 0x4c)
            cpu.reg_write(UC_X86_REG_EBX, mind)
            cpu.reg_write(UC_X86_REG_XMM0, words[11])
            w32(cpu, mind + 0x50, words[11])
            w32(cpu, STACK + 4, words[11])
            w32(cpu, STACK + 8, position)
            cpu.emu_start([0x2a740, 0x7e5c0, 0x7f100][operation], STOP, count=100000)
            if cpu.reg_read(UC_X86_REG_ESP) != STACK + 4:
                raise RuntimeError('retail helper did not return')
            result = words.copy()
            if operation == 0:
                result[0] = cpu.reg_read(UC_X86_REG_EAX)
            elif operation == 1:
                result[0] = r32(cpu, mind + 0x124)
                result[1] = r32(cpu, mind + 0x120)
            else:
                result[0] = r32(cpu, mind + 0x134)
                result[2:5] = struct.unpack('<III', cpu.mem_read(mind + 0x138, 12))
            expected.append(result)
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
    print(f'player mind helpers: {len(expected)} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    sys.exit(main())
