"""Compare six collision sweep entries with retail and their genuine callees."""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator, bits, equivalent
from retail_cpu import ROOT, STOP, STACK, w32, harness
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_ESI, UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_EDI


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 400
    cpu = emulator()
    data, normal, fraction = 0x10002000, 0x10004000, 0x10005000
    lines, expected = [], []
    entries = [('sweeproot', 0x102da0), ('sweepspheres', 0x159cc0),
               ('sweeppoint', 0x102c30), ('sweepedge', 0x102970),
               ('sweepray', 0x159e20), ('sweeptriangle', 0x102e90)]
    for kind, address in entries:
        for case in range(count):
            words = [bits(rng.uniform(-10, 10)) for _ in range(23)]
            if kind == 'sweeproot':
                words[:3] = [bits(rng.uniform(-5, 5)) for _ in range(3)]
                if case % 7 == 0:
                    words[0] = rng.choice([0, 0x80000000, 0x3456bf95, 0xb456bf95])
            if kind == 'sweepspheres':
                words[9], words[13] = bits(rng.uniform(0, 15)), bits(rng.uniform(0, 2))
                if case % 7 == 0:
                    words[3:6] = words[10:13]
            if kind == 'sweeppoint':
                words[6] = bits(rng.uniform(0.1, 12))
            if kind == 'sweepedge':
                words[12] = bits(rng.uniform(0.1, 12))
                if case % 13 == 0:
                    words[:3] = words[3:6]
            if kind == 'sweepray':
                words[18] = bits(rng.uniform(0, 2))
                if case % 13 == 0:
                    words[6:9] = words[9:12]
            if kind == 'sweeptriangle':
                words[15] = bits(rng.uniform(0.1, 12))
                words[16:19] = [0, 0, 0]
                if case % 13 == 0:
                    words[:3] = words[3:6]
            if case % 17 == 0:
                words[rng.randrange(19)] = rng.choice([0, 0x80000000, 0x7fc00001, 0x7f800000, 0xff800000])
            lines.append(kind + ' ' + ' '.join(map(str, words)))
            cpu.mem_write(data, struct.pack('<23I', *words))
            cpu.mem_write(normal, struct.pack('<3I', *words[19:22]))
            w32(cpu, fraction, words[22])
            ptr = lambda index: data + index * 4
            if kind == 'sweeproot':
                args = words[:3]
                cpu.reg_write(UC_X86_REG_ESI, fraction)
            elif kind == 'sweepspheres':
                args = [ptr(0), ptr(3), ptr(6), words[9], ptr(10), words[13], fraction]
            elif kind == 'sweeppoint':
                args = [ptr(0), ptr(3), words[6]]
                cpu.reg_write(UC_X86_REG_EBX, ptr(7))
                cpu.reg_write(UC_X86_REG_EDI, normal)
                cpu.reg_write(UC_X86_REG_EAX, fraction)
            elif kind == 'sweepedge':
                args = [ptr(9), words[12], normal, fraction]
                cpu.reg_write(UC_X86_REG_EAX, ptr(0))
                cpu.reg_write(UC_X86_REG_EBX, ptr(3))
                cpu.reg_write(UC_X86_REG_EDI, ptr(6))
            elif kind == 'sweepray':
                args = [ptr(0), ptr(3), ptr(6), ptr(9), ptr(12), ptr(15), words[18], normal, normal + 4, normal + 8]
            else:
                args = [ptr(3), ptr(6), ptr(9), ptr(12), words[15], normal, fraction]
                cpu.reg_write(UC_X86_REG_EAX, ptr(0))
            w32(cpu, STACK, STOP)
            for index, word in enumerate(args):
                w32(cpu, STACK + 4 + index * 4, word)
            cpu.reg_write(UC_X86_REG_ESP, STACK)
            cpu.emu_start(address, STOP, count=1000000)
            if cpu.reg_read(UC_X86_REG_ESP) != STACK + 4:
                raise RuntimeError(f'{kind} did not return')
            expected.append((cpu.reg_read(UC_X86_REG_EAX) & 0xff,
                             *struct.unpack('<3I', cpu.mem_read(normal, 12)),
                             struct.unpack('<I', cpu.mem_read(fraction, 4))[0]))
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    result = subprocess.run([harness('body_joint_harness')], input='\n'.join(lines) + '\n',
                            text=True, capture_output=True, check=True, env=environment)
    actual = result.stdout.splitlines()
    if len(actual) != len(expected):
        raise RuntimeError(f'expected {len(expected)} results, received {len(actual)}')
    failures = 0
    for index, (line, retail) in enumerate(zip(actual, expected)):
        native = tuple(map(int, line.split()))
        if len(native) != 5 or native[0] != retail[0] or not all(equivalent(a, b) for a, b in zip(native[1:], retail[1:])):
            failures += 1
            if failures <= 12:
                print(f'{lines[index].split()[0]} case={index % count} native={native} retail={retail}')
    print(f'contact sweeps: {len(expected)} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    sys.exit(main())
