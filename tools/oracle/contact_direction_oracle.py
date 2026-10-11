"""Compare the contact-direction reducer with retail, without callee bridges."""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator, bits, equivalent
from retail_cpu import ROOT, STOP, STACK, w32, harness
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_ESI, UC_X86_REG_EAX, UC_X86_REG_EDI


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 400
    cpu = emulator()
    lines, expected = [], []
    for case in range(count):
        records = rng.randrange(0, 25)
        rows = bytearray([0xa5] * (records * 0x54))
        stride = rng.choice([0x54, 0x2a, 0xa8])
        for index in range(records):
            for offset in (0xc, 0x10, 0x14):
                value = bits(rng.uniform(-200, 200))
                if case % 17 == 0:
                    value = rng.choice([0, 0x80000000, 0x7fc00001, 0x7f800000, 0xff800000])
                struct.pack_into('<I', rows, index * 0x54 + offset, value)
        lines.append('contactdirection ' + ' '.join(map(str, [len(rows), stride, *rows])))
        pool, data, output = 0x10002000, 0x10003000, 0x10006000
        cpu.mem_write(pool, struct.pack('<HHIII', stride, 4, data, data + len(rows), data + len(rows)))
        if rows:
            cpu.mem_write(data, bytes(rows))
        cpu.mem_write(output, b'\xa5' * 12)
        w32(cpu, STACK, STOP)
        w32(cpu, STACK + 4, pool)
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.reg_write(UC_X86_REG_ESI, output)
        cpu.emu_start(0xfca60, STOP, count=1000000)
        if cpu.reg_read(UC_X86_REG_ESP) != STACK + 4:
            raise RuntimeError('retail contact-direction reducer did not return')
        expected.append(struct.unpack('<III', cpu.mem_read(output, 12)))
    for case in range(count):
        words = [bits(rng.uniform(-200, 200)) for _ in range(7)] + [rng.randrange(2)]
        if case % 11 == 0:
            words[rng.randrange(7)] = rng.choice([0, 0x80000000, 0x7fc00001, 0x7f800000, 0xff800000])
        if case % 13 == 0:
            words[:3] = words[3:6]
        lines.append('contactposition ' + ' '.join(map(str, words)))
        damage, pool, row, output = 0x10001000, 0x10002000, 0x10003000, 0x10006000
        cpu.mem_write(damage, bytes(0x68))
        cpu.mem_write(damage + 0x18, struct.pack('<III', *words[:3]))
        cpu.mem_write(row + 0x2c, struct.pack('<IIII', *words[3:7]))
        w32(cpu, damage + 0x30, pool if words[7] else 0)
        w32(cpu, pool + 4, row)
        w32(cpu, STACK, STOP)
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.reg_write(UC_X86_REG_EAX, damage)
        cpu.reg_write(UC_X86_REG_EDI, output)
        cpu.emu_start(0x21010, STOP, count=1000000)
        if cpu.reg_read(UC_X86_REG_ESP) != STACK + 4:
            raise RuntimeError('retail contact-position routine did not return')
        expected.append(struct.unpack('<III', cpu.mem_read(output, 12)))
    for case in range(count):
        records = rng.randrange(0, 12)
        rows = bytearray([0xa5] * (records * 0x54))
        words = [bits(rng.uniform(-3, 3)) for _ in range(3)] + [bits(rng.uniform(0.1, 5))]
        for index in range(records):
            offset = index * 0x54
            rows[offset + 0x28] = rng.choice([0, 1, 1, 2])
            for field in range(9):
                struct.pack_into('<I', rows, offset + 0x2c + field * 4, bits(rng.uniform(-5, 5)))
            if rows[offset + 0x28] == 1:
                struct.pack_into('<I', rows, offset + 0x38, bits(rng.uniform(0.1, 8)))
            if case % 19 == 0:
                struct.pack_into('<I', rows, offset + 0x2c, 0x7fc00001)
        lines.append('contactseparate ' + ' '.join(map(str, [len(rows), *words, *rows])))
        pool, data, position, output = 0x10002000, 0x10003000, 0x10005000, 0x10006000
        cpu.mem_write(pool, struct.pack('<HHIII', 0x54, 4, data, data + len(rows), data + len(rows)))
        if rows:
            cpu.mem_write(data, bytes(rows))
        cpu.mem_write(position, struct.pack('<III', *words[:3]))
        cpu.mem_write(output, b'\xa5' * 12)
        w32(cpu, STACK, STOP)
        w32(cpu, STACK + 4, pool)
        w32(cpu, STACK + 8, position)
        w32(cpu, STACK + 12, words[3])
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.reg_write(UC_X86_REG_EAX, output)
        cpu.emu_start(0xfc3f0, STOP, count=1000000)
        if cpu.reg_read(UC_X86_REG_ESP) != STACK + 4:
            raise RuntimeError('retail contact separation did not return')
        expected.append(struct.unpack('<III', cpu.mem_read(output, 12)))
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    result = subprocess.run([harness('body_joint_harness')], input='\n'.join(lines) + '\n',
                            text=True, capture_output=True, check=True, env=environment)
    actual = result.stdout.splitlines()
    if len(actual) != len(expected):
        raise RuntimeError(f'expected {len(expected)} native results, received {len(actual)}')
    failures = 0
    for case, (line, retail) in enumerate(zip(actual, expected)):
        native = tuple(map(int, line.split()))
        if len(native) != 3 or not all(equivalent(a, b) for a, b in zip(native, retail)):
            failures += 1
            if failures <= 8:
                print(f'case={case} native={native} retail={retail}')
                print(lines[case])
    print(f'contact direction/position/separation: {len(expected)} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    sys.exit(main())
