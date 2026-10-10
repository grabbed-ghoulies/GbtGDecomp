"""Differential check: player body overlay 0x0001DF70 (with 0x0001E2A0, 0x0001E770, 0x0003DD50,
0x0002AC70, 0x0001E3E0 and the billboard queue 0x0005C1A0) against the host build.
Needs cache/oracle/mem_00011000.bin, mem_00160000.bin and mem_00210000.bin."""
import os, random, struct, subprocess, sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_ESP
from retail_cpu import ROOT, STOP, STACK, HEAP, w32, r32, harness

QUEUE, COUNT, VIEW, DIGITS = 0x004EE330, 0x00504330, 0x005077A8, 0x00545A80

def load(name):
    return open(os.path.join(ROOT, 'cache', 'oracle', name), 'rb').read()

def new_emu():
    u = Uc(UC_ARCH_X86, UC_MODE_32)
    u.mem_map(0x00010000, 0x175000)
    u.mem_write(0x00011000, load('mem_00011000.bin'))
    u.mem_write(0x00160000, load('mem_00160000.bin')[:0x25000])
    u.mem_map(0x00200000, 0x10000)
    u.mem_write(0x00200000, load('mem_00200000.bin')[:0x3000])
    u.mem_write(0x00203000, load('mem_00203000.bin')[:0xD000])
    u.mem_map(0x00210000, 0x130000)
    u.mem_write(0x00210000, load('mem_00210000.bin'))
    u.mem_map(0x004E0000, 0x80000)
    u.mem_map(STOP, 0x1000); u.mem_write(STOP, b'\xf4')
    u.mem_map(HEAP, 0x100000)
    u.mem_map(STACK - 0x10000, 0x20000)
    return u

def run(u, fn, eax, ebx, ecx, args):
    esp = STACK
    w32(u, esp, STOP)
    for i, a in enumerate(args):
        w32(u, esp + 4 + 4 * i, a)
    u.reg_write(UC_X86_REG_ESP, esp)
    u.reg_write(UC_X86_REG_EAX, eax)
    u.reg_write(UC_X86_REG_EBX, ebx)
    u.reg_write(UC_X86_REG_ECX, ecx)
    u.emu_start(fn, STOP)

def fbits(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]

def rfloat():
    return random.choice([0.0, 1.0, -2.5, random.uniform(-50, 50), random.uniform(0, 3)])

