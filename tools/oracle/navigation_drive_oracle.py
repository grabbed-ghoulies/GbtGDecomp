"""Compare player path/drive calls through real navigation; bridge only heap."""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator, bits
from retail_cpu import ROOT, STOP, STACK, w32, r32, harness
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 1000
    cpu = emulator()
    cpu.mem_map(0x30000000, 0x100000)
    heap_state = {'next': 0x30000000, 'sizes': {}}

    def heap(cpu, address, size, user):
        if address not in (0xf72d0, 0xf73a0):
            return
        sp = cpu.reg_read(UC_X86_REG_ESP)
        requested = r32(cpu, sp + 4)
        result = heap_state['next']
        heap_state['next'] += (requested + 31) & ~15
        if address == 0xf73a0:
            old = cpu.reg_read(UC_X86_REG_EAX)
            cpu.mem_write(result, bytes(cpu.mem_read(old, min(requested, heap_state['sizes'][old]))))
        heap_state['sizes'][result] = requested
        cpu.reg_write(UC_X86_REG_EAX, result)
        cpu.reg_write(UC_X86_REG_ESP, sp + 4)
        cpu.reg_write(UC_X86_REG_EIP, r32(cpu, sp))

    cpu.hook_add(UC_HOOK_CODE, heap, begin=0xf72d0, end=0xf73a0)
    mind, actor, body, grid, cells, nodes, search, device, target, output = [
        0x10001000, 0x10003000, 0x10005000, 0x10007000, 0x10008000,
        0x10009000, 0x1000a000, 0x1000b000, 0x1000c000, 0x1000d000]
    lines, expected = [], []
    for case in range(count):
        words = [case % 2]
        words += [bits(rng.uniform(1, 62)) for _ in range(4)]
        words += [bits(rng.uniform(0, 10)), bits(rng.uniform(0, 50)),
                  bits(rng.choice([0.0, 0.1, 0.5, 1.0, 2.5, 4.0])), bits(rng.choice([0.0, 0.1, 1.0]))]
        words += [rng.getrandbits(18), rng.choice([0, 0, 0, 1]), bits(rng.choice([0.0, 0.0, 1.0])),
                  bits(rng.choice([0.0, 1.0, 10.0])), int(case % 7 != 0),
                  rng.getrandbits(16) if case % 3 else 0]
        lines.append('navigationdrive ' + ' '.join(map(str, words)))
        for address, size in [(mind, 0x200), (actor, 0xe00), (body, 0x900), (search, 0x34),
                              (nodes, 16 * 0x14), (device, 0x90)]:
            cpu.mem_write(address, bytes(size))
        w32(cpu, mind + 8, actor)
        w32(cpu, mind + 0x1c, search if words[13] else 0)
        w32(cpu, actor + 0x95c, body)
        w32(cpu, body + 8, actor)
        w32(cpu, actor + 8, words[1])
        w32(cpu, actor + 0x10, words[2])
        w32(cpu, actor + 0x92c, bits(1))
        cpu.mem_write(body + 0x30, struct.pack('<H', words[10]))
        w32(cpu, body + 0x34, words[11])
        w32(cpu, body + 0x878, words[9])
        for index in range(16):
            flags = 0x200 if words[14] & (1 << index) else 0
            cpu.mem_write(cells + index * 18, struct.pack('<hhhHIHHBB',
                (index % 4) * 16, 0, (index // 4) * 16, flags, 16, 0, 0, 255, 0))
            cpu.mem_write(nodes + index * 20 + 10, struct.pack('<H', index))
        for offset, value in [(0, 4), (4, 4), (8, 16), (12, bits(1)), (16, cells)]:
            w32(cpu, grid + offset, value)
        w32(cpu, 0x50e240, grid)
        # Actual world navigation origin globals (confirmed from 0x00110EF0).
        w32(cpu, 0x50e264, 0)
        w32(cpu, 0x50e26c, 0)
        w32(cpu, search + 8, 16)
        w32(cpu, search + 12, nodes)
        w32(cpu, search + 16, grid)
        w32(cpu, search + 0x18, 0xffffffff)
        w32(cpu, search + 0x1c, 0xffffffff)
        w32(cpu, search + 0x20, words[12])
        w32(cpu, device, 5)
        cpu.mem_write(target, struct.pack('<3I', words[3], 0, words[4]))
        cpu.mem_write(output, struct.pack('<4I', bits(123), bits(456), bits(789), 77))
        w32(cpu, 0x4312dc, 0)
        w32(cpu, 0x5096c0, words[8])
        w32(cpu, 0x4312c4, bits(1))
        heap_state['next'] = 0x30000000
        heap_state['sizes'].clear()
        cpu.reg_write(UC_X86_REG_EAX, mind if words[0] == 0 else 0)
        args = [actor, body, target, 0, output, output + 12] if words[0] == 0 else [
            mind, device, target, words[5], words[6], words[7], 1, output + 12]
        w32(cpu, STACK, STOP)
        for index, arg in enumerate(args):
            w32(cpu, STACK + 4 + index * 4, arg)
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.emu_start(0x136eb0 if words[0] == 0 else 0x136b00, STOP, count=1000000)
        if cpu.reg_read(UC_X86_REG_ESP) != STACK + 4:
            raise RuntimeError('retail navigation drive did not return')
        expected.append([cpu.reg_read(UC_X86_REG_EAX), r32(cpu, output + 12),
            r32(cpu, device + 16), r32(cpu, device + 20),
            *struct.unpack('<3I', cpu.mem_read(output, 12)), r32(cpu, search + 0x28), r32(cpu, search + 0x2c)])
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
    print(f'navigation drive: {count} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    sys.exit(main())
