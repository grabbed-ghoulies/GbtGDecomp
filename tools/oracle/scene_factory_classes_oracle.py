"""Retail scene factories and shared cinema controls/navigation primitives; compare seeded bytes."""
import os
import random
import struct
import subprocess
import sys
from scene_markers_oracle import Retail, RECORDS, XBE
from retail_cpu import ROOT, STOP, STACK, w32, harness
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_XMM0


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 23)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 3000
    lines, expected = [], []
    for case in range(count):
        mode = case % 15
        state, slot = rng.getrandbits(32), rng.getrandbits(32)
        parameters = bytearray(rng.getrandbits(8) for _ in range(0x1000))
        data = bytearray(rng.getrandbits(8) for _ in range(0xC6C))
        # Controller registration has separate native pointer coverage; no capture stubs here.
        struct.pack_into('<I', parameters, 0x334, 0)
        for offset in [0x54C, 0x5CC]:
            name = b'aid_' + bytes([65 + case % 26]) * rng.randrange(0, 120) + b'\0'
            parameters[offset:offset + len(name)] = name
        struct.pack_into('<f', parameters, 0x64C, rng.choice([-1., 0., .25, .75, 1., float('nan')]))
        active = rng.randrange(-2, 5)
        struct.pack_into('<i', data, 0xC58, active)
        for offset in range(0xC5C, 0xC6C, 4):
            struct.pack_into('<I', data, offset, rng.choice([0, rng.getrandbits(32)]))
        if mode == 3:
            struct.pack_into('<I', parameters, 0xA8, 0)
            struct.pack_into('<I', data, 0x88, 0)
            struct.pack_into('<I', data, 8, RECORDS + 0x2000)
        if mode == 4:
            struct.pack_into('<I', parameters, 0, rng.randrange(2))
            struct.pack_into('<I', parameters, 0x88, 0)
            for offset in range(8, 0x20, 4):
                struct.pack_into('<f', parameters, offset, rng.uniform(-100, 100))
            for offset in range(0x420, 0x42C, 4):
                struct.pack_into('<f', parameters, offset, rng.uniform(-100, 100))
            for offset in range(0x10, 0x3C, 4):
                struct.pack_into('<f', data, offset, rng.uniform(-100, 100))
            struct.pack_into('<I', data, 0x20, rng.randrange(2))
            struct.pack_into('<H', data, 8, rng.choice([0xD, 0xE, 0xF, 0x10, 0x11, 0x24, 0x29, 0x2D, 0x2E, 0x2F, 0xFFFF]))
        if mode == 5:
            struct.pack_into('<i', parameters, 0, rng.randrange(-3, 17))
            key = rng.randrange(16)
            struct.pack_into('<H', parameters, 6, key)
            struct.pack_into('<I', parameters, 0xCA0, rng.randrange(2))
            for offset in range(0x400, 0x440, 4):
                struct.pack_into('<H', parameters, offset, rng.randrange(16))
        if mode == 6:
            for offset in [8, 0xC, 0x10, 0x92C, 0x930, 0x134, 0x408, 0x40C]:
                struct.pack_into('<f', parameters, offset, rng.choice([0., .25, 1., -1., 5., float('nan')]))
            struct.pack_into('<H', parameters, 0x130, rng.randrange(2))
            struct.pack_into('<I', parameters, 0x95C, RECORDS + 0x2100)
            struct.pack_into('<I', data, 0x84, RECORDS + 0x2400)
            struct.pack_into('<I', data, 0x88, rng.randrange(4))
            struct.pack_into('<f', data, 0x8C, rng.choice([0., -1., .1, .5, 1., float('nan')]))
            # Exercise the genuine sound-request empty-name branch, without capturing a callee.
            for offset in [0x410, 0x490, 0x510]:
                parameters[offset] = 0
        if mode in [5, 6]:
            struct.pack_into('<I', data, 8, RECORDS + 0x2000)
        if mode in [7, 8]:
            struct.pack_into('<H', data, 0x30, rng.randrange(2))
            struct.pack_into('<f', data, 0x34, rng.choice([0., -0., 1., float('nan')]))
            struct.pack_into('<I', data, 8, RECORDS + 0x2000)
            struct.pack_into('<I', parameters, 0x88, 0)
            struct.pack_into('<I', parameters, 0x95C, RECORDS)
            struct.pack_into('<I', parameters, 0x608, RECORDS + 0x2000)
            struct.pack_into('<I', parameters, 0x610, rng.choice([0, RECORDS + 0x2400]))
            for offset in [8, 0xC, 0x10, 0x420, 0x424, 0x428]:
                struct.pack_into('<f', parameters, offset, rng.uniform(-100, 100))
        if mode == 9:
            struct.pack_into('<I', data, 8, RECORDS + 0x2000)
            struct.pack_into('<I', parameters, 0, rng.randrange(2))
            struct.pack_into('<f', parameters, 0x978, rng.choice([0., 1., -1., float('nan')]))
            parameters[0x969] = rng.randrange(2)
            struct.pack_into('<I', parameters, 0x610, rng.choice([RECORDS + 0x2000, RECORDS + 0x3500]))
            # Nonzero target categories exercise the policy without actor/body query fixtures.
            parameters[0x614] = rng.randrange(1, 256)
            parameters[0x615] = rng.randrange(2)
            parameters[0x616] = rng.randrange(8)
            parameters[0x618] = rng.randrange(2)
        if mode == 10:
            struct.pack_into('<I', data, 0, rng.randrange(7))
            for index in range(4):
                offset = 0x100 + index * 0x80
                struct.pack_into('<I', data, 8 + index * 4, RECORDS + offset)
                struct.pack_into('<I', data, offset, rng.choice([1, 2, 3, 5]))
                struct.pack_into('<I', data, offset + 8, rng.choice([0xFFFFFFFF, rng.getrandbits(32)]))
        if mode == 11:
            struct.pack_into('<I', parameters, 0x88, 0)
            struct.pack_into('<I', parameters, 0x958, RECORDS + 0x5000)
            for offset in [8, 0xC, 0x10, 0x420, 0x424, 0x428, 0x3C0, 0x3C4, 0x3C8]:
                struct.pack_into('<f', parameters, offset, rng.uniform(-100, 100))
            count_markers = rng.randrange(1, 7)
            struct.pack_into('<II', parameters, 0x800, count_markers + 1, (count_markers + 1) * 0x20 + 4)
            for index in range(count_markers + 1):
                offset = 0x808 + index * 0x20
                struct.pack_into('<IHH', parameters, offset, 0x20, 0 if index == 0 else 0x1B, 0x7000 if index == 0 else index)
                for component in range(3):
                    struct.pack_into('<f', parameters, offset + 0xC + component * 4, rng.uniform(-100, 100))
        if mode in [12, 13, 14]:
            for offset in range(0, 0x14, 4):
                struct.pack_into('<f', parameters, offset, rng.uniform(-100, 100))
        cpu = Retail({}).u
        # Actual CRT angle tables used by 0x00105F90; no math capture stub.
        cpu.mem_map(0x00200000, 0x10000)
        # These are separate XBE sections: one read cannot span both.
        cpu.mem_write(0x00200000, XBE(0x00200000, 0x3000))
        cpu.mem_write(0x00203000, XBE(0x00203000, 0xD000))
        cpu.mem_write(RECORDS, bytes(data))
        cpu.mem_write(RECORDS + 0x2000, bytes(parameters))
        w32(cpu, 0x54DD7C, state)
        w32(cpu, RECORDS + 0x4000, slot)
        if mode == 5:
            w32(cpu, 0x50E148, struct.unpack_from('<I', parameters)[0])
            cpu.mem_write(0x50E14C, bytes(parameters[0x400:0x440]))
        if mode == 6:
            for address, offset in [(0x4312DC, 8), (0x5096C0, 0xC), (0x4312C4, 0x10)]:
                cpu.mem_write(address, bytes(parameters[offset:offset + 4]))
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        w32(cpu, STACK, STOP)
        w32(cpu, STACK + 4, RECORDS)
        w32(cpu, STACK + 8, RECORDS + 0x2000)
        if mode == 4:
            w32(cpu, STACK + 4, RECORDS + 0x5000)
            w32(cpu, STACK + 8, RECORDS)
            w32(cpu, RECORDS + 0x5008, RECORDS + 0x2000)
            w32(cpu, RECORDS + 0x5010, RECORDS + 0x2400 if struct.unpack_from('<I', parameters)[0] else 0)
        cpu.reg_write(UC_X86_REG_EDX, RECORDS)
        cpu.reg_write(UC_X86_REG_ESI, RECORDS if mode in [5, 6] else RECORDS + 0x4000)
        if mode == 7:
            cpu.reg_write(UC_X86_REG_EAX, RECORDS + 0x2420)
        if mode == 8:
            cpu.reg_write(UC_X86_REG_EAX, RECORDS + 0x2600)
        if mode == 9:
            cpu.reg_write(UC_X86_REG_EAX, RECORDS + 0x2600)
            cpu.reg_write(UC_X86_REG_ECX, RECORDS)
            cpu.reg_write(UC_X86_REG_EDX, struct.unpack_from('<I', parameters)[0])
        if mode == 10:
            cpu.reg_write(UC_X86_REG_EAX, RECORDS)
            cpu.reg_write(UC_X86_REG_EBX, RECORDS + 0x2000)
            cpu.mem_write(0x507C98, bytes(data[0x800:0x880]))
        if mode == 11:
            w32(cpu, 0x51108C, RECORDS + 0x2800)
            w32(cpu, RECORDS + 0x5010, RECORDS + 0x2400)
            for index, word in enumerate([RECORDS + 0x2000,
                    *struct.unpack_from('<III', parameters, 0x3C0), RECORDS + 0x70]):
                w32(cpu, STACK + 4 + index * 4, word)
        if mode == 12:
            cpu.reg_write(UC_X86_REG_XMM0, struct.unpack_from('<I', parameters)[0])
            w32(cpu, STACK + 4, struct.unpack_from('<I', parameters, 4)[0])
            w32(cpu, STACK + 8, struct.unpack_from('<I', parameters, 8)[0])
        if mode == 13:
            cpu.reg_write(UC_X86_REG_EAX, RECORDS + 0x2000)
            cpu.reg_write(UC_X86_REG_ESI, RECORDS + 0x70)
            w32(cpu, STACK + 4, struct.unpack_from('<I', parameters, 0xC)[0])
            w32(cpu, STACK + 8, struct.unpack_from('<I', parameters, 0x10)[0])
        if mode == 14:
            cpu.mem_write(RECORDS + 0x70, bytes(parameters[:8]))
            cpu.reg_write(UC_X86_REG_ESI, RECORDS + 0x70)
            w32(cpu, STACK + 4, struct.unpack_from('<I', parameters, 8)[0])
        cpu.emu_start([0x6DC10, 0x69660, 0x69910, 0x7C7D0, 0x7C580, 0x7C4C0, 0x7CCD0, 0x134510, 0x7B800, 0x29C80, 0x109260, 0x135B60, 0x100E90, 0x1060F0, 0x105DD0][mode], STOP, count=10000)
        if mode == 12:
            w32(cpu, RECORDS + 0x70, cpu.reg_read(UC_X86_REG_XMM0) & 0xFFFFFFFF)
        if mode == 10:
            cpu.mem_write(RECORDS + 0x800, bytes(cpu.mem_read(0x507C98, 0x80)))
        wanted = [1 if mode in [5, 6, 7, 10, 12, 13, 14] else cpu.reg_read(UC_X86_REG_EAX), struct.unpack('<I', cpu.mem_read(0x54DD7C, 4))[0],
            struct.unpack('<I', cpu.mem_read(RECORDS + 0x4000, 4))[0], *cpu.mem_read(RECORDS, len(data))]
        expected.append(wanted)
        lines.append(' '.join(map(str, [mode, state, slot, *parameters, *data])))
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    result = subprocess.run([harness('scene_factory_classes_harness')], input=str(count) + '\n' + '\n'.join(lines) + '\n',
        text=True, capture_output=True, check=True, env=environment)
    actual = result.stdout.splitlines()
    bad = abs(len(actual) - count)
    for index, (line, wanted) in enumerate(zip(actual, expected)):
        got = list(map(int, line.split()))
        if got != wanted:
            if bad < 5:
                print('case', index, 'mode', index % 15, 'diff', [i for i, pair in enumerate(zip(got, wanted)) if pair[0] != pair[1]][:20])
                if index % 15 == 4:
                    for offset in [0x28, 0x2C, 0x34, 0x38]:
                        first, second = bytes(got[3 + offset:7 + offset]), bytes(wanted[3 + offset:7 + offset])
                        if first != second:
                            print(hex(offset), 'host', struct.unpack('<f', first)[0], 'retail', struct.unpack('<f', second)[0])
                if index % 15 == 13:
                    fixture = list(map(int, lines[index].split()))[3:3 + 0x1000]
                    print('offset inputs', struct.unpack_from('<5f', bytes(fixture)),
                        'host', struct.unpack('<3f', bytes(got[3 + 0x70:3 + 0x7C])),
                        'retail', struct.unpack('<3f', bytes(wanted[3 + 0x70:3 + 0x7C])))
            bad += 1
    print(count, 'cases;', bad, 'mismatches')
    return bool(bad)


if __name__ == '__main__':
    sys.exit(main())
