"""Handler control-flow comparison. Factory creation/cloning are capture boundaries,
not acceptance of the classes they create. Navigation (two populated floor bands), name writes,
attachment allocation/list insertion/shape copies and parameter builders run for real.
"""
import os
import random
import struct
import subprocess
import sys
from scene_markers_oracle import Retail, RECORDS, DEFAULT_NAME, XBE, r16, r32, w32
from retail_cpu import ROOT, STOP, STACK, harness
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP

KINDS = [5, 6, 7, 0xA, 0xD, 0xE, 0xF, 0x11, 0x15, 0x16, 0x17, 0x19]
ENTRY = [0x1007F0, 0x1001A0, 0x100500, 0xFF8F0, 0xFFC00, 0xFF830,
         0xFFCD0, 0xFFD30, 0x100570, 0x100380, 0x100600, 0xFFA30]


def pack_blob(data):
    return [len(data), *data]


def canonical(data, kind):
    if kind in (5, 6, 0x16, 0x17) and len(data) != 0x334:
        return data  # Fully initialized cloned template.
    result = bytearray(len(data))
    def copy(offset, size):
        result[offset:offset + size] = data[offset:offset + size]
    def text(offset):
        end = data.index(0, offset) + 1
        copy(offset, end - offset)
    copy(0, 6)
    copy(8, 0x20)
    if kind == 0xA:
        copy(0x28, 4)
        text(0x2C)
        copy(0xAC, 0x1C)
        return bytes(result)
    for offset in (0x28, 0xA8, 0x128, 0x1A8, 0x228, 0x2B4):
        text(offset)
    copy(0x2A8, 6)
    copy(0x2B0, 4)
    if len(data) == 0x334:
        return bytes(result)
    if kind == 0xE:
        text(0x334)
    elif kind == 0xD:
        copy(0x334, 4)
        text(0x338)
        text(0x3B8)
        copy(0x438, 2)
        copy(0x43C, 0x24)
    elif kind == 0xF:
        copy(0x334, 6)
        text(0x33A)
        copy(0x3BC, 8)
    elif kind == 0x11:
        copy(0x334, 4)
        text(0x338)
        text(0x3B8)
        copy(0x438, 16)
    elif kind == 0x15:
        copy(0x340, 8)
        copy(0x448, 8)
        copy(0x750, 4)
        for offset in (0x348, 0x3C8, 0x450, 0x4D0, 0x550, 0x5D0, 0x650, 0x6D0):
            text(offset)
    return bytes(result)


