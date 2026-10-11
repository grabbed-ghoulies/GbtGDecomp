"""Compare five actual attachment request searches with retail; no callee bridges."""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator
from retail_cpu import ROOT, STOP, STACK, w32, harness
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 400
    cpu = emulator()
    table, data, body, actor = 0x10001000, 0x10002000, 0x10004000, 0x10005000
    entries = [(0x1c560, 4, 0, 0x24), (0x1c5a0, 6, 8, 0x24),
               (0x1cb20, 0x14, 0x18, 0x28), (0x1c7c0, 0x1c, 0x20, 0x2c),
               (0x1c930, 0x24, 0x28, 0x24)]
    lines, expected = [], []
    for operation, (address, count_at, rows_at, stride) in enumerate(entries):
        for case in range(count):
            keys = sorted(rng.randrange(65536) for _ in range(rng.randrange(60)))
            key = rng.choice(keys) if keys and case % 2 == 0 else rng.randrange(65536)
            present, category = rng.randrange(2), rng.choice([0, 1, 4, 4, 4, 255])
            lines.append('attachmentlookup ' + ' '.join(map(str, [operation, len(keys), key, present, category, *keys])))
            cpu.mem_write(table, bytes(0x2c))
            cpu.mem_write(table + count_at, struct.pack('<H', len(keys)))
            w32(cpu, table + rows_at, data)
            for index, value in enumerate(keys):
                cpu.mem_write(data + index * stride, struct.pack('<H', value))
            cpu.mem_write(body, bytes(0x900))
            cpu.mem_write(actor, bytes(0x950))
            w32(cpu, body + 0x5c, table)
            w32(cpu, body + 0x48, actor if present else 0)
            cpu.mem_write(actor + 0x93d, bytes([category]))
            w32(cpu, STACK, STOP)
            cpu.reg_write(UC_X86_REG_ESP, STACK)
            cpu.reg_write(UC_X86_REG_EAX, table)
            cpu.reg_write(UC_X86_REG_EBX, key)
            cpu.reg_write(UC_X86_REG_ECX, body)
            cpu.emu_start(address, STOP, count=100000)
            if cpu.reg_read(UC_X86_REG_ESP) != STACK + 4:
                raise RuntimeError('retail search did not return')
            result = cpu.reg_read(UC_X86_REG_EAX)
            expected.append(0xffffffff if result == 0 else (result - data) // stride)
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    result = subprocess.run([harness('body_joint_harness')], input='\n'.join(lines) + '\n',
                            text=True, capture_output=True, check=True, env=environment)
    actual = list(map(int, result.stdout.split()))
    if len(actual) != len(expected):
        raise RuntimeError('wrong result count')
    failures = 0
    for case, (native, retail) in enumerate(zip(actual, expected)):
        if native != retail:
            failures += 1
            if failures <= 8:
                print(f'case={case} native={native} retail={retail}\n{lines[case]}')
    print(f'attachment request lookups: {len(expected)} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    sys.exit(main())
