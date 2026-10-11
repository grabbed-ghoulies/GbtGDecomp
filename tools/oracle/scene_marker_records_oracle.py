"""Retail comparison of marker record transforms, dispatch order and id walks.

Only heap boundaries and the actual indirect handler slots are bridged.
The handler callbacks record their inputs; this does not validate their bodies.
"""
import os
import random
import struct
import subprocess
import sys
from scene_markers_oracle import Retail, RECORDS
from retail_cpu import ROOT, STOP, STACK, w32, r32, harness
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDI, UC_X86_REG_ESP


def checksum(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 3000
    lines, expected = [], []
    for case in range(count):
        mode = case % 6
        retail = Retail({})
        cpu = retail.u
        cpu.mem_write(0x14D360, b'\xc2\x0c\x00')
        w32(cpu, 0x507834, 1)
        rows = []
        ids = rng.sample(range(1, 64), rng.randrange(9))
        for identity in ids:
            size = rng.choice([0x2C, 0x30, 0x40, 0x80])
            row = bytearray(rng.getrandbits(8) for _ in range(size))
            struct.pack_into('<IHHH', row, 0, size, rng.randrange(0x20), identity, rng.randrange(64))
            rows.append(row)
        records = struct.pack('<II', len(rows), 8 + sum(map(len, rows))) + b''.join(rows)
        swaps = [rng.choice(ids + [0, 99]) for _ in range(rng.randrange(4) * 2)]
        groups = [rng.choice(ids + [0, 0, 99]) for _ in range(rng.randrange(4) * 10)]
        seed = rng.getrandbits(32)
        mask = rng.getrandbits(28)
        identity, kind, wrap = rng.randrange(64), rng.choice([0, 2, 4, 8, 20, -1]), rng.randrange(2)
        line = [mode, seed, len(records), *records, len(swaps) // 2, *swaps,
                len(groups) // 10, *groups, mask, identity, kind & 0xFFFFFFFF, wrap]
        cpu.mem_write(RECORDS, records)
        w32(cpu, 0x51108C, RECORDS)
        w32(cpu, 0x54DD7C, seed)
        w32(cpu, 0x545CC8, len(swaps) // 2)
        w32(cpu, 0x545D1C, len(groups) // 10)
        for base, values in [(0x545C48, swaps), (0x545CCC, groups)]:
            if values:
                cpu.mem_write(base, struct.pack('<' + 'H' * len(values), *values))
        # Install actual indirect callback entries, recording their row arguments.
        calls = []
        def handler(cpu, address, size, user):
            row = r32(cpu, cpu.reg_read(UC_X86_REG_ESP) + 4)
            length = r32(cpu, row)
            header = struct.unpack('<IHH', cpu.mem_read(row, 8))
            calls.extend([header[1], header[2], checksum(cpu.mem_read(row, length))])
        for slot in range(0x1C):
            address = 0xF20100 + slot
            w32(cpu, 0x42A720 + slot * 4, address if mask & (1 << slot) else 0)
            cpu.hook_add(UC_HOOK_CODE, handler, begin=address, end=address)
        cpu.reg_write(UC_X86_REG_EDI, RECORDS)
        cpu.reg_write(UC_X86_REG_EAX, identity)
        w32(cpu, STACK, STOP)
        w32(cpu, STACK + 4, RECORDS if mode in (0, 1) else kind & 0xFFFFFFFF)
        w32(cpu, STACK + 8, wrap)
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        addresses = [0x1401B0, 0x1402B0, 0x11EEC0, 0x11F1C0, 0x127FF0, 0x140020]
        cpu.emu_start(addresses[mode], STOP, count=1000000)
        result = cpu.reg_read(UC_X86_REG_EAX) & 0xFFFF if mode == 3 else 0
        expected.append([result, r32(cpu, 0x54DD7C), checksum(cpu.mem_read(RECORDS, len(records))), len(calls), *calls])
        lines.append(' '.join(map(str, line)))
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    process = subprocess.run([harness('scene_marker_records_harness')], input=str(count) + '\n' + '\n'.join(lines) + '\n',
                             text=True, capture_output=True, check=True, env=environment)
    actual = process.stdout.splitlines()
    bad = abs(len(actual) - count)
    for index, (line, wanted) in enumerate(zip(actual, expected)):
        got = list(map(int, line.split()))
        if got != wanted:
            if bad < 8:
                print(index, 'mode', index % 6, 'expected', wanted, 'actual', got)
            bad += 1
    print(f'scene marker records: {count} cases, {bad} mismatches')
    return int(bad != 0)


if __name__ == '__main__':
    raise SystemExit(main())
