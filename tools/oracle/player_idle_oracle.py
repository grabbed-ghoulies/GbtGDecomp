"""Compare idle-action selection and real request acceptance with retail.

Action tables are absent in this fixture; request rows may be present. No
callees are bridged, including the acceptance that returns false while storing
the selected request.
"""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator
from retail_cpu import ROOT, STOP, STACK, w32, r32, harness
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESI, UC_X86_REG_ESP


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 3000
    lines, expected = [], []
    body, mind, tables, rows = 0x10001000, 0x10003000, 0x10006000, 0x10008000
    keys = [135, 136, 153, 154, 162, 165]
    for index in range(count):
        words = [rng.getrandbits(32), rng.randrange(64)] + [rng.getrandbits(32) for _ in range(3)]
        cpu = emulator()
        data = bytearray()
        for item, key in enumerate(keys):
            if words[1] & (1 << item):
                record = bytearray(0x24)
                struct.pack_into('<H', record, 0, key)
                data += record
        if data:
            cpu.mem_write(rows, bytes(data))
        w32(cpu, tables, rows)
        cpu.mem_write(tables + 4, struct.pack('<H', len(data) // 0x24))
        w32(cpu, body + 0x5c, tables)
        w32(cpu, body + 0x84, words[3])
        w32(cpu, body + 0x88, words[4])
        w32(cpu, mind + 0x14, words[2])
        w32(cpu, 0x54dd7c, words[0])
        w32(cpu, STACK, STOP)
        w32(cpu, STACK + 4, mind)
        cpu.reg_write(UC_X86_REG_ESI, body)
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.emu_start(0x7e3a0, STOP, count=100000)
        expected.append([cpu.reg_read(UC_X86_REG_EAX), r32(cpu, 0x54dd7c),
                         r32(cpu, mind + 0x14), r32(cpu, body + 0x84), r32(cpu, body + 0x88)])
        lines.append('playeridle ' + ' '.join(map(str, words)))
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
    print(f'player idle selection: {count} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    raise SystemExit(main())
