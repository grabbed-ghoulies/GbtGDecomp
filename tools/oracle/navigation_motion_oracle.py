"""Compare navigation motion/input/proximity leaves using real retail callees."""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator, bits
from retail_cpu import ROOT, STOP, STACK, w32, r32, harness
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 2000
    cpu = emulator()
    body, actor, flags, flag_data, device, grid, output, target = [0x10001000, 0x10003000,
        0x10005000, 0x10006000, 0x10007000, 0x10008000, 0x10009000, 0x1000a000]
    lines, expected = [], []
    for case in range(count):
        words = [case % 4, rng.getrandbits(24), rng.getrandbits(32), bits(99), bits(88)]
        words += [bits(rng.choice([0.0, rng.uniform(-2, 2)])) for _ in range(9)]
        words += [bits(rng.uniform(-20, 20)) for _ in range(6)]
        words += [bits(rng.uniform(0, 5)), bits(rng.uniform(0, 3)), rng.randrange(2), rng.randrange(1, 17), 0]
        words += [bits(rng.choice([0.0, rng.uniform(-1, 1)])) for _ in range(2)]
        if case % 20 == 0:
            words[rng.choice([5, 6, 7, 8, 9, 10, 11, 12, 13, 25, 26])] = rng.choice(
                [0x7fc00000, 0xffc00000, 0x7f800000, 0xff800000, 0x80000000, 1])
        lines.append('navigationmotion ' + ' '.join(map(str, words)))
        cpu.mem_write(body, bytes(0x900))
        cpu.mem_write(actor, bytes(0xd00))
        cpu.mem_write(body + 0x1d8, struct.pack('<3I', *words[8:11]))
        cpu.mem_write(body + 0x248, struct.pack('<3I', *words[11:14]))
        w32(cpu, body + 0x888, words[21])
        w32(cpu, body + 8, actor)
        w32(cpu, body + 0x15c, flags)
        w32(cpu, flags + 8, flag_data)
        w32(cpu, flag_data, words[1])
        cpu.mem_write(actor + 0x3c, struct.pack('<3I', *words[5:8]))
        cpu.mem_write(actor + 8, struct.pack('<3I', *words[14:17]))
        w32(cpu, actor + 0x38, words[20])
        w32(cpu, actor + 0x960, device)
        w32(cpu, device, 5)
        cpu.mem_write(device + 0x10, struct.pack('<2I', *words[25:27]))
        w32(cpu, grid + 8, words[23])
        w32(cpu, 0x50e240, grid if words[22] else 0)
        cpu.mem_write(target, struct.pack('<3I', *words[17:20]))
        cpu.mem_write(output, struct.pack('<3I', *words[2:5]))
        cpu.reg_write(UC_X86_REG_EAX, body)
        cpu.reg_write(UC_X86_REG_ESI, body)
        cpu.reg_write(UC_X86_REG_EDI, output)
        cpu.reg_write(UC_X86_REG_EBX, output + 4)
        address, args = [(0x137500, [output + 4]), (0x137610, [output + 8]),
                         (0x137460, [output + 8]), (0x137790, [body])][words[0]]
        if words[0] == 0:
            cpu.reg_write(UC_X86_REG_EAX, output)
        if words[0] == 3:
            cpu.reg_write(UC_X86_REG_ESI, actor)
            cpu.reg_write(UC_X86_REG_EBX, target)
        w32(cpu, STACK, STOP)
        for index, arg in enumerate(args):
            w32(cpu, STACK + 4 + index * 4, arg)
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.emu_start(address, STOP, count=1000000)
        if cpu.reg_read(UC_X86_REG_ESP) != STACK + 4:
            raise RuntimeError('retail motion leaf did not return')
        expected.append([cpu.reg_read(UC_X86_REG_EAX), *struct.unpack('<3I', cpu.mem_read(output, 12))])
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    process = subprocess.run([harness('body_joint_harness')], input='\n'.join(lines) + '\n',
                             text=True, capture_output=True, check=True, env=environment)
    actual = process.stdout.splitlines()
    if len(actual) != count:
        raise RuntimeError('wrong result count')
    failures = 0
    for case, (line, retail) in enumerate(zip(actual, expected)):
        native = list(map(int, line.split()))
        if native != retail:
            failures += 1
            if failures <= 5:
                print(f'case={case} native={native} retail={retail}')
    print(f'navigation motion: {count} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    sys.exit(main())
