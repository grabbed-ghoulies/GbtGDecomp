"""Differential check: balanced/pass tree template instances (join, erase, unregister, insert)."""
import random, struct, subprocess, sys
from retail_cpu import *
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX

R = lambda: random.getrandbits(32)
XMEMALLOC = 0x000F7270
DESTROY_STUB = 0x00E00000
JOINS = [0x0019164F, 0x00191714, 0x00191806, 0x001918CB, 0x00191B1C]
ERASES = [0x0019204A, 0x0019214A, 0x0019224A, 0x0019234A, 0x00192529]
UNREGS = [0x001926FB, 0x001927B9]
INSERTS = [0x001922BB, 0x0019259A]

def signed(value):
    return struct.unpack('<i', struct.pack('<I', value & 0xFFFFFFFF))[0]

def build_tree(keys, signed_keys):
    """Inserts keys into a plain BST and returns parent/first/second/height lists (root is node 0)."""
    n = len(keys)
    parent, first, second = [-1] * n, [-1] * n, [-1] * n
    key = (lambda v: signed(v)) if signed_keys else (lambda v: v)
    for index in range(1, n):
        at = 0
        while True:
            if key(keys[index]) < key(keys[at]):
                if first[at] < 0:
                    first[at] = index; parent[index] = at; break
                at = first[at]
            else:
                if second[at] < 0:
                    second[at] = index; parent[index] = at; break
                at = second[at]
    def height(i):
        return 0 if i < 0 else 1 + max(height(first[i]), height(second[i]))
    return parent, first, second, [height(i) for i in range(n)]

def main():
    random.seed(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    u = new_emu()
    u.mem_map(0x000F7000, 0x1000)
    u.mem_write(XMEMALLOC, b'\xc2\x08\x00')
    u.mem_map(DESTROY_STUB, 0x1000)
    u.mem_write(DESTROY_STUB, b'\xc2\x04\x00')
    state = {'next': 0, 'calls': 0, 'fail': 0, 'destroyed': []}
    def allocate(uc, address, size, user):
        call_index = state['calls']
        state['calls'] += 1
        if call_index == 0 and state['fail']:
            uc.reg_write(UC_X86_REG_EAX, 0)
        else:
            uc.reg_write(UC_X86_REG_EAX, state['next'])
            state['next'] += 0x20
    def destroy(uc, address, size, user):
        state['destroyed'].append(uc.reg_read(UC_X86_REG_ECX))
    u.hook_add(UC_HOOK_CODE, allocate, begin=XMEMALLOC, end=XMEMALLOC)
    u.hook_add(UC_HOOK_CODE, destroy, begin=DESTROY_STUB, end=DESTROY_STUB)

    lines, expected = [], []
    for _ in range(int(sys.argv[2]) if len(sys.argv) > 2 else 800):
        kind = random.choice(['join', 'erase', 'unreg', 'insert', 'pass'])
        pass_layout = kind == 'pass'
        n = random.randint(0 if kind in ('insert', 'pass') else 1, 9)
        if kind in ('insert', 'pass') and n == 0:
            n = 1
        keys = random.sample(range(1, 40), n) if random.random() < 0.7 else [R() for _ in range(n)]
        parent, first, second, heights = build_tree(keys, pass_layout)
        stride = 0x20
        nodes = [HEAP + stride * i for i in range(64)]
        vtable = HEAP + 0x3000
        w32(u, vtable, DESTROY_STUB)
        for i in range(64):
            for word in range(8):
                w32(u, nodes[i] + 4 * word, 0)
        if pass_layout:
            offsets = (0xC, 0x10, 0x14, 0x18)
        else:
            offsets = (0x8, 0xC, 0x10, 0x14)
        for i in range(n):
            w32(u, nodes[i], vtable)
            w32(u, nodes[i] + 4, keys[i])
            for off, link in zip(offsets[:3], (parent[i], first[i], second[i])):
                w32(u, nodes[i] + off, 0 if link < 0 else nodes[link])
            w32(u, nodes[i] + offsets[3], heights[i])
        tree = HEAP + 0x2800
        w32(u, tree + 8, nodes[0] if n else 0)
        w32(u, tree + 0xC, n)
        state['next'] = nodes[n]
        state['calls'] = 0
        state['fail'] = 0
        state['destroyed'] = []
        description = ' '.join('%d %d %d %d %d' % (keys[i], parent[i], first[i], second[i], heights[i]) for i in range(n))
        function = random.randrange(5)
        index = lambda value: nodes.index(value) if value else -1
        if kind == 'join':
            target = random.randrange(n)
            args = [parent[target], first[target], second[target]]
            result = index(call(u, JOINS[function], 0, [0 if a < 0 else nodes[a] for a in args]))
            line = '%d %d %d' % tuple(args)
        elif kind == 'erase':
            target = random.randrange(n)
            result = index(call(u, ERASES[function], tree, [nodes[target]]))
            line = '%d' % target
        elif kind == 'unreg':
            function = random.randrange(2)
            value = random.choice(keys + [R()])
            call(u, UNREGS[function], tree, [value])
            result = -2
            line = '%d' % value
        else:
            function = random.randrange(2)
            state['fail'] = 1 if random.random() < 0.15 else 0
            key = random.choice(keys + [random.randint(1, 40), R()])
            start = 0
            if pass_layout:
                result = index(call(u, 0x00191A82, tree, [key, 0x5555, nodes[start]]))
            else:
                result = index(call(u, INSERTS[function], tree, [key, nodes[start]]))
            line = '%d %d %d' % (key, start, state['fail'])
        used = (state['next'] - nodes[0]) // stride
        text = '%d %d %u |' % (result, index(r32(u, tree + 8)), r32(u, tree + 0xC))
        if pass_layout:
            for i in range(used):
                text += ' %08X %d %d %d %d %d' % (r32(u, nodes[i] + 4), index(r32(u, nodes[i] + 0xC)), index(r32(u, nodes[i] + 0x10)),
                    index(r32(u, nodes[i] + 0x14)), signed(r32(u, nodes[i] + 0x18)), 1 if i >= n and r32(u, nodes[i] + 8) == 0x5555 else 0)
        else:
            text += ''.join(' d%d' % (nodes.index(d) * 10 + 1) for d in state['destroyed'])
            for i in range(used):
                vt = r32(u, nodes[i])
                text += ' %08X %08X %d %d %d %d' % (0 if vt == vtable else vt, r32(u, nodes[i] + 4), index(r32(u, nodes[i] + 8)),
                    index(r32(u, nodes[i] + 0xC)), index(r32(u, nodes[i] + 0x10)), signed(r32(u, nodes[i] + 0x14)))
        expected.append(text)
        lines.append('%s %d %d %d %s %s' % (kind, function, n, 0 if n else -1, description, line))
    got = subprocess.run([harness('trees_harness')], input='\n'.join(lines) + '\n', capture_output=True, text=True).stdout.splitlines()
    bad = 0
    for line, want, have in zip(lines, expected, got):
        if want.strip() != have.strip():
            bad += 1
            if bad < 6:
                print('MISMATCH', line, '\n exp', want, '\n got', have)
    print('cases', len(lines), len(got), 'mismatches', bad)

if __name__ == '__main__':
    main()
