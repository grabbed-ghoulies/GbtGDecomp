"""Compare scoring, angular mind-target query and held-item policy with retail.

All callees execute. One populated target, no models or navigation grids.
"""
import os
import random
import struct
import subprocess
import sys
from unicorn import UC_HOOK_CODE
from body_joint_oracle import emulator, bits, equivalent
from retail_cpu import ROOT, STOP, STACK, w32, r32, harness
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_ESI, UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_XMM0


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 3000
    expected, lines = [], []
    owner, actor, target, body = 0x10001000, 0x10003000, 0x10005000, 0x10006000
    other, held, policy, result = 0x10007000, 0x10008000, 0x10009000, 0x1000A000
    storage, flags, other_storage, other_flags = 0x1000B000, 0x1000B100, 0x1000B200, 0x1000B300
    for index in range(count):
        scalar = lambda low, high: bits(rng.uniform(low, high))
        words = [index % 3, rng.randrange(2)]
        words += [scalar(-10, 10) for _ in range(3)]
        words += [scalar(-50, 50) for _ in range(3)]
        words += [scalar(-7, 7), scalar(0, 4), scalar(-10, 100), rng.randrange(16), rng.choice([0, 4, 8, 0x20, 0x800])]
        words += [rng.randrange(4), rng.choice([0, 0, 1]), rng.randrange(1, 5), scalar(0, 10), rng.randrange(6)]
        words += [scalar(0, 3), scalar(0, 8), scalar(0, 8), rng.randrange(2), rng.randrange(2)]
        words += [rng.choice([0, 0x80]), rng.choice([0, 3]), scalar(0, 3), rng.getrandbits(32), rng.getrandbits(32), rng.getrandbits(32)]
        words += [rng.choice([0, bits(1)]), rng.randrange(2), rng.randrange(5), rng.randrange(5), rng.randrange(8)]
        if index % 10 == 0:
            words[rng.choice([*range(2, 11), 16, *range(18, 21), 25])] = rng.choice([0x7FC00000, 0x7F800000, 0xFF800000, 0x80000000])
        cpu = emulator()
        cpu.mem_map(0, 0x1000)
        cpu.mem_write(owner + 8, struct.pack('<3I', *words[2:5]))
        w32(cpu, owner + 0x38, words[18])
        w32(cpu, owner + 0x95C, body)
        w32(cpu, actor + 0x95C, other)
        cpu.mem_write(actor + 6, struct.pack('<H', words[15]))
        w32(cpu, body + 8, owner)
        w32(cpu, body + 0x48, held if words[17] else 0)
        w32(cpu, body + 0x888, words[19])
        w32(cpu, body + 0x78, words[20])
        w32(cpu, body + 0x15C, storage)
        w32(cpu, storage + 8, flags)
        cpu.mem_write(flags + 5, bytes([words[23]]))
        w32(cpu, other + 0x15C, other_storage)
        w32(cpu, other_storage + 8, other_flags)
        cpu.mem_write(other + 0x94, struct.pack('<H', words[22]))
        cpu.mem_write(held + 0x93D, bytes([words[17]]))
        w32(cpu, owner + 0x978, words[29])
        cpu.mem_write(owner + 0x969, bytes([words[30]]))
        cpu.mem_write(owner + 0x97C, struct.pack('<2H', words[31], words[32]))
        w32(cpu, target + 0x10, actor)
        cpu.mem_write(target + 0x14, bytes([words[13], 0, words[33], 0, words[14]]))
        cpu.mem_write(target + 0x20, struct.pack('<3I', *words[5:8]))
        w32(cpu, target + 0x38, words[16])
        w32(cpu, 0x545988, target + 8 if words[1] else 0)
        w32(cpu, policy + 0x64, words[12])
        w32(cpu, policy + 0x68, words[11])
        w32(cpu, policy + 0xB0, words[24])
        w32(cpu, 0x4275B8, words[25])
        w32(cpu, result, words[27])
        w32(cpu, result + 4, words[28])
        w32(cpu, STACK, STOP)
        if words[0] == 0:
            address, args = 0x129B20, words[8:11]
            cpu.reg_write(UC_X86_REG_ECX, owner + 8)
            cpu.reg_write(UC_X86_REG_EAX, target + 0x20)
            cpu.reg_write(UC_X86_REG_ESI, result + 0x10)
        elif words[0] == 1:
            address = 0x129770
            args = [owner, 0, words[8], words[9], words[31], words[32], result, result + 4, words[11], words[12]]
            cpu.reg_write(UC_X86_REG_XMM0, words[10])
        else:
            address, args = 0x7D6B0, [target if words[21] else 0, words[8]]
            cpu.reg_write(UC_X86_REG_ESI, owner)
            cpu.reg_write(UC_X86_REG_EBX, policy)
            cpu.reg_write(UC_X86_REG_EAX, result)
            w32(cpu, STACK - 104, words[26])
            # Observe the actual incoming uninitialized local after earlier
            # retail callees have reused its stack slot. No callee is bridged.
            observed = []
            def incoming_window(cpu, address, size, user):
                if address == 0x137DE0 and not observed:
                    retained = r32(cpu, cpu.reg_read(UC_X86_REG_ESP) - 8)
                    observed.append(retained)
                    words[26] = retained
            cpu.hook_add(UC_HOOK_CODE, incoming_window, begin=0x137DE0, end=0x137DE0)
        for item, value in enumerate(args):
            w32(cpu, STACK + 4 + item * 4, value)
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.emu_start(address, STOP, count=1000000)
        if words[0] == 0:
            output = [0, r32(cpu, result + 0x18), r32(cpu, result + 0x14), r32(cpu, result + 0x1C), r32(cpu, result + 0x20)]
        else:
            output = [cpu.reg_read(UC_X86_REG_EAX), r32(cpu, result), r32(cpu, result + 4), 0, 0]
        expected.append(output)
        lines.append('playersearch ' + ' '.join(map(str, words)))
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    process = subprocess.run([harness('body_joint_harness')], input='\n'.join(lines) + '\n', text=True, capture_output=True, check=True, env=environment)
    actual = process.stdout.splitlines()
    if len(actual) != count:
        raise RuntimeError('wrong result count')
    failures = 0
    for index, (line, wanted) in enumerate(zip(actual, expected)):
        got = list(map(int, line.split()))
        if len(got) != len(wanted) or any(not equivalent(a, b) if item != 0 else a != b for item, (a, b) in enumerate(zip(got, wanted))):
            if failures < 8:
                print(index, lines[index], 'expected', wanted, 'actual', got)
            failures += 1
    print(f'player search: {count} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    raise SystemExit(main())
