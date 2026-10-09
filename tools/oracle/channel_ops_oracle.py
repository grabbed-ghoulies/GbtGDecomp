"""Differential check: channel-order helpers 0x0018E5E2..0x0018F394."""
import random, subprocess, sys
from retail_cpu import *

R = lambda: random.getrandbits(32)

def channel():
    return random.choice([0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 5, R()])

def main():
    random.seed(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    u = new_emu()
    order_address, other_address, mask_address = HEAP, HEAP + 0x40, HEAP + 0x80
    result_address = HEAP + 0x100
    lines, expected = [], []
    for _ in range(int(sys.argv[2]) if len(sys.argv) > 2 else 2000):
        order = [channel() for _ in range(4)]
        if random.random() < 0.3:
            order = random.sample([0, 1, 2, 3], 4)
        other = [channel() for _ in range(4)]
        mask = [random.choice([0, 0, 1, 0x80, 0xFF]) for _ in range(4)]
        w32(u, order_address, 0x00203794)
        for index, value in enumerate(order):
            w32(u, order_address + 4 + 4 * index, value)
        u.mem_write(mask_address, b'\x90\x37\x20\x00' + bytes(mask))
        out = []
        out.append('%d' % (call(u, 0x0018E6DF, order_address, []) & 0xFF))
        out.append('%d' % (call(u, 0x0018F394, order_address, []) & 0xFF))
        out.append('%u' % call(u, 0x0018E5E2, order_address, []))
        out.append('%u' % call(u, 0x0018E69C, order_address, []))
        out.append('%d' % (call(u, 0x0018E741, order_address, [0x00203794] + other) & 0xFF))
        u.mem_write(result_address, b'\0' * 4 + b'\x09' * 4)
        returned = call(u, 0x0018E548, mask_address, [result_address])
        out.append('%08X %s %d' % (r32(u, result_address), bytes(u.mem_read(result_address + 4, 4)).hex().upper(), returned == result_address))
        for position in range(5):
            out.append('%d' % (call(u, 0x0018E604, order_address, [position]) & 0xFF))
        out.append('%u' % call(u, 0x0018E63F, order_address, []))
        u.mem_write(result_address, b'\0' * 20)
        returned = call(u, 0x0018E700, order_address, [result_address])
        out.append(' '.join('%08X' % r32(u, result_address + 4 * index) for index in range(5)) + ' %d' % (returned == result_address))
        u.mem_write(result_address, b'\0' * 4 + b'\x09' * 4)
        returned = call(u, 0x0018F355, order_address, [result_address])
        out.append('%08X %s %d' % (r32(u, result_address), bytes(u.mem_read(result_address + 4, 4)).hex().upper(), returned == result_address))
        call(u, 0x0018E661, order_address, [])
        out.append(' '.join('%08X' % r32(u, order_address + 4 + 4 * index) for index in range(4)))
        expected.append(' '.join(out))
        lines.append(' '.join(str(value) for value in order + other + mask))
    got = subprocess.run([harness('channel_ops_harness')], input='\n'.join(lines) + '\n', capture_output=True, text=True).stdout.splitlines()
    bad = 0
    for line, want, have in zip(lines, expected, got):
        if want != have:
            bad += 1
            if bad < 6:
                print('MISMATCH', line, '\n exp', want, '\n got', have)
    print('cases', len(lines), len(got), 'mismatches', bad)

if __name__ == '__main__':
    main()
