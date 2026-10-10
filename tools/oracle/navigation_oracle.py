"""Differential check: navigation cell registration 0x001104E0 (with 0x001109E0, 0x00110970 and
0x00110EF0) against the host build. Needs cache/oracle/mem_00011000.bin and mem_00210000.bin."""
import os, random, struct, subprocess, sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP
from retail_cpu import ROOT, STOP, STACK, HEAP, w32, r32, call, harness

ALLOCATE, REALLOCATE = 0x000F72D0, 0x000F73A0
GRIDS, ORIGIN_X, ORIGIN_Z = 0x0050E240, 0x0050E264, 0x0050E26C

def new_emu():
    u = Uc(UC_ARCH_X86, UC_MODE_32)
    u.mem_map(0x00010000, 0x150000)
    u.mem_write(0x00011000, open(os.path.join(ROOT, 'cache', 'oracle', 'mem_00011000.bin'), 'rb').read())
    u.mem_map(0x00210000, 0x130000)
    u.mem_write(0x00210000, open(os.path.join(ROOT, 'cache', 'oracle', 'mem_00210000.bin'), 'rb').read())
    u.mem_map(0x00500000, 0x50000)
    u.mem_map(STOP, 0x1000); u.mem_write(STOP, b'\xf4')
    u.mem_map(HEAP, 0x400000)
    u.mem_map(STACK - 0x10000, 0x20000)
    state = {'next': HEAP + 0x200000, 'sizes': {}}
    u.mem_write(ALLOCATE, b'\xc3')
    u.mem_write(REALLOCATE, b'\xc3')
    def allocate(uc, address, size, user):
        n = r32(uc, uc.reg_read(UC_X86_REG_ESP) + 4)
        block = state['next']
        state['next'] += (n + 0xF) & ~0xF
        state['sizes'][block] = n
        uc.mem_write(block, b'\xcd' * n)
        uc.reg_write(UC_X86_REG_EAX, block)
    def reallocate(uc, address, size, user):
        old = uc.reg_read(UC_X86_REG_EAX)
        n = r32(uc, uc.reg_read(UC_X86_REG_ESP) + 4)
        block = state['next']
        state['next'] += (n + 0xF) & ~0xF
        keep = min(n, state['sizes'][old])
        uc.mem_write(block, b'\xcd' * n)
        uc.mem_write(block, bytes(uc.mem_read(old, keep)))
        state['sizes'][block] = n
        uc.reg_write(UC_X86_REG_EAX, block)
    u.hook_add(UC_HOOK_CODE, allocate, begin=ALLOCATE, end=ALLOCATE)
    u.hook_add(UC_HOOK_CODE, reallocate, begin=REALLOCATE, end=REALLOCATE)
    return u

def fbits(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]

def main():
    random.seed(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 300
    lines, expected = [str(count)], []
    for _ in range(count):
        u = new_emu()
        ox, oz = random.randint(-200, 200), random.randint(-200, 200)
        w32(u, ORIGIN_X, ox)
        w32(u, ORIGIN_Z, oz)
        line = ['%d %d' % (ox, oz)]
        grids = []
        cursor = HEAP
        floor = random.choice([0, 0, 1, 2, 3])
        for g in range(4):
            present = g == floor and random.random() < 0.95 or (g != floor and random.random() < 0.5)
            if not present:
                w32(u, GRIDS + 4 * g, 0)
                line.append('0')
                grids.append(None)
                continue
            width, depth, cell = random.randint(1, 10), random.randint(1, 10), random.choice([4, 8, 16, 32, 50])
            grid = cursor
            cells = grid + 0x20
            cursor = cells + 0x12 * width * depth + 0x10
            w32(u, grid, width); w32(u, grid + 4, depth); w32(u, grid + 8, cell); w32(u, grid + 0x10, cells)
            w32(u, GRIDS + 4 * g, grid)
            words = []
            for i in range(width * depth):
                x = ox + (i % width) * cell + random.randint(-cell, cell)
                z = oz + (i // width) * cell + random.randint(-cell, cell)
                word = random.choice([0, 0, random.getrandbits(16), 0x1F, 0x1E0, 0xFFFF])
                u.mem_write(cells + 0x12 * i, struct.pack('<hhhH', x, 0, z, word))
                words.append('%d %d %d' % (x, z, word))
            grids.append((grid, cells, width, depth))
            line.append('1 %d %d %d %s' % (width, depth, cell, ' '.join(words)))
        calls = random.randint(1, 3)
        line.append('%d %d' % (floor, calls))
        slot = HEAP + 0x1F0000
        w32(u, slot, 0)
        center_slot = HEAP + 0x1F0010
        for _ in range(calls):
            g = grids[floor]
            span = (g[2] if g else 4) * 40
            cx = ox + random.uniform(-40, span)
            cz = oz + random.uniform(-40, span)
            cy = random.uniform(-5, 5)
            radius = random.choice([0.0, random.uniform(0, 60), random.uniform(0, 200), float('nan'), -random.uniform(0, 30)])
            outer = random.choice([0, 0, 1])
            bits = [fbits(cx), fbits(cy), fbits(cz), fbits(radius)]
            u.mem_write(center_slot, struct.pack('<III', *bits[:3]))
            call(u, 0x001104E0, 0, [slot, center_slot, floor, bits[3], outer])
            header = r32(u, slot)
            if header == 0:
                text = 'null'
            else:
                stride = struct.unpack('<H', u.mem_read(header, 2))[0]
                growth = struct.unpack('<H', u.mem_read(header + 2, 2))[0]
                begin, end, cap = r32(u, header + 4), r32(u, header + 8), r32(u, header + 0xC)
                text = '%u %u %u |' % ((end - begin) // 12, (cap - begin) // 12, growth)
                for row in range(begin, end, stride):
                    text += ' %u:%u' % (r32(u, row), r32(u, row + 4))
            text += ' |'
            if g:
                for i in range(g[2] * g[3]):
                    text += ' %u' % struct.unpack('<H', u.mem_read(g[1] + 0x12 * i + 6, 2))[0]
            expected.append(text)
            line.append('%u %u %u %u %d' % (bits[0], bits[1], bits[2], bits[3], outer))
        lines.append(' '.join(line))
    got = subprocess.run([sys.argv[3] if len(sys.argv) > 3 else harness('navigation_harness')], input='\n'.join(lines) + '\n',
        capture_output=True, text=True).stdout.splitlines()
    bad = 0
    for i, (want, have) in enumerate(zip(expected, got)):
        if want.strip() != have.strip():
            bad += 1
            if bad < 4:
                print('MISMATCH', i, '\n exp', want[:300], '\n got', have[:300])
    print('calls', len(expected), len(got), 'mismatches', bad)

if __name__ == '__main__':
    main()