class HandlerRetail(Retail):
    def __init__(self, succeeds, template):
        self.template = bytes(template)
        self.parameters = []
        super().__init__({'create': [succeeds] * 5})
    def dispatch(self, cpu, address, size, name):
        if name == 'clone':
            pointer = cpu.reg_read(UC_X86_REG_EAX)
            if bytes(cpu.mem_read(pointer, 4)) == b'aid_':
                block = self.allocate(len(self.template))
                cpu.mem_write(block, self.template)
                self.ret(block)
            else:
                self.ret(0)
        else:
            if name == 'create':
                pointer = self.arg(0)
                self.parameters.append(bytes(cpu.mem_read(pointer, r16(cpu, pointer))))
            super().dispatch(cpu, address, size, name)


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 3000
    inputs, expected = [], []
    scare_names = XBE(0x30CB18, 11 * 0x80)
    dropper_names = XBE(0x30D098, 5 * 0x80)
    for case in range(count):
        mode = case % len(KINDS)
        kind = KINDS[mode]
        row = bytearray(rng.getrandbits(8) for _ in range(0x480))
        struct.pack_into('<IHHH', row, 0, len(row), kind, 1, rng.choice([0, 2, 99]))
        for offset in range(0xC, 0x28, 4):
            struct.pack_into('<f', row, offset, rng.uniform(-50, 50))
        def text(offset, value):
            data = value.encode() + b'\0'
            row[offset:offset + len(data)] = data
        if kind in (5, 6, 0xA):
            text(0x28, rng.choice(['aid_objparams_test_alpha', 'missing']))
        if kind == 5:
            for offset in (0xA8, 0x128, 0x1A8):
                text(offset, rng.choice(['aid_objparams_test_alpha', 'missing']))
            for offset in (0x230, 0x238):
                struct.pack_into('<i', row, offset, rng.randrange(-10000, 10000))
        if kind in (7, 0xE):
            text(0x28, rng.choice(['', 'aid_objparams_test_alpha', 'marker_name']))
        if kind in (0xD, 0x11):
            text(0x2C, 'first_marker_name')
            text(0xAC, 'second_marker_name')
            for offset in ((0x130, 0x138) if kind == 0xD else (0x12C, 0x134)):
                struct.pack_into('<i', row, offset, rng.randrange(-10000, 10000))
        if kind == 0xF:
            text(0x30, 'switch_marker_name')
        if kind == 0x15:
            for offset in (0x30, 0xB0, 0x138, 0x1B8, 0x238, 0x2B8, 0x338, 0x3B8):
                text(offset, rng.choice(['aid_objparams_test_alpha', 'missing']))
        if kind in (0x16, 0x17):
            struct.pack_into('<i', row, 0x28, rng.randrange(11) if kind == 0x16 else rng.randrange(-1, 7))
        next_row = bytearray(0x480)
        struct.pack_into('<IHHH', next_row, 0, len(next_row), rng.choice([5, 7, 1]), 2, rng.choice([0, 1, 99]))
        struct.pack_into('<6f', next_row, 0xC, *[rng.uniform(-20, 20) for _ in range(6)])
        next_row[0x28:0x28 + 11] = b'chain_name\0'
        block = struct.pack('<II', 2, 8 + len(row) + len(next_row)) + row + next_row
        template = bytearray(rng.getrandbits(8) for _ in range(0x600))
        struct.pack_into('<HHH', template, 0, len(template), 0x6E, 0xD)
        struct.pack_into('<f', template, 0x20, rng.uniform(0.1, 5))
        positions = [struct.unpack('<I', struct.pack('<f', rng.uniform(-30, 30)))[0] for _ in range(9)]
        succeeds, present = rng.randrange(2), rng.randrange(2)
        retail = HandlerRetail(succeeds, template)
        cpu = retail.u
        cpu.mem_write(RECORDS, bytes(block))
        cpu.mem_write(DEFAULT_NAME, b'\0')
        w32(cpu, 0x51108C, RECORDS)
        w32(cpu, 0x50E250, 2)
        cpu.mem_write(0x50E254, struct.pack('<2f', -15, 0))
        cpu.mem_write(0x50E25C, struct.pack('<2f', 0, 30))
        w32(cpu, 0x507834, 1)
        cpu.mem_write(0x14D360, b'\xc2\x0c\x00')
        cpu.mem_write(STACK - 36, struct.pack('<3I', *positions[:3]))
        cpu.mem_write(STACK - 12, struct.pack('<3I', *positions[3:6]))
        cpu.mem_write(0x50DD80, struct.pack('<3I', *positions[6:]))
        w32(cpu, 0x511B20, RECORDS + 0x70934 if present else 0)
        w32(cpu, STACK, STOP)
        w32(cpu, STACK + 4, RECORDS + 8)
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.emu_start(ENTRY[mode], STOP, count=1000000)
        names = bytes(cpu.mem_read(0x51254C, r32(cpu, 0x512548) * 0x90))
        attachment = r32(cpu, RECORDS + 0x708F8)
        shape = b''
        if attachment:
            shape = bytes(cpu.mem_read(attachment + 0x14, 4))[:1] + bytes(cpu.mem_read(attachment + 0x19, 1)) + bytes(cpu.mem_read(attachment + 0x16, 2)) + bytes(cpu.mem_read(attachment + 0x1C, 0x4C))
        expected.append((bytes(cpu.mem_read(RECORDS, len(block))), [canonical(p, kind) for p in retail.parameters], names, shape))
        inputs.append(' '.join(map(str, [kind, succeeds, present, *pack_blob(block), *pack_blob(template),
            *pack_blob(scare_names), *pack_blob(dropper_names), *positions])))
    env = os.environ.copy()
    env['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + env.get('PATH', '')
    result = subprocess.run([harness('scene_marker_handlers_harness')], input=str(count) + '\n' + '\n'.join(inputs) + '\n',
        text=True, capture_output=True, env=env)
    if result.returncode:
        print('host exit', result.returncode, result.stderr[-1000:])
    actual = result.stdout.splitlines()
    bad = abs(len(actual) - count)
    for case, (line, want) in enumerate(zip(actual, expected)):
        words = iter(map(int, line.split()))
        def blob():
            return bytes(next(words) for _ in range(next(words)))
        records = blob()
        parameters = [canonical(blob(), KINDS[case % len(KINDS)]) for _ in range(next(words))]
        names, shape = blob(), blob()
        got = records, parameters, names, shape
        if got != want:
            if bad < 8:
                print('case', case, 'kind', hex(KINDS[case % len(KINDS)]), 'sections', [i for i in range(4) if got[i] != want[i]])
                if parameters != want[1]:
                    print('parameter sizes', list(map(len, parameters)), list(map(len, want[1])))
                    for a, b in zip(parameters, want[1]):
                        print('diff offsets', [i for i, (x, y) in enumerate(zip(a, b)) if x != y][:30])
                if shape != want[3]:
                    print('shape', shape.hex(), want[3].hex())
            bad += 1
    print(count, 'handler cases;', bad, 'mismatches')
    return bool(bad)


if __name__ == '__main__':
    sys.exit(main())
