"""Compare navigation spawn candidates with retail code; only heap calls are bridged."""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator, bits
from retail_cpu import ROOT, STOP, STACK, w32, r32, harness
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 400
    cpu = emulator()
    cpu.mem_map(0x30000000, 0x100000)
    state = {'next': 0x30000000, 'sizes': {}}

    def heap(cpu, address, size, user):
        if address not in (0xf72d0, 0xf73a0):
            return
        sp = cpu.reg_read(UC_X86_REG_ESP)
        requested = r32(cpu, sp + 4)
        result = state['next']
        state['next'] += (requested + 31) & ~15
        if address == 0xf73a0:
            previous = cpu.reg_read(UC_X86_REG_EAX)
            cpu.mem_write(result, bytes(cpu.mem_read(previous, min(requested, state['sizes'][previous]))))
        state['sizes'][result] = requested
        cpu.reg_write(UC_X86_REG_EAX, result)
        cpu.reg_write(UC_X86_REG_ESP, sp + 4)
        cpu.reg_write(UC_X86_REG_EIP, r32(cpu, sp))

    cpu.hook_add(UC_HOOK_CODE, heap, begin=0xf72d0, end=0xf73a0)
    lines, expected = [], []
    for case in range(count):
        width, depth = rng.randrange(2, 7), rng.randrange(2, 7)
        cell = rng.choice([16, 32, 64])
        ox, oz = rng.randrange(-40, 40), rng.randrange(-40, 40)
        cx, cz = rng.uniform(ox - 20, ox + width * cell), rng.uniform(oz - 20, oz + depth * cell)
        minimum, maximum = rng.choice([-1., 0., 10., 30.]), rng.choice([-1., 0., 20., 60., 200.])
        flags = rng.randrange(16) & ~4
        if case % 17 == 0:
            maximum = float('nan')
        cells = bytearray()
        for z in range(depth):
            for x in range(width):
                cell_flags = rng.choice([0, 0, 0, 1, 0x200, 0x800])
                region = rng.choice([0, 0, 1, 0x1001])
                cells += struct.pack('<hhhHIHHBB', ox + x * cell + cell // 2, 0,
                    oz + z * cell + cell // 2, cell_flags, rng.randrange(32), region, 10, 0, 0)
        words = [width, depth, cell, ox & 0xffffffff, oz & 0xffffffff, bits(cx), bits(cz),
                 bits(minimum), bits(maximum), flags]
        lines.append('spawncandidates ' + ' '.join(map(str, words + list(cells))))
        grid, data, center = 0x10002000, 0x10007000, 0x10003000
        w32(cpu, grid, width)
        w32(cpu, grid + 4, depth)
        w32(cpu, grid + 8, cell)
        w32(cpu, grid + 0x10, data)
        cpu.mem_write(data, bytes(cells))
        cpu.mem_write(center, struct.pack('<fff', cx, 0., cz))
        w32(cpu, 0x50e240, grid)
        w32(cpu, 0x50e264, ox)
        w32(cpu, 0x50e26c, oz)
        args = [0, center, words[7], words[8], 0, 0, flags, 0]
        w32(cpu, STACK, STOP)
        for i, arg in enumerate(args):
            w32(cpu, STACK + 4 + i * 4, arg)
        state['next'] = 0x30000000
        state['sizes'].clear()
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.reg_write(UC_X86_REG_EAX, 0)
        cpu.emu_start(0x10f7f0, STOP, count=1000000)
        if cpu.reg_read(UC_X86_REG_ESP) != STACK + 4:
            raise RuntimeError('candidate collector did not return')
        pool = cpu.reg_read(UC_X86_REG_EAX)
        output = [0]
        if pool:
            begin, end = r32(cpu, pool + 4), r32(cpu, pool + 8)
            output = [(end - begin) // 12]
            for row in range(begin, end, 12):
                output += [r32(cpu, row), r32(cpu, row + 4)]
        expected.append(output)
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    host = subprocess.run([harness('body_joint_harness')], input='\n'.join(lines) + '\n',
                          text=True, capture_output=True, check=True, env=environment)
    outputs = host.stdout.splitlines()
    if len(outputs) != count:
        raise RuntimeError(f'host returned {len(outputs)} rows for {count}')
    mismatches = 0
    for case, (line, retail) in enumerate(zip(outputs, expected)):
        actual = list(map(int, line.split()))
        if actual != retail:
            mismatches += 1
            if mismatches <= 8:
                print(case, retail, actual)
    print(f'{count} spawn candidate cases, {mismatches} mismatches')
    return bool(mismatches)


if __name__ == '__main__':
    sys.exit(main())

