"""Compare actual attached-actor action selectors with retail machine code."""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator, BODY, ACTOR
from retail_cpu import ROOT, STOP, STACK, w32, harness
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESI, UC_X86_REG_ESP


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 400
    cpu = emulator()
    addresses = [0x26b00, 0x26bb0, 0x27220, 0x27320, 0x273d0, 0x27480, 0x272d0, 0x27060, 0x27140]
    attached, host, result = 0x10006000, 0x10008000, 0x1000a000
    rows, expected = [], []
    for mode, address in enumerate(addresses):
        for case in range(count):
            words = [mode, rng.randrange(8), rng.choice([0xffff, 0, 1, 2]), rng.randrange(4),
                     rng.randrange(4), rng.randrange(7), rng.randrange(4), rng.randrange(7),
                     rng.randrange(4), rng.randrange(8)]
            cpu.mem_write(BODY, bytes(0x924))
            cpu.mem_write(ACTOR, bytes(0xd00))
            cpu.mem_write(attached, bytes(0xd00))
            cpu.mem_write(host, bytes(0xd00))
            w32(cpu, BODY + 8, ACTOR)
            w32(cpu, BODY + 0x48, attached)
            cpu.mem_write(ACTOR + 6, struct.pack('<H', words[3]))
            cpu.mem_write(attached + 0x93d, bytes([words[1]]))
            cpu.mem_write(attached + 0x93f, bytes([words[8]]))
            cpu.mem_write(attached + 0x944, struct.pack('<H', words[2]))
            for offset, value in zip([0x948, 0x94c, 0x958, 0x95c], words[4:8]):
                cpu.mem_write(attached + offset, struct.pack('<H', value))
            w32(cpu, attached + 0xd8c, host if words[9] & 1 else 0)
            cpu.mem_write(host + 0xcdc, struct.pack('<H', words[9] >> 1))
            w32(cpu, STACK, STOP)
            w32(cpu, STACK + 4, BODY)
            cpu.reg_write(UC_X86_REG_ESP, STACK)
            cpu.reg_write(UC_X86_REG_ECX, BODY)
            if mode >= 7:
                cpu.mem_write(result, struct.pack('<HH', 0xabcd, 0x1234))
                w32(cpu, STACK + 4, result + 2)
                cpu.reg_write(UC_X86_REG_ESI, result)
            cpu.emu_start(address, STOP, count=100000)
            value = cpu.reg_read(UC_X86_REG_EAX)
            if mode >= 7:
                value = struct.unpack('<I', cpu.mem_read(result, 4))[0]
            expected.append(value)
            rows.append('actionid ' + ' '.join(map(str, words)))
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    host_result = subprocess.run([harness('body_joint_harness')], input='\n'.join(rows) + '\n',
                                 text=True, capture_output=True, check=True, env=environment)
    outputs = list(map(int, host_result.stdout.splitlines()))
    if len(outputs) != len(expected):
        raise RuntimeError('host output count differs')
    failures = 0
    for index, (actual, wanted) in enumerate(zip(outputs, expected)):
        if actual != wanted:
            failures += 1
            if failures <= 12:
                print(rows[index], hex(actual), hex(wanted))
    print(f'{len(expected)} body action selector cases, {failures} mismatches')
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
