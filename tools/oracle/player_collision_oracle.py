"""Compare 0x00074670's empty-world path with genuine retail dependencies."""
import os
import random
import struct
import subprocess
import sys
from body_joint_oracle import emulator, bits, equivalent
from retail_cpu import ROOT, STOP, STACK, w32, harness
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX


def main():
    rng = random.Random(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 400
    cpu = emulator()
    body, actor, flags, flag_bytes, pool = 0x10001000, 0x10008000, 0x10005000, 0x10005100, 0x10006000
    lines, expected = [], []
    for case in range(count):
        words = [bits(rng.uniform(-200, 200)) for _ in range(6)]
        words += [bits(rng.uniform(0.1, 50)), bits(rng.uniform(0.1, 3)), bits(rng.uniform(0.1, 3))]
        words += [bits(rng.choice([0., 1., -1.])), bits(rng.uniform(0, 0.1)), bits(rng.uniform(0, 2)),
                  bits(rng.uniform(-1, 3)), bits(rng.choice([0., 1., -1.])), rng.randrange(80),
                  rng.randrange(256), rng.getrandbits(32), rng.getrandbits(32), rng.randrange(2)]
        if case % 7 == 0:
            words[3:6] = words[:3]
        if case % 17 == 0:
            words[rng.choice([9, 10, 11, 12, 13])] = 0x7fc00001
        lines.append('bodycollisionempty ' + ' '.join(map(str, words)))
        initial = bytearray(0xa50)
        struct.pack_into('<III', initial, 0x18, *words[3:6])
        struct.pack_into('<I', initial, 0x894, words[7])
        struct.pack_into('<I', initial, 0x8a4, words[8])
        struct.pack_into('<I', initial, 0x340, words[14])
        initial[0x874], initial[0x19e] = words[15], words[18]
        cpu.mem_write(body, bytes(initial))
        cpu.mem_write(actor, bytes(0xd00))
        cpu.mem_write(actor + 8, struct.pack('<III', *words[:3]))
        for offset, word in [(0x38, words[6]), (0x92c, words[12]), (0x930, words[13]),
                             (0x2a8, 0xa5a5a5a5), (0x95c, body)]:
            w32(cpu, actor + offset, word)
        w32(cpu, body + 8, actor)
        w32(cpu, body + 0x15c, flags)
        w32(cpu, flags + 8, flag_bytes)
        cpu.mem_write(flag_bytes, struct.pack('<II', *words[16:18]))
        for address, value in [(0x4312dc, words[9]), (0x5096c0, words[10]), (0x4312c4, words[11]),
                               (0x4ee2b8, 0), (0x511af4, 0), (0x511a18, 0), (0x511a20, pool)]:
            w32(cpu, address, value)
        cpu.mem_write(0x50dd80, bytes(12))
        cpu.mem_write(pool, struct.pack('<HHIII', 0x54, 10, pool + 0x10, pool + 0x10, pool + 0x358))
        w32(cpu, STACK, STOP)
        w32(cpu, STACK + 4, body)
        cpu.reg_write(UC_X86_REG_ESP, STACK)
        cpu.emu_start(0x74670, STOP, count=1000000)
        if cpu.reg_read(UC_X86_REG_ESP) != STACK + 4:
            raise RuntimeError('retail player collision update did not return')
        result = cpu.reg_read(UC_X86_REG_EAX)
        actual_body = bytearray(cpu.mem_read(body, 0xa50))
        actual_body[8:12] = bytes(4)
        actual_body[0x15c:0x160] = bytes(4)
        expected.append((result, bytes(actual_body),
                         struct.unpack('<III', cpu.mem_read(actor + 8, 12)),
                         struct.unpack('<I', cpu.mem_read(actor + 0x2a8, 4))[0],
                         bytes(cpu.mem_read(flag_bytes, 8))))
    environment = os.environ.copy()
    environment['PATH'] = os.path.join(ROOT, 'build', 'Release') + os.pathsep + environment.get('PATH', '')
    result = subprocess.run([harness('body_joint_harness')], input='\n'.join(lines) + '\n',
                            text=True, capture_output=True, check=True, env=environment)
    native_lines = result.stdout.splitlines()
    if len(native_lines) != len(expected):
        raise RuntimeError(f'expected {len(expected)} results, received {len(native_lines)}')
    failures = 0
    float_offsets = set(range(0x18, 0x24, 4)) | set(range(0x248, 0x254, 4)) | set(range(0x344, 0x710, 4))
    for index, (line, retail) in enumerate(zip(native_lines, expected)):
        native = list(map(int, line.split()))
        native_body = bytes(native[1:1 + 0xa50])
        different = []
        for offset in range(0, 0xa50, 4):
            a, b = struct.unpack_from('<I', native_body, offset)[0], struct.unpack_from('<I', retail[1], offset)[0]
            if not (equivalent(a, b) if offset in float_offsets else a == b):
                different.append(hex(offset))
        position = native[1 + 0xa50:4 + 0xa50]
        if native[0] != retail[0] or different or not all(equivalent(a, b) for a, b in zip(position, retail[2])) or native[4 + 0xa50] != retail[3] or bytes(native[5 + 0xa50:]) != retail[4]:
            failures += 1
            if failures <= 8:
                print(f'case={index} body={different} result={native[0]}/{retail[0]} position={position}/{retail[2]}')
    print(f'player collision empty world: {count} cases, {failures} mismatches')
    return int(failures != 0)


if __name__ == '__main__':
    sys.exit(main())
