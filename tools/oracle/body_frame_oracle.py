"""Differential check of body movement, blends, pulse, timers and wash.

Retail executes actual math and latch callees. Null scene recipients isolate
these per-body methods; this oracle does not establish live ownership/spawning.
"""
import os
import random
import struct
import subprocess
import sys

from body_joint_oracle import emulator, bits, equivalent, write, BODY, ACTOR
from retail_cpu import ROOT, STOP, STACK, w32, harness
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 400
    cpu = emulator()
    lines, expected = [], []
    routines = [
        ('movement', 0x75120, [0x18, 0x1c, 0x20]),
        ('blends', 0x1ede0, [0xe8, 0xf4]),
        ('pulse', 0x1fb40, [0x25c, 0x168, 0x16c, 0x174]),
        ('timers', 0x27960, [0x75c, 0x758, 0x7e8]),
        ('wash', 0x1dd00, [0xac, 0xb0, 0xb4, 0xb8, 0xbc, 0xc0, 0xc4, 0xc8, 0xcc, 0xd0, 0xd4]),
        ('triangle', 0x10acd0, [0x100, 0x104, 0x108]),
        ('sphere', 0x10abd0, [0x100, 0x104, 0x108]),
        ('cylinder', 0x120760, [0x100, 0x104, 0x108, 0x110, 0x114, 0x118]),
        ('commandtimer', 0x1ae50, [0x34]),
        ('commandcounters', 0x1ab50, []),
        ('commandpush', 0x19a90, [0x100, 0x104, 0x108]),
        ('damagepulse', 0x1ff60, [0x168, 0x16c]),
        ('damagehealth', 0x21120, []),
    ]
    for kind, address, floats in routines:
        for case in range(count):
            body = bytearray([0xa5] * 0x924)
            write(body, 8, ACTOR)
            for offset in [0x18, 0x1c, 0x20, 0xe4, 0xe8, 0xf0, 0xf4, 0x25c, 0x168, 0x16c,
                           0x174, 0x75c, 0x758, 0x7e8, 0xac, 0xb0, 0xb4, 0xb8,
                           0xbc, 0xc0, 0xc4, 0xc8, 0xcc, 0xd0, 0xd4]:
                write(body, offset, bits(rng.uniform(-2, 3)))
            if floats and case % 11 == 0:
                write(body, rng.choice(floats), rng.choice([0x7fc00001, 0x7f800000, 0xff800000, 0x80000000]))
            for offset in range(0x260, 0x266):
                body[offset] = rng.randrange(3)
            body[0xec], body[0xf8] = rng.randrange(8), rng.randrange(8)
            body[0x7e4], body[0x7e5] = rng.randrange(2), rng.randrange(2)
            write(body, 0x760, 0)
            write(body, 0x7f0, 0)
            struct.pack_into('<HH', body, 0x94, rng.randrange(5), rng.randrange(5))
            body[0xa4], body[0xa6], body[0xa7] = case % 6, rng.randrange(2), rng.randrange(2)
            body[0xa8], body[0xa9], body[0xaa] = rng.randrange(2), rng.randrange(2), rng.randrange(2)
            words = [bits(0)] * 3 + [bits(rng.choice([0, 1, -1, 0.5])), bits(rng.choice([0, 1, -1])),
                    bits(rng.choice([0, 1, -1])), bits(rng.uniform(0, 0.5)), bits(rng.choice([1, -1, 2]))]
            words += [bits(rng.uniform(-10, 10)) for _ in range(3)]
            slot = rng.randrange(2)
            if kind == 'wash':
                words[9] = rng.randrange(2)
            if kind in ('triangle', 'sphere', 'cylinder'):
                for offset in range(4, 0x28, 4):
                    write(body, offset, bits(rng.uniform(-10, 10)))
                for offset in [0x100, 0x104, 0x108]:
                    write(body, offset, bits(rng.uniform(-10, 10)))
                words[8] = bits(rng.uniform(-2, 10))
                if case % 13 == 0:
                    write(body, 0x100, rng.choice([0x7fc00001, 0x7f800000]))
            if kind == 'cylinder':
                for offset in [4, 8, 12, 0x14, 0x1c, 0x44, 0x48, 0x4c, 0x54, 0x5c, 0x110, 0x114, 0x118]:
                    write(body, offset, bits(rng.uniform(-2, 5)))
                if case % 13 == 0:
                    write(body, 8, 0x7fc00001)
            if kind == 'commandtimer':
                write(body, 0x34, bits(rng.uniform(-5, 10)))
                words[8:11] = [bits(rng.uniform(-5, 5)), bits(rng.uniform(-5, 5)), rng.randrange(2**32)]
                if case % 11 == 0:
                    write(body, 0x34, rng.choice([0x7fc00001, 0x7f800000, 0xff800000]))
                if case % 13 == 0:
                    words[8] = 0x7fc00001
            if kind == 'commandcounters':
                words[8:10] = [rng.randrange(2**32), rng.choice([0, 1, 2, 0xffffffff])]
                body[0x309:0x30d] = bytes(rng.randrange(256) for _ in range(4))
            if kind == 'commandpush':
                words[8] = rng.choice([0, 1, 40, 0xffffffff, 0x80000000, rng.randrange(2**32)])
                words[9] = bits(rng.uniform(-100, 100))
                for offset in [0x100, 0x104, 0x108]:
                    write(body, offset, bits(rng.uniform(-5, 5)))
            if kind == 'damagepulse':
                words[8:10] = [rng.choice([0, 1, 2, 0xffffffff, 0x80000000, rng.randrange(2**32)]), rng.randrange(4)]
                write(body, 0x16c, bits(rng.uniform(-1, 3)))
                if case % 11 == 0:
                    write(body, 0x16c, rng.choice([0x7fc00001, 0x7f800000, 0xff800000]))
            if kind == 'damagehealth':
                words[8:11] = [rng.choice([0, 1, 2, 0xffffffff, 0x80000000, rng.randrange(2**32)]),
                              rng.choice([0, 1, 2, 0xffffffff]), rng.randrange(8) | (rng.randrange(3) << 8)]
                struct.pack_into('<H', body, 0x94, rng.randrange(65536))
                body[0x405] = rng.choice([0, 0x10])
                write(body, 0x15c, 0x10005000)
            cpu.mem_write(BODY, bytes(body))
            cpu.mem_write(ACTOR, bytes(0xd00))
            w32(cpu, ACTOR + 0x92c, words[3])
            w32(cpu, ACTOR + 0x930, words[4])
            w32(cpu, ACTOR + 0x978, words[8])
            w32(cpu, ACTOR + 0x98c, words[9] if kind == 'wash' else 0)
            cpu.mem_write(ACTOR + 0x969, bytes([slot]))
            for location, word in zip([0x4312dc, 0x5096c0, 0x4312c4], words[5:8]):
                w32(cpu, location, word)
            w32(cpu, 0x511aa4, 0)
            w32(cpu, STACK, STOP)
            cpu.reg_write(UC_X86_REG_ESP, STACK)
            cpu.reg_write(UC_X86_REG_EAX, BODY)
            cpu.reg_write(UC_X86_REG_ECX, BODY)
            cpu.reg_write(UC_X86_REG_EDX, 0)
            cpu.reg_write(UC_X86_REG_ESI, BODY)
            if kind == 'damagepulse':
                cpu.reg_write(UC_X86_REG_ESI, words[8])
                cpu.mem_write(ACTOR + 0xcdc, struct.pack('<H', 2 if words[9] & 1 else 0))
                w32(cpu, ACTOR + 0x93c, (words[9] >> 1) & 1)
            if kind == 'damagehealth':
                cpu.reg_write(UC_X86_REG_EAX, words[8])
                w32(cpu, STACK + 4, words[9])
                w32(cpu, 0x10005008, BODY + 0x400)
                cpu.mem_write(ACTOR + 0xcdc, struct.pack('<H', 2 if words[10] & 1 else 0))
                w32(cpu, ACTOR + 0x93c, (words[10] >> 1) & 1)
                w32(cpu, 0x50e764, (words[10] >> 2) & 1)
                w32(cpu, 0x50e768, (words[10] >> 2) & 1)
                w32(cpu, 0x51248c, 0)
                cpu.mem_write(0x30f144, bytes([(words[10] >> 8) & 255]))
            if kind == 'commandtimer':
                cpu.reg_write(UC_X86_REG_ECX, BODY)
                cpu.mem_write(0x10005000 + 0x10, struct.pack('<II', *words[8:10]))
                cpu.reg_write(UC_X86_REG_EAX, 0x10005000)
                w32(cpu, 0x54dd7c, words[10])
            if kind == 'commandcounters':
                cpu.mem_write(0x10005000 + 0x10, struct.pack('<I', words[8]))
                cpu.mem_write(0x10005100 + 0x10, struct.pack('<I', words[9]))
                cpu.reg_write(UC_X86_REG_EDX, 0x10005100)
                cpu.reg_write(UC_X86_REG_EAX, BODY)
                cpu.reg_write(UC_X86_REG_ECX, 0x10005000)
            if kind == 'commandpush':
                cpu.mem_write(ACTOR + 0x3c, bytes(body[0x100:0x10c]))
                cpu.reg_write(UC_X86_REG_EDI, BODY)
                w32(cpu, STACK + 4, words[8])
                w32(cpu, STACK + 8, words[9])
            if kind == 'movement':
                cpu.mem_write(0x10005000, struct.pack('<III', *words[8:11]))
                cpu.reg_write(UC_X86_REG_ECX, 0x10005000)
            if kind in ('triangle', 'sphere'):
                w32(cpu, STACK + 4, words[8])
                cpu.reg_write(UC_X86_REG_EAX, BODY + 0x100)
                cpu.reg_write(UC_X86_REG_ECX, BODY)
                cpu.reg_write(UC_X86_REG_ESI, BODY)
                cpu.reg_write(UC_X86_REG_EDI, BODY + 0x100)
            if kind == 'cylinder':
                cpu.mem_write(0x10005000, struct.pack('<III', *words[8:11]))
                w32(cpu, STACK + 4, 0x10005000)
                w32(cpu, STACK + 8, BODY + 0x100)
                cpu.reg_write(UC_X86_REG_EBX, BODY)
                cpu.reg_write(UC_X86_REG_EDI, BODY + 0x40)
                w32(cpu, 0x511188, 0)
                cpu.mem_write(0x5111ac, bytes(body[0x110:0x11c]))
            cpu.emu_start(address, STOP, count=100000)
            if kind == 'damagehealth':
                w32(cpu, BODY + 0x110, cpu.reg_read(UC_X86_REG_EAX))
            if kind == 'commandtimer':
                cpu.mem_write(BODY + 0x110, bytes(cpu.mem_read(0x54dd7c, 4)))
            if kind == 'commandpush':
                cpu.mem_write(BODY + 0x100, bytes(cpu.mem_read(ACTOR + 0x3c, 12)))
            if kind in ('triangle', 'sphere', 'cylinder'):
                cpu.mem_write(BODY + 0x10c, bytes([cpu.reg_read(UC_X86_REG_EAX) & 0xff]))
            if kind == 'cylinder':
                cpu.mem_write(BODY + 0x110, bytes(cpu.mem_read(0x5111ac, 12)))
            if cpu.reg_read(UC_X86_REG_ESP) != STACK + 4:
                raise RuntimeError(f'{kind} did not return')
            expected.append((kind, case, bytes(cpu.mem_read(BODY, len(body))), floats))
            lines.append(' '.join(map(str, [kind, *body, *words, slot])))
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    result = subprocess.run([harness('body_joint_harness')], input='\n'.join(lines) + '\n',
                            text=True, capture_output=True, check=True, env=environment)
    outputs = result.stdout.splitlines()
    if len(outputs) != len(expected):
        raise RuntimeError(f'host returned {len(outputs)} rows for {len(expected)} cases')
    mismatches = 0
    for line, (kind, case, retail, floats) in zip(outputs, expected):
        host = bytes(map(int, line.split()[2:]))
        ignored = {byte for offset in floats for byte in range(offset, offset + 4)}
        different = [offset for offset in range(len(retail)) if offset not in ignored and retail[offset] != host[offset]]
        different += [offset for offset in floats if not equivalent(struct.unpack_from('<I', retail, offset)[0],
                                                                  struct.unpack_from('<I', host, offset)[0])]
        if different:
            mismatches += 1
            if mismatches <= 8:
                print(kind, case, 'offsets', [hex(offset) for offset in different[:12]])
                for offset in different[:4]:
                    print(hex(offset), retail[offset:offset+4].hex(), host[offset:offset+4].hex())
    print(f'{len(expected)} body frame cases, {mismatches} mismatches')
    return bool(mismatches)


if __name__ == '__main__':
    sys.exit(main())
