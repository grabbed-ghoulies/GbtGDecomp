"""Compare shape-target refresh/redirect and target acceptance with retail.

No callees are bridged. Attached model-radius cases use separate model fixtures;
this fixture supplies live contact shapes but no body-held model attachment.
"""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator, bits, equivalent
from retail_cpu import ROOT, STOP, STACK, w32, r32, harness
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 3000
    lines, expected = [], []
    mind, body, actor, attachment = 0x10001000, 0x10002000, 0x10003000, 0x10004000
    target_body, target_actor, storage, flags = 0x10006000, 0x10007000, 0x10008000, 0x10008100
    candidate, event = 0x10009000, 0x1000B000
    edge = [0x7FC00000, 0xFFC12345, 0x7F800000, 0xFF800000, 0x80000000, 1]
    for index in range(count):
        scalar = lambda low, high: bits(rng.uniform(low, high))
        words = [index % 3, rng.randrange(2), rng.randrange(2), rng.choice([0, bits(1)]),
                 rng.randrange(2), rng.choice([0, 0, bits(1)]), rng.getrandbits(32),
                 rng.getrandbits(32), rng.randrange(2), rng.choice([mind + 0xB8, 0, 123]),
                 scalar(-8, 8), scalar(0, 100), rng.getrandbits(32)]
        words += [scalar(-50, 50) for _ in range(3)]
        words += [scalar(0, 10), scalar(0, 10), scalar(0, 10)]
        words += words[13:16] if index % 2 else [scalar(-100, 100) for _ in range(3)]
        words += [scalar(0, 10), scalar(0, 10), rng.randrange(5)]
        words += [scalar(-100, 100) for _ in range(3)]
        words += [scalar(0, 20), rng.randrange(2), scalar(0, 1)]
        if index % 10 == 0:
            words[rng.choice([3, 5, 10, 11, *range(13, 24), *range(25, 29), 30])] = rng.choice(edge)
        shapes = bytes(rng.randrange(256) for _ in range(64))
        cpu = emulator()
        cpu.mem_map(0, 0x1000)
        cpu.mem_write(mind, bytes([0xA5]) * 0x200)
        w32(cpu, mind + 8, actor)
        w32(cpu, mind + 0x14, words[9])
        w32(cpu, mind + 0x28, event if words[29] else 0)
        w32(cpu, actor + 0x95c, body)
        w32(cpu, actor + 0x958, mind)
        w32(cpu, actor + 0x8f8, attachment if words[8] else 0)
        w32(cpu, actor + 0xcec, words[12])
        w32(cpu, body + 8, actor)
        cpu.mem_write(body + 0x6a, bytes(words[1:3]))
        w32(cpu, body + 0x70, words[3])
        cpu.mem_write(body + 0x30, struct.pack('<H', words[4]))
        w32(cpu, body + 0x34, words[5])
        w32(cpu, body + 0x15c, storage)
        w32(cpu, storage + 8, flags)
        cpu.mem_write(flags, struct.pack('<2I', *words[6:8]))
        cpu.mem_write(actor + 8, struct.pack('<3I', *words[13:16]))
        w32(cpu, actor + 0x38, words[16])
        w32(cpu, body + 0x78, words[17])
        w32(cpu, body + 0x888, words[18])
        cpu.mem_write(target_actor + 8, struct.pack('<3I', *words[19:22]))
        w32(cpu, target_actor + 0x38, words[22])
        w32(cpu, target_actor + 0x95c, target_body)
        w32(cpu, target_body + 8, target_actor)
        w32(cpu, target_body + 0x888, words[23])
        cpu.mem_write(candidate + 0x14, bytes([words[24]]))
        cpu.mem_write(candidate + 0x20, struct.pack('<3I', *words[25:28]))
        w32(cpu, candidate + 0x38, words[28])
        cpu.mem_write(event + 8, b'\x03\x00')
        w32(cpu, event + 0x18, words[30])
        cpu.mem_write(attachment + 0x1c, shapes)
        w32(cpu, STACK, STOP)
        if words[0] == 0:
            address, args = 0x7ea20, [mind, words[10], words[11]]
            cpu.reg_write(UC_X86_REG_EAX, body)
        elif words[0] == 1:
            address, args = 0x7e9e0, [mind, words[10]]
            cpu.reg_write(UC_X86_REG_EAX, body)
            cpu.reg_write(UC_X86_REG_ESI, candidate)
        else:
            address, args = 0x7e960, [candidate]
            cpu.reg_write(UC_X86_REG_EAX, target_actor)
            cpu.reg_write(UC_X86_REG_EDI, actor)
        for item, value in enumerate(args):
            w32(cpu, STACK + 4 + item * 4, value)
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.emu_start(address, STOP, count=1000000)
        if cpu.reg_read(UC_X86_REG_ESP) != STACK + 4:
            raise RuntimeError('retail target query did not return')
        expected.append([cpu.reg_read(UC_X86_REG_EAX), *struct.unpack('<128I', cpu.mem_read(mind, 0x200))])
        lines.append('playertarget ' + ' '.join(map(str, [*words, *shapes])))
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    process = subprocess.run([harness('body_joint_harness')], input='\n'.join(lines) + '\n',
                             text=True, capture_output=True, check=True, env=environment)
    actual = process.stdout.splitlines()
    if len(actual) != count:
        raise RuntimeError('wrong result count')
    float_indexes = {1 + offset // 4 for offset in [0xD8, 0xDC, 0xE0, 0xF8, 0xFC, 0x100]}
    failures = 0
    for index, (line, wanted) in enumerate(zip(actual, expected)):
        got = list(map(int, line.split()))
        differences = [(item, a, b) for item, (a, b) in enumerate(zip(got, wanted))
                       if item in float_indexes and not equivalent(a, b)]
        integer_differences = [(item, a, b) for item, (a, b) in enumerate(zip(got, wanted))
                              if item not in float_indexes and a != b]
        if len(got) != len(wanted) or differences or integer_differences:
            if failures < 8:
                print(index, lines[index], 'differences', differences + integer_differences)
            failures += 1
    print(f'player target: {count} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    raise SystemExit(main())
