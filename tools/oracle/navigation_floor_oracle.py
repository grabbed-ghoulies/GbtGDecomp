"""Compare nearest-cell and floor-connection queries with no callee bridges."""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator, bits
from retail_cpu import ROOT, STOP, STACK, w32, harness
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ESP


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 1000
    cpu = emulator()
    grid, cells, header, rows, point, output = [0x10001000, 0x10002000, 0x10003000,
                                              0x10004000, 0x10005000, 0x10006000]
    lines, expected = [], []

    def call(address, args):
        w32(cpu, STACK, STOP)
        for index, value in enumerate(args):
            w32(cpu, STACK + 4 + index * 4, value)
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.emu_start(address, STOP, count=1000000)
        if cpu.reg_read(UC_X86_REG_ESP) != STACK + 4:
            raise RuntimeError('retail floor query did not return')
        return cpu.reg_read(UC_X86_REG_EAX)

    for case in range(count):
        source, target = rng.randrange(2), rng.randrange(2)
        index = rng.randrange(16)
        connection_indexes = rng.sample(range(16), rng.randrange(17))
        point_words = [bits(rng.uniform(-100, 100)) for _ in range(3)]
        data = bytearray()
        for cell in range(16):
            packed = rng.choice([0, 0, 0, 1, 0x200])
            region = rng.choice([0, 0, 1, 0x1001, 0x8000])
            data += struct.pack('<hhhHIHHBB', rng.randrange(-100, 100), rng.randrange(-10, 11),
                                rng.randrange(-100, 100), packed, 0, region, 50, rng.getrandbits(8), 0)
        lines.append('navigationfloor ' + ' '.join(map(str,
            [source, target, index, len(connection_indexes), *point_words, *data, *connection_indexes])))
        cpu.mem_write(cells, bytes(data))
        cpu.mem_write(point, struct.pack('<3I', *point_words))
        for offset, value in [(0, 4), (4, 4), (0x10, cells)]:
            w32(cpu, grid + offset, value)
        for address in (0x50e240, 0x50e244):
            w32(cpu, address, grid)
        for address in (0x50e248, 0x50e24c):
            w32(cpu, address, header)
        w32(cpu, header + 4, rows)
        w32(cpu, header + 8, rows + len(connection_indexes) * 12)
        for row, cell in enumerate(connection_indexes):
            w32(cpu, rows + row * 12, cell)
        cpu.mem_write(output, bytes(12))
        found = call(0x111660, [index, source, output])
        result = [found, *struct.unpack('<3I', cpu.mem_read(output, 12))]
        cpu.mem_write(output, bytes(12))
        cpu.reg_write(UC_X86_REG_EBX, source)
        selected = call(0x110d00, [target, point, output])
        result += [selected, *struct.unpack('<3I', cpu.mem_read(output, 12))]
        expected.append(result)
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
    print(f'navigation floors: {count} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    sys.exit(main())
