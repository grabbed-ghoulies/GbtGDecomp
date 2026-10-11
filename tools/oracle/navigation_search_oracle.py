"""Compare the complete retail A* loop and reset, with no callee bridges."""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator, bits
from retail_cpu import ROOT, STOP, STACK, w32, r32, harness
from unicorn.x86_const import UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_EAX, UC_X86_REG_ESP


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 1000
    cpu = emulator()
    search, grid, cells, nodes = 0x10001000, 0x10002000, 0x10003000, 0x10005000
    lines, expected = [], []

    def call(address, args):
        w32(cpu, STACK, STOP)
        for index, value in enumerate(args):
            w32(cpu, STACK + 4 + index * 4, value)
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.emu_start(address, STOP, count=1000000)
        if cpu.reg_read(UC_X86_REG_ESP) != STACK + 4:
            raise RuntimeError('retail search did not return')

    def reference(value):
        if value == 0:
            return 0
        if value == search:
            return 1
        if nodes <= value < nodes + 16 * 0x14 and (value - nodes) % 0x14 == 0:
            return (value - nodes) // 0x14 + 2
        raise RuntimeError(f'unknown node reference {value:x}')

    for case in range(count):
        start, finish = rng.randrange(16), rng.randrange(16)
        mask = rng.getrandbits(18)
        rate = bits(rng.choice([0.0, 1.0, 4.0, 10.0, 100.0]))
        elapsed = bits(rng.choice([0.0, 0.016, 0.1, 0.3, 1.0]))
        steps = rng.randrange(1, 9)
        data = bytearray()
        for index in range(16):
            packed = rng.choice([0, 0, 1, 0x200]) | (rng.randrange(16) << 5)
            region = rng.choice([0, 0, 1, 2, 0x1001, 0x8000]) if case % 3 else 0
            connections = rng.getrandbits(8) if case % 3 else 255
            value = rng.randrange(1, 65) | (rng.randrange(18) << 16)
            if case % 4 == 0:
                value |= rng.randrange(18) << 24
            if case % 3 == 0:
                packed &= ~0x21f
            data += struct.pack('<hhhHIHHBB', (index % 4) * 16, rng.randrange(-5, 6),
                                (index // 4) * 16, packed, value, region, 50, connections, 0)
        lines.append('navigationsearch ' + ' '.join(map(str, [start, finish, mask, rate, elapsed, steps, *data])))
        cpu.mem_write(search, bytes(0x34))
        cpu.mem_write(nodes, bytes(16 * 0x14))
        for index in range(16):
            cpu.mem_write(nodes + index * 0x14 + 0xa, struct.pack('<H', index))
        cpu.mem_write(cells, bytes(data))
        w32(cpu, grid, 4)
        w32(cpu, grid + 4, 4)
        w32(cpu, grid + 8, 16)
        w32(cpu, grid + 0xc, bits(1.0))
        w32(cpu, grid + 0x10, cells)
        w32(cpu, search + 8, 16)
        w32(cpu, search + 0xc, nodes)
        w32(cpu, search + 0x10, grid)
        cpu.reg_write(UC_X86_REG_EAX, search)
        # 0x36C80: EAX search, two stack cell indexes.
        call(0x36c80, [finish, start])
        w32(cpu, search + 0x20, rate)
        output = []
        for step in range(steps):
            cpu.reg_write(UC_X86_REG_EDI, search)
            call(0x36ca0, [mask, elapsed])
            output.append(cpu.reg_read(UC_X86_REG_EAX))
        output += [r32(cpu, search + 0x2c), r32(cpu, search + 0x24),
                   reference(r32(cpu, search)), reference(r32(cpu, search + 4))]
        for index in range(16):
            row = nodes + index * 0x14
            output += [reference(r32(cpu, row)), reference(r32(cpu, row + 4)),
                       r32(cpu, row + 8), r32(cpu, row + 12), r32(cpu, row + 16)]
        output += list(cpu.mem_read(cells, len(data)))
        expected.append(output)
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    result = subprocess.run([harness('body_joint_harness')], input='\n'.join(lines) + '\n',
                            text=True, capture_output=True, check=True, env=environment)
    actual = result.stdout.splitlines()
    if len(actual) != count:
        raise RuntimeError('wrong result count')
    failures = 0
    for case, (line, retail) in enumerate(zip(actual, expected)):
        native = list(map(int, line.split()))
        if native != retail:
            failures += 1
            if failures <= 5:
                differences = [(i, a, b) for i, (a, b) in enumerate(zip(native, retail)) if a != b]
                print(f'case={case} differences={differences[:12]}')
    print(f'navigation search: {count} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    sys.exit(main())
