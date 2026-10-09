"""Differential check: owner chain predicates, tree destruction/registration and the instruction set."""
import random, struct, subprocess, sys
from retail_cpu import *
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP

R = lambda: random.getrandbits(32)
XMEMALLOC, XMEMFREE = 0x000F7270, 0x000F7280
ACTIVE_OWNER = 0x001A4D34
DESTROY_STUB = 0x00E00000
SET_STUB = 0x00E00010

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
    u.mem_write(XMEMFREE, b'\xc2\x08\x00')
    u.mem_map(DESTROY_STUB, 0x1000)
    u.mem_write(DESTROY_STUB, b'\xc2\x04\x00')
    u.mem_write(SET_STUB, b'\xc2\x04\x00')
    state = {'next': 0, 'calls': 0, 'fail': -1, 'events': [], 'nodes': []}
    def allocate(uc, address, size, user):
        index = state['calls']
        state['calls'] += 1
        if index == state['fail']:
            uc.reg_write(UC_X86_REG_EAX, 0)
        else:
            uc.reg_write(UC_X86_REG_EAX, state['next'])
            state['next'] += 0x20
    def free(uc, address, size, user):
        tag = r32(uc, uc.reg_read(UC_X86_REG_ESP) + 8)
        state['events'].append(-100 if tag == 0x24870000 else -200)
    def destroy(uc, address, size, user):
        node = uc.reg_read(UC_X86_REG_ECX)
        flags = r32(uc, uc.reg_read(UC_X86_REG_ESP) + 4)
        state['events'].append(state['nodes'].index(node) * 10 + flags)
    u.hook_add(UC_HOOK_CODE, allocate, begin=XMEMALLOC, end=XMEMALLOC)
    u.hook_add(UC_HOOK_CODE, free, begin=XMEMFREE, end=XMEMFREE)
    u.hook_add(UC_HOOK_CODE, destroy, begin=DESTROY_STUB, end=DESTROY_STUB)
    def set_destroy(uc, address, size, user):
        state['events'].append(-1000 - r32(uc, uc.reg_read(UC_X86_REG_ESP) + 4))
    u.hook_add(UC_HOOK_CODE, set_destroy, begin=SET_STUB, end=SET_STUB)
    lines, expected = [], []
    for _ in range(int(sys.argv[2]) if len(sys.argv) > 2 else 600):
        kind = random.choice(['owners', 'destroy', 'register', 'register', 'setval', 'copyset', 'release', 'swapown', 'swapown', 'swapown', 'writers', 'writers'])
        if kind == 'owners':
            n = random.randint(1, 6)
            owners = [HEAP + 0x40 * i for i in range(n)]
            description = []
            for i in range(n):
                derived = random.choice([-1] + list(range(i + 1, n)))
                channel_owner = random.choice([-1] + list(range(i)))
                channels = [random.choice([0, 0, 1, R()]) for _ in range(4)]
                w32(u, owners[i] + 0x30, 0 if derived < 0 else owners[derived])
                w32(u, owners[i] + 0x34, 0 if channel_owner < 0 else owners[channel_owner])
                for j in range(4):
                    w32(u, owners[i] + 0x20 + 4 * j, channels[j])
                description.append('%d %d %s' % (derived, channel_owner, ' '.join(map(str, channels))))
            text = ''
            for i in range(n):
                for other in range(-1, n):
                    text += '%d' % (call(u, 0x0018E78B, owners[i], [0 if other < 0 else owners[other]]) & 0xFF)
                for channel in range(4):
                    text += '%d%d' % (call(u, 0x0018F56F, owners[i], [channel]) & 0xFF, call(u, 0x0018F549, owners[i], [channel]) & 0xFF)
                text += ' '
            expected.append(text)
            lines.append('owners %d %s' % (n, ' '.join(description)))
        elif kind == 'setval':
            calls = [(random.choice([random.randint(-4, 6) & 0xFFFFFFFF, R()]), random.randrange(16)) for _ in range(random.randint(1, 8))]
            tree = HEAP + 0x2800
            w32(u, tree + 8, 0)
            w32(u, tree + 0xC, 0)
            base = HEAP + 0x40000
            data_base = HEAP + 0x5000
            state['next'], state['calls'], state['fail'] = base, 0, -1
            result = HEAP + 0x6000
            text = '|'
            for key, value in calls:
                w32(u, result, 1)
                returned = call(u, 0x00192830, tree, [result, key, data_base + 4 * value])
                node = r32(u, result)
                text += ' %d%d' % ((node - base) // 0x20 if node else -1, returned == result)
            used = (state['next'] - base) // 0x20
            nodes = [base + 0x20 * i for i in range(used)]
            index = lambda v: nodes.index(v) if v else -1
            text += ' | %d %u' % (index(r32(u, tree + 8)), r32(u, tree + 0xC))
            for node in nodes:
                text += ' %08X %d %d %d %d %d' % (r32(u, node + 4), index(r32(u, node + 0xC)), index(r32(u, node + 0x10)), index(r32(u, node + 0x14)),
                    signed(r32(u, node + 0x18)), (r32(u, node + 8) - data_base) // 4)
            expected.append(text)
            lines.append('setval 0 %d %s' % (len(calls), ' '.join('%d %d' % c for c in calls)))
        elif kind == 'copyset':
            n = random.randint(1, 8)
            instructions = [HEAP + 0x1000 + 0x50 * i for i in range(n)]
            sequences = [random.choice([0, 1, 2, 0xFFFFFFFF]) for _ in range(n)]
            for address, sequence in zip(instructions, sequences):
                w32(u, address + 0x40, sequence)
            source_values = random.sample(instructions, random.randint(0, n))
            initial_values = random.sample(instructions, random.randint(0, n))
            base = HEAP + 0x40000
            state['next'], state['calls'], state['fail'] = base, 0, -1
            source, target = HEAP + 0x2800, HEAP + 0x2900
            for tree in (source, target):
                w32(u, tree, 0x002037D0)
                w32(u, tree + 8, 0)
                w32(u, tree + 0xC, 0)
            for value in source_values:
                call(u, 0x001927D1, source, [value])
            for value in initial_values:
                call(u, 0x001927D1, target, [value])
            first_new = state['next']
            instruction = HEAP + 0x900
            w32(u, instruction + 0x44, 0x77)
            w32(u, instruction + 0x48, target)
            call(u, 0x001929E7, instruction, [source])
            text = '%u %u |' % (r32(u, instruction + 0x44), r32(u, target + 0xC))
            def walk(node, out):
                if node:
                    walk(r32(u, node + 0xC), out)
                    out.append(r32(u, node + 4))
                    walk(r32(u, node + 0x10), out)
            values = []
            walk(r32(u, target + 8), values)
            text += ''.join(' %08X' % v for v in values) + ' | %d' % ((state['next'] - first_new) // 0x20)
            expected.append(text)
            lines.append('copyset %d %d %s %s %d %s' % (len(source_values), len(initial_values), ' '.join(map(str, source_values)),
                ' '.join(map(str, initial_values)), n, ' '.join('%d %d' % (a, q) for a, q in zip(instructions, sequences))))
        elif kind == 'release':
            has_set = random.choice([0, 1, 1])
            registered = random.choice([0, 1, 1])
            others = random.sample(range(0x10000000, 0x10000800, 0x20), random.randint(0, 5))
            set_address = HEAP + 0x2900
            others = [o for o in others if o != set_address]
            arena = 0x001A4D58
            w32(u, arena + 8, 0)
            w32(u, arena + 0xC, 0)
            base = HEAP + 0x40000
            state['next'], state['calls'], state['fail'] = base, 0, -1
            for value in others + ([set_address] if registered else []):
                call(u, 0x00192772, arena, [value])
            used = (state['next'] - base) // 0x20
            nodes = [base + 0x20 * i for i in range(used)]
            state['nodes'] = nodes
            vtable = HEAP + 0x3000
            w32(u, vtable, DESTROY_STUB)
            for node in nodes:
                w32(u, node, vtable)
            set_vtable = HEAP + 0x3100
            w32(u, set_vtable, 0)
            w32(u, set_vtable + 4, SET_STUB)
            w32(u, set_address, set_vtable)
            instruction = HEAP + 0x900
            w32(u, instruction + 0x48, set_address if has_set else 0)
            state['events'] = []
            call(u, 0x00192A2B, instruction, [])
            text = '%d %u' % (1 if r32(u, instruction + 0x48) == 0 else 0, r32(u, arena + 0xC))
            text += ''.join(' e%d' % e for e in state['events'])
            def walk(node, out):
                if node:
                    walk(r32(u, node + 0xC), out)
                    out.append(r32(u, node + 4))
                    walk(r32(u, node + 0x10), out)
            values = []
            walk(r32(u, arena + 8), values)
            text += ''.join(' %08X' % v for v in values)
            expected.append(text)
            lines.append('release %d %d %d %s %d' % (has_set, registered, len(others), ' '.join(map(str, others)), set_address))
        elif kind in ('swapown', 'writers'):
            n = random.randint(1, 4)
            m = random.randint(1, 3)
            owners = [HEAP + 0x1000 + 0x40 * i for i in range(n)]
            instructions = [HEAP + 0x2000 + 0x50 * i for i in range(m)]
            operands = [HEAP + 0x3000 + 0x40 * i for i in range(3 * m)]
            owner_addr = lambda i: 0 if i < 0 else owners[i]
            description = []
            for k in range(m):
                vop = random.choice([0, 1, 3, 4, 5, 2])
                sop = random.choice([0, 1, 2])
                flag = random.choice([0, 0, 1])
                output = random.choice([-1, -1, 0])
                vdst = random.choice([-1] + list(range(n)) * 2)
                sdst = random.choice([-1] + list(range(n)))
                masks = [random.choice([0, 1]) for _ in range(8)]
                sequence = random.choice([0, 1, 2])
                ins = instructions[k]
                for word in range(0x13):
                    w32(u, ins + 4 * word, 0)
                u.mem_write(ins + 0x10, bytes([flag]))
                u.mem_write(ins + 0x20, bytes(masks[:4]))
                u.mem_write(ins + 0x28, bytes(masks[4:]))
                w32(u, ins + 0x2C, vop)
                w32(u, ins + 0x30, sop)
                w32(u, ins + 0x34, owner_addr(output))
                w32(u, ins + 0x38, owner_addr(vdst))
                w32(u, ins + 0x3C, owner_addr(sdst))
                w32(u, ins + 0x40, sequence)
                orders = []
                for slot in range(3):
                    op = operands[3 * k + slot]
                    order = [random.randint(0, 4) for _ in range(4)]
                    w32(u, ins + 4 + 4 * slot, op)
                    w32(u, op + 4, ins)
                    w32(u, op + 0xC, slot)
                    w32(u, op + 0x10, 0)
                    for j in range(4):
                        w32(u, op + 0x1C + 4 * j, order[j])
                    orders.append(' '.join(map(str, order)))
                description.append('%d %d %d %d %d %d %s %d %s' % (vop, sop, flag, output, vdst, sdst, ' '.join(map(str, masks)), sequence, ' '.join(orders)))
            registry_nodes = HEAP + 0x5000
            for i in range(n):
                derived = random.choice([-1] + list(range(i + 1, n)))
                channel_owner = random.choice([-1] + list(range(i)))
                codes = [random.choice([0, 0, -1] + list(range(1, m + 1)) * 2) for _ in range(4)]
                operand = random.choice([-1, -1] + list(range(3 * m)))
                parent = random.choice([-1] + list(range(n)))
                w32(u, owners[i] + 0x30, owner_addr(derived))
                w32(u, owners[i] + 0x34, owner_addr(channel_owner))
                for j in range(4):
                    w32(u, owners[i] + 0x20 + 4 * j, 0 if codes[j] == 0 else 0xFFFFFFFF if codes[j] < 0 else instructions[codes[j] - 1])
                w32(u, owners[i] + 0xC, 0 if operand < 0 else operands[operand])
                if operand >= 0:
                    w32(u, operands[operand] + 0x10, owner_addr(parent))
                count = random.randint(0, 3)
                parents, firsts, seconds = random_tree(count)
                nodes = [registry_nodes + 0x20 * j for j in range(count)]
                registry_nodes += 0x20 * count
                values = [random.randrange(3 * m) for _ in range(count)]
                for j in range(count):
                    w32(u, nodes[j] + 4, operands[values[j]])
                    w32(u, nodes[j] + 8, 0 if parents[j] < 0 else nodes[parents[j]])
                    w32(u, nodes[j] + 0xC, 0 if firsts[j] < 0 else nodes[firsts[j]])
                    w32(u, nodes[j] + 0x10, 0 if seconds[j] < 0 else nodes[seconds[j]])
                w32(u, owners[i] + 0x18, nodes[0] if count else 0)
                description.append('%d %d %d %d %s %d %s' % (derived, channel_owner, operand, parent, ' '.join(map(str, codes)), count,
                    ' '.join('%d %d %d %d' % (values[j], parents[j], firsts[j], seconds[j]) for j in range(count))))
            target = random.randrange(n)
            if kind == 'writers':
                active = random.choice([-1] + list(range(n)))
                w32(u, ACTIVE_OWNER, owner_addr(active))
                set_members = random.sample(range(m), random.randint(0, m))
                mask = [random.choice([0, 1]) for _ in range(4)]
                # Retail requires a written channel 0 on any foreign parent; skip graphs that would dereference null.
                set_tree = HEAP + 0x2800
                w32(u, set_tree + 8, 0)
                w32(u, set_tree + 0xC, 0)
                base = HEAP + 0x40000
                state['next'], state['calls'], state['fail'] = base, 0, -1
                for k in set_members:
                    call(u, 0x001927D1, set_tree, [instructions[k]])
                # Half the cases enter through an operand (0x00190813), which climbs to operand.parent.
                use_operand = random.random() < 0.5
                operand_index = random.randrange(3 * m) if use_operand else -1
                walked = r32(u, operands[operand_index] + 0x10) if use_operand else owners[target]
                operand_address = r32(u, walked + 0xC) if walked else 0
                foreign = r32(u, operand_address + 0x10) if operand_address else 0
                if foreign == owner_addr(active):
                    foreign = 0
                if foreign and r32(u, foreign + 0x20) in (0, 0xFFFFFFFF):
                    continue
                packed_mask = struct.unpack('<I', bytes(mask))[0]
                if use_operand:
                    result = call(u, 0x00190813, operands[operand_index], [R(), 0x00203790, packed_mask, set_tree]) & 0xFF
                else:
                    result = call(u, 0x00190774, owners[target], [R(), 0x00203790, packed_mask, set_tree]) & 0xFF
                expected.append('%d' % result)
                lines.append('writers %d %d %s %d %d %d %s %s %d' % (n, m, ' '.join(description), target, active, len(set_members),
                    ' '.join(str(1000 + k) for k in set_members), ' '.join(map(str, mask)), operand_index))
                continue
            first, second = random.randint(0, 3), random.randint(0, 3)
            climb, propagate = random.choice([0, 1]), random.choice([0, 1])
            result = call(u, 0x00190FFE, owners[target], [first, second, climb | (R() & 0xFFFFFF00), propagate | (R() & 0xFFFFFF00)]) & 0xFF
            def code(value):
                if value == 0:
                    return 0
                if value == 0xFFFFFFFF:
                    return -1
                return instructions.index(value) + 1
            text = '%d' % result
            for owner in owners:
                text += ' ' + ','.join('%d' % code(r32(u, owner + 0x20 + 4 * j)) for j in range(4))
            for ins in instructions:
                text += ' %s%s' % (bytes(u.mem_read(ins + 0x20, 4)).hex().upper(), bytes(u.mem_read(ins + 0x28, 4)).hex().upper())
            for op in operands:
                text += ' ' + ''.join('%u' % r32(u, op + 0x1C + 4 * j) for j in range(4))
            expected.append(text)
            lines.append('swapown %d %d %s %d %d %d %d %d' % (n, m, ' '.join(description), target, first, second, climb, propagate))
        elif kind == 'destroy':
            function = random.randrange(7)
            pass_layout = function >= 4
            n = random.randint(0, 7)
            flags = random.choice([0, 1, 3, 2])
            parent, first, second = random_tree(n)
            nodes = [HEAP + 0x20 * i for i in range(n)]
            state['nodes'] = nodes
            vtable = HEAP + 0x3000
            w32(u, vtable, DESTROY_STUB)
            offsets = (0xC, 0x10, 0x14) if pass_layout else (0x8, 0xC, 0x10)
            for i in range(n):
                w32(u, nodes[i], vtable)
                for off, link in zip(offsets, (parent[i], first[i], second[i])):
                    w32(u, nodes[i] + off, 0 if link < 0 else nodes[link])
            tree = HEAP + 0x2800
            w32(u, tree, 0)
            w32(u, tree + 4, 0x1234)
            w32(u, tree + 8, nodes[0] if n else 0)
            w32(u, tree + 0xC, n)
            state['events'] = []
            root = nodes[0] if n else 0
            if function in (0, 1, 4):
                call(u, [0x0018F2AA, 0x0018F2D3, 0, 0, 0x0018F1BC][function], 0, [root])
            elif function in (2, 5):
                call(u, 0x00190322 if function == 2 else 0x00190163, tree, [])
            else:
                call(u, 0x00190AA0 if function == 3 else 0x00190AC6, tree, [flags | (R() & 0xFFFFFF00)])
            text = '%08X %d %u |' % (r32(u, tree), 1 if r32(u, tree + 8) else 0, r32(u, tree + 0xC))
            text += ''.join(' %d' % (1 if r32(u, nodes[i] + offsets[0]) else 0) for i in range(n))
            text += ''.join(' e%d' % e for e in state['events'])
            expected.append(text)
            lines.append('destroy %d %d %d %s' % (function, n, flags, ' '.join('%d %d %d' % (parent[i], first[i], second[i]) for i in range(n))))
        else:
            function = random.randrange(4)
            n = random.randint(1, 8)
            instructions = [HEAP + 0x1000 + 0x50 * i for i in range(n)]
            if function == 2:
                values = list(instructions)
                random.shuffle(values)
                values += [random.choice(values) for _ in range(random.randint(0, 2))]
            else:
                values = [random.choice([random.randint(-5, 30) & 0xFFFFFFFF, R()]) for _ in range(n)]
            sequences = [random.choice([0, 1, 2, 0xFFFFFFFF, R()]) for _ in values]
            seq_of = {}
            for value, sequence in zip(values, sequences):
                if function == 2:
                    seq_of.setdefault(value, sequence)
            for value, sequence in seq_of.items():
                w32(u, value + 0x40, sequence)
            # The host map keys by value; keep the first sequence per address, matching retail memory.
            sequences = [seq_of.get(value, sequence) for value, sequence in zip(values, sequences)]
            fail = random.choice([-1, -1, -1, 0, 1, 2])
            tree = HEAP + 0x2800
            w32(u, tree + 8, 0)
            w32(u, tree + 0xC, 0)
            base = HEAP + 0x40000
            state['next'], state['calls'], state['fail'] = base, 0, fail
            for value in values:
                if function == 3:
                    call(u, 0x001924DA, tree, [value, 0x5555])
                else:
                    call(u, [0x00192772, 0x0019287F, 0x001927D1][function], tree, [value])
            used = (state['next'] - base) // 0x20
            nodes = [base + 0x20 * i for i in range(used)]
            index = lambda v: nodes.index(v) if v else -1
            text = '%d %u |' % (index(r32(u, tree + 8)), r32(u, tree + 0xC))
            for node in nodes:
                if function == 3:
                    text += ' %08X %08X %d %d %d %d' % (r32(u, node), r32(u, node + 4), index(r32(u, node + 0xC)), index(r32(u, node + 0x10)),
                        index(r32(u, node + 0x14)), signed(r32(u, node + 0x18)))
                else:
                    text += ' %08X %08X %d %d %d %d' % (r32(u, node), r32(u, node + 4), index(r32(u, node + 8)), index(r32(u, node + 0xC)),
                        index(r32(u, node + 0x10)), signed(r32(u, node + 0x14)))
            if function == 2:
                text += ' |'
                for value in values:
                    text += ' %d' % index(call(u, 0x0018F174, tree, [value]))
                    text += ''.join('%d' % (call(u, 0x0018E94C, 0, [value, other]) & 0xFF) for other in values)
            expected.append(text)
            lines.append('register %d %d %d %s %s' % (function, len(values), fail, ' '.join(map(str, values)), ' '.join(map(str, sequences))))
    got = subprocess.run([harness('sets_harness')], input='\n'.join(lines) + '\n', capture_output=True, text=True).stdout.splitlines()
    bad = 0
    for line, want, have in zip(lines, expected, got):
        if want.strip() != have.strip():
            bad += 1
            if bad < 6:
                print('MISMATCH', line, '\n exp', want, '\n got', have)
    print('cases', len(lines), len(got), 'mismatches', bad)

if __name__ == '__main__':
    main()
