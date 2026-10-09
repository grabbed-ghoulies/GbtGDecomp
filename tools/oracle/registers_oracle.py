"""Differential check: pass-tree balancing, register tables, owner equivalence, masks and list append."""
import random, struct, subprocess, sys
from retail_cpu import *
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX

R = lambda: random.getrandbits(32)
XMEMALLOC = 0x000F7270
DESTROY_STUB = 0x00E00000
ACTIVE_OWNER = 0x001A4D34

def signed(value):
    return struct.unpack('<i', struct.pack('<I', value & 0xFFFFFFFF))[0]

def random_tree(n):
    parent, first, second = [-1] * n, [-1] * n, [-1] * n
    for index in range(1, n):
        while True:
            at = random.randrange(index)
            slot = first if random.randint(0, 1) == 0 else second
            if slot[at] == -1:
                slot[at] = index
                parent[index] = at
                break
    return parent, first, second

def main():
    random.seed(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    u = new_emu()
    u.mem_map(0x000F7000, 0x1000)
    u.mem_write(XMEMALLOC, b'\xc2\x08\x00')
    state = {'next': HEAP + 0x40000, 'calls': 0, 'failures': [], 'order': [], 'destroyed': []}

    def allocate(uc, address, size, user):
        call_index = state['calls']
        state['calls'] += 1
        if call_index in state['failures']:
            uc.reg_write(UC_X86_REG_EAX, 0)
        else:
            uc.reg_write(UC_X86_REG_EAX, state['next'])
            state['order'].append(state['next'])
            state['next'] += 0x20
    u.hook_add(UC_HOOK_CODE, allocate, begin=XMEMALLOC, end=XMEMALLOC)
    u.mem_map(DESTROY_STUB, 0x1000)
    u.mem_write(DESTROY_STUB, b'\xc2\x04\x00')
    def destroy(uc, address, size, user):
        state['destroyed'].append(uc.reg_read(UC_X86_REG_ECX))
    u.hook_add(UC_HOOK_CODE, destroy, begin=DESTROY_STUB, end=DESTROY_STUB)

    lines, expected = [], []
    for _ in range(int(sys.argv[2]) if len(sys.argv) > 2 else 600):
        kind = random.choice(['rotate', 'heights', 'balance', 'rebalance', 'join', 'join', 'erase', 'erase', 'tables', 'equal', 'masks', 'append', 'instr', 'setops', 'tomove', 'tomove', 'reach', 'reach', 'reach', 'swap', 'swap', 'swap', 'diag', 'diag'])
        if kind in ('rotate', 'heights', 'balance', 'rebalance', 'join', 'erase'):
            n = random.randint(2, 9)
            parent, first, second = random_tree(n)
            heights = [random.randint(-2, 6) for _ in range(n)]
            if kind == 'rotate':
                direction = random.randint(0, 1)
                candidates = [i for i in range(n) if (second if direction == 0 else first)[i] >= 0]
                if not candidates:
                    continue
                target = random.choice(candidates)
            elif kind in ('rebalance', 'join', 'erase'):
                direction = {'rebalance': 3, 'join': 4, 'erase': 5}[kind]
                def height(i):
                    if i < 0:
                        return 0
                    return 1 + max(height(first[i]), height(second[i]))
                heights = [height(i) for i in range(n)]
                if kind == 'rebalance':
                    leaves = [i for i in range(1, n) if first[i] < 0 and second[i] < 0]
                    target = random.choice(leaves) if leaves else n - 1
                else:
                    target = random.randrange(n)
            elif kind == 'balance':
                direction = 2
                # Retail dereferences the heavier child, so heights must describe existing children.
                heights = [0] * n
                def height(i):
                    if i < 0:
                        return 0
                    return 1 + max(height(first[i]), height(second[i]))
                heights = [height(i) + random.choice([0, 0, 0, 1, -1]) if random.random() < 0.3 else height(i) for i in range(n)]
                target = random.randrange(n)
            else:
                direction = 0
                target = random.choice([-1] + list(range(n)))
            nodes = [HEAP + 0x20 * i for i in range(n)]
            for i in range(n):
                w32(u, nodes[i] + 0xC, 0 if parent[i] < 0 else nodes[parent[i]])
                w32(u, nodes[i] + 0x10, 0 if first[i] < 0 else nodes[first[i]])
                w32(u, nodes[i] + 0x14, 0 if second[i] < 0 else nodes[second[i]])
                w32(u, nodes[i] + 0x18, heights[i])
            if kind == 'heights':
                call(u, 0x00190554, 0 if target < 0 else nodes[target], [])
            elif kind == 'balance':
                try:
                    call(u, 0x00190BDD, nodes[target], [])
                except Exception:
                    continue
            elif kind == 'rebalance':
                tree = HEAP + 0x800
                w32(u, tree + 8, nodes[0])
                call(u, 0x00191990, tree, [nodes[target]])
                prefix = '%d | ' % nodes.index(r32(u, tree + 8))
            elif kind == 'erase':
                tree = HEAP + 0x800
                vtable = HEAP + 0xA00
                w32(u, vtable, DESTROY_STUB)
                for node in nodes:
                    w32(u, node, vtable)
                w32(u, tree + 8, nodes[0])
                w32(u, tree + 0xC, n)
                state['destroyed'] = []
                following = call(u, 0x00192469, tree, [nodes[target]])
                root = r32(u, tree + 8)
                destroyed = state['destroyed']
                prefix = '%d %d %u %d | ' % (nodes.index(following) if following else -1, nodes.index(root) if root else -1, r32(u, tree + 0xC),
                    nodes.index(destroyed[0]) * 10 + 1 if destroyed else -1)
            elif kind == 'join':
                joined = call(u, 0x001919BD, 0, [R(), r32(u, nodes[target] + 0x10), r32(u, nodes[target] + 0x14)])
                prefix = '%d | ' % (nodes.index(joined) if joined else -1)
            else:
                call(u, 0x0019060B if direction == 0 else 0x00190656, nodes[target], [])
            index = lambda value: nodes.index(value) if value else -1
            expected.append((prefix if kind in ('rebalance', 'join', 'erase') else '') + ' '.join('%d %d %d %d' % (index(r32(u, node + 0xC)), index(r32(u, node + 0x10)), index(r32(u, node + 0x14)),
                signed(r32(u, node + 0x18))) for node in nodes))
            lines.append('%s %d %d %d %s' % ('rotate' if kind in ('balance', 'rebalance', 'join', 'erase') else kind, n, target, direction,
                ' '.join('%d %d %d %d' % (parent[i], first[i], second[i], heights[i]) for i in range(n))))
        elif kind == 'tables':
            owner_count = random.randint(1, 6)
            owners = [HEAP + 0x1000 + 0x40 * i for i in range(owner_count)]
            links = []
            for i in range(owner_count):
                derived = random.choice([-1] + list(range(i + 1, owner_count)))
                channel = random.choice([-1] + list(range(i)))
                w32(u, owners[i] + 0x30, 0 if derived < 0 else owners[derived])
                w32(u, owners[i] + 0x34, 0 if channel < 0 else owners[channel])
                links.append('%d %d' % (derived, channel))
            tables = HEAP + 0x3000
            text = []
            for tree_offset, base in ((0x28, HEAP), (0x3C, HEAP + 0x800)):
                n = random.randint(0, 6)
                nodes = [base + 0x20 * i for i in range(n)]
                keys = random.sample(range(0, 14), n) if tree_offset == 0x28 else [random.choice([0x100, 0x100, 5, 0x200, R()]) for _ in range(n)]
                description = []
                for i in range(n):
                    first = random.choice([-1] + list(range(i + 1, n)))
                    second = random.choice([-1] + list(range(i + 1, n)))
                    data = random.choice([-1] + list(range(owner_count)))
                    w32(u, nodes[i] + 4, keys[i])
                    w32(u, nodes[i] + 8, 0 if data < 0 else owners[data])
                    w32(u, nodes[i] + 0x10, 0 if first < 0 else nodes[first])
                    w32(u, nodes[i] + 0x14, 0 if second < 0 else nodes[second])
                    description.append('%d %d %d %d' % (keys[i], first, second, data))
                w32(u, tables + tree_offset + 8, nodes[0] if n else 0)
                text.append('%d %s' % (n, ' '.join(description)))
            queries = [(random.choice([0, 1, 5, 12, 13, R(), random.randint(0, 13)]), random.choice([-1] + list(range(owner_count))))
                       for _ in range(4)]
            out = []
            for register_kind, owner in queries:
                found = call(u, 0x00191E8F, tables, [register_kind, R(), R()])
                kind_found = signed(call(u, 0x00191F00, tables, [0 if owner < 0 else owners[owner]]))
                # A null owner is dereferenced after a failed lookup, as in retail; only live owners are tested.
                kind_one = call(u, 0x00192629, tables, [owners[owner]]) & 0xFF if owner >= 0 else 0
                out.append('%d %d %d' % (owners.index(found) if found else -1, kind_found, kind_one))
            expected.append(' '.join(out))
            lines.append('tables %d %s %s %d %s' % (owner_count, ' '.join(links), ' '.join(text), len(queries),
                ' '.join('%d %d' % query for query in queries)))
        elif kind == 'equal':
            n = random.randint(1, 6)
            owners = [HEAP + 0x40 * i for i in range(n)]
            operands = [HEAP + 0x800 + 0x40 * i for i in range(n)]
            active = random.choice([-1] + list(range(n)))
            w32(u, ACTIVE_OWNER, 0 if active < 0 else owners[active])
            description = []
            for i in range(n):
                owner_kind = random.choice([3, 3, 3, 2, 0])
                source = random.choice([1, 2, R()])
                operand = random.choice([-1] + list(range(n)))
                parent = random.choice([-1] + list(range(n)))
                w32(u, owners[i] + 4, owner_kind)
                w32(u, owners[i] + 8, source)
                w32(u, owners[i] + 0xC, 0 if operand < 0 else operands[operand])
                w32(u, operands[i] + 0x10, 0 if parent < 0 else owners[parent])
                description.append('%d %d %d %d' % (owner_kind, source, operand, parent))
            queries = [(random.choice([-1] + list(range(n))), random.choice([-1] + list(range(n)))) for _ in range(5)]
            out = []
            for first, second in queries:
                out.append('%d' % (call(u, 0x0018F4A9, 0 if first < 0 else owners[first], [0 if second < 0 else owners[second]]) & 0xFF))
            expected.append(' '.join(out))
            lines.append('equal %d %d %s %d %s' % (n, active, ' '.join(description), len(queries), ' '.join('%d %d' % q for q in queries)))
        elif kind == 'masks':
            owner_channels = [random.choice([0, 0, 1, R()]) for _ in range(4)]
            operand_channels = [random.randint(0, 4) for _ in range(4)]
            mask = [random.choice([0, 1, 0x80]) for _ in range(4)]
            order = [random.randint(0, 4) for _ in range(4)]
            other = [random.choice([0, 1, 2, 3, 4, R()]) for _ in range(4)]
            owner, operand, result = HEAP, HEAP + 0x100, HEAP + 0x200
            for i in range(4):
                w32(u, owner + 0x20 + 4 * i, owner_channels[i])
                w32(u, operand + 0x1C + 4 * i, operand_channels[i])
            u.mem_write(result, b'\0' * 4 + b'\x09' * 4)
            returned_a = call(u, 0x0018F595, owner, [result])
            a = '%08X %s' % (r32(u, result), bytes(u.mem_read(result + 4, 4)).hex().upper())
            u.mem_write(result, b'\0' * 4 + b'\x09' * 4)
            returned_b = call(u, 0x0018F86B, operand, [result, 0x00203790, struct.unpack('<I', bytes(mask))[0]])
            b = '%08X %s' % (r32(u, result), bytes(u.mem_read(result + 4, 4)).hex().upper())
            order_address = HEAP + 0x300
            w32(u, order_address, 0x00203794)
            for i in range(4):
                w32(u, order_address + 4 + 4 * i, order[i])
            u.mem_write(result, b'\0' * 20)
            returned_c = call(u, 0x0018F436, order_address, [result, 0x00203794] + other)
            c = ' '.join('%08X' % r32(u, result + 4 * i) for i in range(5))
            expected.append('%s %s %s %d%d%d' % (a, b, c, returned_a == result, returned_b == result, returned_c == result))
            lines.append('masks %s' % ' '.join(str(v) for v in owner_channels + operand_channels + mask + order + other))
        elif kind == 'instr':
            vop = random.choice([0, 0, 1, 1, 3, 4, 0xD, 2, R(), 5, 6, 7, 8, 8, 9, 0xFFFFFFFF])
            sop = random.choice([0, 1, 1, 2, R(), 6, 6, 7, 7])
            flag = random.choice([0, 0, 1, 0x80])
            has_output = random.randint(0, 1)
            vector_destination, scalar_destination = random.randint(0, 3), random.randint(0, 3)
            destination = random.choice([vector_destination, scalar_destination, random.randint(0, 3)])
            markers = [random.choice([0, 1, 0xFF]) for _ in range(3)]
            kinds = [random.choice([0, 1, 2, 3, R()]) for _ in range(3)]
            has_parents = [random.randint(0, 1) for _ in range(3)]
            parent_channels = [[random.choice([0, 0xFFFFFFFF, 1, R()]) for _ in range(4)] for _ in range(3)]
            parents = [HEAP + 0x600 + 0x40 * i for i in range(3)]
            masks = [random.choice([0, 1, 0x80]) for _ in range(8)]
            instruction, owners, operands = HEAP, [HEAP + 0x100 + 0x40 * i for i in range(3)], [HEAP + 0x300 + 0x40 * i for i in range(3)]
            owner_address = lambda index: owners[index] if index < 3 else 0
            for word in range(0x13):
                w32(u, instruction + 4 * word, 0)
            for i in range(3):
                w32(u, instruction + 4 + 4 * i, operands[i])
                u.mem_write(operands[i] + 0x14, bytes([markers[i]]))
                w32(u, operands[i] + 0xC, kinds[i])
                w32(u, operands[i] + 4, instruction)
                w32(u, operands[i] + 0x10, parents[i] if has_parents[i] else 0)
                for j in range(4):
                    w32(u, parents[i] + 0x20 + 4 * j, parent_channels[i][j])
            u.mem_write(instruction + 0x10, bytes([flag]))
            w32(u, instruction + 0x14, 0x00203790)
            w32(u, instruction + 0x1C, 0x00203790)
            u.mem_write(instruction + 0x20, bytes(masks[:4]))
            w32(u, instruction + 0x24, 0x00203790)
            u.mem_write(instruction + 0x28, bytes(masks[4:]))
            w32(u, instruction + 0x2C, vop)
            w32(u, instruction + 0x30, sop)
            w32(u, instruction + 0x34, owners[2] if has_output else 0)
            w32(u, instruction + 0x38, owner_address(vector_destination))
            w32(u, instruction + 0x3C, owner_address(scalar_destination))
            slots = ''.join('%d' % (call(u, 0x0018E988, instruction, [slot]) & 0xFF) for slot in range(4))
            slots += ' ' + ''.join('%d' % (call(u, 0x0018E9DC, instruction, [slot, channel]) & 0xFF) for slot in range(4) for channel in range(4))
            slots += ' ' + ''.join('%d' % (call(u, 0x0018F848, operands[i], [channel]) & 0xFF) for i in range(3) for channel in range(4))
            result = HEAP + 0x900
            for slot in range(4):
                u.mem_write(result, b'\0' * 4 + b'\x09' * 4)
                call(u, 0x0018EB4C, instruction, [result, slot])
                slots += ' %08X%s' % (r32(u, result), bytes(u.mem_read(result + 4, 4)).hex().upper())
            slots += ' %d' % (call(u, 0x0018EB96, instruction, []) & 0xFF)
            negated = call(u, 0x0018ED95, instruction, [owner_address(destination)]) & 0xFF
            converted = call(u, 0x0018FF4B, instruction, []) & 0xFF
            owner_index = lambda value: owners.index(value) if value else -1
            text = '%s %d %d %u %u %u %d %d' % (slots, negated, converted, r32(u, instruction + 0x2C), r32(u, instruction + 0x30),
                u.mem_read(instruction + 0x10, 1)[0], owner_index(r32(u, instruction + 0x38)), owner_index(r32(u, instruction + 0x3C)))
            text += ''.join(' %d' % operands.index(r32(u, instruction + 4 + 4 * i)) for i in range(3))
            text += ''.join(' %u %u' % (u.mem_read(operands[i] + 0x14, 1)[0], r32(u, operands[i] + 0xC)) for i in range(3))
            text += ' %s %s' % (bytes(u.mem_read(instruction + 0x20, 4)).hex().upper(), bytes(u.mem_read(instruction + 0x28, 4)).hex().upper())
            expected.append(text)
            lines.append('instr %d %d %d %d %d %d %d %s %s' % (vop, sop, flag, has_output, vector_destination, scalar_destination, destination,
                ' '.join('%d %d %d %s' % (markers[i], kinds[i], has_parents[i], ' '.join(map(str, parent_channels[i]))) for i in range(3)), ' '.join(map(str, masks))))
        elif kind == 'tomove':
            n = random.randint(1, 5)
            writer_count = random.randint(1, 3)
            owners = [HEAP + 0x1000 + 0x40 * i for i in range(n)]
            writers = [HEAP + 0x2000 + 0x50 * i for i in range(writer_count)]
            check = random.choice([0, 1, 1, 1, 0x80])
            vop = random.choice([1, 1, 1, 0, 2])
            sop = random.choice([0, 0, 0, 1])
            flag = random.choice([0, 1, 0x80])
            vector_destination = random.choice([-1] + list(range(n)) * 3)
            u.mem_write(0x001A4D33, bytes([check]))
            description = []
            for i in range(writer_count):
                destination = random.choice([-1] + list(range(n)))
                w32(u, writers[i] + 0x3C, 0 if destination < 0 else owners[destination])
                description.append(str(destination))
            for i in range(n):
                derived = random.choice([-1] + list(range(i + 1, n)))
                channel_owner = random.choice([-1] + list(range(i)))
                codes = [random.choice([0, 0, -1, -1] + list(range(1, writer_count + 1))) for _ in range(4)]
                w32(u, owners[i] + 0x30, 0 if derived < 0 else owners[derived])
                w32(u, owners[i] + 0x34, 0 if channel_owner < 0 else owners[channel_owner])
                for j, code in enumerate(codes):
                    w32(u, owners[i] + 0x20 + 4 * j, 0 if code == 0 else 0xFFFFFFFF if code < 0 else writers[code - 1])
                description.append('%d %d %s' % (derived, channel_owner, ' '.join(map(str, codes))))
            instruction = HEAP
            operands = [HEAP + 0x300 + 0x40 * i for i in range(3)]
            for word in range(0x13):
                w32(u, instruction + 4 * word, 0)
            for i in range(3):
                w32(u, instruction + 4 + 4 * i, operands[i])
                w32(u, operands[i] + 0xC, i + 7)
            u.mem_write(instruction + 0x10, bytes([flag]))
            u.mem_write(instruction + 0x20, bytes([1, 0, 0x80, 0]))
            u.mem_write(instruction + 0x28, bytes([0, 1, 0, 1]))
            w32(u, instruction + 0x2C, vop)
            w32(u, instruction + 0x30, sop)
            w32(u, instruction + 0x38, 0 if vector_destination < 0 else owners[vector_destination])
            converted = call(u, 0x0018FFC9, instruction, []) & 0xFF
            owner_index = lambda value: owners.index(value) if value else -1
            text = '%d %u %u %u %d %d' % (converted, r32(u, instruction + 0x2C), r32(u, instruction + 0x30), u.mem_read(instruction + 0x10, 1)[0],
                owner_index(r32(u, instruction + 0x38)), owner_index(r32(u, instruction + 0x3C)))
            for i in range(3):
                operand = r32(u, instruction + 4 + 4 * i)
                text += ' %d %u' % (operands.index(operand), r32(u, operand + 0xC))
            text += ' %s %s' % (bytes(u.mem_read(instruction + 0x20, 4)).hex().upper(), bytes(u.mem_read(instruction + 0x28, 4)).hex().upper())
            expected.append(text)
            lines.append('tomove %d %d %d %d %d %d %d %s' % (n, writer_count, check, vop, sop, flag, vector_destination, ' '.join(description)))
        elif kind == 'reach':
            n = random.randint(1, 5)
            owners = [HEAP + 0x1000 + 0x40 * i for i in range(n)]
            description = []
            for i in range(n):
                channel_owner = random.choice([-1] + list(range(i)))
                channels = [random.choice([0, 0, 1, R()]) for _ in range(4)]
                w32(u, owners[i] + 0x34, 0 if channel_owner < 0 else owners[channel_owner])
                for j in range(4):
                    w32(u, owners[i] + 0x20 + 4 * j, channels[j])
                description.append('%d %s' % (channel_owner, ' '.join(map(str, channels))))
            reader, writer = HEAP, HEAP + 0x100
            operands = [HEAP + 0x300 + 0x40 * i for i in range(3)]
            for instruction in (reader, writer):
                for word in range(0x13):
                    w32(u, instruction + 4 * word, 0)
                masks = [random.choice([0, 1, 1, 0x80]) for _ in range(12)]
                vop = random.choice([0, 1, 3, 4, 5, 8, 0xD, 2])
                sop = random.choice([0, 1, 6, 7, 2])
                flag = random.choice([0, 1])
                links = [random.choice([-1] + list(range(n))) for _ in range(3)]
                for k, offset in enumerate((0x14, 0x1C, 0x24)):
                    w32(u, instruction + offset, 0x00203790)
                    u.mem_write(instruction + offset + 4, bytes(masks[4 * k:4 * k + 4]))
                w32(u, instruction + 0x2C, vop)
                w32(u, instruction + 0x30, sop)
                u.mem_write(instruction + 0x10, bytes([flag]))
                for k, offset in enumerate((0x34, 0x38, 0x3C)):
                    w32(u, instruction + offset, 0 if links[k] < 0 else owners[links[k]])
                description.append('%s %d %d %d %s' % (' '.join(map(str, masks)), vop, sop, flag, ' '.join(map(str, links))))
            active = random.choice([-1] + list(range(n)))
            w32(u, ACTIVE_OWNER, 0 if active < 0 else owners[active])
            links = [random.choice([-1, 0, 1, 2]) for _ in range(n)]
            for i in range(n):
                w32(u, owners[i] + 0xC, 0 if links[i] < 0 else operands[links[i]])
            operand_masks = [[random.choice([0, 0, 1]) for _ in range(4)] for _ in range(3)]
            description.append('%d %s %s' % (active, ' '.join(map(str, links)), ' '.join(' '.join(map(str, m)) for m in operand_masks)))
            for i in range(3):
                operand_kind = random.choice([0, 1, 2, 3, i, i])
                parent = random.choice([-1] + list(range(n)) * 2)
                order = [random.randint(0, 4) for _ in range(4)]
                w32(u, reader + 4 + 4 * i, operands[i])
                w32(u, operands[i] + 4, reader)
                w32(u, operands[i] + 0xC, operand_kind)
                w32(u, operands[i] + 0x10, 0 if parent < 0 else owners[parent])
                for j in range(4):
                    w32(u, operands[i] + 0x1C + 4 * j, order[j])
                description.append('%d %d %s' % (operand_kind, parent, ' '.join(map(str, order))))
            text = ''
            for slot in range(3):
                text += '%d%d' % (call(u, 0x0018FC2A, reader, [slot, writer]) & 0xFF, call(u, 0x0018FD7D, reader, [slot, writer]) & 0xFF)
            text += ' ' + ''.join('%d' % (call(u, 0x001906BB, operands[i], [channel]) & 0xFF) for i in range(3) for channel in range(5))
            parents_present = all(r32(u, operands[i] + 0x10) for i in range(3))
            text += ' %s' % (('%d' % (call(u, 0x001908AE, reader, [writer]) & 0xFF)) if r32(u, writer + 0x2C) != 0xD or parents_present else 'x')
            text += ' %d%d' % (call(u, 0x00190858, reader, [writer]) & 0xFF, call(u, 0x0018FED0, reader, [writer]) & 0xFF)
            text += ''.join('%d' % (call(u, 0x001907E9, operands[i], [0x00203790, struct.unpack('<I', bytes(operand_masks[i]))[0]]) & 0xFF) for i in range(3))
            expected.append(text)
            lines.append('reach %d %s' % (n, ' '.join(description)))
        elif kind == 'swap':
            vop = random.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xD, 0xFFFFFFFF])
            sop = random.choice([0, 0, 1, 2, 3, 4, 5, 0xFFFFFFFF])
            flag = random.choice([0, 0, 1])
            has_output = random.choice([0, 0, 1])
            vector_destination, scalar_destination = random.choice([-1, 0, 0, 1]), random.choice([-1, 0, 1, 1])
            destination = random.choice([vector_destination, scalar_destination, -1, 0, 1])
            match_first, match_second = random.randint(0, 4), random.randint(0, 4)
            first, second = random.randint(0, 3), random.randint(0, 3)
            query = random.choice([0, 0, 1])
            masks = [random.choice([0, 1, 0x80]) for _ in range(8)]
            orders = [[random.randint(0, 4) for _ in range(4)] for _ in range(3)]
            instruction, owners = HEAP, [HEAP + 0x100, HEAP + 0x140]
            operands = [HEAP + 0x300 + 0x40 * i for i in range(3)]
            owner_address = lambda index: 0 if index < 0 else owners[index]
            for word in range(0x13):
                w32(u, instruction + 4 * word, 0)
            for i in range(3):
                w32(u, instruction + 4 + 4 * i, operands[i])
                for j in range(4):
                    w32(u, operands[i] + 0x1C + 4 * j, orders[i][j])
            u.mem_write(instruction + 0x10, bytes([flag]))
            w32(u, instruction + 0x1C, 0x00203790)
            u.mem_write(instruction + 0x20, bytes(masks[:4]))
            w32(u, instruction + 0x24, 0x00203790)
            u.mem_write(instruction + 0x28, bytes(masks[4:]))
            w32(u, instruction + 0x2C, vop)
            w32(u, instruction + 0x30, sop)
            w32(u, instruction + 0x34, owners[1] if has_output else 0)
            w32(u, instruction + 0x38, owner_address(vector_destination))
            w32(u, instruction + 0x3C, owner_address(scalar_destination))
            share_owner, share_first, share_second = random.randint(0, 1), random.randint(0, 3), random.randint(0, 3)
            codes = [[random.choice([0, -1, 1, 1]) for _ in range(4)] for _ in range(2)]
            for k in range(2):
                for j in range(4):
                    w32(u, owners[k] + 0x20 + 4 * j, 0 if codes[k][j] == 0 else 0xFFFFFFFF if codes[k][j] < 0 else instruction)
            shared = call(u, 0x0018F815, owners[share_owner], [share_first, share_second]) & 0xFF
            match = call(u, 0x0018EBEA, instruction, [owner_address(destination), match_first, match_second]) & 0xFF
            swapped = call(u, 0x0018EDEE, instruction, [owner_address(destination), first, second, query | (R() & 0xFFFFFF00)]) & 0xFF
            text = '%d %d %d %s %s' % (shared, match, swapped, bytes(u.mem_read(instruction + 0x20, 4)).hex().upper(), bytes(u.mem_read(instruction + 0x28, 4)).hex().upper())
            for i in range(3):
                text += ' ' + ''.join('%u' % r32(u, operands[i] + 0x1C + 4 * j) for j in range(4))
            expected.append(text)
            lines.append('swap %d %d %d %d %d %d %d %d %d %d %d %d %s %s' % (vop, sop, flag, has_output, vector_destination, scalar_destination,
                destination, match_first, match_second, first, second, query, ' '.join(map(str, masks)), ' '.join(' '.join(map(str, o)) for o in orders))
                + ' %d %d %d %s' % (share_owner, share_first, share_second, ' '.join(' '.join(map(str, c)) for c in codes)))
        elif kind == 'diag':
            n = random.randint(0, 6)
            channel = random.randint(0, 4)
            masks = [random.choice([0, 1]) for _ in range(8)]
            instruction = HEAP
            operands = [HEAP + 0x300 + 0x40 * i for i in range(3)]
            for word in range(0x13):
                w32(u, instruction + 4 * word, 0)
            w32(u, instruction + 0x2C, 4)
            w32(u, instruction + 0x30, 1)
            u.mem_write(instruction + 0x20, bytes(masks[:4]))
            u.mem_write(instruction + 0x28, bytes(masks[4:]))
            description = []
            for i in range(3):
                operand_kind = random.choice([0, 1, 2, 3])
                order = [random.randint(0, 4) for _ in range(4)]
                w32(u, instruction + 4 + 4 * i, operands[i])
                w32(u, operands[i] + 4, instruction)
                w32(u, operands[i] + 0xC, operand_kind)
                for j in range(4):
                    w32(u, operands[i] + 0x1C + 4 * j, order[j])
                description.append('%d %s' % (operand_kind, ' '.join(map(str, order))))
            parent, first, second = random_tree(n) if n else ([], [], [])
            nodes = [HEAP + 0x1000 + 0x20 * i for i in range(n)]
            for i in range(n):
                value = random.randrange(3)
                w32(u, nodes[i] + 4, operands[value])
                w32(u, nodes[i] + 8, 0 if parent[i] < 0 else nodes[parent[i]])
                w32(u, nodes[i] + 0xC, 0 if first[i] < 0 else nodes[first[i]])
                w32(u, nodes[i] + 0x10, 0 if second[i] < 0 else nodes[second[i]])
                description.append('%d %d %d %d' % (parent[i], first[i], second[i], value))
            owner = HEAP + 0x2000
            w32(u, owner + 0x18, nodes[0] if n else 0)
            expected.append('%d' % (call(u, 0x00190D14, owner, [channel]) & 0xFF))
            lines.append('diag %d %d %s %s' % (n, channel, ' '.join(map(str, masks)), ' '.join(description)))
        elif kind == 'setops':
            mask = [random.choice([0, 1, 0x80]) for _ in range(4)]
            other = [random.choice([0, 1, 0xFF]) for _ in range(4)]
            source, result = HEAP, HEAP + 0x40
            w32(u, source, 0x00203790)
            u.mem_write(source + 4, bytes(mask))
            out = []
            for function in (0x0018E4EB, 0x0018E48C):
                u.mem_write(result, b'\0' * 4 + b'\x09' * 4)
                returned = call(u, function, source, [result, 0x00203790, struct.unpack('<I', bytes(other))[0]])
                out.append(('%08X %s' % (r32(u, result), bytes(u.mem_read(result + 4, 4)).hex().upper()), returned == result))
            expected.append('%s %s %d%d' % (out[0][0], out[1][0], out[0][1], out[1][1]))
            lines.append('setops %s' % ' '.join(map(str, mask + other)))
        else:
            appends = random.randint(0, 5)
            failures = random.sample(range(appends), random.randint(0, min(2, appends))) if appends else []
            state['failures'], state['calls'], state['order'] = failures, 0, []
            listing = HEAP + 0x3000
            for word in range(4):
                w32(u, listing + 4 * word, 0)
            for index in range(appends):
                call(u, 0x0019010E, listing, [0x1000 + index])
            allocated = state['order']
            index = lambda value: allocated.index(value) if value else -1
            text = '%d %d %u |' % (index(r32(u, listing + 4)), index(r32(u, listing + 8)), r32(u, listing + 0xC))
            for node in allocated:
                text += ' %d %d %d' % (r32(u, node + 4) - 0x1000, index(r32(u, node + 8)), index(r32(u, node + 0xC)))
            expected.append(text)
            lines.append('append 0 %d %d %s' % (appends, len(failures), ' '.join(map(str, failures))))
    got = subprocess.run([harness('registers_harness')], input='\n'.join(lines) + '\n', capture_output=True, text=True).stdout.splitlines()
    bad = 0
    for line, want, have in zip(lines, expected, got):
        if want.strip() != have.strip():
            bad += 1
            if bad < 6:
                print('MISMATCH', line, '\n exp', want, '\n got', have)
    print('cases', len(lines), len(got), 'mismatches', bad)

if __name__ == '__main__':
    main()
