"""Retail scene factories and shared cinema controls/navigation primitives; compare seeded bytes."""
import os
import random
import struct
import subprocess
import sys
from scene_markers_oracle import Retail, RECORDS, XBE
from retail_cpu import ROOT, STOP, STACK, w32, harness
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_XMM0


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 23)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 3000
    lines, expected = [], []
    for case in range(count):
        mode = case % 27
        state, slot = rng.getrandbits(32), rng.getrandbits(32)
        parameters = bytearray(rng.getrandbits(8) for _ in range(0x1000))
        data = bytearray(rng.getrandbits(8) for _ in range(0xC6C))
        # Controller registration has separate native pointer coverage; no capture stubs here.
        struct.pack_into('<I', parameters, 0x334, 0)
        for offset in [0x54C, 0x5CC]:
            name = b'aid_' + bytes([65 + case % 27]) * rng.randrange(0, 120) + b'\0'
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
        if mode == 15:
            struct.pack_into('<H', parameters, 0x808, rng.choice([0x25, 0x26, 0x27]))
            struct.pack_into('<I', parameters, 0x608, RECORDS + 0x2000)
            struct.pack_into('<I', parameters, 0x610, rng.choice([0, RECORDS + 0x2400]))
            struct.pack_into('<I', parameters, 0x95C, RECORDS)
            struct.pack_into('<I', parameters, 0x88, 0)
            struct.pack_into('<I', data, 8, RECORDS + 0x2000)
            struct.pack_into('<H', data, 0x30, rng.randrange(2))
            struct.pack_into('<f', data, 0x34, rng.choice([0., -0., 1., float('nan')]))
            for offset in [8, 0xC, 0x10, 0x18, 0x420, 0x424, 0x428, 0x670, 0x674, 0x678, 0x810, 0x814, 0x818]:
                struct.pack_into('<f', parameters, offset, rng.uniform(-100, 100))
            for offset in [0x810, 0x81C]:
                struct.pack_into('<f', parameters, offset, rng.choice([0., -1., 180., rng.uniform(0, 360), float('nan')]))
        if mode == 16:
            struct.pack_into('<I', parameters, 0, rng.randrange(4))
            for offset in [0x88, 0x204]:
                struct.pack_into('<I', parameters, offset, 0)
            for offset in [8, 0xC, 0x10, 0x420, 0x424, 0x428, 0x440, 0x444, 0x448, 0x3C0, 0x3C4]:
                struct.pack_into('<f', parameters, offset, rng.choice([rng.uniform(-100, 100), 0., float('nan')]))
        if mode == 17:
            count_shapes = rng.randrange(-2, 9)
            struct.pack_into('<i', parameters, 0x800, count_shapes)
            for index in range(max(0, count_shapes)):
                offset = 0x810 + index * 0x20
                parameters[offset] = rng.randrange(6)
                struct.pack_into('<II', parameters, offset + 8, 0x20, rng.choice([0x1235, 0, 0x1234]))
        if mode in [18, 19]:
            struct.pack_into('<I', parameters, 0, rng.randrange(4 if mode == 18 else 3))
            parameters[0x400] = rng.choice([0, 3])
            struct.pack_into('<H', parameters, 0x408, 0xFFFF)
            struct.pack_into('<I', parameters, 0x3C4, rng.randrange(2))
            struct.pack_into('<f', parameters, 0x3C0, rng.uniform(.1, 3))
            for offset in [0x410, 0x414, 0x418, 0x41C, 0x600, 0x604, 0x608, 0x620, 0x624, 0x628]:
                struct.pack_into('<f', parameters, offset, rng.uniform(-3, 3))
            for offset in [0x500, 0x540]:
                matrix = [1,0,0,0,0,1,0,0,0,0,1,0]
                if rng.randrange(2):
                    matrix = [rng.uniform(-2, 2) for _ in range(12)]
                struct.pack_into('<12f', parameters, offset, *matrix)
            for offset in [0x700, 0x704, 0x708]:
                struct.pack_into('<f', data, offset, rng.uniform(-3, 3))
        if mode == 19:
            struct.pack_into('<III', parameters, 0x3A0, rng.getrandbits(32), rng.getrandbits(32), rng.getrandbits(32))
            struct.pack_into('<I', parameters, 0x3B0, rng.getrandbits(32))
            count_shapes = rng.randrange(0, 4)
            struct.pack_into('<i', parameters, 0x800, count_shapes)
            for index in range(count_shapes):
                offset = 0x808 + index * 0x24
                parameters[offset:offset + 0x24] = parameters[0x400:0x424]
                parameters[offset] = rng.randrange(4)
                parameters[offset + 1] = 0 if index == count_shapes - 1 else 0x24
                struct.pack_into('<h', parameters, offset + 2, rng.choice([-1, 0, 1]))
                struct.pack_into('<I', parameters, offset + 4, rng.getrandbits(32))
        if mode == 20:
            subtype = rng.randrange(4)
            struct.pack_into('<I', parameters, 0, subtype)
            for offset in list(range(0x400, 0x424, 4)) + list(range(0x500, 0x530, 4)) + list(range(0x540, 0x570, 4)) + [0x600,0x604,0x608,0x620,0x624,0x628,0x640,0x644,0x648,0x3C0]:
                struct.pack_into('<f', parameters, offset, rng.uniform(-10, 10))
            struct.pack_into('<I', parameters, 0x3C4, rng.randrange(2))
            if subtype == 0:
                for axis in range(3):
                    struct.pack_into('<h', parameters, 0x810 + axis * 2, rng.randrange(-5, 2))
                    struct.pack_into('<h', parameters, 0x816 + axis * 2, rng.randrange(2, 12))
                    struct.pack_into('<h', parameters, 0x824 + axis * 2, rng.randrange(-10, 11))
                struct.pack_into('<H', parameters, 0x822, rng.randrange(1, 10))
            else:
                struct.pack_into('<III', parameters, 0x800, 0, 1, 2)
        if mode == 21:
            mesh_subtype = rng.randrange(6)
            struct.pack_into('<I', parameters, 0, mesh_subtype)
            for offset in list(range(0x400, 0x424, 4)) + [0x600,0x604,0x608]:
                struct.pack_into('<f', parameters, offset, rng.uniform(-3, 3))
            for offset in [0x500, 0x540]:
                struct.pack_into('<12f', parameters, offset, 1,0,0,0,0,1,0,0,0,0,1,0)
            for offset in [0x700,0x704,0x708]:
                struct.pack_into('<f', data, offset, rng.uniform(-3, 3))
            struct.pack_into('<f', parameters, 0x3C0, rng.uniform(.01, 2))
            struct.pack_into('<I', parameters, 0x3C4, rng.randrange(2))
            struct.pack_into('<II', parameters, 0x3B0, 0xFFFFFFFF, 0)
            parameters[0x800:0x834] = bytes(0x34)
            struct.pack_into('<HHHH', parameters, 0x81C, 1,1,1,8)
            struct.pack_into('<II', parameters, 0x82C, 1, RECORDS + 0x2850)
            struct.pack_into('<III', parameters, 0x850, 0,1,2)
            struct.pack_into('<I', parameters, 0x85C, rng.getrandbits(32))
            if mesh_subtype >= 3:
                struct.pack_into('<III', parameters, 0x810, 1, RECORDS + 0x2850, 0)
        if mode == 22:
            struct.pack_into('<I', parameters, 0, rng.randrange(2))
            for offset in list(range(0x400,0x424,4)) + [0x600,0x604,0x608,0x640,0x644,0x648,0x3C0]:
                struct.pack_into('<f', parameters, offset, rng.choice([0., rng.uniform(-3,3), rng.uniform(-3,3), rng.uniform(-3,3)]))
        if mode == 23:
            struct.pack_into('<I', parameters, 0, rng.randrange(4))
            for offset in [8,0xC,0x10,0x34,0x38,0x1C4,0x1C8,0x1CC,0x420,0x424,0x428,0x600,0x604,0x608] + list(range(0x404,0x420,4)) + list(range(0x444,0x460,4)):
                struct.pack_into('<f', parameters, offset, rng.uniform(-10,10))
            for offset in [0x700,0x704,0x708]:
                struct.pack_into('<f', data, offset, rng.uniform(-10,10))
            struct.pack_into('<I', parameters, 0x88, 0)
        if mode == 24:
            struct.pack_into('<II', parameters, 0, 0x800, rng.choice([1,1,1,0]))
            struct.pack_into('<II', parameters, 0x800, 4, 0x900)
            struct.pack_into('<I', parameters, 0x88, 0)
            struct.pack_into('<3f', parameters, 8, 0,0,0)
            struct.pack_into('<2f', parameters, 0x34, 1,1)
            struct.pack_into('<3f', parameters, 0x1C4, 20,20,-10)
            struct.pack_into('<I', parameters, 0x118, rng.getrandbits(32))
            struct.pack_into('<I', parameters, 0x3A0, rng.getrandbits(32))
            struct.pack_into('<II', parameters, 0x3C0, rng.randrange(4), rng.choice([1,1,1,0]))
            struct.pack_into('<12f', parameters, 0x500, 1,0,0,0,0,1,0,0,0,0,1,0)
            for offset in [0x400,0x440,0x460,0x480]:
                parameters[offset:offset+0x20] = bytes(0x20)
                parameters[offset:offset+3] = bytes([4,0x20,1])
                struct.pack_into('<3f', parameters, offset+4, rng.uniform(-2,2),rng.uniform(-2,2),rng.uniform(-2,2))
                struct.pack_into('<f', parameters, offset+0x14, rng.uniform(.01,4))
                struct.pack_into('<f', parameters, offset+0x1C, rng.uniform(.01,4))
            nodes = rng.randrange(1,4)
            struct.pack_into('<I', parameters, 0x900, nodes)
            for index in range(nodes):
                offset = 0x908 + index * 0x30
                parameters[offset:offset+0x30] = bytes(0x30)
                parameters[offset] = rng.choice([0,0,1,3])
                parameters[offset+1] = 0x30 if index + 1 < nodes else 0
                struct.pack_into('<h', parameters, offset+2, 0)
                struct.pack_into('<I', parameters, offset+4, rng.getrandbits(32))
                struct.pack_into('<H', parameters, offset+8, 0xFFFF)
                for field in [0x10,0x14,0x18,0x1C,0x20]:
                    struct.pack_into('<f', parameters, offset+field, rng.uniform(.01,3))
                struct.pack_into('<3f', parameters, offset+0x24, 0,1,0)
            for offset in [0x700,0x704,0x708,0x740,0x744,0x748]:
                struct.pack_into('<f', data, offset, rng.uniform(-3,3))
        if mode == 25:
            struct.pack_into('<II', parameters, 0, rng.randrange(5), rng.randrange(2))
            for offset in [0x680,0x684,0x688,0x6A0,0x6A4,0x6A8,0x600,0x604,0x608,0x520,0x524,0x528,0x540,0x544,0x548]:
                struct.pack_into('<f', parameters, offset, rng.uniform(-4,4))
            struct.pack_into('<2f', parameters, 0x3C0, rng.uniform(.01,4),rng.uniform(.01,4))
            for offset in [0x400,0x440]:
                parameters[offset:offset+0x20] = bytes(0x20)
                parameters[offset:offset+3] = bytes([4,0x20,1])
                struct.pack_into('<3f', parameters, offset+4, rng.uniform(-2,2),rng.uniform(-2,2),rng.uniform(-2,2))
                struct.pack_into('<f', parameters, offset+0x14, rng.uniform(.01,4))
                struct.pack_into('<f', parameters, offset+0x1C, rng.uniform(.01,4))
            for offset in [0x700,0x704,0x708,0x740,0x744,0x748]:
                struct.pack_into('<f', data, offset, rng.uniform(-3,3))
            if struct.unpack_from('<I',parameters)[0] == 0:
                vector = rng.choice([(0.,0.,0.),(1.,0.,0.),(0.,1.,0.),(0.,0.,1.),
                    (1.000004,0.,0.),(1e-6,0.,0.),struct.unpack_from('<3f',parameters,0x680)])
                struct.pack_into('<3f',parameters,0x680,*vector)
            elif struct.unpack_from('<I',parameters)[0] == 1:
                value = rng.uniform(-2,2)
                struct.pack_into('<2f',parameters,0x3C0,value,value+rng.choice([0.,1e-6,1e-5,2e-5,-1e-6,-1e-5,-2e-5]))
        if mode == 26:
            struct.pack_into('<II',parameters,0,rng.choice([0xB6,0x7D,0xB8]),rng.randrange(9))
            for row in range(struct.unpack_from('<I',parameters,4)[0]):
                struct.pack_into('<I',parameters,0x400+row*4,rng.choice([0xB6,0x7D,0xB8,0xB9]))
        cpu = Retail({}).u
        if mode == 25:
            cpu.mem_map(0x00180000, 0x20000)
            cpu.mem_write(0x00184600, XBE(0x00184600, 0xE40))
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
        if mode == 15:
            w32(cpu, STACK + 4, RECORDS + 0x2600)
            w32(cpu, STACK + 8, RECORDS + 0x2800)
        entry = [0x6DC10, 0x69660, 0x69910, 0x7C7D0, 0x7C580, 0x7C4C0, 0x7CCD0, 0x134510, 0x7B800, 0x29C80, 0x109260, 0x135B60, 0x100E90, 0x1060F0, 0x105DD0, 0x7C0E0, 0, 0x559F0, 0, 0, 0, 0, 0, 0, 0, 0, 0][mode]
        if mode == 16:
            subtype = struct.unpack_from('<I', parameters)[0]
            entry = [0x15A270, 0x3E4A0, 0x3E590, 0x3E680][subtype]
            if subtype == 0:
                args = [RECORDS + 0x2008, struct.unpack_from('<I', parameters, 0x3C4)[0],
                    RECORDS + 0x2420, struct.unpack_from('<I', parameters, 0x3C0)[0]]
                for index, word in enumerate(args):
                    w32(cpu, STACK + 4 + index * 4, word)
            else:
                cpu.reg_write(UC_X86_REG_EAX, RECORDS + 0x2000)
                cpu.reg_write(UC_X86_REG_EBX, RECORDS + 0x2420)
                w32(cpu, STACK + 4, RECORDS + (0x2420 if subtype == 3 else 0x2440))
                w32(cpu, STACK + 8, struct.unpack_from('<I', parameters, 0x3C0)[0])
        if mode == 17:
            cpu.reg_write(UC_X86_REG_EDX, RECORDS + 0x2800)
        if mode == 18:
            subtype = struct.unpack_from('<I', parameters)[0]
            entry = [0x55CB0, 0x55F80, 0x56580, 0x563F0][subtype]
            radius = struct.unpack_from('<I', parameters, 0x3C0)[0]
            record = RECORDS if struct.unpack_from('<I', parameters, 0x3C4)[0] else 0
            if subtype < 3:
                args = [RECORDS + 0x2400, RECORDS + 0x2500, RECORDS + 0x2540, 0, RECORDS + 0x2600]
                args += [radius, 0] if subtype == 2 else [RECORDS + 0x700]
                if subtype == 0:
                    args += [record]
                if subtype == 1:
                    args += [radius, record]
                for index, word in enumerate(args):
                    w32(cpu, STACK + 4 + index * 4, word)
            else:
                cpu.reg_write(UC_X86_REG_ESI, RECORDS)
                cpu.reg_write(UC_X86_REG_EDI, RECORDS + 0x2500)
                cpu.reg_write(UC_X86_REG_ECX, RECORDS + 0x2600)
                cpu.reg_write(UC_X86_REG_EAX, RECORDS + 0x2620)
                w32(cpu, STACK + 4, RECORDS + 0x2400)
                w32(cpu, STACK + 8, radius)
            cpu.mem_write(0x50DD80, bytes(12))
        if mode == 19:
            subtype = struct.unpack_from('<I', parameters)[0]
            cpu.mem_write(0x423858, XBE(0x423858, 48))
            entry = [0x568E0, 0x56A60, 0x56BE0][subtype]
            mask = struct.unpack_from('<I', parameters, 0x3B0)[0]
            radius = struct.unpack_from('<I', parameters, 0x3C0)[0]
            record = RECORDS if struct.unpack_from('<I', parameters, 0x3C4)[0] else 0
            if subtype == 2:
                cpu.reg_write(UC_X86_REG_EBX, RECORDS + 0x2800)
                args = [RECORDS + 0x2500, RECORDS + 0x2540, 0, RECORDS + 0x2600,
                    radius, mask, RECORDS + 0x23A4, 0]
            else:
                cpu.reg_write(UC_X86_REG_EBX, RECORDS + 0x2600)
                cpu.reg_write(UC_X86_REG_EDI, RECORDS + 0x700)
                args = [RECORDS + 0x2800, RECORDS + 0x2500, RECORDS + 0x2540, 0]
                if subtype == 1:
                    args += [radius]
                args += [mask, RECORDS + 0x23A4, record]
            for index, word in enumerate(args):
                w32(cpu, STACK + 4 + index * 4, word)
            cpu.mem_write(0x50DD80, bytes(12))
        if mode == 20:
            subtype = struct.unpack_from('<I', parameters)[0]
            radius = struct.unpack_from('<I', parameters, 0x3C0)[0]
            entry = [0x54340, 0x103400, 0x54170, 0x54DE0][subtype]
            if subtype == 0:
                cpu.reg_write(UC_X86_REG_EAX, RECORDS + 0x2600)
                cpu.reg_write(UC_X86_REG_ECX, RECORDS + 0x2620)
                cpu.reg_write(UC_X86_REG_EDI, RECORDS + 0x700)
                cpu.reg_write(UC_X86_REG_EBX, RECORDS + 0x70C)
                args = [RECORDS + 0x2800, radius]
            elif subtype == 1:
                cpu.reg_write(UC_X86_REG_ECX, RECORDS + 0x2620)
                cpu.reg_write(UC_X86_REG_EDX, RECORDS + 0x2418)
                cpu.reg_write(UC_X86_REG_ESI, RECORDS + 0x240C)
                cpu.reg_write(UC_X86_REG_EDI, RECORDS + 0x2600)
                args = [RECORDS + 0x2400, RECORDS + 0x700]
            elif subtype == 2:
                cpu.reg_write(UC_X86_REG_EAX, RECORDS + 0x2400)
                cpu.reg_write(UC_X86_REG_ECX, RECORDS + 0x2640)
                cpu.reg_write(UC_X86_REG_EDX, RECORDS + 0x2600)
                cpu.reg_write(UC_X86_REG_ESI, RECORDS)
                cpu.reg_write(UC_X86_REG_XMM0, 0)
                from unicorn.x86_const import UC_X86_REG_XMM1
                cpu.reg_write(UC_X86_REG_XMM1, radius)
                args = [RECORDS + 0x2800, RECORDS + 0x2500]
            else:
                cpu.reg_write(UC_X86_REG_EAX, RECORDS + 0x2640)
                cpu.reg_write(UC_X86_REG_ECX, RECORDS + 0x2540 if struct.unpack_from('<I', parameters, 0x3C4)[0] else 0)
                cpu.reg_write(UC_X86_REG_EDX, RECORDS + 0x2400)
                cpu.reg_write(UC_X86_REG_EDI, RECORDS)
                from unicorn.x86_const import UC_X86_REG_XMM1
                cpu.reg_write(UC_X86_REG_XMM1, radius)
                args = [RECORDS + 0x2800, RECORDS + 0x2500, RECORDS + 0x2600]
            for index, word in enumerate(args):
                w32(cpu, STACK + 4 + index * 4, word)
        if mode == 21:
            subtype = struct.unpack_from('<I', parameters)[0]
            entry = [0x53720, 0x53AA0, 0x53F20, 0x54520, 0x54850, 0x54C40][subtype]
            record = RECORDS if struct.unpack_from('<I', parameters, 0x3C4)[0] else 0
            args = [RECORDS + 0x2800, RECORDS + 0x2500, RECORDS + 0x2540, 0,
                RECORDS + 0x2400, RECORDS + 0x2600]
            if subtype in [0,1,3,4]:
                args += [RECORDS + 0x700]
            if subtype in [1,2,4,5]:
                args += [struct.unpack_from('<I', parameters, 0x3C0)[0]]
            args += [0xFFFFFFFF, 0, 0 if subtype in [2,5] else record]
            for index, word in enumerate(args):
                w32(cpu, STACK + 4 + index * 4, word)
            cpu.mem_write(0x50DD80, bytes(12))
        if mode == 26:
            entry = 0x116890
            nodes = struct.unpack_from('<I',parameters,4)[0]
            w32(cpu,0x50E4DC,RECORDS+0x6000 if nodes else 0)
            for row in range(nodes):
                w32(cpu,RECORDS+0x6004+row*0x20,RECORDS+0x6000+(row+1)*0x20 if row+1<nodes else 0)
                w32(cpu,RECORDS+0x6008+row*0x20,struct.unpack_from('<I',parameters,0x400+row*4)[0])
            cpu.reg_write(UC_X86_REG_ECX,struct.unpack_from('<I',parameters)[0])
        if mode == 25:
            subtype = struct.unpack_from('<I',parameters)[0]
            entry = [0x184F99,0x184600,0x184EDD,0x120BA0,0x120950][subtype]
            radius,fixed_radius = struct.unpack_from('<II',parameters,0x3C0)
            if subtype == 0:
                args = [RECORDS+0x700,RECORDS+0x2680]
            elif subtype == 1:
                args = [radius,fixed_radius]
            elif subtype == 2:
                cpu.reg_write(UC_X86_REG_ECX, RECORDS+0x2680)
                args = [RECORDS+0x700,radius]
            elif subtype == 3:
                cpu.reg_write(UC_X86_REG_ESI, RECORDS+0x2680)
                cpu.reg_write(UC_X86_REG_EAX, RECORDS+0x26A0)
                cpu.reg_write(UC_X86_REG_EDI, RECORDS+0x700)
                args = [radius,fixed_radius,RECORDS+0x2600]
                cpu.mem_write(0x50DD80,bytes(12))
            else:
                cpu.mem_write(RECORDS+0x6008,bytes(parameters[0x520:0x52C]))
                cpu.mem_write(RECORDS+0x6108,bytes(parameters[0x540:0x54C]))
                w32(cpu,RECORDS+0x6088,0)
                w32(cpu,RECORDS+0x6188,0)
                w32(cpu,0x511188,0)
                cpu.mem_write(0x5111AC,bytes(data[0x740:0x74C]))
                cpu.mem_write(0x50DD80,bytes(12))
                args = [RECORDS+0x6000,RECORDS+0x2400,RECORDS+0x6100,RECORDS+0x2440,
                    RECORDS+0x2600 if struct.unpack_from('<I',parameters,4)[0] else 0,RECORDS+0x2680,RECORDS+0x700]
            for index,word in enumerate(args):
                w32(cpu,STACK+4+index*4,word)
        if mode == 24:
            entry = 0x3C0F0
            w32(cpu, RECORDS + 0x2000, RECORDS + 0x2800)
            w32(cpu, RECORDS + 0x2804, RECORDS + 0x2900)
            w32(cpu, RECORDS + 0x2204, RECORDS + 0x5000 if struct.unpack_from('<I',parameters,0x3C4)[0] else 0)
            w32(cpu, RECORDS + 0x5000, RECORDS + 0x2000)
            w32(cpu, RECORDS + 0x5164, 0)
            w32(cpu, RECORDS + 0x5168, 0)
            cpu.mem_write(RECORDS + 0x516C, bytes(parameters[0x3A0:0x3A4]))
            cpu.mem_write(RECORDS + 0x22AC, bytes(parameters[0x500:0x530]))
            w32(cpu, 0x511188, 0)
            cpu.mem_write(0x5111AC, bytes(data[0x740:0x74C]))
            args = [RECORDS+0x2000,0,0,RECORDS+0x2400,RECORDS+0x2440,
                struct.unpack_from('<I',parameters,0x3C0)[0],RECORDS+0x700]
            for index,word in enumerate(args):
                w32(cpu,STACK+4+index*4,word)
        if mode == 23:
            subtype = struct.unpack_from('<I', parameters)[0]
            entry = [0x3DD50,0x1206A0,0x418B0,0x3C470][subtype]
            cpu.mem_write(RECORDS + 0x6008, bytes(parameters[0x420:0x42C]))
            w32(cpu, RECORDS + 0x6088, 0)
            if subtype == 0:
                cpu.reg_write(UC_X86_REG_ECX, RECORDS + 0x2000)
                cpu.reg_write(UC_X86_REG_EAX, RECORDS + 0x700)
                w32(cpu, STACK + 4, RECORDS + 0x70C)
                w32(cpu, STACK + 8, RECORDS + 0x710)
            elif subtype == 1:
                cpu.reg_write(UC_X86_REG_EAX, RECORDS + 0x2400)
                cpu.reg_write(UC_X86_REG_ECX, RECORDS + 0x2440)
                cpu.reg_write(UC_X86_REG_EDX, RECORDS + 0x2600)
            elif subtype == 2:
                cpu.reg_write(UC_X86_REG_EAX, RECORDS + 0x2000)
                cpu.reg_write(UC_X86_REG_EBX, RECORDS + 0x700)
                w32(cpu, STACK + 4, RECORDS + 0x6000)
            else:
                cpu.reg_write(UC_X86_REG_EBX, RECORDS + 0x2400)
                w32(cpu, STACK + 4, RECORDS + 0x2000)
                w32(cpu, STACK + 8, RECORDS + 0x6000)
                w32(cpu, STACK + 12, RECORDS + 0x700)
        if mode == 22:
            subtype = struct.unpack_from('<I', parameters)[0]
            entry = [0x15A2E0, 0x15A440][subtype]
            if subtype == 0:
                cpu.reg_write(UC_X86_REG_EBX, RECORDS + 0x2600)
                cpu.reg_write(UC_X86_REG_ESI, RECORDS + 0x240C)
                cpu.reg_write(UC_X86_REG_EDI, RECORDS + 0x2418)
                args = [RECORDS + 0x2640]
            else:
                args = [RECORDS + 0x2600, struct.unpack_from('<I', parameters, 0x3C0)[0],
                    RECORDS + 0x2400, RECORDS + 0x240C, RECORDS + 0x2418]
            for index, word in enumerate(args):
                w32(cpu, STACK + 4 + index * 4, word)
        try:
            cpu.emu_start(entry, STOP, count=10000)
            if cpu.reg_read(UC_X86_REG_EIP) != STOP:
                raise RuntimeError('retail call did not return before its instruction budget')
        except Exception:
            print('retail failure', case, mode, subtype, hex(entry), hex(cpu.reg_read(UC_X86_REG_EIP)), hex(cpu.reg_read(UC_X86_REG_ESP)))
            raise
        if mode == 24 or (mode == 25 and subtype == 4):
            cpu.mem_write(RECORDS + 0x740, bytes(cpu.mem_read(0x5111AC,12)))
        if mode == 12:
            w32(cpu, RECORDS + 0x70, cpu.reg_read(UC_X86_REG_XMM0) & 0xFFFFFFFF)
        if mode == 10:
            cpu.mem_write(RECORDS + 0x800, bytes(cpu.mem_read(0x507C98, 0x80)))
        wanted = [1 if mode in [5, 6, 7, 10, 12, 13, 14] or (mode == 18 and subtype == 3) or (mode == 20 and subtype != 1) or (mode == 23 and subtype in [0,2]) or (mode == 25 and subtype in [0,2,3]) else int(cpu.reg_read(UC_X86_REG_EAX) != 0) if mode == 26 else cpu.reg_read(UC_X86_REG_EAX), struct.unpack('<I', cpu.mem_read(0x54DD7C, 4))[0],
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
                print('case', index, 'mode', index % 27, 'diff', [i for i, pair in enumerate(zip(got, wanted)) if pair[0] != pair[1]][:20])
                if index % 27 == 25:
                    print('push result',struct.unpack('<3f',bytes(got[3+0x700:3+0x70C])),struct.unpack('<3f',bytes(wanted[3+0x700:3+0x70C])))
                if index % 27 == 4:
                    for offset in [0x28, 0x2C, 0x34, 0x38]:
                        first, second = bytes(got[3 + offset:7 + offset]), bytes(wanted[3 + offset:7 + offset])
                        if first != second:
                            print(hex(offset), 'host', struct.unpack('<f', first)[0], 'retail', struct.unpack('<f', second)[0])
                if index % 27 == 13:
                    fixture = list(map(int, lines[index].split()))[3:3 + 0x1000]
                    print('offset inputs', struct.unpack_from('<5f', bytes(fixture)),
                        'host', struct.unpack('<3f', bytes(got[3 + 0x70:3 + 0x7C])),
                        'retail', struct.unpack('<3f', bytes(wanted[3 + 0x70:3 + 0x7C])))
                if index % 27 == 19:
                    fixture = bytes(list(map(int, lines[index].split()))[3:3 + 0x1000])
                    print('dispatch subtype', struct.unpack_from('<I', fixture)[0], 'return', got[0], wanted[0],
                        'count', struct.unpack_from('<i', fixture, 0x800)[0],
                        'rows', [struct.unpack_from('<BBhI', fixture, 0x808 + row * 0x24) for row in range(max(0, struct.unpack_from('<i', fixture, 0x800)[0]))])
                if index % 27 == 18:
                    fixture = bytes(list(map(int, lines[index].split()))[3:3 + 0x1000])
                    print('sphere subtype', struct.unpack_from('<I', fixture)[0],
                        'end host', struct.unpack('<3f', bytes(got[3 + 0x700:3 + 0x70C])),
                        'retail', struct.unpack('<3f', bytes(wanted[3 + 0x700:3 + 0x70C])))
            bad += 1
    print(count, 'cases;', bad, 'mismatches')
    return bool(bad)


if __name__ == '__main__':
    sys.exit(main())
