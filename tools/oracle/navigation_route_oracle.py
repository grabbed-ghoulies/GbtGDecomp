"""Compare route construction, append, endpoints and validation; bridge only heap."""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator, bits
from retail_cpu import ROOT, STOP, STACK, w32, r32, harness
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EDX, UC_X86_REG_XMM0
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP


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
            old = cpu.reg_read(UC_X86_REG_EAX)
            cpu.mem_write(result, bytes(cpu.mem_read(old, min(requested, state['sizes'][old]))))
        state['sizes'][result] = requested
        cpu.reg_write(UC_X86_REG_EAX, result)
        cpu.reg_write(UC_X86_REG_ESP, sp + 4)
        cpu.reg_write(UC_X86_REG_EIP, r32(cpu, sp))

    cpu.hook_add(UC_HOOK_CODE, heap, begin=0xf72d0, end=0xf73a0)
    search, grid, data, nodes, slot, points = [0x10001000, 0x10002000, 0x10003000, 0x10005000, 0x10007000, 0x10008000]
    lines, expected = [], []

    def call(address, args):
        w32(cpu, STACK, STOP)
        for index, arg in enumerate(args):
            w32(cpu, STACK + 4 + index * 4, arg)
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.emu_start(address, STOP, count=1000000)
        if cpu.reg_read(UC_X86_REG_ESP) != STACK + 4:
            raise RuntimeError('retail route routine did not return')

    for case in range(count):
        operation = case % 4
        cells = bytearray()
        for index in range(16):
            occupied = rng.choice([0, 0, 0, 1, 0x200])
            region = rng.choice([0, 0, 1, 0x1001, 0x8000])
            cells += struct.pack('<hhhHIHHBB', rng.randrange(-200, 200), rng.randrange(-50, 50),
                                 rng.randrange(-200, 200), occupied, 0, region, 50, 255, 0)
        route = rng.sample(range(16), rng.randrange(2, 17))
        point_words = [bits(rng.uniform(-100, 100)) for _ in range(9)]
        lines.append('navigationroute ' + ' '.join(map(str, [operation, len(route), *cells, *route, *point_words])))
        cpu.mem_write(search, bytes(0x34))
        cpu.mem_write(nodes, bytes(16 * 0x14))
        cpu.mem_write(data, bytes(cells))
        cpu.mem_write(points, struct.pack('<9I', *point_words))
        w32(cpu, grid, 4)
        w32(cpu, grid + 4, 4)
        w32(cpu, grid + 8, 16)
        w32(cpu, grid + 0x10, data)
        w32(cpu, search + 0xc, nodes)
        w32(cpu, search + 0x10, grid)
        w32(cpu, search + 0x18, route[-1])
        w32(cpu, search + 0x1c, route[0])
        w32(cpu, slot, 0)
        for index, cell in enumerate(route):
            previous = route[index + 1] if index + 1 < len(route) else 0xffff
            cpu.mem_write(nodes + cell * 0x14 + 8, struct.pack('<HH', previous, cell))
        state['next'] = 0x30000000
        state['sizes'].clear()
        call(0x37470, [search, slot, points, points + 12])
        pool = r32(cpu, slot)
        if operation == 1:
            cpu.reg_write(UC_X86_REG_EDI, slot)
            cpu.reg_write(UC_X86_REG_ESI, points + 24)
            call(0x376b0, [route[0]])
            pool = r32(cpu, slot)
        if operation == 2:
            cpu.reg_write(UC_X86_REG_EAX, pool)
            cpu.reg_write(UC_X86_REG_EBX, points + 12)
            cpu.reg_write(UC_X86_REG_EDI, points)
            call(0x376f0, [])
        w32(cpu, search + 0x14, pool)
        retargeted = 0
        if operation == 3:
            call(0x37980, [search, point_words[6] % 16, point_words[7] % 16])
            retargeted = cpu.reg_read(UC_X86_REG_EAX)
            pool = r32(cpu, search + 0x14)
        cpu.reg_write(UC_X86_REG_ECX, search)
        call(0x36e90, [])
        output = [cpu.reg_read(UC_X86_REG_EAX)]
        call(0x37340, [search, route[0], route[-1]])
        connected = cpu.reg_read(UC_X86_REG_EAX)
        w32(cpu, grid + 0xc, point_words[0])
        cpu.reg_write(UC_X86_REG_ESI, grid)
        cpu.reg_write(UC_X86_REG_EAX, route[0])
        cpu.reg_write(UC_X86_REG_EDX, route[-1])
        call(0x36a60, [])
        output = [connected, cpu.reg_read(UC_X86_REG_XMM0) & 0xffffffff] + output
        output = [retargeted, r32(cpu, search + 0x18), r32(cpu, search + 0x1c),
                  r32(cpu, search + 0x30)] + output
        begin, end = r32(cpu, pool + 4), r32(cpu, pool + 8)
        output.append((end - begin) // 16)
        output += list(struct.unpack('<' + 'I' * ((end - begin) // 4), cpu.mem_read(begin, end - begin)))
        call(0x37d50, [pool, points + 0x40, points + 0x50, point_words[8]])
        output += list(struct.unpack('<3I', cpu.mem_read(points + 0x40, 12)))
        output += list(struct.unpack('<3I', cpu.mem_read(points + 0x50, 12)))
        cpu.reg_write(UC_X86_REG_EAX, pool)
        call(0x377b0, [])
        # Store the actual x87 result to float, as the player caller does.
        cpu.mem_write(points + 0x100, b'\xd9\x1d' + struct.pack('<I', points + 0x80) + b'\xc3')
        call(points + 0x100, [])
        output.append(r32(cpu, points + 0x80))
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
                print(f'case={case} native={native} retail={retail}')
    print(f'navigation routes: {count} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    sys.exit(main())
