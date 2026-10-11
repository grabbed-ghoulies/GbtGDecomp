"""Compare translated damage table selectors with actual retail machine code."""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator, BODY, ACTOR, bits, equivalent
from retail_cpu import ROOT, STOP, STACK, w32, harness
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_XMM0


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 400
    cpu = emulator()
    rows, expected = [], []
    addresses = [0x1339c0, 0x133bd0, 0x28da0, 0x23cb0, 0x23e20, 0x23f70]
    for mode, address in enumerate(addresses):
        for case in range(count):
            words = [mode, rng.randrange(0x48, 0x87), rng.randrange(256), rng.randrange(256), rng.randrange(256),
                     bits(rng.uniform(-50, 50)), bits(rng.uniform(-50, 50)), rng.randrange(4),
                     rng.randrange(2), rng.randrange(4), rng.randrange(4), rng.randrange(256)]
            if case % 7 == 0:
                words[4] = words[3]
            if case % 11 == 0:
                words[rng.choice([5, 6])] = rng.choice([0x7fc00001, 0x7f800000, 0xff800000, 0x4f000000])
            record = struct.pack('<BBHII', words[3], words[4], 0, words[5], words[6])
            cpu.mem_write(0x433c9c, record * 256)
            cpu.mem_write(0x10007000, bytes([words[11]]) * 0x1000)
            w32(cpu, 0x545ac4, 0x10007000)
            cpu.mem_write(BODY, bytes(0x924))
            cpu.mem_write(ACTOR, bytes(0xd00))
            w32(cpu, BODY + 8, ACTOR)
            cpu.mem_write(ACTOR + 0xcdc, struct.pack('<H', words[7]))
            w32(cpu, ACTOR + 0x93c, words[8])
            cpu.mem_write(ACTOR + 0xcf4, struct.pack('<H', words[9]))
            w32(cpu, STACK, STOP)
            w32(cpu, STACK + 4, words[2])
            cpu.reg_write(UC_X86_REG_ESP, STACK)
            cpu.reg_write(UC_X86_REG_EAX, words[1] if mode < 3 else BODY)
            cpu.reg_write(UC_X86_REG_ECX, ACTOR if mode == 2 else words[10])
            cpu.reg_write(UC_X86_REG_EDX, words[10])
            cpu.emu_start(address, STOP, count=100000)
            if cpu.reg_read(UC_X86_REG_ESP) != STACK + 4:
                raise RuntimeError(f'mode {mode} did not return')
            value = cpu.reg_read(UC_X86_REG_EAX) if mode in (2, 3) else cpu.reg_read(UC_X86_REG_XMM0) & 0xffffffff
            expected.append((mode, case, value))
            rows.append('damagevalue ' + ' '.join(map(str, words)))
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    result = subprocess.run([harness('body_joint_harness')], input='\n'.join(rows) + '\n',
                            text=True, capture_output=True, check=True, env=environment)
    outputs = result.stdout.splitlines()
    if len(outputs) != len(expected):
        raise RuntimeError(f'host returned {len(outputs)} rows for {len(expected)} cases')
    mismatches = 0
    for output, (mode, case, retail) in zip(outputs, expected):
        host = int(output)
        same = host == retail if mode in (2, 3) else equivalent(host, retail)
        if not same:
            mismatches += 1
            if mismatches <= 8:
                print(mode, case, hex(retail), hex(host))
    print(f'{len(expected)} body damage table cases, {mismatches} mismatches')
    return bool(mismatches)


if __name__ == '__main__':
    sys.exit(main())
