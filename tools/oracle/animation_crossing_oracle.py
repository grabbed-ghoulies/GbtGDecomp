"""Compare the actual retail animation crossing predicate, including NaNs."""
import os
import random
import subprocess
import sys
from body_joint_oracle import emulator, bits
from retail_cpu import ROOT, STOP, STACK, w32, harness
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 3000
    edges = [bits(value) for value in [0., -0., -1., 1., .5, float('inf'), -float('inf')]]
    edges += [0x7FC00000, 0xFFC12345, 0x7FA00001, 1, 0x80000001]
    lines, expected = [], []
    actor, controller, slots, player = 0x10001000, 0x10003000, 0x10005000, 0x10006000
    for index in range(count):
        values = [rng.choice(edges) if rng.randrange(2) else rng.getrandbits(32) for _ in range(4)]
        words = [rng.choice([0, 1, 1, 1]), rng.choice([0, 1, 1, 1])] + values
        cpu = emulator()
        w32(cpu, actor + 0x50, controller if words[0] else 0)
        w32(cpu, controller + 0x58, slots)
        w32(cpu, slots, player if words[1] else 0)
        w32(cpu, player + 0x14, words[2])
        w32(cpu, player + 0x10c, words[3])
        w32(cpu, player + 0x1c, words[4])
        w32(cpu, STACK, STOP)
        w32(cpu, STACK + 4, words[5])
        cpu.reg_write(UC_X86_REG_EAX, actor)
        cpu.reg_write(UC_X86_REG_ECX, 0)
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.emu_start(0x3b1f0, STOP, count=10000)
        expected.append(cpu.reg_read(UC_X86_REG_EAX))
        lines.append('animationcrossing ' + ' '.join(map(str, words)))
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    process = subprocess.run([harness('body_joint_harness')], input='\n'.join(lines) + '\n',
                             text=True, capture_output=True, check=True, env=environment)
    actual = list(map(int, process.stdout.split()))
    if len(actual) != count:
        raise RuntimeError('wrong result count')
    failures = 0
    for index, (got, wanted) in enumerate(zip(actual, expected)):
        if got != wanted:
            if failures < 8:
                print(index, lines[index], 'expected', wanted, 'actual', got)
            failures += 1
    print(f'animation crossing: {count} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    raise SystemExit(main())
