"""Compare player body control messages with genuine retail callees.

Exercises scalar branches, empty attachment/anchor lists, and suspension
counts. Resource-created attachments and populated anchor lists remain outside
this fixture; no game callees are bridged.
"""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator
from retail_cpu import ROOT, STOP, STACK, w32, r32, harness
from unicorn.x86_const import UC_X86_REG_ESP


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 3000
    lines, expected = [], []
    body, actor, message = 0x10001000, 0x10003000, 0x10005000
    attachment, storage, flags = 0x10006000, 0x10007000, 0x10007100
    for index in range(count):
        mode = rng.choice([0x86, 0x87])
        words = [mode, rng.choice([actor, actor, 0]), rng.randrange(20),
                 rng.getrandbits(32), rng.choice([0, 0, 1]),
                 rng.choice([0, 1, rng.getrandbits(32)]),
                 rng.choice([0, rng.getrandbits(32)]), rng.choice([0, 1, 7]),
                 rng.randrange(256), rng.randrange(256), rng.getrandbits(32), rng.randrange(2)]
        cpu = emulator()
        w32(cpu, body + 8, actor)
        w32(cpu, body + 0x94, words[5])
        w32(cpu, body + 0x948, words[6])
        w32(cpu, body + 0x94c, words[7])
        w32(cpu, body + 0x15c, storage)
        w32(cpu, storage + 8, flags)
        cpu.mem_write(actor + 0x197, bytes([words[8]]))
        cpu.mem_write(attachment + 0x18, bytes([words[9]]))
        w32(cpu, actor + 0x8f8, attachment if words[11] else 0)
        w32(cpu, actor + 0x92c, words[10])
        cpu.mem_write(message, struct.pack('<5I', *words[:5]))
        w32(cpu, STACK, STOP)
        w32(cpu, STACK + 4, body)
        w32(cpu, STACK + 8, message)
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.emu_start(0x75b60, STOP, count=1000000)
        if cpu.reg_read(UC_X86_REG_ESP) != STACK + 4:
            raise RuntimeError('retail control dispatch did not return')
        expected.append([r32(cpu, body + 0x94), r32(cpu, body + 0x948),
                         r32(cpu, body + 0x94c), cpu.mem_read(actor + 0x197, 1)[0],
                         cpu.mem_read(attachment + 0x18, 1)[0], r32(cpu, actor + 0x92c),
                         cpu.mem_read(flags, 1)[0], r32(cpu, body + 0x924)])
        lines.append('playerbodycontrol ' + ' '.join(map(str, words)))
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
        if got != wanted:
            if failures < 8:
                print(index, lines[index], 'expected', wanted, 'actual', got)
            failures += 1
    print(f'player body control: {count} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    raise SystemExit(main())
