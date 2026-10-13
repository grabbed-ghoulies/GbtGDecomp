"""Retail scene factories and shared cinema controls/navigation primitives; compare seeded bytes."""
import os
import random
import struct
import subprocess
import sys
from scene_markers_oracle import Retail, RECORDS, XBE
from unicorn import UC_HOOK_CODE
from retail_cpu import ROOT, STOP, STACK, w32, harness
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_XMM0


def resource_hash(name):
    result = 0
    for character in name[4:].split(b'\0',1)[0]:
        character &= 0xDF
        result = ((result << 4)+character)&0xFFFFFFFF
        high = result&0xF0000000
        if high:
            result ^= high|(high>>24)
    return result


class ResourceRetail(Retail):
    def dispatch(self, uc, address, size, name):
        if name in ['acquire','release']:
            return
        super().dispatch(uc,address,size,name)

def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 23)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 3000
    lines, expected = [], []
    for case in range(count):
        mode = int(sys.argv[3]) if len(sys.argv) > 3 else case % 43
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
        if mode == 27:
            subtype = rng.randrange(7)
            struct.pack_into('<Ii f I III I ff f i i I', parameters, 0,
                subtype, rng.randrange(-2,17), rng.choice([-1.,0.,2.,10.,float('nan')]),
                rng.randrange(16), rng.randrange(2), rng.randrange(2), rng.randrange(2),
                rng.choice([0,1,0xFFF]), rng.choice([-100.,0.,100.,float('nan')]),
                rng.choice([-100.,0.,100.,float('nan')]), 123., -77, rng.randrange(9), rng.getrandbits(32))
            parameters[0x400:0x800] = XBE(0x2FCFB8,0x400)
            for cell in range(16):
                struct.pack_into('<hhh', parameters, 0x800+cell*18,
                    rng.randrange(-100,100),rng.randrange(-100,100),rng.randrange(-100,100))
                # Surface costs have 18 initialized retail entries; exercise their real values.
                parameters[0x800+cell*18+10] = rng.randrange(18)
                parameters[0x800+cell*18+11] = rng.randrange(18)
                if rng.randrange(2):
                    parameters[0x800+cell*18+7] &= ~2
                    struct.pack_into('<H',parameters,0x800+cell*18+12,0)
            struct.pack_into('<fI',parameters,0x38,rng.choice([-1000.,0.,1000.,float('nan')]),rng.randrange(9))
            for pair in range(8):
                struct.pack_into('<II',parameters,0x200+pair*8,rng.randrange(16),rng.randrange(16))
        if mode == 28:
            subtype = rng.randrange(4)
            width,depth = rng.randrange(1,5),rng.randrange(1,5)
            if subtype in [0,2] and rng.randrange(4) == 0:
                width = 0
            struct.pack_into('<IIII f',parameters,0,subtype,width,depth,rng.choice([1,8,16,32]),rng.uniform(100,1000))
            for cell in range(width*depth):
                coordinates = [rng.uniform(-100,100) for _ in range(3)]
                if subtype == 0 and rng.randrange(4) == 0:
                    coordinates[rng.randrange(3)] = float('nan')
                struct.pack_into('<3f',parameters,0x800+cell*16,*coordinates)
        if mode == 26:
            struct.pack_into('<II',parameters,0,rng.choice([0xB6,0x7D,0xB8]),rng.randrange(9))
            for row in range(struct.unpack_from('<I',parameters,4)[0]):
                struct.pack_into('<I',parameters,0x400+row*4,rng.choice([0xB6,0x7D,0xB8,0xB9]))
        if mode == 29:
            configuration = rng.randrange(4)
            struct.pack_into('<If',parameters,0,configuration,rng.uniform(200,500))
            parameters[0x400:0x800] = XBE(0x2FCFB8,0x400)
            struct.pack_into('<II',parameters,0x800,0x20,4)
            for row,(kind,offset) in enumerate([(0xB,0x80 if configuration in [1,2] else 0),
                    (0x17,0x90 if configuration in [2,3] else 0),(0x19,0xA0),(0x1A,0xA8)]):
                struct.pack_into('<II',parameters,0x820+row*8,kind,offset)
            for floor in range(2):
                struct.pack_into('<4I',parameters,0x880+floor*16,4,4,16,0x100+floor*0x200)
                pairs = rng.randrange(9)
                struct.pack_into('<HHI',parameters,0x8A0+floor*8,0,pairs,0x500+floor*0x40)
                for pair in range(8):
                    struct.pack_into('<II',parameters,0xD00+floor*0x40+pair*8,rng.randrange(16),rng.randrange(16))
                for cell in range(16):
                    struct.pack_into('<3fBBH',parameters,0x900+floor*0x200+cell*16,
                        float((cell%4)*16),float(rng.randrange(-50,150)),float((cell//4)*16),
                        (rng.randrange(18)<<3)|rng.randrange(8),rng.getrandbits(8),rng.choice([0,0,0,rng.randrange(0x1000)]))
        if mode == 30:
            subtype = rng.randrange(6)
            struct.pack_into('<IIIII',parameters,0,subtype,rng.randrange(4),rng.randrange(2),0,rng.randrange(2))
            for axis in range(4):
                struct.pack_into('<f',parameters,0x20+axis*4,rng.choice([0.,.5,1.,-1.,2.]))
            struct.pack_into('<II',parameters,0x44,rng.randrange(2),rng.randrange(2))
            for offset in list(range(0x50,0x80,4))+list(range(0x100,0x180,4))+list(range(0x200,0x280,4)):
                struct.pack_into('<f',parameters,offset,rng.uniform(-2,2))
            for vertex in range(8):
                for axis in range(3):
                    struct.pack_into('<f',parameters,0x400+vertex*12+axis*4,
                        rng.choice([float('nan'),rng.uniform(-100,100)]) if subtype == 0 else rng.uniform(-100,100))
            for index in range(3):
                struct.pack_into('<I',parameters,0x380+index*4,rng.randrange(8))
            parameters[0x600:0x640] = bytes(0x40)
            struct.pack_into('<I',parameters,0x600,rng.randrange(2))
            parameters[0x610] = 2
            struct.pack_into('<I',parameters,0x614,rng.choice([0,rng.getrandbits(32)]))
            struct.pack_into('<II',parameters,0x618,0x30,rng.choice([0x1235,0]))
            event_count = rng.randrange(5)
            struct.pack_into('<I',parameters,0x620,event_count)
            for event in range(event_count):
                struct.pack_into('<3I',parameters,0x500+event*20,*[rng.randrange(8) for _ in range(3)])
            struct.pack_into('<II',parameters,0x800,0x20,2)
            struct.pack_into('<4I',parameters,0x820,5,rng.choice([0,0xA0]),2,rng.choice([0,0x100]))
            for offset in range(0xAC8,0xAE0,4):
                struct.pack_into('<f',parameters,offset,rng.uniform(-100,100))
            node_count = rng.randrange(9)
            struct.pack_into('<I',parameters,0x900,node_count)
            for node in range(node_count):
                struct.pack_into('<Hh',parameters,0x904+node*0x34,rng.choice([8,9,0x1F,3]),rng.choice([-2,-1,0,1]))
                for offset in [0x10,0x20,0x30]:
                    struct.pack_into('<f',parameters,0x904+node*0x34+offset,rng.uniform(-10,10))
            if subtype == 0:
                for offset in range(0x700,0x718,4):
                    struct.pack_into('<f',data,offset,rng.choice([float('nan'),rng.uniform(-100,100)]))
            if subtype == 2:
                struct.pack_into('<i',data,0x700,rng.randrange(6))
            if subtype == 3:
                struct.pack_into('<II',parameters,0x14,rng.randrange(9),rng.randrange(2))
            if subtype == 4:
                for row in range(2):
                    struct.pack_into('<II',parameters,0x820+row*8,rng.choice([0x11,2]),rng.choice([0,0xA0]))
            if subtype == 5:
                struct.pack_into('<I',parameters,0x14,rng.getrandbits(32))
        if mode == 31:
            subtype = rng.randrange(7)
            struct.pack_into('<IIII',parameters,0,subtype,rng.randrange(7),rng.randrange(2),rng.randrange(2))
            struct.pack_into('<i',parameters,0x18,rng.randrange(-1,4))
            struct.pack_into('<i',data,0x700,rng.randrange(4))
            for offset in range(0x704,0x718,4):
                struct.pack_into('<f',data,offset,rng.choice([-1.,0.,.3,.5,1.,1.2,2.,float('nan')]))
            colors = rng.choice([(0,0,0),(255,255,255),(127,127,127),(255,0,0),(0,255,0),(0,0,255),
                tuple(rng.randrange(256) for _ in range(3))])
            parameters[0x20:0x23] = bytes(colors)
            names = [b'aid_sector_alpha',b'aid_SECTOR_ALPHA',b'aid_sector_beta',b'none']
            query = rng.choice(names)
            parameters[0x380:0x380+len(query)+1] = query+b'\0'
            for row in range(3):
                offset = 0x100+row*0x8C
                parameters[offset:offset+0x80] = rng.choice(names).ljust(0x80,b'\0')
                struct.pack_into('<3f',parameters,offset+0x80,*[rng.choice([0.,.3,.7,1.,1.2,2.]) for _ in range(3)])
            for offset in [0,8,0x18,0x20]:
                struct.pack_into('<f',parameters,0x800+offset,rng.uniform(-20,20))
            struct.pack_into('<H',parameters,0x824,rng.randrange(5))
            struct.pack_into('<H',parameters,0x828,rng.randrange(5))
            if subtype == 4:
                kind = struct.unpack_from('<I',parameters,4)[0]
                matrix = [rng.uniform(-2,2) for _ in range(16)]
                if kind in [1,2,4,5]:
                    matrix = [0.]*16
                    if kind != 2:
                        for diagonal in [0,5,10,15]:
                            matrix[diagonal] = 1e-10 if kind == 4 else 1e10 if kind == 5 else 1.
                elif kind == 3:
                    matrix[4:8] = matrix[0:4]
                elif kind == 6:
                    matrix[4:8] = matrix[0:4]
                    matrix[5] += 2**-20
                struct.pack_into('<16f',parameters,0x400,*matrix)
            elif subtype == 5:
                struct.pack_into('<Q',parameters,0x400,rng.choice([rng.getrandbits(64),0x7FF0000000000000,
                    0xFFF0000000000000,0x7FF8000000000001,0x7FEFFFFFFFFFFFFF,0x0000000000000001]))
        if mode == 32:
            subtype = rng.randrange(4)
            points, edges = rng.randrange(1,17), rng.randrange(25)
            if subtype == 1:
                edges = max(2,edges)
            struct.pack_into('<III',parameters,0,subtype,points,edges)
            struct.pack_into('<I',data,0,points)
            struct.pack_into('<I',data,4,RECORDS+0x4000)
            struct.pack_into('<I',data,8,edges)
            struct.pack_into('<I',data,0x14,RECORDS+0x5000)
            data[0x18] = rng.randrange(2)
            for row in range(16):
                parameters[0x400+row*0x30+0x28] = rng.choice([0,0,0,rng.randrange(1,256)])
            for row in range(24):
                struct.pack_into('<II',parameters,0x800+row*0x1C,rng.randrange(points),rng.randrange(points))
        if mode == 33:
            subtype,flags = rng.randrange(4),rng.randrange(64)
            struct.pack_into('<6I',parameters,0,subtype,flags,rng.randrange(9),rng.randrange(9),rng.randrange(4),rng.randrange(256))
            struct.pack_into('<i',parameters,0x1C,rng.choice([0,0,1,-1]))
            parameters[0x20] = rng.randrange(256)
            for row in range(8):
                struct.pack_into('<II',parameters,0x400+row*8,rng.randrange(4),rng.getrandbits(32))
            if subtype in [0,3]:
                struct.pack_into('<I',data,0x9E4,RECORDS+0x6000 if flags&1 else 0)
            if subtype == 3:
                for offset,bit,address in [(0x9D8,2,0x7000),(0x9DC,4,0x8000),(0x940,8,0x9000),(0x94C,16,0xA000),(0x950,32,0xB000)]:
                    struct.pack_into('<I',data,offset,RECORDS+address if flags&bit else 0)
                links=struct.unpack_from('<I',parameters,0x10)[0]
                struct.pack_into('<III',data,0x934,RECORDS+0x4000 if links&1 else 0,RECORDS+0x4100 if links&2 else 0,struct.unpack_from('<I',parameters,0x1C)[0])
        if mode == 34:
            source_count = rng.randrange(2,65)
            struct.pack_into('<I',parameters,0,source_count)
            kind = case % 7
            for row in range(64):
                point = [rng.uniform(-20,20) for _ in range(3)]
                if kind == 0:
                    point = [0.,0.,0.]
                elif kind == 1:
                    point = [float(row),0.,0.]
                elif kind == 2:
                    point = [rng.choice([0.,.5,1.,2.]) for _ in range(3)]
                elif kind == 3:
                    point = [rng.uniform(-.1,.1) for _ in range(3)]
                elif kind == 4:
                    point[1] = 1.
                elif kind == 5:
                    point = [rng.uniform(-200,200) for _ in range(3)]
                struct.pack_into('<3f',parameters,0x400+row*12,*point)
        if mode == 35:
            subtype = rng.randrange(8)
            animation_rows = rng.randrange(4)
            matrix_count = rng.randrange(-1,animation_rows+1)
            bypass = int(subtype == 6)
            struct.pack_into('<I',parameters,0,subtype)
            struct.pack_into('<IiI',parameters,0xC,animation_rows,matrix_count,bypass)
            parameters[0x400:0xA00] = bytes(0x600)
            struct.pack_into('<II',parameters,0x400,0x100,1)
            struct.pack_into('<II',parameters,0x500,0x11,0x180)
            struct.pack_into('<I',parameters,0x588,0 if subtype == 4 else RECORDS+0x8800)
            struct.pack_into('<I',parameters,0x62C,bypass)
            parameters[0x701] = animation_rows
            struct.pack_into('<I',parameters,0x3EC,RECORDS+0xD000)
            for row in range(3):
                points = rng.randrange(0 if bypass else 2,9)
                struct.pack_into('<II',parameters,0x40+row*8,points,RECORDS+0x9000+row*0x1000)
                for index in range(8):
                    struct.pack_into('<3f',parameters,0xA00+row*0x80+index*12,*[rng.uniform(-10,10) for _ in range(3)])
                matrix = [rng.uniform(-2,2) for _ in range(16)]
                if case%3 == 0:
                    matrix = [0.]*16
                    for diagonal in [0,5,10,15]:
                        matrix[diagonal] = 1.
                elif case%3 == 1:
                    matrix[4:8] = matrix[0:4]
                struct.pack_into('<16f',parameters,0x100+row*64,*matrix)
        if mode == 36:
            subtype = rng.randrange(3)
            struct.pack_into('<IIHHiI',parameters,0,subtype,rng.randrange(2),rng.randrange(3),rng.randrange(3),rng.randrange(13),rng.randrange(2))
            rooms = [b'',b'room',b'aid_room_all',b'aid_room_ALL',b'aid_room_whitebackground',b'aid_room_WhiteBackground',
                b'aid_room_somewhere',b'aid_room_',b'_',b'aid_room_123',b'aid_room_{all}',b'aid_room_'+bytes([rng.randrange(33,256)])]
            room = rng.choice(rooms)
            parameters[0x100:0x100+len(room)+1] = room+b'\0'
            suffix = room.rsplit(b'_',1)[-1] if b'_' in room else b'(null)'
            aliases = {b'all':b'centralhallway',b'whitebackground':b'(storybook desk)'}
            suffix = aliases.get(suffix.lower(),suffix)
            first,second = struct.unpack_from('<HH',parameters,8)
            ordinal = struct.unpack_from('<H',parameters,4)[0]
            camera = b'aid_cutscene_ghoulies_doors_cameras_'+suffix+b'_'+str(ordinal).encode()+[b'open',b'noentry',b'ghouly'][first]+[b'open',b'noentry',b'ghouly'][second]
            parameters[0x200:0x280] = camera.lower()[:127].ljust(128,b'\0')
            if rng.randrange(2):
                shape = struct.unpack_from('<i',parameters,0xC)[0]
                variants = [b'open',b'noentry',b'ghouly']
                shapes = [f'{kind}_{height}_high{side}'.encode() for kind in ['square','round'] for height in [1,2,3] for side in ['right','left']]
                clip = (b'aid_cutscene_ghoulies_doors_'+suffix+b'_'+str(ordinal).encode() if shape == 12 else b'aid_cutscene_ghoulies_doors_'+shapes[shape]+b'_')+variants[first]+variants[second]
                parameters[0x200:0x280] = clip.lower()[:127].ljust(128,b'\0')
            for offset in range(0x400,0x40C,4):
                struct.pack_into('<f',parameters,offset,rng.uniform(-100,100))
            for offset in range(0x40C,0x418,4):
                struct.pack_into('<f',parameters,offset,rng.choice([0.,-0.,rng.uniform(-4,4)]))
            for offset in range(0x418,0x424,4):
                struct.pack_into('<f',parameters,offset,rng.choice([1.,1.,0.,-1.,rng.uniform(-2,2)]))
        if mode == 37:
            parameters[0x400:0x900] = bytes(0x500)
            enabled = rng.randrange(2)
            struct.pack_into('<I',parameters,0x400,0x40 if enabled else 0)
            aid_names = [b'aid_background_alpha',b'aid_BACKGROUND_ALPHA',b'aid_background_beta',b'AID_BACKGROUND_ALPHA',b'other',b'']
            query = rng.choice(aid_names)
            parameters[0x100:0x100+len(query)+1] = query+b'\0'
            for row in range(6):
                name = rng.choice(aid_names)
                parameters[0x440+row*0x80:0x4C0+row*0x80] = name.ljust(0x80,b'\0')
        if mode == 38:
            subtype = rng.randrange(6)
            struct.pack_into('<I',parameters,0,subtype)
            if subtype == 1:
                struct.pack_into('<II',parameters,4,rng.randrange(2),rng.randrange(2))
                struct.pack_into('<I',parameters,0x410,0x100)
            elif subtype == 2:
                struct.pack_into('<I',parameters,4,rng.randrange(5))
            elif subtype == 3:
                struct.pack_into('<iiHH',parameters,4,rng.randrange(-2,5),rng.randrange(-2,5),
                    rng.choice([0,1,2,0xFFF,0x1000,0xFFFF]),rng.getrandbits(16))
                for row in range(16):
                    struct.pack_into('<H',parameters,0x40C+row*18,rng.choice([0,1,2,0xFFF,rng.getrandbits(16)]))
            elif subtype in [4,5]:
                names = [b'unknown',b'part',b'PART',b'part_two',b'{part}',b'']
                struct.pack_into('<i',parameters,4,rng.randrange(-2,4) if subtype == 4 else rng.randrange(4))
                struct.pack_into('<IIi',parameters,8,rng.randrange(2),rng.randrange(2),rng.randrange(-2,4))
                query = rng.choice(names)
                parameters[0x100:0x180] = query.ljust(0x80,b'\0')
                table = 0x400 if subtype == 4 else 0x700
                for row in range(3):
                    parameters[table+row*0x22C:table+row*0x22C+0x80] = rng.choice(names).ljust(0x80,b'\0')
                    parameters[table+row*0x22C+0x1A0:table+row*0x22C+0x220] = (b'aid_fxrumble_fixture_'+str(row).encode()).ljust(0x80,b'\0')
                if subtype == 5:
                    parameters[0x400:0x600] = bytes(0x200)
                    struct.pack_into('<I',parameters,0x410,0x80)
                    struct.pack_into('<I',parameters,0x490,0x100 if struct.unpack_from('<I',parameters,0xC)[0] else 0)
                    struct.pack_into('<I',parameters,0x504,rng.choice([0,0x60]))
                    for row in range(4):
                        parameters[0x517+row*12] = rng.choice([0,16,32])
                    for row in range(3):
                        parameters[0x560+row*16:0x570+row*16] = rng.choice(names).ljust(16,b'\0')
                    parameters[0x300:0x380] = XBE(0x421E58,128).split(b'\0',1)[0].ljust(128,b'\0')
        if mode == 39:
            subtype = rng.randrange(8)
            struct.pack_into('<I',parameters,0,subtype)
            if subtype < 2:
                rows_count = rng.randrange(9)
                part = rng.randrange(8)
                struct.pack_into('<5I',parameters,4,rows_count,part,rng.randrange(2),rng.randrange(2),rng.randrange(2))
                struct.pack_into('<I',data,0x944,rows_count)
                struct.pack_into('<I',data,0x94C,RECORDS+0x8000)
                struct.pack_into('<I',data,0x204,RECORDS+0x6400 if struct.unpack_from('<I',parameters,0xC)[0] else 0)
                for row in range(8):
                    struct.pack_into('<Iif',parameters,0x400+row*0x9C,rng.randrange(8),rng.randrange(-3,260),rng.choice([0.,-0.,1.,float('nan')]))
                parameters[0x900:0xB00] = bytes(0x200)
                struct.pack_into('<I',parameters,0x910,0x80)
                struct.pack_into('<I',parameters,0x990,0x100 if struct.unpack_from('<I',parameters,0x14)[0] else 0)
                for row in range(8):
                    parameters[0xA19+row*12] = rng.choice([0,1,0xFF])
                    parameters[0xA1A+row*12] = rng.randrange(256)
            elif subtype == 2:
                struct.pack_into('<i',parameters,4,rng.randrange(-2,4))
                names = [b'unknown',b'part',b'PART',b'{part}',b'']
                parameters[0x100:0x180] = rng.choice(names).ljust(128,b'\0')
                for row in range(3):
                    parameters[0x400+row*0x22C:0x480+row*0x22C] = rng.choice(names).ljust(128,b'\0')
            elif subtype < 6:
                struct.pack_into('<iiI',parameters,4,rng.choice([0,1,-1]),rng.choice([0,1,-1]),rng.getrandbits(32))
                owner = data if subtype == 5 else parameters
                base = 0 if subtype == 5 else 0x400
                for offset in [0x98,0x9C,0x14C,0x150]:
                    struct.pack_into('<f',owner,base+offset,rng.choice([0.,-0.,-1.,.25,1.,100.,float('nan'),float('inf')]))
                struct.pack_into('<I',owner,base+0x148,rng.choice([0,1,2,8,0x7FFFFFFF,0x80000001,0xFFFFFFFF]))
                owner[base+0x154] = rng.randrange(32)
                for offset in [0xF4,0xF8,0xFC,0x100]:
                    struct.pack_into('<f',data,0x800+offset,rng.choice([0.,-0.,-5.,1.,5.,float('nan')]))
                struct.pack_into('<I',data,0x948,RECORDS+0x2400)
                if subtype == 5:
                    struct.pack_into('<h',data,6,rng.choice([-1,0,1,2,3]))
                    struct.pack_into('<I',data,0x13C,rng.choice([0,1,2,3,4]))
                    for row in range(4):
                        struct.pack_into('<h',data,0x390+row*0x150+0x5A,rng.choice([-1,0,1,2,3]))
            else:
                parameters[0x100:0x180] = b'aid_fx_background_fixture'.ljust(128,b'\0')
                struct.pack_into('<I',parameters,4,rng.randrange(2))
                struct.pack_into('<I',parameters,0x10,rng.randrange(1,8))
                struct.pack_into('<I',data,0x144,RECORDS+0x6800 if subtype == 6 and struct.unpack_from('<I',parameters,4)[0] else 0)
                struct.pack_into('<I',data,0x158,0)
                parameters[0x800:0x900] = bytes(0x100)
                commands = rng.randrange(4)
                for row in range(commands):
                    struct.pack_into('<IIII',parameters,0x800+row*16,0,rng.choice([0x1F41,0x1F5B]),rng.getrandbits(32),rng.getrandbits(32))
                # 1F41 has stride 8; use a fixed 16-byte 1F5B stream for wrapper fixtures.
                for row in range(commands):
                    struct.pack_into('<I',parameters,0x804+row*16,0x1F5B)
                struct.pack_into('<I',parameters,0x804+commands*16,rng.choice([0,0x1F5C]))
        if mode == 42:
            subtype = rng.randrange(3)
            struct.pack_into('<IIIII',parameters,0,subtype,rng.getrandbits(32),rng.randrange(4),rng.randrange(2),rng.choice([0,RECORDS+0x400]))
            if subtype == 2:
                for index in range(2):
                    obj = 0x400+index*0x80
                    struct.pack_into('<I',data,obj+4,RECORDS+0x480 if index == 0 else 0)
                    struct.pack_into('<I',data,obj+0x24,rng.choice([0,RECORDS+0x600+index*0x10]))
                    struct.pack_into('<I',data,0x600+index*0x10,rng.choice([0,RECORDS+0x620+index*0x20]))
                    struct.pack_into('<I',data,0x630+index*0x20,RECORDS+0x660+index*0x20)
                    shapes = 0x6A0 if index == 0 else 0x780
                    struct.pack_into('<I',data,0x664+index*0x20,rng.choice([0,RECORDS+shapes]))
                    struct.pack_into('<i',data,shapes,rng.randrange(-1,3))
                    for part in range(2):
                        shape = shapes+0x10+part*0x30
                        data[shape] = rng.choice([0,2])
                        struct.pack_into('<II',data,shape+8,0x30,rng.choice([0x1235,0,0x1234]))
                        events = 0x900+(index*2+part)*0x40
                        struct.pack_into('<II',data,shape+0x10,rng.randrange(4),RECORDS+events)
                        for event in range(3):
                            data[events+event*0x14+0x10] = rng.randrange(4)
        if mode == 41:
            subtype = rng.randrange(2)
            struct.pack_into('<IiI',parameters,0,subtype,rng.randrange(-2,5),rng.randrange(4))
            if subtype == 0:
                tag = rng.choice([0,1,5,0xFFF,0x1005,0xFFFF])
                struct.pack_into('<HH',parameters,8,tag,rng.randrange(0x10000))
                for row in range(64):
                    struct.pack_into('<H',data,row*0x12+0xC,rng.randrange(16)*0x1000+rng.choice([0,1,5,0xFFF]))
            else:
                parameters[0x400:0x62C] = bytes(0x22C)
                parameters[0x700:0x900] = bytes(0x200)
                parameters[0x417:0x41C] = b'part\0'
                parameters[0x400:0x408] = rng.choice([b'part\0\0\0\0',b'unknown\0',b'absent\0\0'])
                struct.pack_into('<ii',parameters,0x10,rng.randrange(-2,7),rng.randrange(-1,2))
                struct.pack_into('<I',parameters,0x704,rng.choice([0,0x100]))
                parameters[0x719] = rng.choice([0,1,3,15,31,0xFF])
                parameters[0x71A] = rng.randrange(256)
                parameters[0x800:0x805] = b'part\0'
                struct.pack_into('<i',data,4,rng.choice([-3,-1,0,1,254,255,256]))
        if mode == 40:
            struct.pack_into('<III',parameters,0,rng.randrange(2),rng.randrange(2),rng.randrange(2))
            for row in range(2):
                struct.pack_into('<i',data,row*0x48,-1)
                struct.pack_into('<I',data,row*0x48+0x44,rng.randrange(2))
                for offset in range(4,0x1C,4):
                    struct.pack_into('<f',data,row*0x48+offset,rng.uniform(-100,100))
            for offset in [0x400,0x500,0x540]:
                values = [rng.choice([-2.,-1.,.5,1.,2.]) if i in [0,5,10] else rng.uniform(-100,100) if i in [3,7,11] else 0. for i in range(12)]
                if offset == 0x400:
                    struct.pack_into('<12f',parameters,offset,*values)
                else:
                    struct.pack_into('<16f',parameters,offset,values[0],values[4],values[8],0.,values[1],values[5],values[9],0.,values[2],values[6],values[10],0.,values[3],values[7],values[11],1.)
            for offset in range(0x600,0x61C,4):
                struct.pack_into('<f',parameters,offset,rng.uniform(-100,100))
            struct.pack_into('<I',parameters,0x72C,RECORDS+0x8000)
        retail = ResourceRetail({}) if mode == 39 else Retail({})
        cpu = retail.u
        if mode == 39:
            # Unicorn picks the source for two quiet NaNs; native MULSS retains
            # the destination. Checked with F3 0F 59 C1 on this host CPU.
            particle_nan = [None]
            def correct_particle_nan(u,address,size,user):
                if address == 0xEFB06:
                    left = u.reg_read(UC_X86_REG_XMM0)&0xFFFFFFFF
                    right = struct.unpack('<I',u.mem_read(u.reg_read(UC_X86_REG_EAX)+0x98,4))[0]
                    is_nan = lambda bits: bits&0x7F800000 == 0x7F800000 and bits&0x7FFFFF != 0
                    particle_nan[0] = left|0x400000 if is_nan(left) and is_nan(right) else None
                elif particle_nan[0] is not None:
                    value = u.reg_read(UC_X86_REG_XMM0)
                    u.reg_write(UC_X86_REG_XMM0,(value&~0xFFFFFFFF)|particle_nan[0])
                    particle_nan[0] = None
            cpu.hook_add(UC_HOOK_CODE,correct_particle_nan,begin=0xEFB06,end=0xEFB06)
            cpu.hook_add(UC_HOOK_CODE,correct_particle_nan,begin=0xEFB0E,end=0xEFB0E)
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
        entry = [0x6DC10, 0x69660, 0x69910, 0x7C7D0, 0x7C580, 0x7C4C0, 0x7CCD0, 0x134510, 0x7B800, 0x29C80, 0x109260, 0x135B60, 0x100E90, 0x1060F0, 0x105DD0, 0x7C0E0, 0, 0x559F0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0][mode] if mode < 42 else 0
        if mode == 42:
            subtype = struct.unpack_from('<I',parameters,0)[0]
            callback = 0x12C930 if subtype == 0 or subtype == 2 and struct.unpack_from('<I',parameters,0xC)[0] else 0x12C950
            if subtype < 2:
                entry = callback
                w32(cpu,STACK+4,RECORDS)
                w32(cpu,STACK+8,struct.unpack_from('<I',parameters,4)[0])
            else:
                entry = 0x12C8A0
                cpu.mem_write(0x423000,XBE(0x423000,0x1000))
                w32(cpu,0x511AF4,struct.unpack_from('<I',parameters,0x10)[0])
                for index,value in enumerate([struct.unpack_from('<I',parameters,8)[0],callback,struct.unpack_from('<I',parameters,4)[0]]):
                    w32(cpu,STACK+4+index*4,value)
        if mode == 41:
            subtype = struct.unpack_from('<I',parameters,0)[0]
            if subtype == 0:
                entry = 0x1117E0
                w32(cpu,0x50E250,struct.unpack_from('<I',parameters,4)[0])
                for index in range(4):
                    grid = RECORDS+0x6000+index*0x20
                    w32(cpu,0x50E240+index*4,grid)
                    cpu.mem_write(grid,struct.pack('<iiIII',4,4,0,0,RECORDS+index*0x120))
                w32(cpu,STACK+4,struct.unpack_from('<H',parameters,8)[0])
                cpu.reg_write(UC_X86_REG_EBX,struct.unpack_from('<H',parameters,0xA)[0])
            else:
                entry = 0x450A0
                cpu.mem_write(RECORDS+0x6000,bytes(0x9E8))
                flags = struct.unpack_from('<I',parameters,8)[0]
                w32(cpu,RECORDS+0x6204,RECORDS+0x8000 if flags&1 else 0)
                w32(cpu,RECORDS+0x6310,RECORDS+0x8400 if flags&2 else 0)
                w32(cpu,RECORDS+0x694C,RECORDS)
                cpu.mem_write(RECORDS+0x8294,bytes(data[0x900:0x908]))
                cpu.mem_write(RECORDS+0x8694,bytes(data[0x910:0x918]))
                cpu.mem_write(0x5078D8,bytes(data[0x940:0x9C0]))
                w32(cpu,0x546290,struct.unpack_from('<I',parameters,0x10)[0])
                w32(cpu,0x546294,RECORDS+0x2400)
                w32(cpu,0x546298,struct.unpack_from('<I',parameters,0x14)[0])
                cpu.mem_write(RECORDS+0x9000,bytes(24))
                cpu.reg_write(UC_X86_REG_EDI,RECORDS+0x6000)
                cpu.reg_write(UC_X86_REG_EAX,0)
                cpu.reg_write(UC_X86_REG_ECX,0)
                cpu.reg_write(UC_X86_REG_ESI,RECORDS+0x9000)
                for index,value in enumerate([RECORDS+0x9000,0,RECORDS+0x2700,RECORDS+0x900C]):
                    w32(cpu,STACK+4+index*4,value)
        if mode == 40:
            entry = 0x40640
            cpu.mem_map(0x160000,0x20000)
            cpu.mem_write(0x160000,XBE(0x160000,0x20000))
            cpu.mem_write(RECORDS+0x6000,bytes(0x9E8))
            cpu.mem_write(RECORDS+0x62AC,bytes(parameters[0x400:0x430]))
            cpu.mem_write(RECORDS+0x6008,bytes(parameters[0x610:0x61C]))
            w32(cpu,RECORDS+0x6288,RECORDS)
            w32(cpu,RECORDS+0x6290,RECORDS+0x7000)
            cpu.mem_write(RECORDS+0x7000,bytes(parameters[0x700:0x730]))
            cpu.mem_write(RECORDS+0x8000,bytes(parameters[0x500:0x580]))
            cpu.mem_write(0x50D7A0,bytes(data[0x800:0x830]))
            cpu.mem_write(0x50DD80,bytes(parameters[0x600:0x60C]))
            w32(cpu,0x50DD70,RECORDS+0x840 if struct.unpack_from('<I',parameters,8)[0] else 0)
            cpu.reg_write(UC_X86_REG_EAX,struct.unpack_from('<I',parameters,0)[0])
            for index,value in enumerate([RECORDS+0x6000,RECORDS+0x700,RECORDS+0x710,struct.unpack_from('<I',parameters,4)[0]]):
                w32(cpu,STACK+4+index*4,value)
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
        if mode == 39:
            subtype = struct.unpack_from('<I',parameters)[0]
            entry = [0x44EA0,0x45CB0,0x44E70,0xEF8C0,0xEFB20,0xEFE30,0xED6E0,0xED990][subtype]
            if subtype < 2:
                cpu.mem_write(RECORDS+0x8000,bytes(parameters[0x400:0x8E0]))
                cpu.reg_write(UC_X86_REG_ECX,RECORDS)
                cpu.reg_write(UC_X86_REG_ESI,part)
                cpu.reg_write(UC_X86_REG_EAX,part)
                cpu.reg_write(UC_X86_REG_EDX,RECORDS)
                w32(cpu,RECORDS+0x6400,RECORDS+0x6500 if struct.unpack_from('<I',parameters,0x10)[0] else 0)
                w32(cpu,RECORDS+0x6510,RECORDS+0x6600)
                w32(cpu,RECORDS+0x6610,RECORDS+0x6700 if struct.unpack_from('<I',parameters,0x14)[0] else 0)
                cpu.mem_write(RECORDS+0x6700,bytes(parameters[0xA00:0xB00]))
            elif subtype == 2:
                w32(cpu,0x546294,RECORDS+0x2400)
                cpu.mem_write(0x546298,bytes(parameters[4:8]))
                cpu.reg_write(UC_X86_REG_EAX,RECORDS+0x2100)
            elif subtype < 6:
                cpu.reg_write(UC_X86_REG_EDX,RECORDS+0x800 if subtype == 3 else RECORDS+0x2400)
                cpu.reg_write(UC_X86_REG_ECX,RECORDS+0x800)
                cpu.reg_write(UC_X86_REG_ESI,RECORDS)
                if subtype == 5:
                    cpu.reg_write(UC_X86_REG_EDX,struct.unpack_from('<I',parameters,0xC)[0])
                cpu.mem_write(0x5096E0,bytes(parameters[4:8]))
                cpu.mem_write(0x5096E4,bytes(parameters[8:12]))
            else:
                for address,size in [(0x35E90,0x170),(0x35C00,0x80)]:
                    cpu.mem_write(address,XBE(address,size))
                w32(cpu,0x4DF250,RECORDS+0x6000)
                cpu.mem_write(RECORDS+0x6000,struct.pack('<HHIII',0x8C,1,RECORDS+0x6100,RECORDS+0x618C,RECORDS+0x618C))
                cpu.mem_write(RECORDS+0x6100,bytes(parameters[0x100:0x180]))
                cpu.mem_write(RECORDS+0x6180,struct.pack('<IiI',1,struct.unpack_from('<I',parameters,0x10)[0],resource_hash(bytes(parameters[0x100:0x180]))))
                w32(cpu,0x4DF09C,RECORDS+0x6200)
                cpu.mem_write(RECORDS+0x6200,struct.pack('<HHIII',0x18,1,RECORDS+0x6300,RECORDS+0x6318,RECORDS+0x6318))
                w32(cpu,RECORDS+0x6308,RECORDS+0x6800)
                cpu.mem_write(RECORDS+0x6800,bytes(parameters[0x800:0x900]))
                if subtype == 6:
                    cpu.reg_write(UC_X86_REG_EDI,RECORDS)
                else:
                    cpu.reg_write(UC_X86_REG_EAX,RECORDS+0x2100)
                    w32(cpu,STACK+4,RECORDS)
                    w32(cpu,STACK+8,0)
        if mode == 38:
            subtype = struct.unpack_from('<I',parameters)[0]
            entry = [0x42BA0,0x40BE0,0x2CBF0,0x36A00,0x44C20,0x46470][subtype]
            if subtype == 0:
                w32(cpu,STACK+4,RECORDS+0x800)
            elif subtype == 1:
                w32(cpu,RECORDS+0x204,RECORDS+0x6400 if struct.unpack_from('<I',parameters,4)[0] else 0)
                w32(cpu,RECORDS+0x6400,RECORDS+0x6000 if struct.unpack_from('<I',parameters,8)[0] else 0)
                w32(cpu,RECORDS+0x6010,RECORDS+0x6100)
                cpu.mem_write(RECORDS+0x6108,bytes(parameters[0x508:0x50C]))
                cpu.reg_write(UC_X86_REG_EAX,RECORDS)
            elif subtype == 2:
                nodes_count = struct.unpack_from('<I',parameters,4)[0]
                for index in range(6):
                    w32(cpu,RECORDS+0x4000+index*0x20,RECORDS+0x4000+(index-1)*0x20 if 0<index<=nodes_count else 0)
                    w32(cpu,RECORDS+0x4004+index*0x20,RECORDS+0x4000+(index+1)*0x20 if index<nodes_count else 0)
                w32(cpu,RECORDS+0x40A4,RECORDS+0x4000)
                cpu.reg_write(UC_X86_REG_EAX,RECORDS+0x4000)
                cpu.reg_write(UC_X86_REG_ECX,RECORDS+0x40A0)
            elif subtype == 3:
                cpu.mem_write(RECORDS+0x6000,bytes(parameters[4:12]))
                w32(cpu,RECORDS+0x6010,RECORDS+0x6400)
                cpu.mem_write(RECORDS+0x6400,bytes(parameters[0x400:0x520]))
                cpu.reg_write(UC_X86_REG_ESI,RECORDS+0x6000)
                w32(cpu,STACK+4,struct.unpack_from('<H',parameters,0xC)[0])
                w32(cpu,STACK+8,struct.unpack_from('<H',parameters,0xE)[0])
            elif subtype == 4:
                w32(cpu,0x546294,RECORDS+0x2400)
                cpu.mem_write(0x546298,bytes(parameters[4:8]))
                w32(cpu,STACK+4,RECORDS+0x2100)
            else:
                cpu.mem_write(0x421E58,bytes(parameters[0x300:0x380]))
                w32(cpu,RECORDS+0x204,RECORDS+0x6400 if struct.unpack_from('<I',parameters,8)[0] else 0)
                w32(cpu,RECORDS+0x6400,RECORDS+0x6000)
                w32(cpu,RECORDS+0x6010,RECORDS+0x6100)
                w32(cpu,RECORDS+0x6110,RECORDS+0x6200 if struct.unpack_from('<I',parameters,0xC)[0] else 0)
                cpu.mem_write(RECORDS+0x6200,bytes(parameters[0x500:0x600]))
                w32(cpu,0x546294,RECORDS+0x8000)
                cpu.mem_write(RECORDS+0x8000,bytes(parameters[0x700:0xD84]))
                cpu.mem_write(0x546298,bytes(parameters[0x10:0x14]))
                cpu.reg_write(UC_X86_REG_EAX,RECORDS)
                cpu.reg_write(UC_X86_REG_ESI,struct.unpack_from('<I',parameters,4)[0])
        if mode == 37:
            subtype = 0
            entry = 0x35A40
            cpu.reg_write(UC_X86_REG_EAX,RECORDS+0x2400)
            cpu.reg_write(UC_X86_REG_EBX,RECORDS+0x2100)
            w32(cpu,RECORDS+0x2400,RECORDS+0x2440 if enabled else 0)
        if mode == 36:
            subtype = struct.unpack_from('<I',parameters)[0]
            entry = [0x1410F0,0x5B890,0x103CB0][subtype]
            cpu.mem_map(0x160000,0x20000)
            cpu.mem_write(0x160000,XBE(0x160000,0x20000))
            for address in [0x424000,0x425000,0x434000]:
                cpu.mem_write(address,XBE(address,0x1000))
            cpu.mem_write(0x545D40,bytes(parameters[0x100:0x180]))
            if subtype == 1:
                cpu.reg_write(UC_X86_REG_EAX,struct.unpack_from('<H',parameters,0xA)[0])
                cpu.reg_write(UC_X86_REG_ECX,struct.unpack_from('<H',parameters,8)[0])
                cpu.reg_write(UC_X86_REG_EDX,struct.unpack_from('<i',parameters,0xC)[0])
                for index,value in enumerate([RECORDS+0x800,RECORDS+0x880,struct.unpack_from('<H',parameters,4)[0]]):
                    w32(cpu,STACK+4+index*4,value)
                w32(cpu,0x4DF250,RECORDS+0x6000)
                cpu.mem_write(RECORDS+0x6000,struct.pack('<HHIII',0x8C,1,RECORDS+0x6100,
                    RECORDS+0x6100+0x8C*struct.unpack_from('<I',parameters,0x10)[0],RECORDS+0x618C))
                cpu.mem_write(RECORDS+0x6100,bytes(parameters[0x200:0x280]))
                w32(cpu,RECORDS+0x6188,resource_hash(bytes(parameters[0x200:0x280])))
                cpu.mem_write(0x4DF110,bytes(16*0x10))
            elif subtype == 2:
                cpu.reg_write(UC_X86_REG_ESI,RECORDS+0x800)
                cpu.reg_write(UC_X86_REG_EDI,RECORDS+0x840 if struct.unpack_from('<I',parameters,4)[0] else 0)
                cpu.reg_write(UC_X86_REG_EBX,RECORDS+0x2418)
                w32(cpu,STACK+4,RECORDS+0x2400)
                w32(cpu,STACK+8,RECORDS+0x240C)
        if mode == 35:
            subtype = struct.unpack_from('<I',parameters)[0]
            entry = 0x44490
            cpu.mem_map(0x160000,0x20000)
            cpu.mem_write(0x160000,XBE(0x160000,0x20000))
            cpu.mem_write(RECORDS+0x8000,bytes(parameters[0x400:0xA00]))
            for offset,value in [(0x8000,RECORDS+0x8400),(0x8010,RECORDS+0x8200),(0x8210,0 if subtype == 3 else RECORDS+0x8300),(0x8400,0x11),(0x8404,RECORDS+0x8480),(0x8488,0 if subtype == 4 else RECORDS+0x8800),
                    (0xF204,0 if subtype == 1 else RECORDS+0xF400),(0xF400,0 if subtype == 2 else RECORDS+0x8000),(0xF950,0 if subtype == 5 else RECORDS),(0xF28C,matrix_count),(0xF290,RECORDS+0xE000),(0xE02C,RECORDS+0xD000)]:
                w32(cpu,RECORDS+offset,value)
            w32(cpu,0x511B20,0 if subtype == 0 else RECORDS+0xF934)
            cpu.mem_write(RECORDS+0x8800,bytes(parameters[0x40:0x58]))
            cpu.mem_write(RECORDS+0xD000,bytes(parameters[0x100:0x1C0]))
            for row in range(3):
                cpu.mem_write(RECORDS+0x9000+row*0x1000,bytes(parameters[0xA00+row*0x80:0xA00+row*0x80+96]))
            w32(cpu,0x507834,1)
            cpu.mem_write(0x14D360,b'\xc2\x0c\x00')
            graph_allocations=[]
            def bind_animation_graph(u,address,size,user):
                pointer = RECORDS+0x10000+len(graph_allocations)*0x10000
                graph_allocations.append(pointer)
                retail.next = pointer
            cpu.hook_add(UC_HOOK_CODE,bind_animation_graph,begin=0x10B160,end=0x10B160)
            def animation_heap_boundary(u,address,size,kind):
                retail.ret(retail.allocate(retail.arg(2)) if kind == 'allocate' else retail.sizes[retail.arg(2)])
            for address,kind in [(0x14E872,'allocate'),(0x14DED0,'size')]:
                cpu.mem_write(address,b'\xc2\x0c\x00')
                cpu.hook_add(UC_HOOK_CODE,animation_heap_boundary,begin=address,end=address,user_data=kind)
        if mode == 34:
            subtype = 0
            entry = 0x10B1F0
            cpu.mem_map(0x160000,0x20000)
            cpu.mem_write(0x160000,XBE(0x160000,0x20000))
            cpu.reg_write(UC_X86_REG_EAX,RECORDS+0x2400)
            cpu.reg_write(UC_X86_REG_ECX,RECORDS)
            w32(cpu,STACK+4,source_count)
            w32(cpu,0x507834,1)
            cpu.mem_write(0x14D360,b'\xc2\x0c\x00')
            def bind_graph(u,address,size,user):
                retail.next = RECORDS+0x10000
            cpu.hook_add(UC_HOOK_CODE,bind_graph,begin=0x10B160,end=0x10B160)
            def graph_heap_boundary(u,address,size,kind):
                if kind == 'allocate':
                    retail.ret(retail.allocate(retail.arg(2)))
                else:
                    retail.ret(retail.sizes[retail.arg(2)])
            for address,kind in [(0x14E872,'allocate'),(0x14DED0,'size')]:
                cpu.mem_write(address,b'\xc2\x0c\x00')
                cpu.hook_add(UC_HOOK_CODE,graph_heap_boundary,begin=address,end=address,user_data=kind)
        if mode == 32:
            subtype = struct.unpack_from('<I',parameters)[0]
            entry = [0x10B160,0x1021A0,0x102200,0x1024A0][subtype]
            if subtype == 0:
                retail.next = RECORDS+0x6000
                cpu.reg_write(UC_X86_REG_ESI,RECORDS)
                cpu.reg_write(UC_X86_REG_EDI,struct.unpack_from('<I',parameters,4)[0])
                cpu.reg_write(UC_X86_REG_EBX,struct.unpack_from('<I',parameters,8)[0])
            else:
                cpu.mem_write(RECORDS+0x4000,bytes(parameters[0x400:0x700]))
                cpu.mem_write(RECORDS+0x5000,bytes(parameters[0x800:0xAA0]))
                if subtype == 1:
                    w32(cpu,0x5462E0,RECORDS)
                    w32(cpu,STACK+4,RECORDS+0x5000)
                    w32(cpu,STACK+8,RECORDS+0x501C)
                else:
                    cpu.mem_map(0x160000,0x20000)
                    cpu.mem_write(0x160000,XBE(0x160000,0x20000))
                    cpu.reg_write(UC_X86_REG_ESI,RECORDS)
                    cpu.reg_write(UC_X86_REG_EAX,RECORDS)
        if mode == 33:
            subtype,flags,rows,animation_count,links,bitmap = struct.unpack_from('<6I',parameters)
            entry = [0x43D50,0x44620,0xED2E0,0x430C0][subtype]
            released=[]
            def observe_free(u,address,size,user):
                released.append(u.reg_read(UC_X86_REG_EAX) if address == 0x663E0 else retail.arg(2))
            for address in [0x663E0,0x14D360]:
                cpu.hook_add(UC_HOOK_CODE,observe_free,begin=address,end=address)
            cpu.mem_write(0x14D360,b'\xc2\x0c\x00')
            w32(cpu,0x507834,1)
            if subtype in [0,3] and flags&1:
                cpu.mem_write(RECORDS+0x6000,struct.pack('<BBHIII',8,10,0,RECORDS+0x6010,RECORDS+0x6010+rows*8,RECORDS+0x6010+rows*8))
                for row in range(rows):
                    index,original=struct.unpack_from('<II',parameters,0x400+row*8)
                    cpu.mem_write(RECORDS+0x6010+row*8,struct.pack('<II',RECORDS+0x800+index*20,original))
            if subtype in [1,3]:
                w32(cpu,0x511B20,RECORDS+0xF934)
                for offset,value in [(0xF204,RECORDS+0xE000),(0xE000,RECORDS+0xE100),(0xE110,RECORDS+0xE200),(0xE210,RECORDS+0xE300),(0xF950,RECORDS+0xB000)]:
                    w32(cpu,RECORDS+offset,value)
                cpu.mem_write(RECORDS+0xE301,bytes([animation_count]))
                for row in range(animation_count):
                    w32(cpu,RECORDS+0xB028+row*0x1E0,RECORDS+0xC000+row*0x100 if bitmap&(1<<row) else 0)
            if subtype in [2,3]:
                cpu.mem_write(RECORDS+0x9000,bytes(0x390))
                cpu.mem_write(RECORDS+0x936E,bytes(parameters[0x20:0x21]))
                cpu.mem_write(RECORDS+0x92E4,bytes(parameters[0x22:0x24]))
            if subtype == 0:
                cpu.reg_write(UC_X86_REG_EBX,RECORDS)
            elif subtype == 2:
                cpu.reg_write(UC_X86_REG_EAX,RECORDS+0x9000)
            elif subtype == 3:
                w32(cpu,RECORDS+0x4004,RECORDS+0x934)
                w32(cpu,RECORDS+0x4100,RECORDS+0x934)
                cpu.mem_write(0x4DEA50,bytes(data[0x700:0x704]))
        if mode == 31:
            subtype = struct.unpack_from('<I',parameters)[0]
            entry = [0xFEB70,0xFEBE0,0xFEAC0,0xFEFD0,0x151FAD,0x16744D,0x52660][subtype]
            if subtype in [4,5]:
                cpu.mem_map(0x160000,0x20000)
                cpu.mem_write(0x160000,XBE(0x160000,0x20000))
                if subtype == 4:
                    alias = struct.unpack_from('<I',parameters,0xC)[0]
                    for index,pointer in enumerate([RECORDS+0x2400 if alias else RECORDS+0x700,
                            RECORDS+0x740 if struct.unpack_from('<I',parameters,8)[0] else 0,RECORDS+0x2400]):
                        w32(cpu,STACK+4+index*4,pointer)
                else:
                    cpu.mem_write(STACK+4,bytes(parameters[0x400:0x408]))
            else:
                cpu.mem_write(0x50D780,bytes(data[0x700:0x718]))
                w32(cpu,0x50D798,RECORDS+0x2100)
                cpu.mem_write(0x50D79C,bytes(parameters[0x18:0x1C]))
                if subtype in [0,1]:
                    cpu.mem_map(0x160000,0x20000)
                    cpu.mem_write(0x160000,XBE(0x160000,0x20000))
                    cpu.mem_write(RECORDS+0x750,bytes(parameters[0x20:0x23]))
                    for index,word in enumerate([0,0,RECORDS+0x750,RECORDS+0x751,RECORDS+0x752]):
                        w32(cpu,STACK+4+index*4,word)
                elif subtype == 2:
                    for index,word in enumerate([RECORDS+0x2380,0x50D78C,0x50D790,0x50D794]):
                        w32(cpu,STACK+4+index*4,word)
                elif subtype == 3:
                    cpu.mem_map(0x160000,0x20000)
                    cpu.mem_write(0x160000,XBE(0x160000,0x20000))
                    cpu.reg_write(UC_X86_REG_EAX,RECORDS+0x2800)
                    cpu.reg_write(UC_X86_REG_ECX,RECORDS+0x2380)
                    cpu.mem_write(0x50D770,bytes(16))
                else:
                    positions = []
                    def observe_color_call(u,address,size,user):
                        pointer = retail.arg(1)
                        positions.append(bytes(u.mem_read(pointer,12)))
                    cpu.hook_add(UC_HOOK_CODE,observe_color_call,begin=0xFEB70,end=0xFEB70)
                    cpu.reg_write(UC_X86_REG_ESI,RECORDS+0x2800)
                    w32(cpu,STACK+4,0xFEB70)
        if mode == 30:
            subtype = struct.unpack_from('<I',parameters)[0]
            entry = [0x43C70,0x43190,0x432B0,0x10BD40,0x53000,0x155FF0][subtype]
            if subtype == 0:
                cpu.mem_write(RECORDS+0x6200,bytes(data[0x700:0x718]))
                w32(cpu,RECORDS+0x6218,RECORDS+0x7500)
                w32(cpu,STACK+4,RECORDS+0x2380)
                w32(cpu,STACK+8,RECORDS+0x6200)
            elif subtype == 3:
                cpu.reg_write(UC_X86_REG_EAX,RECORDS+0x7500 if struct.unpack_from('<I',parameters,0x18)[0] else RECORDS+0x700)
                cpu.reg_write(UC_X86_REG_EBX,RECORDS+0x7500)
                cpu.reg_write(UC_X86_REG_EDI,struct.unpack_from('<I',parameters,0x14)[0])
            elif subtype in [4,5]:
                base = RECORDS+0xA000
                cpu.mem_write(base,bytes(parameters[0x800:0x1000]))
                if subtype == 4:
                    w32(cpu,base,base+0x20)
                    for row in range(2):
                        offset = struct.unpack_from('<I',parameters,0x824+row*8)[0]
                        w32(cpu,base+0x24+row*8,base+offset if offset else 0)
                    cpu.reg_write(UC_X86_REG_ECX,base)
                else:
                    w32(cpu,STACK+4,base)
                    w32(cpu,STACK+8,struct.unpack_from('<I',parameters,0x14)[0])
            else:
                base = RECORDS+0xA000
                cpu.mem_write(base,bytes(parameters[0x800:0x1000]))
                w32(cpu,base,base+0x20)
                for row in range(2):
                    offset = struct.unpack_from('<I',parameters,0x824+row*8)[0]
                    w32(cpu,base+0x24+row*8,base+offset if offset else 0)
                present = struct.unpack_from('<I',parameters,4)[0]
                w32(cpu,RECORDS+0x2204,RECORDS+0x9000 if present else 0)
                w32(cpu,RECORDS+0x9000,base if present != 1 else 0)
                if subtype == 1:
                    cpu.reg_write(UC_X86_REG_EAX,RECORDS+0x2000)
                    cpu.mem_write(RECORDS+0x2030,bytes(parameters[0x20:0x2C]))
                    cpu.mem_write(RECORDS+0x2200,bytes(parameters[0x2C:0x30]))
                    w32(cpu,base+0x10,RECORDS+0xB000)
                    w32(cpu,RECORDS+0xB004,RECORDS+0x7000 if struct.unpack_from('<I',parameters,0x10)[0] else 0)
                    cpu.mem_write(RECORDS+0x7000,bytes(parameters[0x600:0x640]))
                    w32(cpu,RECORDS+0x700C,RECORDS+0x7500)
                    w32(cpu,RECORDS+0x7024,RECORDS+0x7700)
                    cpu.mem_write(RECORDS+0x7700,bytes(parameters[0x500:0x550]))
                    cpu.mem_write(0x42381C,XBE(0x42381C,0x3C))
                    for offset,address,size in [(0x700,0x50EA00,2),(0x704,0x50EA04,12),(0x710,0x50EA10,12),
                            (0x71C,0x50EA7C,12),(0x728,0x50EA88,12)]:
                        cpu.mem_write(address,bytes(data[offset:offset+size]))
                else:
                    w32(cpu,STACK+4,RECORDS+0x2000)
                    cpu.mem_write(RECORDS+0x22AC,bytes(parameters[0x50:0x80]))
                    for pose in range(2):
                        pointer = RECORDS+0x6000+pose*0x200
                        w32(cpu,pointer+4,struct.unpack_from('<I',parameters,0x44+pose*4)[0])
                        cpu.mem_write(pointer+16,bytes(parameters[0x100+pose*0x100:0x180+pose*0x100]))
                    w32(cpu,RECORDS+0x9164,RECORDS+0x6000)
                    w32(cpu,RECORDS+0x9168,RECORDS+0x6200 if struct.unpack_from('<I',parameters,8)[0] else 0)
                    for offset,address,size in [(0x700,0x50E930,4),(0x704,0x50E944,4),(0x708,0x50E948,4),
                            (0x70C,0x50E938,12),(0x718,0x50E950,4),(0x71C,0x50E958,12),
                            (0x728,0x509B3C,16),(0x740,0x4DD798,48),(0x800,0x50E834,252)]:
                        cpu.mem_write(address,bytes(data[offset:offset+size]))
            cpu.mem_write(RECORDS+0x7500,bytes(parameters[0x400:0x460]))
        if mode == 29:
            entry = 0x111820
            configuration = struct.unpack_from('<I',parameters)[0]
            base = RECORDS+0xA000
            cpu.mem_write(base,bytes(parameters[0x800:0x1000]))
            w32(cpu,base,base+0x20)
            for row in range(4):
                offset = struct.unpack_from('<I',parameters,0x824+row*8)[0]
                w32(cpu,base+0x24+row*8,base+offset if offset else 0)
            for floor in range(2):
                w32(cpu,base+0x8C+floor*16,base+0x100+floor*0x200)
                w32(cpu,base+0xA4+floor*8,base+0x500+floor*0x40)
            w32(cpu,RECORDS+0x8204,RECORDS+0x9000)
            w32(cpu,RECORDS+0x9000,base)
            w32(cpu,0x511B20,RECORDS+0x8934 if configuration else 0)
            cpu.mem_write(0x50EA14,bytes(parameters[4:8]))
            w32(cpu,0x511AF4,0)
            w32(cpu,0x50E248,0)
            w32(cpu,0x50E24C,0)
            w32(cpu,0x507834,1)
            # Actual heap boundaries: HeapAlloc and HeapSize use three stdcall arguments.
            def heap_boundary(u,address,size,user):
                retail.ret(retail.allocate(retail.arg(2)) if user == 'allocate' else retail.sizes[retail.arg(2)])
            for address,kind in [(0x14E872,'allocate'),(0x14DED0,'size')]:
                cpu.mem_write(address,b'\xc2\x0c\x00')
                cpu.hook_add(UC_HOOK_CODE,heap_boundary,begin=address,end=address,user_data=kind)
            cpu.mem_write(0x14D360,b'\xc2\x0c\x00')
        if mode == 28:
            subtype,width,depth,cell = struct.unpack_from('<4I',parameters)
            entry = [0x4F1B0,0x4F460,0x362F0,0x111B80][subtype]
            cpu.mem_write(RECORDS+0x6000,struct.pack('<4I',width,depth,cell,RECORDS+0x6800))
            cpu.mem_write(RECORDS+0x6800,bytes(parameters[0x800:0x900]))
            if subtype == 0:
                cpu.reg_write(UC_X86_REG_ECX,RECORDS+0x700)
                cpu.reg_write(UC_X86_REG_EAX,RECORDS+0x70C)
                w32(cpu,STACK+4,RECORDS+0x6000)
            elif subtype == 1:
                cpu.reg_write(UC_X86_REG_EAX,RECORDS+0x6000)
                cpu.reg_write(UC_X86_REG_EDI,RECORDS+0x724)
                cpu.mem_write(STACK-0x18,bytes(data[0x700:0x718]))
                for index,pointer in enumerate([RECORDS+0x718,RECORDS+0x71C,RECORDS+0x720]):
                    w32(cpu,STACK+4+index*4,pointer)
            elif subtype == 2:
                cpu.reg_write(UC_X86_REG_EAX,depth)
                w32(cpu,STACK+4,width)
                w32(cpu,STACK+8,cell)
            else:
                w32(cpu,STACK+4,0)
                w32(cpu,STACK+8,RECORDS+0x6000)
                cpu.mem_write(0x50EA14,bytes(parameters[0x10:0x14]))
                w32(cpu,0x511AF4,0) # The actual collision-probe world list is empty in this fixture.
        if mode == 27:
            subtype = struct.unpack_from('<I',parameters)[0]
            entry = [0x111FC0,0x1121B0,0x112560,0x36380,0x1122E0,0x112680,0x1127D0][subtype]
            cpu.mem_write(RECORDS+0x6000,struct.pack('<iii f I',4,4,8,1.,RECORDS+0x6800))
            cpu.mem_write(RECORDS+0x6800,bytes(parameters[0x800:0x920]))
            w32(cpu,0x50E240,RECORDS+0x6000)
            cpu.mem_write(0x50E254,bytes(parameters[0x20:0x24]))
            cpu.mem_write(0x50E25C,bytes(parameters[0x24:0x28]))
            cpu.mem_write(0x50E258,bytes(parameters[0x38:0x3C]))
            w32(cpu,0x50E248,0)
            cpu.reg_write(UC_X86_REG_EAX,0)
            cpu.reg_write(UC_X86_REG_EBX,0)
            cpu.reg_write(UC_X86_REG_EDI,0)
            if subtype == 0:
                cpu.reg_write(UC_X86_REG_ECX,struct.unpack_from('<I',parameters,4)[0])
            elif subtype in [1,2,6]:
                w32(cpu,STACK+4,struct.unpack_from('<I',parameters,4)[0])
                w32(cpu,STACK+8,struct.unpack_from('<I',parameters,8)[0])
            elif subtype == 4:
                w32(cpu,STACK+4,0)
                w32(cpu,STACK+8,struct.unpack_from('<I',parameters,4)[0])
            elif subtype == 5:
                cpu.mem_write(RECORDS+0x6200,struct.pack('<HHI',0,struct.unpack_from('<I',parameters,0x3C)[0],RECORDS+0x6400))
                cpu.mem_write(RECORDS+0x6400,bytes(parameters[0x200:0x240]))
                w32(cpu,STACK+4,0)
                w32(cpu,STACK+8,RECORDS+0x6200)
            else:
                cpu.reg_write(UC_X86_REG_EAX,RECORDS+0x6000)
                cpu.reg_write(UC_X86_REG_ECX,struct.unpack_from('<I',parameters,0xC)[0])
                cpu.mem_write(RECORDS+0x78,bytes(parameters[0x28:0x34]))
                args = [*struct.unpack_from('<4I',parameters,0x10),
                    struct.unpack_from('<I',parameters,0x34)[0],RECORDS+0x78,RECORDS+0x7C,RECORDS+0x80]
                for index,word in enumerate(args):
                    w32(cpu,STACK+4+index*4,word)
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
            cpu.emu_start(entry, STOP, count=100000000 if mode in [34,35] else 100000 if mode in [27,28,29,30,31,32,33,34] else 10000)
            if cpu.reg_read(UC_X86_REG_EIP) != STOP:
                raise RuntimeError('retail call did not return before its instruction budget')
        except Exception:
            print('retail failure', case, mode, subtype, hex(entry), hex(cpu.reg_read(UC_X86_REG_EIP)), hex(cpu.reg_read(UC_X86_REG_ESP)))
            raise
        if mode == 41 and subtype == 1:
            cpu.mem_write(RECORDS+0x900,bytes(cpu.mem_read(RECORDS+0x8294,8)))
            cpu.mem_write(RECORDS+0x910,bytes(cpu.mem_read(RECORDS+0x8694,8)))
            cpu.mem_write(RECORDS+0x940,bytes(cpu.mem_read(0x5078D8,128)))
            cpu.mem_write(RECORDS+0xB80,bytes(cpu.mem_read(0x546290,4)))
        if mode == 40:
            cpu.mem_write(RECORDS+0x800,bytes(cpu.mem_read(0x50D7A0,48)))
            cpu.mem_write(RECORDS+0x750,bytes(cpu.mem_read(0x50DD70,4)))
        if mode == 39:
            value = cpu.reg_read(UC_X86_REG_EAX) if subtype in [0,1,2,5,7] else 0
            w32(cpu,RECORDS+0xB80,value)
            if subtype in [6,7]:
                cpu.mem_write(RECORDS+0xB84,bytes(cpu.mem_read(RECORDS+0x6184,4)))
        if mode == 38:
            value = cpu.reg_read(UC_X86_REG_EAX) if subtype in [1,3,4,5] else 0
            w32(cpu,RECORDS+0x700,value)
            if subtype == 1 or subtype == 5:
                cpu.mem_write(RECORDS+0x204,bytes(data[0x204:0x208]))
            if subtype == 2:
                for index in range(6):
                    cpu.mem_write(RECORDS+0x800+index*8,bytes(cpu.mem_read(RECORDS+0x4000+index*0x20,8)))
            elif subtype == 3:
                cpu.mem_write(RECORDS+0x800,bytes(cpu.mem_read(RECORDS+0x6400,0x120)))
            elif subtype == 5:
                cpu.mem_write(RECORDS+0x800,bytes(cpu.mem_read(value,128)).split(b'\0',1)[0]+b'\0')
        if mode == 37:
            w32(cpu,RECORDS+0x700,cpu.reg_read(UC_X86_REG_EAX))
        if mode == 36 and subtype == 0:
            suffix = cpu.reg_read(UC_X86_REG_EAX)
            w32(cpu,RECORDS+0x700,RECORDS+0x2500+suffix-0x545D40 if 0x545D40 <= suffix < 0x545DC0 else suffix)
            if suffix:
                value = bytes(cpu.mem_read(suffix,128)).split(b'\0',1)[0]+b'\0'
                cpu.mem_write(RECORDS+0x800,value)
            else:
                cpu.mem_write(RECORDS+0x800,b'\0')
        if mode == 35:
            w32(cpu,RECORDS+0x700,len(graph_allocations))
            graph_bytes = bytearray()
            for pointer in graph_allocations:
                size = retail.sizes[pointer]
                graph_bytes.extend(struct.pack('<I',size))
                graph_bytes.extend(cpu.mem_read(pointer,size))
        if mode == 34:
            points = struct.unpack('<I',cpu.mem_read(RECORDS,4))[0]
            capacity = struct.unpack('<I',cpu.mem_read(RECORDS+0xC,4))[0]
            graph_size = points*0x3C+capacity*0x1D
            w32(cpu,RECORDS+0x700,graph_size)
            graph_bytes = bytes(cpu.mem_read(RECORDS+0x10000,graph_size))
        if mode == 32 and subtype != 0:
            cpu.mem_write(RECORDS+0x400,bytes(cpu.mem_read(RECORDS+0x4000,0x300)))
            cpu.mem_write(RECORDS+0x800,bytes(cpu.mem_read(RECORDS+0x5000,0x2A0)))
            if subtype == 1:
                w32(cpu,RECORDS+0x700,cpu.reg_read(UC_X86_REG_EAX))
        if mode == 33:
            w32(cpu,RECORDS+0x500,len(released))
            for index,address in enumerate(released):
                w32(cpu,RECORDS+0x504+index*4,address)
            if subtype == 2:
                cpu.mem_write(RECORDS+0x750,bytes(cpu.mem_read(RECORDS+0x936E,1)))
                cpu.mem_write(RECORDS+0x752,bytes(cpu.mem_read(RECORDS+0x92E4,2)))
            elif subtype == 3:
                cpu.mem_write(RECORDS+0x700,bytes(cpu.mem_read(0x4DEA50,4)))
                cpu.mem_write(RECORDS+0x708,bytes(cpu.mem_read(RECORDS+0x4004,4)))
                cpu.mem_write(RECORDS+0x70C,bytes(cpu.mem_read(RECORDS+0x4100,4)))
        if mode == 31:
            if subtype == 4:
                if struct.unpack_from('<I',parameters,0xC)[0]:
                    cpu.mem_write(RECORDS+0x700,bytes(cpu.mem_read(RECORDS+0x2400,64)))
            elif subtype != 5:
                cpu.mem_write(RECORDS+0x700,bytes(cpu.mem_read(0x50D780,24)))
                if subtype == 0:
                    cpu.mem_write(RECORDS+0x750,bytes(data[0x750:0x753]))
                if subtype in [3,6]:
                    cpu.mem_write(RECORDS+0x800,bytes(cpu.mem_read(RECORDS+0x2800,128)))
                    if subtype == 3:
                        for row in range(4):
                            pointer = struct.unpack('<I',cpu.mem_read(0x50D770+row*4,4))[0]
                            w32(cpu,RECORDS+0x718+row*4,int(pointer == RECORDS+0x2800))
                    else:
                        w32(cpu,RECORDS+0x4F0,len(positions))
                        for index,point in enumerate(positions):
                            cpu.mem_write(RECORDS+0x500+index*12,point)
        if mode == 30:
            if subtype == 0:
                cpu.mem_write(RECORDS+0x700,bytes(cpu.mem_read(RECORDS+0x6200,24)))
            elif subtype == 1:
                for offset,address,size in [(0x700,0x50EA00,2),(0x704,0x50EA04,12),(0x710,0x50EA10,12),
                        (0x71C,0x50EA7C,12),(0x728,0x50EA88,12)]:
                    cpu.mem_write(RECORDS+offset,bytes(cpu.mem_read(address,size)))
            elif subtype == 3:
                if struct.unpack_from('<I',parameters,0x18)[0]:
                    cpu.mem_write(RECORDS+0x700,bytes(cpu.mem_read(RECORDS+0x7500,12)))
            elif subtype in [4,5]:
                w32(cpu,RECORDS+0x700,cpu.reg_read(UC_X86_REG_EAX))
            else:
                for offset,address,size in [(0x700,0x50E930,4),(0x704,0x50E944,4),(0x708,0x50E948,4),
                        (0x70C,0x50E938,12),(0x718,0x50E950,4),(0x71C,0x50E958,12),
                        (0x728,0x509B3C,16),(0x740,0x4DD798,48),(0x800,0x50E834,252)]:
                    cpu.mem_write(RECORDS+offset,bytes(cpu.mem_read(address,size)))
        if mode == 29:
            grid_count = struct.unpack('<I',cpu.mem_read(0x50E250,4))[0]
            w32(cpu,RECORDS+0x700,grid_count)
            for floor in range(grid_count):
                grid = struct.unpack('<I',cpu.mem_read(0x50E240+floor*4,4))[0]
                cpu.mem_write(RECORDS+0x704+floor*16,bytes(cpu.mem_read(grid,16)))
                cells = struct.unpack('<I',cpu.mem_read(grid+16,4))[0]
                cpu.mem_write(RECORDS+0x800+floor*0x120,bytes(cpu.mem_read(cells,0x120)))
            cpu.mem_write(RECORDS+0x730,bytes(cpu.mem_read(0x50E254,16)))
            if grid_count:
                for offset,address in enumerate([0x50E264,0x50E26C,0x50E268,0x50E270]):
                    cpu.mem_write(RECORDS+0x740+offset*4,bytes(cpu.mem_read(address,4)))
                nodes = struct.unpack('<I',cpu.mem_read(0x50E24C,4))[0]
                cpu.mem_write(RECORDS+0x500,bytes(cpu.mem_read(nodes,320)))
            pool = struct.unpack('<I',cpu.mem_read(0x50E248,4))[0]
            rows = 0
            if pool:
                begin,end = struct.unpack('<II',cpu.mem_read(pool+4,8))
                rows = (end-begin)//12
                for row in range(rows):
                    cpu.mem_write(RECORDS+0x664+row*8,bytes(cpu.mem_read(begin+row*12,8)))
            w32(cpu,RECORDS+0x660,rows)
        if mode == 28:
            if subtype == 1:
                cpu.mem_write(RECORDS+0x700,bytes(cpu.mem_read(STACK-0x18,24)))
                w32(cpu,RECORDS+0x728,cpu.reg_read(UC_X86_REG_EAX))
            elif subtype in [2,3]:
                grid = cpu.reg_read(UC_X86_REG_EAX) if subtype == 2 else struct.unpack('<I',cpu.mem_read(0x50E240,4))[0]
                cpu.mem_write(RECORDS+0x700,bytes(cpu.mem_read(grid,16)))
                cells = struct.unpack('<I',cpu.mem_read(grid+16,4))[0]
                cpu.mem_write(RECORDS+0x800,bytes(cpu.mem_read(cells,width*depth*18)))
                if subtype == 3:
                    blocked = cpu.reg_read(UC_X86_REG_EAX)
                    cpu.mem_write(RECORDS+0x600,bytes(cpu.mem_read(blocked,8)))
                    bits = struct.unpack('<I',cpu.mem_read(blocked+8,4))[0]
                    cpu.mem_write(RECORDS+0x608,bytes(cpu.mem_read(bits,(width*depth+7)//8)))
                    for offset,address in enumerate([0x50E264,0x50E26C,0x50E268,0x50E270]):
                        cpu.mem_write(RECORDS+0x710+offset*4,bytes(cpu.mem_read(address,4)))
        if mode == 27:
            cpu.mem_write(RECORDS+0x800,bytes(cpu.mem_read(RECORDS+0x6800,0x120)))
            cpu.mem_write(RECORDS+0x70,bytes(cpu.mem_read(0x50E254,4)))
            cpu.mem_write(RECORDS+0x74,bytes(cpu.mem_read(0x50E25C,4)))
            if subtype == 6:
                pool = struct.unpack('<I',cpu.mem_read(0x50E248,4))[0]
                rows = 0
                if pool:
                    begin,end = struct.unpack('<II',cpu.mem_read(pool+4,8))
                    rows = (end-begin)//12
                    for row in range(rows):
                        cpu.mem_write(RECORDS+0x604+row*8,bytes(cpu.mem_read(begin+row*12,8)))
                w32(cpu,RECORDS+0x600,rows)
        if mode == 24 or (mode == 25 and subtype == 4):
            cpu.mem_write(RECORDS + 0x740, bytes(cpu.mem_read(0x5111AC,12)))
        if mode == 12:
            w32(cpu, RECORDS + 0x70, cpu.reg_read(UC_X86_REG_XMM0) & 0xFFFFFFFF)
        if mode == 10:
            cpu.mem_write(RECORDS + 0x800, bytes(cpu.mem_read(0x507C98, 0x80)))
        wanted = [1 if mode in [5, 6, 7, 10, 12, 13, 14, 28, 29, 30, 32, 33, 34, 35, 37, 38, 39, 40, 41, 42] or (mode == 36 and subtype == 2) or (mode == 31 and subtype != 5) or (mode == 18 and subtype == 3) or (mode == 20 and subtype != 1) or (mode == 23 and subtype in [0,2]) or (mode == 25 and subtype in [0,2,3]) or (mode == 27 and subtype != 3) else int(cpu.reg_read(UC_X86_REG_EAX) != 0) if mode in [26,27,36] else cpu.reg_read(UC_X86_REG_EAX), struct.unpack('<I', cpu.mem_read(0x54DD7C, 4))[0],
            slot if mode in [32,33,38] else struct.unpack('<I', cpu.mem_read(RECORDS + 0x4000, 4))[0], *cpu.mem_read(RECORDS, len(data))]
        if mode in [34,35]:
            wanted.extend(graph_bytes)
        expected.append(wanted)
        lines.append(' '.join(map(str, [mode, state, slot, *parameters, *data])))
    assert len(expected) == count and len(lines) == count, 'fixture count changed during generation'
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    result = subprocess.run([harness('scene_factory_classes_harness')], input=str(count) + '\n' + '\n'.join(lines) + '\n',
        text=True, capture_output=True, check=True, env=environment)
    actual = result.stdout.splitlines()
    bad = abs(len(actual) - len(expected))
    for index, (line, wanted) in enumerate(zip(actual, expected)):
        got = list(map(int, line.split()))
        if got != wanted:
            case_mode = int(lines[index].split(' ', 1)[0])
            if bad < 5:
                print('case', index, 'mode', case_mode, 'diff', [i for i, pair in enumerate(zip(got, wanted)) if pair[0] != pair[1]][:20])
                if case_mode == 39:
                    fixture = bytes(list(map(int,lines[index].split()))[3:3+0x1000])
                    print('effect subtype',struct.unpack_from('<I',fixture)[0],
                        'bytes',[(hex(i-3),got[i],wanted[i]) for i in range(len(got)) if got[i] != wanted[i]][:10])
                    source = fixture[0x400:] if struct.unpack_from('<I',fixture)[0] == 3 else bytes(list(map(int,lines[index].split()))[3+0x1000:])
                    print('scale inputs',struct.unpack_from('<II',source,0x98))
                if case_mode == 40:
                    fixture = bytes(list(map(int,lines[index].split()))[3:3+0x1000])
                    row = struct.unpack_from('<I',fixture,0)[0]*0x48
                    print('scales',struct.unpack('<3f',bytes(got[3+row+0x1C:3+row+0x28])),struct.unpack('<3f',bytes(wanted[3+row+0x1C:3+row+0x28])))
                if case_mode == 36:
                    fixture = bytes(list(map(int,lines[index].split()))[3:3+0x1000])
                    print('door subtype',struct.unpack_from('<I',fixture)[0], 'pose',struct.unpack_from('<9f',fixture,0x400),
                        'host',struct.unpack('<12I',bytes(got[3+0x800:3+0x830])),
                        'retail',struct.unpack('<12I',bytes(wanted[3+0x800:3+0x830])))
                    print('return',got[0],wanted[0], 'camera',bytes(got[3+0x880:3+0x900]).split(b'\0')[0], 'record',fixture[0x200:0x280].split(b'\0')[0])
                if case_mode == 35:
                    fixture = bytes(list(map(int,lines[index].split()))[3:3+0x1000])
                    print('animation',struct.unpack_from('<I',fixture)[0],struct.unpack_from('<IiI',fixture,0xC),
                        'host',struct.unpack('<12I',bytes(got[3:51])), 'retail',struct.unpack('<12I',bytes(wanted[3:51])),
                        'sizes',len(got),len(wanted))
                if case_mode == 31:
                    fixture = bytes(list(map(int, lines[index].split()))[3:3 + 0x1000])
                    print('sector subtype',struct.unpack_from('<I',fixture)[0], 'rgb',list(fixture[0x20:0x23]),
                        'settings',struct.unpack('<5f',bytes(wanted[3+0x704:3+0x718])),
                        'host',got[3+0x750:3+0x753], 'retail',wanted[3+0x750:3+0x753])
                    if struct.unpack_from('<I',fixture)[0] == 4:
                        print('matrix kind',struct.unpack_from('<I',fixture,4)[0], 'input',struct.unpack_from('<16f',fixture,0x400),
                            'host',struct.unpack('<17f',bytes(got[3+0x700:3+0x744])),
                            'retail',struct.unpack('<17f',bytes(wanted[3+0x700:3+0x744])))
                if case_mode == 30:
                    fixture = bytes(list(map(int, lines[index].split()))[3:3 + 0x1000])
                    print('background subtype',struct.unpack_from('<5I',fixture),
                        'host',struct.unpack('<12f',bytes(got[3+0x700:3+0x730])),
                        'retail',struct.unpack('<12f',bytes(wanted[3+0x700:3+0x730])))
                if case_mode == 25:
                    print('push result',struct.unpack('<3f',bytes(got[3+0x700:3+0x70C])),struct.unpack('<3f',bytes(wanted[3+0x700:3+0x70C])))
                if case_mode == 4:
                    for offset in [0x28, 0x2C, 0x34, 0x38]:
                        first, second = bytes(got[3 + offset:7 + offset]), bytes(wanted[3 + offset:7 + offset])
                        if first != second:
                            print(hex(offset), 'host', struct.unpack('<f', first)[0], 'retail', struct.unpack('<f', second)[0])
                if case_mode == 13:
                    fixture = list(map(int, lines[index].split()))[3:3 + 0x1000]
                    print('offset inputs', struct.unpack_from('<5f', bytes(fixture)),
                        'host', struct.unpack('<3f', bytes(got[3 + 0x70:3 + 0x7C])),
                        'retail', struct.unpack('<3f', bytes(wanted[3 + 0x70:3 + 0x7C])))
                if case_mode == 19:
                    fixture = bytes(list(map(int, lines[index].split()))[3:3 + 0x1000])
                    print('dispatch subtype', struct.unpack_from('<I', fixture)[0], 'return', got[0], wanted[0],
                        'count', struct.unpack_from('<i', fixture, 0x800)[0],
                        'rows', [struct.unpack_from('<BBhI', fixture, 0x808 + row * 0x24) for row in range(max(0, struct.unpack_from('<i', fixture, 0x800)[0]))])
                if case_mode == 18:
                    fixture = bytes(list(map(int, lines[index].split()))[3:3 + 0x1000])
                    print('sphere subtype', struct.unpack_from('<I', fixture)[0],
                        'end host', struct.unpack('<3f', bytes(got[3 + 0x700:3 + 0x70C])),
                        'retail', struct.unpack('<3f', bytes(wanted[3 + 0x700:3 + 0x70C])))
            bad += 1
    print(count, 'cases;', bad, 'mismatches')
    return bool(bad)


if __name__ == '__main__':
    sys.exit(main())
