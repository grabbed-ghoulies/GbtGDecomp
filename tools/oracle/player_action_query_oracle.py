"""Compare nine action-target query functions with their retail machine code.

All callees execute; no model/navigation or allocation boundaries are needed.
"""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator, bits, equivalent
from retail_cpu import ROOT, STOP, STACK, w32, r32, harness
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_XMM0


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 3000
    lines, expected = [], []
    owner, actor, focus, target = 0x10001000, 0x10003000, 0x10004000, 0x10005000
    body, other, mind, result = 0x10006000, 0x10007000, 0x10008000, 0x10009000
    storage, flags, other_storage, other_flags = 0x1000A000, 0x1000A100, 0x1000A200, 0x1000A300
    for index in range(count):
        scalar = lambda low, high: bits(rng.uniform(low, high))
        words = [index % 9] + [scalar(-30, 30) for _ in range(9)]
        words += [scalar(-7, 7), scalar(0, 5), scalar(0, 10), scalar(0, 10), scalar(0, 10)]
        words += [rng.randrange(5), rng.randrange(16), rng.getrandbits(32), rng.getrandbits(32)]
        words += [rng.choice([0, 0, 1]) for _ in range(3)]
        words += [rng.choice([0, bits(1)]), rng.choice([0, 0, 1]), rng.choice([0, 0, bits(1)]), rng.randrange(3)]
        words += [rng.choice([0, bits(1)]), rng.randrange(4), scalar(-1, 1), rng.choice([0, scalar(-1, 1)])]
        words += [rng.randrange(2), rng.randrange(4), rng.randrange(2), rng.randrange(3)]
        words += [rng.choice([0, 0, rng.getrandbits(32)]), rng.choice([0, 0, rng.getrandbits(32)])]
        words += [rng.randrange(2), rng.randrange(4), rng.randrange(2), rng.randrange(2), rng.randrange(2)]
        if index % 10 == 0:
            words[rng.choice([*range(1, 15), 22, 24, 26, 28, 29])] = rng.choice([0x7FC00000, 0x7F800000, 0xFF800000, 0x80000000])
        cpu = emulator()
        cpu.mem_map(0, 0x1000)
        for address, values in [(owner + 8, words[1:4]), (actor + 8, words[4:7]), (focus + 8, words[7:10]), (target + 0x20, words[4:7])]:
            cpu.mem_write(address, struct.pack('<3I', *values))
        w32(cpu, owner + 0x18, words[10])
        w32(cpu, owner + 0x34, words[11])
        w32(cpu, owner + 0x38, words[11])
        w32(cpu, owner + 0x95C, body)
        cpu.mem_write(owner + 0xCDE, struct.pack('<H', words[25]))
        w32(cpu, body + 8, owner)
        w32(cpu, body + 0x884, words[12])
        w32(cpu, body + 0x888, words[13])
        cpu.mem_write(body + 0x6A, bytes(words[19:22]))
        w32(cpu, body + 0x70, words[22])
        cpu.mem_write(body + 0x30, struct.pack('<H', words[23]))
        w32(cpu, body + 0x34, words[24])
        w32(cpu, body + 0x15C, storage)
        w32(cpu, storage + 8, flags)
        cpu.mem_write(flags, struct.pack('<2I', *words[34:36]))
        w32(cpu, actor + 0x95C, other)
        w32(cpu, other + 8, actor)
        w32(cpu, other + 0x15C, other_storage)
        w32(cpu, other_storage + 8, other_flags)
        cpu.mem_write(other_flags, struct.pack('<2I', *words[17:19]))
        w32(cpu, other + 0x23C, words[26])
        cpu.mem_write(other + 0x94, struct.pack('<H', words[27]))
        w32(cpu, other + 0x174, words[28])
        w32(cpu, other + 0x16C, words[29])
        w32(cpu, target + 0x10, actor)
        cpu.mem_write(target + 0x14, bytes([words[15], 0, words[16]]))
        w32(cpu, target + 0x30, words[14])
        w32(cpu, target + 0x38, words[14])
        w32(cpu, mind + 0x88, target if words[30] else 0)
        cpu.mem_write(mind + 0x90, bytes([words[31]]))
        w32(cpu, mind + 0x80, words[32])
        w32(cpu, actor + 0xCE0, words[36])
        cpu.mem_write(actor + 0xCDC, struct.pack('<H', words[37]))
        w32(cpu, actor + 0x93C, words[38])
        w32(cpu, 0x512248, words[39])
        w32(cpu, 0x512200, words[40])
        w32(cpu, result + 4, 0xA5A5A5A5)
        w32(cpu, STACK, STOP)
        addresses = [0x1CC10, 0x26830, 0x1D180, 0x134650, 0x1413C0, 0x1D130, 0x1CD00, 0x7DDA0, 0x414F0]
        mode = words[0]
        args = []
        if mode in (0, 1):
            cpu.reg_write(UC_X86_REG_EAX, body)
            cpu.reg_write(UC_X86_REG_ECX, target)
            args = [words[10] if mode == 0 else result + 4]
        elif mode == 2:
            cpu.reg_write(UC_X86_REG_ESI, body)
        elif mode == 3:
            cpu.reg_write(UC_X86_REG_ECX, other)
        elif mode in (4, 5):
            cpu.reg_write(UC_X86_REG_EAX, target)
            cpu.reg_write(UC_X86_REG_ESI, actor)
        elif mode == 6:
            cpu.reg_write(UC_X86_REG_EAX, 0)
            cpu.reg_write(UC_X86_REG_ECX, body)
            cpu.reg_write(UC_X86_REG_EBX, target)
        elif mode == 7:
            cpu.reg_write(UC_X86_REG_ESI, target)
            cpu.reg_write(UC_X86_REG_EDI, owner)
            args = [mind, [0, actor, focus][words[33]]]
        else:
            cpu.reg_write(UC_X86_REG_EAX, owner)
            args = [focus]
        for item, value in enumerate(args):
            w32(cpu, STACK + 4 + item * 4, value)
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.emu_start(addresses[mode], STOP, count=1000000)
        value = cpu.reg_read(UC_X86_REG_XMM0) & 0xFFFFFFFF if mode == 8 else cpu.reg_read(UC_X86_REG_EAX)
        expected.append([value, r32(cpu, result + 4), cpu.mem_read(mind + 0x90, 1)[0]])
        lines.append('playeractionquery ' + ' '.join(map(str, words)))
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    process = subprocess.run([harness('body_joint_harness')], input='\n'.join(lines) + '\n', text=True, capture_output=True, check=True, env=environment)
    actual = process.stdout.splitlines()
    if len(actual) != count:
        raise RuntimeError('wrong result count')
    failures = 0
    for index, (line, wanted) in enumerate(zip(actual, expected)):
        got = list(map(int, line.split()))
        matches = equivalent(got[0], wanted[0]) if index % 9 == 8 else got[0] == wanted[0]
        if len(got) != len(wanted) or not matches or got[1:] != wanted[1:]:
            if failures < 8:
                print(index, lines[index], 'expected', wanted, 'actual', got)
            failures += 1
    print(f'player action queries: {count} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    raise SystemExit(main())
