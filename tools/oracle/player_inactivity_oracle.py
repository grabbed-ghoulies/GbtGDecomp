"""Compare idle timing and input gates with genuine retail dependencies.

Uses keyboard axes, absent animation controller and action table. Animation
crossing and idle row acceptance have separate differential fixtures.
"""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator, bits, equivalent
from retail_cpu import ROOT, STOP, STACK, w32, r32, harness
from unicorn.x86_const import UC_X86_REG_ESP


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 3000
    edges = [bits(x) for x in [0., -0., .01, .1, .2, 1., -1., 10., 11., float('inf'), -float('inf')]]
    edges += [0x7FC00000, 0xFFC12345, 1, 0x80000001]
    lines, expected = [], []
    body, mind, actor, device = 0x10001000, 0x10003000, 0x10004000, 0x10005000
    storage, flags = 0x10006000, 0x10006100
    for index in range(count):
        scalar = lambda: rng.choice(edges) if rng.randrange(3) else rng.getrandbits(32)
        words = [rng.getrandbits(32), 0, rng.getrandbits(32), rng.getrandbits(32), rng.getrandbits(32),
                 rng.choice([0, 1]), scalar(), scalar(), rng.choice([0, 1, 2, 3, 0xFFFFFFFF]),
                 rng.choice([0, 1, 9]), rng.randrange(2), rng.choice([0, 0, 1]),
                 rng.choice([0, 0, scalar()]), rng.randrange(2), rng.randrange(2),
                 rng.choice([0, 0x2C]), rng.randrange(2)] + [scalar() for _ in range(7)]
        cpu = emulator()
        w32(cpu, mind + 8, actor)
        w32(cpu, actor + 0x95c, body)
        w32(cpu, mind + 0x14, words[2])
        cpu.mem_write(mind + 0x144, struct.pack('<5I', *words[5:10]))
        cpu.mem_write(mind + 0x20, bytes([words[10]]))
        cpu.mem_write(body + 0x30, struct.pack('<H', words[11]))
        w32(cpu, body + 0x34, words[12])
        w32(cpu, body + 0x48, words[13])
        w32(cpu, body + 0x50, words[14])
        cpu.mem_write(body + 0x748, struct.pack('<H', words[15]))
        w32(cpu, body + 0x15c, storage)
        w32(cpu, storage + 8, flags)
        cpu.mem_write(flags + 6, bytes([words[16]]))
        w32(cpu, body + 0x84, words[3])
        w32(cpu, body + 0x88, words[4])
        w32(cpu, 0x54dd7c, words[0])
        for address, word in zip([0x4312dc, 0x5096c0, 0x4312c4], words[17:20]):
            w32(cpu, address, word)
        w32(cpu, device, 3)
        w32(cpu, device + 4, 1)
        cpu.mem_write(0x507c98, struct.pack('<4I', *words[20:24]))
        w32(cpu, STACK, STOP)
        w32(cpu, STACK + 4, mind)
        w32(cpu, STACK + 8, device)
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.emu_start(0x7e160, STOP, count=100000)
        output = [0, r32(cpu, 0x54dd7c), r32(cpu, mind + 0x14),
                  r32(cpu, body + 0x84), r32(cpu, body + 0x88)]
        output += [r32(cpu, mind + offset) for offset in range(0x144, 0x158, 4)]
        expected.append(output)
        lines.append('playerinactivity ' + ' '.join(map(str, words)))
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    process = subprocess.run([harness('body_joint_harness')], input='\n'.join(lines) + '\n',
                             text=True, capture_output=True, check=True, env=environment)
    actual = process.stdout.splitlines()
    if len(actual) != count:
        raise RuntimeError('wrong result count')
    failures = 0
    for index, (line, wanted) in enumerate(zip(actual, expected)):
        got = list(map(int, line.split()))
        if len(got) != len(wanted) or any(not equivalent(a, b) if j in (6, 7) else a != b
                                         for j, (a, b) in enumerate(zip(got, wanted))):
            if failures < 8:
                print(index, lines[index], 'expected', wanted, 'actual', got)
            failures += 1
    print(f'player inactivity: {count} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    raise SystemExit(main())
