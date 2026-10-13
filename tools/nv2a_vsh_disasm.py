# Disassembles NV2A vertex programs (0x2078 header, 4 words per instruction) from lines of the form
# "TRACE prog <handle>: <hex words...>". Usage: python nv2a_vsh_disasm.py log.txt [handle...]
import sys

def f(w, d, pos, size):
    return (w[d] >> pos) & ((1 << size) - 1)

MAC = ['nop', 'mov', 'mul', 'add', 'mad', 'dp3', 'dph', 'dp4', 'dst', 'min', 'max', 'slt', 'sge', 'arl', 'm14', 'm15']
ILU = ['nop', 'mov', 'rcp', 'rcc', 'rsq', 'exp', 'log', 'lit']
SW = 'xyzw'
OUT = {0: 'oPos', 3: 'oD0', 4: 'oD1', 5: 'oFog', 6: 'oPts', 7: 'oB0', 8: 'oB1', 9: 'oT0', 10: 'oT1', 11: 'oT2', 12: 'oT3'}


def mask(m):
    return ''.join(c for i, c in enumerate('xyzw') if m & (8 >> i))


def src(w, which):
    if which == 'a':
        neg, sw, r, mux = f(w, 1, 8, 1), [f(w, 1, 6, 2), f(w, 1, 4, 2), f(w, 1, 2, 2), f(w, 1, 0, 2)], f(w, 2, 28, 4), f(w, 2, 26, 2)
    elif which == 'b':
        neg, sw, r, mux = f(w, 2, 25, 1), [f(w, 2, 23, 2), f(w, 2, 21, 2), f(w, 2, 19, 2), f(w, 2, 17, 2)], f(w, 2, 13, 4), f(w, 2, 11, 2)
    else:
        neg, sw = f(w, 2, 10, 1), [f(w, 2, 8, 2), f(w, 2, 6, 2), f(w, 2, 4, 2), f(w, 2, 2, 2)]
        r = (f(w, 2, 0, 2) << 2) | f(w, 3, 30, 2)
        mux = f(w, 3, 28, 2)
    if mux == 1:
        reg = 'r%d' % r
    elif mux == 2:
        reg = 'v%d' % f(w, 1, 9, 4)
    elif mux == 3:
        c = f(w, 1, 13, 8) - 96
        reg = 'c[%s%d]' % ('a0.x+' if f(w, 3, 1, 1) else '', c)
    else:
        reg = '?%d' % mux
    s = ''.join(SW[i] for i in sw)
    return ('-' if neg else '') + reg + ('' if s == 'xyzw' else '.' + s)


def dis(words):
    count = words[0] >> 16
    for i in range(count):
        w = words[1 + i * 4: 5 + i * 4]
        mac, ilu = f(w, 1, 21, 4), f(w, 1, 25, 3)
        macm, outr, ilum, om, orb, addr, omux = f(w, 3, 24, 4), f(w, 3, 20, 4), f(w, 3, 16, 4), f(w, 3, 12, 4), f(w, 3, 11, 1), f(w, 3, 3, 8), f(w, 3, 2, 1)
        parts = []
        if mac:
            dst = []
            if macm:
                dst.append('r%d.%s' % (outr, mask(macm)))
            if om and omux == 0:
                dst.append(('%s' % OUT.get(addr, 'o%d' % addr) if orb else 'c[%d]' % (addr - 96)) + '.' + mask(om))
            if mac == 13:
                dst.append('a0.x')
            name = MAC[mac]
            ins = {1: 'a', 2: 'ab', 3: 'ac', 4: 'abc', 5: 'ab', 6: 'ab', 7: 'ab', 8: 'ab', 9: 'ab', 10: 'ab', 11: 'ab', 12: 'ab', 13: 'a'}.get(mac, 'abc')
            parts.append('%s %s, %s' % (name, ','.join(dst), ', '.join(src(w, x) for x in ins)))
        if ilu:
            dst = []
            if ilum:
                dst.append('r%d.%s' % (1 if mac else outr, mask(ilum)))
            if om and omux == 1:
                dst.append(('%s' % OUT.get(addr, 'o%d' % addr) if orb else 'c[%d]' % (addr - 96)) + '.' + mask(om))
            parts.append('%s %s, %s' % (ILU[ilu], ','.join(dst), src(w, 'c')))
        print('%3d  %s%s' % (i, ' + '.join(parts) or 'nop', '  [final]' if f(w, 3, 0, 1) else ''))


for line in open(sys.argv[1], errors='replace'):
    if line.startswith('TRACE prog') and (len(sys.argv) < 3 or line.split()[2].rstrip(':') in sys.argv[2:]):
        print(line.split(':')[0])
        dis([int(x, 16) for x in line.split(':')[1].split()])
