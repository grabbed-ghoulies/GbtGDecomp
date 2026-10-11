"""Compare the complete player gaze chain with genuine retail callees.

Uses populated model-anchor lists and keyboard axes, with absent models and
navigation grids. Marker/pose and populated path cases need separate coverage.
"""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator, bits, equivalent
from retail_cpu import ROOT, STOP, STACK, w32, r32, harness
from unicorn.x86_const import UC_X86_REG_ESI, UC_X86_REG_ESP


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 3000
    lines, expected = [], []
    mind, body, actor, device = 0x10001000, 0x10002000, 0x10003000, 0x10008000
    actors = [actor, 0x10004000, 0x10005000]
    anchors = [0x10006000 + index * 0x100 for index in range(4)]
    edge = [0x7FC00000, 0xFFC12345, 0x7F800000, 0xFF800000, 0x80000000, 1]
    for index in range(count):
        scalar = lambda low, high: bits(rng.uniform(low, high))
        words = [rng.getrandbits(32), rng.choice([0, 0, 0, 1]), rng.choice([0, 0, bits(1)]),
                 scalar(0, 3), scalar(0, 10)]
        words += [scalar(-100, 100) for _ in range(3)]
        words += [scalar(-10, 10), rng.randrange(2), rng.randrange(2), rng.randrange(5),
                  scalar(0, 1), scalar(0, 1), rng.choice([0, 0, 1])]
        words += [scalar(-100, 100) for _ in range(3)]
        words += [rng.choice([0, scalar(-2, 2)]) for _ in range(2)]
        words += [rng.randrange(2), scalar(0, 60), scalar(0, 200)]
        words += [rng.randrange(3) for _ in range(4)]
        words += [scalar(-100, 100) for _ in range(12)]
        words += [scalar(0, 10) for _ in range(4)]
        if index % 8 == 0:
            words[rng.choice([2, 3, 4, 5, 6, 7, 8, 12, 13, 18, 19, 21, 22, *range(27, 43)])] = rng.choice(edge)
        cpu = emulator()
        w32(cpu, mind + 8, actor)
        w32(cpu, actor + 0x95c, body)
        w32(cpu, actor + 0x960, device)
        w32(cpu, body + 8, actor)
        cpu.mem_write(body + 0x30, struct.pack('<H', words[1]))
        w32(cpu, body + 0x34, words[2])
        w32(cpu, body + 0x884, words[3])
        w32(cpu, actor + 0x34, words[4])
        cpu.mem_write(actor + 8, struct.pack('<3I', *words[5:8]))
        w32(cpu, actor + 0x18, words[8])
        cpu.mem_write(mind + 0x20, bytes([words[9]]))
        w32(cpu, mind + 0xc, actors[2] if words[10] else 0)
        w32(cpu, mind + 0x30, anchors[words[11]] if words[11] < 4 else 0)
        cpu.mem_write(mind + 0x34, struct.pack('<3I', *words[12:15]))
        cpu.mem_write(mind + 0x40, struct.pack('<3I', *words[15:18]))
        w32(cpu, device, 3)
        w32(cpu, device + 4, 1)
        cpu.mem_write(0x507ca0, struct.pack('<2I', *words[18:20]))
        w32(cpu, 0x54dd7c, words[0])
        w32(cpu, 0x510658, anchors[0] + 8)
        w32(cpu, 0x50e240, 0)
        for item, address in enumerate(anchors):
            w32(cpu, address + 12, anchors[item + 1] + 8 if item < 3 else 0)
            w32(cpu, address + 0x10, actors[words[23 + item]])
            cpu.mem_write(address + 0x28, struct.pack('<3I', *words[27 + item * 3:30 + item * 3]))
            w32(cpu, address + 0x34, words[39 + item])
        w32(cpu, STACK, STOP)
        for item, value in enumerate(words[20:23]):
            w32(cpu, STACK + 4 + item * 4, value)
        cpu.reg_write(UC_X86_REG_ESI, mind)
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.emu_start(0x137140, STOP, count=1000000)
        if cpu.reg_read(UC_X86_REG_ESP) != STACK + 4:
            raise RuntimeError('retail gaze did not return')
        output = [r32(cpu, 0x54dd7c)]
        output += [r32(cpu, mind + offset) for offset in range(0x30, 0x40, 4)]
        output += list(struct.unpack('<3I', cpu.mem_read(body + 0x2fc, 12)))
        output += [cpu.mem_read(body + 0x308, 1)[0]]
        expected.append(output)
        lines.append('playergaze ' + ' '.join(map(str, words)))
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
        if len(got) != len(wanted) or any(not equivalent(a, b) if item in (2, 3, 5, 6, 7) else a != b
                                         for item, (a, b) in enumerate(zip(got, wanted))):
            if failures < 8:
                print(index, lines[index], 'expected', wanted, 'actual', got)
            failures += 1
    print(f'player gaze: {count} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    raise SystemExit(main())
