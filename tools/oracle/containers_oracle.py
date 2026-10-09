"""Differential check: validator node, list and tree container helpers."""
import random, subprocess, sys
from retail_cpu import *
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EAX

R = lambda: random.getrandbits(32)
DESTROY_STUB = 0x00E00000
XMEMALLOC = 0x000F7270
TREE = 0x001A4D38

def tree_text(u, node):
    if node == 0:
        return ' .'
    text = ' (%08X %d' % (r32(u, node + 4), struct.unpack('<i', struct.pack('<I', r32(u, node + 0x14)))[0])
    return text + tree_text(u, r32(u, node + 0xC)) + tree_text(u, r32(u, node + 0x10)) + ')'

def random_bst(n):
    """Returns parent/first/second index triples for a random binary tree rooted at node 0."""
    parent, first, second = [-1] * n, [-1] * n, [-1] * n
    for index in range(1, n):
        while True:
            at = random.randrange(index)
            side = random.randint(0, 1)
            slot = first if side == 0 else second
            if slot[at] == -1:
                slot[at] = index
                parent[index] = at
                break
    return parent, first, second

def in_order(first, second, node, out):
    if node < 0:
        return
    in_order(first, second, first[node], out)
    out.append(node)
    in_order(first, second, second[node], out)

def main():
    random.seed(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    u = new_emu()
    u.mem_map(DESTROY_STUB, 0x1000)
    u.mem_write(DESTROY_STUB, b'\xc2\x04\x00')
    u.mem_map(0x000F7000, 0x1000)
    u.mem_write(XMEMALLOC, b'\xc2\x08\x00')
    state = {'destroyed': [], 'alloc_next': HEAP + 0x40000, 'alloc_calls': 0, 'failures': []}

    def hook(uc, address, size, user):
        if address == DESTROY_STUB:
            state['destroyed'].append(uc.reg_read(UC_X86_REG_ECX))
        elif address == XMEMALLOC:
            call_index = state['alloc_calls']
            state['alloc_calls'] += 1
            if call_index in state['failures']:
                uc.reg_write(UC_X86_REG_EAX, 0)
            else:
                uc.reg_write(UC_X86_REG_EAX, state['alloc_next'])
                state['alloc_next'] += 0x20
    u.hook_add(UC_HOOK_CODE, hook, begin=DESTROY_STUB, end=DESTROY_STUB)
    u.hook_add(UC_HOOK_CODE, hook, begin=XMEMALLOC, end=XMEMALLOC)

    lines, expected = [], []
    for _ in range(int(sys.argv[2]) if len(sys.argv) > 2 else 600):
        kind = random.choice(['init', 'height', 'tree', 'pass', 'last', 'list', 'single', 'link', 'base'])
        if kind == 'init':
            value, data = R(), R()
            parent = HEAP + 0x900
            out = []
            for index, function in enumerate([0x001902D9, 0x001903E1, 0x0019042A]):
                node = HEAP + 0x40 * index
                for word in range(6):
                    w32(u, node + 4 * word, 9)
                w32(u, node + 0xC, parent)
                w32(u, node + 0x10, parent)
                returned = call(u, function, node, [value, 0 if index == 1 else parent])
                out.append('%08X %08X %d %d %d %d %d' % (r32(u, node), r32(u, node + 4), r32(u, node + 8) == parent,
                    r32(u, node + 0xC) == 0, r32(u, node + 0x10) == 0, r32(u, node + 0x14), returned == node))
            node = HEAP + 0x200
            for word in range(7):
                w32(u, node + 4 * word, 9)
            w32(u, node + 0x10, parent)
            w32(u, node + 0x14, parent)
            returned = call(u, 0x00190473, node, [value, data, parent])
            out.append('%08X %08X %08X %d %d %d %d %d' % (r32(u, node), r32(u, node + 4), r32(u, node + 8), r32(u, node + 0xC) == parent,
                r32(u, node + 0x10) == 0, r32(u, node + 0x14) == 0, r32(u, node + 0x18), returned == node))
            lines.append('init %d %d' % (value, data))
            expected.append(' '.join(out))
        elif kind == 'height':
            has_first, has_second = random.randint(0, 1), random.randint(0, 1)
            first_height = random.choice([random.randint(-3, 5), struct.unpack('<i', struct.pack('<I', R()))[0], 0x7FFFFFFF])
            second_height = random.choice([random.randint(-3, 5), first_height, -0x80000000])
            node, first, second = HEAP, HEAP + 0x40, HEAP + 0x80
            w32(u, first + 0x18, first_height)
            w32(u, second + 0x18, second_height)
            w32(u, node + 0x10, first if has_first else 0)
            w32(u, node + 0x14, second if has_second else 0)
            result = struct.unpack('<i', struct.pack('<I', call(u, 0x0019051C, node, [])))[0]
            lines.append('height %d %d %d %d' % (has_first, first_height, has_second, second_height))
            expected.append('%d' % result)
        elif kind == 'tree':
            n = random.randint(1, 9)
            parent, first, second = random_bst(n)
            nodes = [HEAP + 0x20 * index for index in range(n)]
            for index in range(n):
                w32(u, nodes[index] + 8, 0 if parent[index] < 0 else nodes[parent[index]])
                w32(u, nodes[index] + 0xC, 0 if first[index] < 0 else nodes[first[index]])
                w32(u, nodes[index] + 0x10, 0 if second[index] < 0 else nodes[second[index]])
            order = []
            in_order(first, second, 0, order)
            candidates = [index for index in range(n) if index != order[0] or index == 0]
            queries = [random.choice(candidates) for _ in range(4)]
            iterator = HEAP + 0x800
            out = []
            for query in queries:
                w32(u, iterator, nodes[query])
                call(u, 0x00190337, iterator, [])
                value = r32(u, iterator)
                out.append(str(nodes.index(value) if value else -1))
            lines.append('tree %d %s %d %s' % (n, ' '.join('%d %d %d' % (parent[i], first[i], second[i]) for i in range(n)),
                len(queries), ' '.join(map(str, queries))))
            expected.append(' '.join(out))
        elif kind == 'pass':
            n = random.randint(0, 7)
            nodes = [HEAP + 0x20 * index for index in range(n)]
            description = []
            for index in range(n):
                key = random.choice([R(), random.randint(-5, 5) & 0xFFFFFFFF])
                first = random.choice([-1] + list(range(index + 1, n)))
                second = random.choice([-1] + list(range(index + 1, n)))
                w32(u, nodes[index] + 4, key)
                w32(u, nodes[index] + 0x10, 0 if first < 0 else nodes[first])
                w32(u, nodes[index] + 0x14, 0 if second < 0 else nodes[second])
                description.append('%d %d %d' % (key, first, second))
            tree = HEAP + 0x800
            w32(u, tree + 8, nodes[0] if n else 0)
            keys = [random.choice([R(), random.randint(-5, 5) & 0xFFFFFFFF]) for _ in range(4)]
            if n:
                keys += [r32(u, nodes[random.randrange(n)] + 4) for _ in range(2)]
            out = []
            for key in keys:
                found = call(u, 0x00190B12, tree, [key])
                out.append(str(nodes.index(found) if found else -1))
            lines.append('pass %d %s %d %s' % (n, ' '.join(description), len(keys), ' '.join(map(str, keys))))
            expected.append(' '.join(out))
        elif kind == 'last':
            n = random.randint(0, 4)
            stored_count = random.choice([n, 0, 1, R()]) if n else random.choice([0, 0, 1])
            if n == 0:
                stored_count = 0
            owner = HEAP + 0x900
            nodes = [HEAP + 0x20 * index for index in range(n)]
            values = [R() for _ in range(n)]
            values_address = HEAP + 0x600
            for index in range(n):
                w32(u, values_address + 4 * index, values[index])
                w32(u, nodes[index] + 4, values_address + 4 * index)
                w32(u, nodes[index] + 0xC, nodes[index + 1] if index + 1 < n else 0)
            w32(u, owner + 0x18, nodes[0] if n else 0)
            w32(u, owner + 0x1C, stored_count)
            result = call(u, 0x00190CA6, owner, [])
            lines.append('last %d %d %s' % (stored_count, n, ' '.join(map(str, values))))
            expected.append('%08X' % (r32(u, result) if result else 0))
        elif kind == 'list':
            n = random.randint(0, 5)
            vtable = HEAP + 0xA00
            w32(u, vtable, DESTROY_STUB)
            nodes = [HEAP + 0x20 * index for index in range(n)]
            for index in range(n):
                w32(u, nodes[index], vtable)
                w32(u, nodes[index] + 8, nodes[index - 1] if index > 0 else 0)
                w32(u, nodes[index] + 0xC, nodes[index + 1] if index + 1 < n else 0)
            owner = HEAP + 0x900
            w32(u, owner, 0)
            w32(u, owner + 4, nodes[0] if n else 0)
            w32(u, owner + 8, nodes[-1] if n else 0)
            w32(u, owner + 0xC, n)
            state['destroyed'] = []
            call(u, 0x001909B1, owner, [])
            first, last = r32(u, owner + 4), r32(u, owner + 8)
            text = '%08X %d %d %u |' % (r32(u, owner), nodes.index(first) if first else -1, nodes.index(last) if last else -1, r32(u, owner + 0xC))
            text += ''.join(' %d' % nodes.index(node) for node in state['destroyed'])
            lines.append('list %d' % n)
            expected.append(text)
        elif kind == 'single':
            channel = random.choice([0, 1, 2, 3, 4, 5, 31, 32, 33, R()])
            mask = HEAP
            u.mem_write(mask, b'\0' * 4 + b'\x09' * 4)
            returned = call(u, 0x0018F314, mask, [channel])
            lines.append('single %d' % channel)
            expected.append('%08X %s %d' % (r32(u, mask), bytes(u.mem_read(mask + 4, 4)).hex().upper(), returned == mask))
        elif kind == 'link':
            has_previous, has_next = random.randint(0, 1), random.randint(0, 1)
            node, previous, next_node, value = HEAP, HEAP + 0x40, HEAP + 0x80, HEAP + 0xC0
            for address in (node, previous, next_node):
                u.mem_write(address, b'\0' * 0x10)
            returned = call(u, 0x0018F1E5, node, [value, previous if has_previous else 0, next_node if has_next else 0])
            lines.append('link %d %d' % (has_previous, has_next))
            expected.append('%d %d %d %d %d %d' % (r32(u, node + 4) == value and r32(u, node) == 0x002037A4,
                r32(u, node + 8) == (previous if has_previous else 0), r32(u, node + 0xC) == (next_node if has_next else 0),
                r32(u, previous + 0xC) == node, r32(u, next_node + 8) == node, returned == node))
        else:
            count = random.randint(1, 6)
            failures = [0] if random.random() < 0.2 else []
            state['failures'] = failures
            state['alloc_calls'] = 0
            w32(u, TREE + 8, 0)
            w32(u, TREE + 0xC, 0)
            out = []
            addresses = []
            for index in range(count):
                instruction = HEAP + 0x100 + 0x50 * index
                address = random.choice([instruction, random.choice(addresses) if addresses else instruction])
                for word in range(0x14):
                    w32(u, instruction + 4 * word, 0)
                for offset in (0x14, 0x1C, 0x24):
                    w32(u, instruction + offset, 1)
                    u.mem_write(instruction + offset + 4, b'\x07' * 4)
                # The registered address is the object pointer itself in retail.
                returned = call(u, 0x001929B7, instruction, [])
                addresses.append(instruction)
                out.append('%08X %08X %02X%02X%02X%02X %08X %08X %d' % (r32(u, instruction), r32(u, instruction + 0x14),
                    u.mem_read(instruction + 0x18, 1)[0], u.mem_read(instruction + 0x1B, 1)[0], u.mem_read(instruction + 0x21, 1)[0],
                    u.mem_read(instruction + 0x2A, 1)[0], r32(u, instruction + 0x1C), r32(u, instruction + 0x24), returned == instruction))
            state['failures'] = []
            lines.append('base %d %d %s %s' % (count, len(failures), ' '.join(map(str, failures)),
                ' '.join(str(address) for address in addresses)))
            expected.append(' '.join(out) + ' | %u' % r32(u, TREE + 0xC) + tree_text(u, r32(u, TREE + 8)))
    got = subprocess.run([harness('containers_harness')], input='\n'.join(lines) + '\n', capture_output=True, text=True).stdout.splitlines()
    bad = 0
    for line, want, have in zip(lines, expected, got):
        if want.strip() != have.strip():
            bad += 1
            if bad < 6:
                print('MISMATCH', line, '\n exp', want, '\n got', have)
    print('cases', len(lines), len(got), 'mismatches', bad)

if __name__ == '__main__':
    main()
