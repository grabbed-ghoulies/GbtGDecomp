"""Compare complete player-mind request preparation without callee bridges."""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator, bits
from retail_cpu import ROOT, STOP, STACK, w32, r32, harness
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 2000
    cpu = emulator()
    mind, actor, reference, message = 0x10001000, 0x10003000, 0x10005000, 0x10007000
    kinds = [0xf, 0x10, 0x11, 0x16, 0x24, 0x29, 0x2d, 0x2e, 0x2f, 0, 0x50]
    lines, expected = [], []
    for case in range(count):
        words = [rng.getrandbits(32), rng.randrange(2)]
        words += [bits(rng.uniform(-50, 50)) for _ in range(9)]
        payload = [bits(rng.uniform(-10, 10)) for _ in range(16)]
        payload[2] = (rng.getrandbits(16) << 16) | kinds[case % len(kinds)]
        payload[8] = rng.randrange(2)
        if case % 10 == 0:
            payload[rng.choice([4, 5, 7, 8])] = rng.choice([0, 0x80000000, 0x7fc00000, 0x7f800000])
        words += payload
        lines.append('playerrequest ' + ' '.join(map(str, words)))
        cpu.mem_write(mind, bytes([0xa5]) * 0x200)
        cpu.mem_write(actor, bytes(0xd00))
        cpu.mem_write(actor + 8, struct.pack('<3I', *words[2:5]))
        cpu.mem_write(actor + 0x14, struct.pack('<3I', *words[5:8]))
        cpu.mem_write(reference + 0x20, struct.pack('<3I', *words[8:11]))
        w32(cpu, mind + 8, actor)
        w32(cpu, mind + 0x10, reference if words[1] else 0)
        w32(cpu, 0x54dd7c, words[0])
        cpu.mem_write(message, struct.pack('<16I', *payload))
        w32(cpu, STACK, STOP)
        w32(cpu, STACK + 4, mind)
        w32(cpu, STACK + 8, message)
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.emu_start(0x7ebc0, STOP, count=1000000)
        if cpu.reg_read(UC_X86_REG_ESP) != STACK + 4:
            raise RuntimeError('retail request preparation did not return')
        output = [cpu.reg_read(UC_X86_REG_EAX), r32(cpu, 0x54dd7c)]
        output += [r32(cpu, mind + index * 4) for index in range(128) if index not in (2, 4)]
        output += list(struct.unpack('<16I', cpu.mem_read(message, 64)))
        expected.append(output)
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
                differences = [(index, a, b) for index, (a, b) in enumerate(zip(native, retail)) if a != b]
                print(f'case={case} differences={differences[:8]}')
    print(f'player request: {count} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    sys.exit(main())