def main():
    random.seed(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    cases = int(sys.argv[2]) if len(sys.argv) > 2 else 400
    lines, expected = [], []
    for _ in range(cases):
        u = new_emu()
        sprites = [HEAP + 0x100 * i for i in range(14)]
        line = []
        kind = random.choice(['body', 'body', 'body', 'digits'])
        for i in range(4):
            frames, divisor = random.choice([0, 0, 1, 4, 12, 255]), random.randint(1, 16)
            u.mem_write(sprites[i] + 0x10, bytes([frames, divisor]))
            line.append('%d %d' % (frames, divisor))
        for d in range(10):
            width = random.randint(0, 400)
            u.mem_write(sprites[4 + d] + 8, struct.pack('<H', width))
            w32(u, DIGITS + 4 * d, sprites[4 + d])
            line.append(str(width))
        view = HEAP + 0x2000
        forward = [fbits(random.choice([0.0, random.uniform(-1, 1)])) for _ in range(3)]
        for j, b in enumerate(forward):
            w32(u, view + 0x1C + 4 * j, b)
        w32(u, VIEW, view)
        line.append('%u %u %u' % tuple(forward))
        w32(u, COUNT, 0)
        if kind == 'digits':
            value = random.choice([0, random.randint(0, 9), random.randint(10, 99), random.randint(100, 999)])
            pos = [fbits(rfloat()) for _ in range(3)]
            color = random.getrandbits(32)
            position = HEAP + 0x3000
            u.mem_write(position, struct.pack('<III', *pos))
            run(u, 0x0001E3E0, value, position, 0, [color])
            line.append('%d %u %u %u %u' % (value, pos[0], pos[1], pos[2], color))
        else:
            draw_kind = random.choice([2, 2, 2, 0, 1])
            body, owner, attached, attributes = HEAP + 0x4000, HEAP + 0x5000, HEAP + 0x6000, HEAP + 0x7000
            e0 = random.choice([0, 0, 1])
            dc = fbits(random.choice([0.0, random.uniform(0, 999), 12.0]))
            a4, a5, a8, a9 = random.choice([0, 1, 1]), random.getrandbits(8), random.choice([0, 1, 1]), random.choice([0, 1])
            maximum = random.choice([0, 100, random.randint(1, 900)])
            health = random.randint(0, max(maximum, 1)) if maximum else random.randint(0, 999)
            if maximum and random.random() < 0.4:
                health = maximum * random.choice([1, 2, 3]) // 4  # Exact colour thresholds when divisible.
            health = min(health, 999)
            bc = fbits(rfloat())
            w32(u, body + 8, owner)
            w32(u, body + 0xE0, e0); w32(u, body + 0xDC, dc)
            u.mem_write(body + 0xA4, bytes([a4, a5])); u.mem_write(body + 0xA8, bytes([a8, a9]))
            u.mem_write(body + 0x94, struct.pack('<HH', health, maximum))
            w32(u, body + 0xBC, bc)
            w32(u, body + 0x98, sprites[0]); w32(u, body + 0x9C, sprites[1]); w32(u, body + 0xA0, sprites[2]); w32(u, body + 0xD8, sprites[3])
            values = [fbits(rfloat()) for _ in range(8)]
            for off, b in zip((8, 0xC, 0x10, 0x34, 0x38, 0x1C4, 0x1C8, 0x1CC), values):
                w32(u, owner + off, b)
            w32(u, owner + 0x88, 0)
            tag = random.randint(0, 5)
            time = fbits(random.choice([0.0, random.uniform(0, 4)]))
            flag = random.choice([0, 1])
            timer = fbits(random.choice([0.0, 1.0, -1.0]))
            blocked = random.choice([0, 0, 1])
            has_attributes = random.choice([0, 1])
            attribute_word = random.choice([0, 1])
            u.mem_write(owner + 6, struct.pack('<H', tag))
            w32(u, owner + 0x924, time); u.mem_write(owner + 0x969, bytes([flag])); w32(u, owner + 0x978, timer); w32(u, owner + 0x98C, blocked)
            w32(u, attributes + 0xB8, attribute_word)
            w32(u, owner + 0x964, attributes if has_attributes else 0)
            has_attached = random.choice([0, 1, 1])
            state = random.choice([0, 1, 1, 2, 3])
            count = random.randint(0, 3)
            u.mem_write(attached + 0x93D, bytes([state]))
            u.mem_write(attached + 0x944, struct.pack('<h', count))
            rows = []
            for r in range(count):
                key, cls = random.randint(0, 5), random.choice([1, 3, 3, 7])
                u.mem_write(attached + 0x948 + 0x10 * r, struct.pack('<HH', key, 0))
                u.mem_write(attached + 0x94C + 0x10 * r, struct.pack('<H', cls))
                rows.append('%d %d' % (key, cls))
            w32(u, body + 0x48, attached if has_attached else 0)
            run(u, 0x0001DF70, draw_kind, 0, body, [])
            line.append('%d %u %u %u %u %u %u %u %u %u %s %d %u %d %u %d %d %d %d %d %d %s' % (draw_kind, e0, dc, a4, a5, a8, a9, health, maximum, bc,
                ' '.join(map(str, values)), tag, time, flag, timer, blocked, has_attributes, attribute_word, has_attached, state, count, ' '.join(rows)))
        n = r32(u, COUNT)
        text = '%d' % n
        for i in range(n):
            e = QUEUE + 0x28 * i
            s = r32(u, e)
            sprite = (s - sprites[0]) // 0x100
            text += ' %d:%08X,%08X,%08X:%08X,%08X:%08X:%08X:%u' % ((sprite,) + tuple(r32(u, e + 4 + 4 * k) for k in range(8)))
        expected.append(text)
        lines.append(kind + ' ' + ' '.join(line))
    exe = sys.argv[3] if len(sys.argv) > 3 else harness('overlay_harness')
    got = subprocess.run([exe], input='\n'.join(lines) + '\n', capture_output=True, text=True).stdout.splitlines()
    bad = 0
    for line, want, have in zip(lines, expected, got):
        if want.strip() != have.strip():
            bad += 1
            if bad < 4:
                print('MISMATCH', line[:200], '\n exp', want[:400], '\n got', have[:400])
    print('cases', len(lines), len(got), 'mismatches', bad)

if __name__ == '__main__':
    main()
