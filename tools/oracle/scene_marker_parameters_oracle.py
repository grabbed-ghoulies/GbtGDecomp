"""Compare seven parameter helpers with retail code, including untouched bytes."""
import os
import random
import struct
import subprocess
import sys
from scene_markers_oracle import Retail, RECORDS, DEFAULT_NAME
from retail_cpu import ROOT, STOP, STACK, w32, harness
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 3000
    lines, expected = [], []
    for case in range(count):
        mode = case % 7
        value, identity, following = rng.getrandbits(32), rng.getrandbits(16), rng.getrandbits(16)
        first = 'aid_' + 'x' * rng.randrange(1, 110)
        second = rng.choice(['aid_', 'not_']) + 'y' * rng.randrange(1, 110)
        vectors = [rng.getrandbits(32) for _ in range(6)]
        values = [rng.getrandbits(32) for _ in range(9)]
        data = bytes(rng.getrandbits(8) for _ in range(0x800))
        cpu = Retail({}).u
        cpu.mem_write(RECORDS, data)
        cpu.mem_write(RECORDS + 0x1000, first.encode() + b'\0')
        cpu.mem_write(DEFAULT_NAME, second.encode() + b'\0')
        cpu.mem_write(RECORDS + 0x2000, struct.pack('<6I', *vectors))
        cpu.reg_write(UC_X86_REG_EAX, RECORDS if mode == 0 or mode >= 3 else RECORDS + 0x1000)
        cpu.reg_write(UC_X86_REG_EDI, RECORDS + 0x2000 if mode == 0 or mode >= 3 else RECORDS)
        cpu.reg_write(UC_X86_REG_ESI, RECORDS + 0x200C if mode == 0 else DEFAULT_NAME)
        cpu.reg_write(UC_X86_REG_EBX, RECORDS + 0x1000)
        cpu.reg_write(UC_X86_REG_EDX, RECORDS)
        cpu.reg_write(UC_X86_REG_ECX, RECORDS + 0x200C)
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        w32(cpu, STACK, STOP)
        w32(cpu, STACK + 4, identity if mode == 0 else value)
        w32(cpu, STACK + 8, following)
        args = None
        if mode == 3:
            args = [identity, following, value, DEFAULT_NAME, value & 0xFFFF, *values]
        elif mode == 4:
            args = [identity, following, value, value & 0xFFFF, *values[:2]]
        elif mode == 5:
            args = [identity, following, value, DEFAULT_NAME, *values[:4]]
        elif mode == 6:
            args = [*values[:4], DEFAULT_NAME, RECORDS + 0x1000, DEFAULT_NAME,
                RECORDS + 0x1000, DEFAULT_NAME, RECORDS + 0x1000, DEFAULT_NAME, values[4]]
        if args is not None:
            for index, argument in enumerate(args):
                w32(cpu, STACK + 4 + index * 4, argument)
        cpu.emu_start([0x6D410, 0x146C80, 0x146CC0, 0x6AF70, 0x6BA70, 0x6C170, 0x725E0][mode], STOP, count=10000)
        expected.append(list(cpu.mem_read(RECORDS, len(data))))
        lines.append(' '.join(map(str, [mode, value, identity, following, first, second, *vectors, *values, *data])))
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    result = subprocess.run([harness('scene_marker_parameters_harness')],
        input=str(count) + '\n' + '\n'.join(lines) + '\n', text=True, capture_output=True, check=True, env=environment)
    actual = result.stdout.splitlines()
    bad = abs(len(actual) - count)
    for index, (line, wanted) in enumerate(zip(actual, expected)):
        got = list(map(int, line.split()))
        if got != wanted:
            if bad < 5:
                print('case', index, 'mode', index % 7, 'diff offsets', [i for i, pair in enumerate(zip(got, wanted)) if pair[0] != pair[1]][:20])
            bad += 1
    print(count, 'cases;', bad, 'mismatches')
    return bool(bad)


if __name__ == '__main__':
    sys.exit(main())
