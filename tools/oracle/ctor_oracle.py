"""Differential check: instruction construction 0x00192DF1 (normal path) against an SEH-shimmed host build."""
import random, struct, subprocess, sys
from retail_cpu import *
R = lambda: random.getrandbits(32)
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX

XMEMALLOC = 0x000F7270
TREE = 0x001A4D38
ACTIVE_OWNER = 0x001A4D34
BASE = 0x10040000

DESTROY_STUB = 0x00E00000
EXPRESSIONS = 0x001A4D48

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

def run_pass_destructor(u, state, lines, derived_pass=False, deleting_pass=False):
    from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_ESP
    state['fail'] = -1
    events = []
    u.mem_map(DESTROY_STUB, 0x1000)
    u.mem_write(DESTROY_STUB, b'\xc2\x04\x00')
    owners = {}
    def destroy(uc, address, size, user):
        node = uc.reg_read(UC_X86_REG_ECX)
        flags = r32(uc, uc.reg_read(UC_X86_REG_ESP) + 4)
        kind, index = owners[node]
        if kind == 'list':
            events.append(1000 + index * 10 + flags)
        elif kind == 'registry':
            events.append(2000 + index * 10 + flags)
        else:
            events.append(kind * 100 + index * 10 + flags)
    u.hook_add(UC_HOOK_CODE, destroy, begin=DESTROY_STUB, end=DESTROY_STUB)
    vtable = HEAP + 0x3000
    w32(u, vtable, DESTROY_STUB)
    pass_object = 0x10001000
    u.mem_write(pass_object, b'\0' * 0x60)
    list_count = random.randint(0, 3)
    list_nodes = [HEAP + 0x4000 + 0x20 * i for i in range(list_count)]
    for i, node in enumerate(list_nodes):
        owners[node] = ('list', i)
        w32(u, node, vtable)
        w32(u, node + 8, list_nodes[i - 1] if i > 0 else 0)
        w32(u, node + 0xC, list_nodes[i + 1] if i + 1 < list_count else 0)
    w32(u, pass_object + 4, 0x002037C8)
    w32(u, pass_object + 8, list_nodes[0] if list_count else 0)
    w32(u, pass_object + 0xC, list_nodes[-1] if list_count else 0)
    w32(u, pass_object + 0x10, list_count)
    description = []
    for which, offset in enumerate((0x14, 0x28, 0x3C)):
        n = random.randint(0, 4)
        parent, first, second = random_tree(n)
        nodes = [HEAP + 0x5000 + 0x400 * which + 0x20 * i for i in range(n)]
        for i, node in enumerate(nodes):
            owners[node] = (which, i)
            w32(u, node, vtable)
            for off, link in zip((0xC, 0x10, 0x14), (parent[i], first[i], second[i])):
                w32(u, node + off, 0 if link < 0 else nodes[link])
        w32(u, pass_object + offset, 0x002037D8)
        w32(u, pass_object + offset + 8, nodes[0] if n else 0)
        w32(u, pass_object + offset + 0xC, n)
        description.append('%d %s' % (n, ' '.join('%d %d %d' % (parent[i], first[i], second[i]) for i in range(n))))
    base = HEAP + 0x40000
    state['next'], state['calls'] = base, 0
    registries = []
    for tree, candidates in ((EXPRESSIONS, [0x10001024, 0x10001038, 0x1000104C]), (TREE, [0x10001000])):
        w32(u, tree + 8, 0)
        w32(u, tree + 0xC, 0)
        values = [v for v in candidates if random.random() < 0.8] + [random.randrange(0x10002000, 0x10003000, 4) for _ in range(random.randint(0, 3))]
        random.shuffle(values)
        for value in values:
            call(u, 0x00192655, tree, [value])
        registries.append('%d %s' % (len(values), ' '.join(map(str, values))))
    used = (state['next'] - base) // 0x100
    for i in range(used):
        node = base + 0x100 * i
        owners[node] = ('registry', i)
        w32(u, node, vtable)
    extra = ''
    if derived_pass:
        n = random.randint(0, 5)
        parent, first, second = random_tree(n)
        nodes = [HEAP + 0x7000 + 0x20 * i for i in range(n)]
        for i, node in enumerate(nodes):
            owners[node] = (3, i)
            w32(u, node, vtable)
            for off, link in zip((8, 0xC, 0x10), (parent[i], first[i], second[i])):
                w32(u, node + off, 0 if link < 0 else nodes[link])
        w32(u, pass_object + 0x90, nodes[0] if n else 0)
        w32(u, pass_object + 0x94, n)
        extra = ' %d %s' % (n, ' '.join('%d %d %d' % links for links in zip(parent, first, second)))
    pass_frees = []
    if deleting_pass:
        flags = R()
        extra += ' %u' % flags
        u.mem_write(0x000F7280, b'\xc2\x08\x00')
        def free(uc, address, size, user):
            esp = uc.reg_read(UC_X86_REG_ESP)
            if r32(uc, esp + 4) == pass_object:
                pass_frees.append(r32(uc, esp + 8))
        u.hook_add(UC_HOOK_CODE, free, begin=0x000F7280, end=0x000F7280)
        returned = call(u, 0x00193EBE, pass_object, [flags])
    else:
        call(u, 0x00193649 if derived_pass else 0x001934A5, pass_object, [])
    text = '%08X %08X %08X %08X %u %u %u %u %u %u' % (r32(u, pass_object), r32(u, pass_object + 0x14), r32(u, pass_object + 0x28),
        r32(u, pass_object + 0x3C), r32(u, pass_object + 0x20), r32(u, pass_object + 0x34), r32(u, pass_object + 0x48), r32(u, pass_object + 0x10),
        r32(u, EXPRESSIONS + 0xC), r32(u, TREE + 0xC))
    if derived_pass:
        text += ' %08X %u' % (r32(u, pass_object + 0x88), r32(u, pass_object + 0x94))
    if deleting_pass:
        text += ' %u %08X %d' % (len(pass_frees), pass_frees[0] if pass_frees else 0, returned == pass_object)
    text += ''.join(' e%d' % e for e in events)
    lines.append('%s %d 0 %s %s%s' % ('programdelete' if deleting_pass else 'programdtor' if derived_pass else 'passdtor',
        list_count, ' '.join(description), ' '.join(registries), extra))
    return text

def run_owner_sets(u, state, lines, kind):
    from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_ESP
    n = random.randint(1, 4)
    m = random.randint(1, 3)
    owners = [HEAP + 0x1000 + 0x40 * i for i in range(n)]
    instructions = [HEAP + 0x2000 + 0x50 * i for i in range(m)]
    operands = [HEAP + 0x3000 + 0x40 * i for i in range(3 * m)]
    owner_addr = lambda i: 0 if i < 0 else owners[i]
    description = []
    biased = kind == 'negate' and random.random() < 0.6
    for k in range(m):
        vop = random.choice([0, 1, 1, 3, 4])
        sop = random.choice([0, 1, 1, 2])
        flag = random.choice([0, 0, 1])
        output = random.choice([-1, -1, 0])
        vdst = random.choice([-1] + list(range(n)) * 2)
        sdst = random.choice([-1] + list(range(n)) * 2)
        sequence = random.choice([0, 1, 2])
        masks = [random.choice([0, 1]) for _ in range(8)]
        if biased and random.random() < 0.8:
            vop, sop, output, vdst = 1, 0, -1, 0
        ins = instructions[k]
        u.mem_write(ins, b'\0' * 0x50)
        u.mem_write(ins + 0x10, bytes([flag]))
        u.mem_write(ins + 0x20, bytes(masks[:4]))
        u.mem_write(ins + 0x28, bytes(masks[4:]))
        w32(u, ins + 0x2C, vop)
        w32(u, ins + 0x30, sop)
        w32(u, ins + 0x34, owner_addr(output))
        w32(u, ins + 0x38, owner_addr(vdst))
        w32(u, ins + 0x3C, owner_addr(sdst))
        w32(u, ins + 0x40, sequence)
        slots = []
        for slot in range(3):
            op = operands[3 * k + slot]
            marker = random.choice([0, 1])
            parent = random.choice(list(range(n)))
            order = [random.randint(0, 4) for _ in range(4)]
            u.mem_write(op, b'\0' * 0x2C)
            w32(u, ins + 4 + 4 * slot, op)
            w32(u, op + 4, ins)
            w32(u, op + 0xC, slot)
            w32(u, op + 0x10, owner_addr(parent))
            u.mem_write(op + 0x14, bytes([marker]))
            for j in range(4):
                w32(u, op + 0x1C + 4 * j, order[j])
            slots.append('%d %d %s' % (marker, parent, ' '.join(map(str, order))))
        description.append('%d %d %d %d %d %d %d %s %s' % (vop, sop, flag, output, vdst, sdst, sequence, ' '.join(map(str, masks)), ' '.join(slots)))
    registry_nodes = HEAP + 0x5000
    for i in range(n):
        owner_kind = random.choice([0, 1, 1, 2, 3]) if kind == 'negate' else random.choice([1, 3, 3]) if kind == 'equiv' else random.choice([1, 5, 5]) if kind == 'dependencies' else random.choice([1, 1, 6, 6, 5]) if kind == 'reorder' else random.choice([1, 4, 4, 5]) if kind in ('ownerreader', 'channelreader', 'unwritten', 'unuseddest') else 1
        derived = random.choice([-1, -1] + list(range(i + 1, n)))
        channel_owner = random.choice([-1, -1] + list(range(i)))
        operand = random.choice([-1] + list(range(3 * m)) * 2)
        codes = [random.choice([0, 0, -1] + list(range(1, m + 1)) * 2) for _ in range(4)]
        if kind == 'negate' and random.random() < 0.6:
            codes = [c if c >= 0 else 0 for c in codes]
        if biased and i == 0:
            owner_kind, derived, channel_owner = 1, -1, -1
            codes = [random.choice([0] + list(range(1, m + 1))) for _ in range(4)]
        u.mem_write(owners[i], b'\0' * 0x40)
        w32(u, owners[i] + 4, owner_kind)
        w32(u, owners[i] + 0x30, owner_addr(derived))
        w32(u, owners[i] + 0x34, owner_addr(channel_owner))
        w32(u, owners[i] + 0xC, 0 if operand < 0 else operands[operand])
        for j in range(4):
            w32(u, owners[i] + 0x20 + 4 * j, 0 if codes[j] == 0 else 0xFFFFFFFF if codes[j] < 0 else instructions[codes[j] - 1])
        count = random.randint(0, 3)
        values = random.sample(range(3 * m), min(count, 3 * m))
        count = len(values)
        parents, firsts, seconds = random_tree(count)
        nodes = [registry_nodes + 0x20 * j for j in range(count)]
        registry_nodes += 0x20 * count
        for j in range(count):
            w32(u, nodes[j] + 4, operands[values[j]])
            w32(u, nodes[j] + 8, 0 if parents[j] < 0 else nodes[parents[j]])
            w32(u, nodes[j] + 0xC, 0 if firsts[j] < 0 else nodes[firsts[j]])
            w32(u, nodes[j] + 0x10, 0 if seconds[j] < 0 else nodes[seconds[j]])
        w32(u, owners[i] + 0x18, nodes[0] if count else 0)
        description.append('%d %d %d %d %s %d %s' % (owner_kind, derived, channel_owner, operand, ' '.join(map(str, codes)), count,
            ' '.join('%d %d %d %d' % (values[j], parents[j], firsts[j], seconds[j]) for j in range(count))))
    target = 0 if biased else random.randrange(n)
    if kind == 'destinations':
        mode = random.randrange(11)
        instruction_index = random.randrange(m)
        owner_index = random.choice([-1] + list(range(n)))
        if mode == 1:
            # A null matched destination with a nonzero mask dereferences null in retail.
            # Unicorn's mapped FS page would hide that invalid graph.
            owner_index = random.randrange(n)
        writer_code = random.randrange(4) if mode in (9, 10) else random.choice([-1, 0] + list(range(1, m + 1)))
        channel = random.randrange(4)
        mask = [random.choice([0, 0, 1, 2, 255]) for _ in range(4)]
        output_masks = [random.choice([0, 0, 1, 2, 255]) for _ in range(4 * m)]
        for j, instruction in enumerate(instructions):
            u.mem_write(instruction + 0x18, bytes(output_masks[4*j:4*j+4]))
        writer = 0 if mode in (9, 10) else 0xFFFFFFFF if writer_code < 0 else 0 if writer_code == 0 else instructions[writer_code - 1]
        function = [0x190D50, 0x1908FC, 0x18F608, 0x18F6B7, 0x18F766,
            0x19070B, 0x19072E, 0x190751, 0x191C0B, 0x190D77, 0x191C47][mode]
        packed = struct.unpack('<I', bytes(mask))[0]
        if mode == 0:
            receiver, args = owners[target], [channel, writer]
        elif mode == 1:
            receiver, args = instructions[instruction_index], [channel, owner_addr(owner_index)]
        elif mode in (2, 3, 4):
            receiver, args = instructions[instruction_index], [owner_addr(owner_index), 0xDEADBEEF, packed]
        elif mode in (5, 6, 7):
            receiver, args = instructions[instruction_index], [owner_addr(owner_index)]
        elif mode == 9:
            receiver, args = owners[target], [channel, writer_code]
        elif mode == 10:
            receiver, args = owners[target], []
        else:
            receiver, args = owners[target], [0xDEADBEEF, packed, writer]
        returned = call(u, function, receiver, args)
        result = owner_index if mode in (2, 3, 4) and returned == owner_addr(owner_index) else -3 if mode in (2, 3, 4) else -2
        if mode == 9:
            result = returned & 0xFF
        text = '%d |' % result
        def writer_value(value):
            return value if value in (0, 0xFFFFFFFF) else instructions.index(value) + 1
        text += ''.join(' %u' % writer_value(r32(u, owner + 0x20 + 4 * j)) for owner in owners for j in range(4))
        text += ' |'
        for instruction in instructions:
            text += ''.join(' %d' % (-1 if r32(u, instruction + off) == 0 else owners.index(r32(u, instruction + off)))
                for off in (0x34, 0x38, 0x3C))
            for off in (0x14, 0x1C, 0x24):
                text += ' %08X' % r32(u, instruction + off)
                text += ''.join(' %u' % value for value in u.mem_read(instruction + off + 4, 4))
        text += ' |' + ''.join(' %u' % r32(u, operand + 0x1C + 4 * j) for operand in operands for j in range(4))
        lines.append('destinations %d %d %s %d %s' % (n, m, ' '.join(description), target,
            ' '.join(map(str, [mode, instruction_index, owner_index, writer_code, channel] + mask + output_masks))))
        return text
    if kind in ('unwritten', 'unuseddest'):
        target = random.randrange(n if kind == 'unwritten' else m)
        result = call(u, 0x00191CC0 if kind == 'unwritten' else 0x00191C74,
            owners[target] if kind == 'unwritten' else instructions[target], []) & 0xFF
        lines.append('%s %d %d %s %d' % (kind, n, m, ' '.join(description), target))
        return '%d' % result
    if kind in ('ownerreader', 'channelreader'):
        channel = random.randrange(4)
        result = call(u, 0x00191BE1 if kind == 'ownerreader' else 0x0018F513, owners[target], [channel]) & 0xFF
        lines.append('%s %d %d %s %d %d' % (kind, n, m, ' '.join(description), target, channel))
        return '%d' % result
    if kind == 'equiv':
        # 0x00194311: operand a against operand b; mode 1 copies b's marker/parent from a, mode 2 also its order,
        # mode 3 copies the parent and order but keeps b's marker.
        a, b = random.randrange(3 * m), random.randrange(3 * m)
        mode = random.choice([0, 1, 2, 2, 3])
        active = random.choice([-1] + list(range(n)))
        w32(u, ACTIVE_OWNER, owner_addr(active))
        if mode in (1, 2):
            u.mem_write(operands[b] + 0x14, bytes(u.mem_read(operands[a] + 0x14, 1)))
        if mode >= 1:
            w32(u, operands[b] + 0x10, r32(u, operands[a] + 0x10))
        if mode >= 2:
            u.mem_write(operands[b] + 0x1C, bytes(u.mem_read(operands[a] + 0x1C, 0x10)))
            if random.random() < 0.4:
                w32(u, operands[b] + 0x1C + 4 * random.randrange(4), random.randint(0, 4))
        orders = [' '.join('%u' % r32(u, operands[b] + 0x1C + 4 * j) for j in range(4))]
        sources = [random.choice([0, 0, 1, 5]) for _ in range(n)]
        for i in range(n):
            w32(u, owners[i] + 8, sources[i])
        result = call(u, 0x00194311, operands[a], [operands[b]]) & 0xFF
        lines.append('equiv %d %d %s %d %d %d %d %d %s %s' % (n, m, ' '.join(description), target, a, b, mode, active, orders[0],
            ' '.join(map(str, sources))))
        return '%d' % result
    if kind in ('retire', 'installreg', 'installpass'):
        # 0x00193EE2 on a pass whose register tree maps kinds to owners; frees are reported by the erased key.
        registry = HEAP + 0x2800
        w32(u, registry + 8, 0)
        w32(u, registry + 0xC, 0)
        base = HEAP + 0x40000
        state['next'], state['calls'], state['fail'] = base, 0, -1
        members = random.sample(range(3 * m), random.randint(0, 3 * m))
        for j in members:
            call(u, 0x00192655, registry, [operands[j]])
        pass_object = HEAP + 0xA000
        u.mem_write(pass_object, b'\0' * 0x54)
        keys = random.sample(range(13), random.randint(0, 5))
        # Prefer late owners: their channel_owner chains reach the loop in 0x00193EE2.
        datas = [max(random.randrange(n), random.randrange(n)) for _ in keys]
        result_slot = HEAP + 0xA800
        for key, data in zip(keys, datas):
            call(u, 0x00192830, pass_object + 0x28, [result_slot, key, owners[data]])
        pass_nodes = set()
        def collect(node):
            if node:
                pass_nodes.add(node)
                collect(r32(u, node + 0x10))
                collect(r32(u, node + 0x14))
        collect(r32(u, pass_object + 0x30))
        retire_kind = random.choice([-1, 13, 0x80000000] + keys * 3 + list(range(13)))
        events = []
        try:
            u.mem_map(0x000F7000, 0x1000)
        except Exception:
            pass
        u.mem_write(0x000F7280, b'\xc2\x08\x00')
        def free(uc, address, size, user):
            pointer = r32(uc, uc.reg_read(UC_X86_REG_ESP) + 4)
            if pointer in pass_nodes:
                events.append(r32(uc, pointer + 4))
        hook = u.hook_add(UC_HOOK_CODE, free, begin=0x000F7280, end=0x000F7280)
        extra = ''
        if kind == 'installreg':
            special_owner = random.choice([-2, -1] + list(range(n)))
            special_node = HEAP + 0xA900
            u.mem_write(special_node, b'\0' * 0x1C)
            w32(u, special_node + 4, 0x100)
            w32(u, special_node + 8, owner_addr(special_owner))
            if special_owner != -2:
                w32(u, pass_object + 0x44, special_node)
                w32(u, pass_object + 0x48, 1)
            allocation_fail = random.choice([-1, -1, 0])
            state['fail'] = state['calls'] if allocation_fail == 0 else -1
            w32(u, owners[target] + 8, retire_kind)
            result = call(u, 0x00193E52, pass_object, [owners[target]]) == owners[target]
            extra = ' %d %d' % (special_owner, allocation_fail)
        elif kind == 'installpass':
            retire_kind = random.choice([-1] + list(range(n)))
            if retire_kind == -1 and random.random() < 0.5:
                target = -1
            w32(u, pass_object + 0x50, owner_addr(retire_kind))
            result = call(u, 0x00191EC3, pass_object, [owner_addr(target)]) == owner_addr(target)
        else:
            result = call(u, 0x00193EE2, pass_object, [retire_kind, registry]) & 0xFF
        u.hook_del(hook)
        text = '%d %u |' % (result, r32(u, pass_object + 0x34))
        def walk(node):
            if node:
                walk(r32(u, node + 0x10))
                nonlocal text
                text += ' %u:%d' % (r32(u, node + 4), owners.index(r32(u, node + 8)))
                walk(r32(u, node + 0x14))
        walk(r32(u, pass_object + 0x30))
        text += ' |' + ''.join(' %u,%u' % (r32(u, o + 4), r32(u, o + 8)) for o in owners)
        if kind in ('installreg', 'installpass'):
            text += ' | %d' % (-1 if r32(u, pass_object + 0x50) == 0 else owners.index(r32(u, pass_object + 0x50)))
            text += ''.join(' %d' % (-1 if r32(u, owner + off) == 0 else owners.index(r32(u, owner + off)))
                for owner in owners for off in (0x30, 0x34))
        text += ''.join(' f%u' % e for e in events)
        lines.append('%s %d %d %s %d %d %s %d %s %d%s' % (kind, n, m, ' '.join(description), target, len(members), ' '.join(map(str, members)),
            len(keys), ' '.join('%d %d' % kd for kd in zip(keys, datas)), retire_kind if retire_kind < 0x80000000 else retire_kind - 0x100000000, extra))
        return text
    # The retail functions compare operand and instruction pointers; the harness maps them to 0x3000+j / 0x2000+k,
    # which preserves order because both are allocated in ascending address order.
    if kind in ('movechannels', 'reorder', 'link'):
        mask = [random.randint(0, 4) for _ in range(4)] if kind == 'reorder' else [random.choice([0, 1, 2, 0xFF]) for _ in range(4)]
        climb, propagate = random.randint(0, 1), random.randint(0, 1)
        extra = ''
        if kind == 'link':
            other = random.choice([-1] + list(range(n)))
            if other == -1 and random.random() < 0.5:
                target = -1
            query = random.choice([0, 0, 1, 2, 0xFF])
            other_mask = [random.choice([0, 1, 2, 0xFF]) for _ in range(4)]
            result = call(u, 0x00191DF3, owner_addr(target), [owner_addr(other), query | (R() & 0xFFFFFF00),
                0x00203790, struct.unpack('<I', bytes(mask))[0], 0x00203790, struct.unpack('<I', bytes(other_mask))[0]]) & 0xFF
            climb, propagate = other & 0xFFFFFFFF, query
            extra = ' ' + ' '.join(map(str, other_mask))
        elif kind == 'reorder':
            result = call(u, 0x001911A8, owners[target], [0x00203794] + mask) & 0xFF
        else:
            result = call(u, 0x00191417, owners[target], [0x00203790, struct.unpack('<I', bytes(mask))[0],
                climb | (R() & 0xFFFFFF00), propagate | (R() & 0xFFFFFF00)]) & 0xFF
        def writer_code(writer):
            return writer if writer in (0, 0xFFFFFFFF) else instructions.index(writer) + 1
        text = '%d |' % result + ''.join(' %u' % writer_code(r32(u, owner + 0x20 + 4 * j)) for owner in owners for j in range(4))
        text += ' |' + ''.join(' %u' % byte for ins in instructions for off in (0x20, 0x28) for byte in u.mem_read(ins + off, 4))
        text += ' |' + ''.join(' %u' % r32(u, operand + 0x1C + 4 * j) for operand in operands for j in range(4))
        if kind == 'link':
            text += ' |' + ''.join(' %d' % (-1 if r32(u, owner + off) == 0 else owners.index(r32(u, owner + off)))
                for owner in owners for off in (0x30, 0x34))
        lines.append('%s %d %d %s %d %s %d %d%s' % (kind, n, m, ' '.join(description), target, ' '.join(map(str, mask)), climb, propagate, extra))
        return text
    if kind in ('readers', 'readmask', 'dependencies'):
        registry = HEAP + 0x2800
        w32(u, registry + 8, 0)
        w32(u, registry + 0xC, 0)
        state['next'], state['calls'], state['fail'] = HEAP + 0x40000, 0, -1
        members = random.sample(range(3 * m), random.randint(0, 3 * m))
        for j in members:
            call(u, 0x00192655, registry, [operands[j]])
        mask = [random.choice([0, 1, 2, 0xFF]) for _ in range(4)]
        instruction = random.choice([-1] + list(range(m)))
        climb_derived, climb_channel = random.randint(0, 1), random.randint(0, 1)
        if kind == 'dependencies':
            pass_object = HEAP + 0xA000
            pass_owner = random.choice([-1] + list(range(n)))
            w32(u, pass_object + 0x50, owner_addr(pass_owner))
            writers = HEAP + 0xA100
            w32(u, writers + 8, 0)
            w32(u, writers + 0xC, 0)
            writer_members = random.sample(range(m), random.randint(0, m))
            for j in writer_members:
                call(u, 0x001927D1, writers, [instructions[j]])
            result = call(u, 0x00191D2D, owners[target], [pass_object, writers, registry,
                0 if instruction < 0 else instructions[instruction], 0x00203790, struct.unpack('<I', bytes(mask))[0]]) & 0xFF
            lines.append('dependencies %d %d %s %d %d %s %s %d %d %d %d %d %s' % (n, m, ' '.join(description), target,
                len(members), ' '.join(map(str, members)), ' '.join(map(str, mask)), instruction, climb_derived, climb_channel,
                pass_owner, len(writer_members), ' '.join(map(str, writer_members))))
            return '%d' % result
        if kind == 'readmask':
            output = HEAP + 0x2900
            returned = call(u, 0x00190F87, owners[target], [output, registry, 0 if instruction < 0 else instructions[instruction]])
            lines.append('readmask %d %d %s %d %d %s %s %d %d %d' % (n, m, ' '.join(description), target, len(members),
                ' '.join(map(str, members)), ' '.join(map(str, mask)), instruction, climb_derived, climb_channel))
            return '%d %08X %s' % (returned == output, r32(u, output), ' '.join(str(v) for v in u.mem_read(output + 4, 4)))
        result = call(u, 0x00190E27, owners[target], [registry, 0x00203790, struct.unpack('<I', bytes(mask))[0],
            0 if instruction < 0 else instructions[instruction], climb_derived | (R() & 0xFFFFFF00), climb_channel | (R() & 0xFFFFFF00)]) & 0xFF
        lines.append('readers %d %d %s %d %d %s %s %d %d %d' % (n, m, ' '.join(description), target, len(members), ' '.join(map(str, members)),
            ' '.join(map(str, mask)), instruction, climb_derived, climb_channel))
        return '%d' % result
    events = []
    base = HEAP + 0x40000
    state['next'], state['calls'], state['fail'] = base, 0, -1
    freed = []
    try:
        u.mem_map(0x000F7000, 0x1000)
    except Exception:
        pass
    u.mem_write(0x000F7280, b'\xc2\x08\x00')
    def free(uc, address, size, user):
        pointer = r32(uc, uc.reg_read(UC_X86_REG_ESP) + 4)
        events.append(((pointer - base) // 0x100) * 10 + 1)
    hook = u.hook_add(UC_HOOK_CODE, free, begin=0x000F7280, end=0x000F7280)
    result = call(u, 0x00192B84, owners[target], []) & 0xFF
    u.hook_del(hook)
    used = (state['next'] - base) // 0x100
    text = '%d %d' % (result, used)
    text += ''.join(' d%d' % e for e in events)
    text += ''.join(' %u' % u.mem_read(op + 0x14, 1)[0] for op in operands)
    lines.append('negate %d %d %s %d' % (n, m, ' '.join(description), target))
    return text

def run_move_unwritten(u, state, lines, active_kind):
    from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_ESP
    state['fail'] = -1
    owners = [HEAP + 0xD000 + 0x40 * i for i in range(4)]
    reader_count = random.randrange(9)
    readers = [HEAP + 0xE000 + 0x40 * i for i in range(reader_count)]
    derived = random.choice([-1, 3, 3, 3])
    instruction = HEAP + 0xF000
    deleted = []
    u.mem_map(DESTROY_STUB, 0x1000)
    u.mem_write(DESTROY_STUB, b'\xc2\x04\x00')
    w32(u, 0x2037E8, DESTROY_STUB)
    def destroy(uc, address, size, user):
        deleted.extend([r32(uc, uc.reg_read(UC_X86_REG_ECX) + 4), r32(uc, uc.reg_read(UC_X86_REG_ESP) + 4)])
    u.hook_add(UC_HOOK_CODE, destroy, begin=DESTROY_STUB, end=DESTROY_STUB)
    description = []
    for i, owner in enumerate(owners):
        kind = random.choice([1, 2, 3, 4, 5, 6])
        next_owner = random.choice([-1, i + 1, i + 1]) if i < 2 else -1
        writers = [random.choice([0, 0, 1]) for _ in range(4)]
        u.mem_write(owner, b'\0' * 0x38)
        w32(u, owner + 4, kind)
        w32(u, owner + 0x10, 0x2037B8)
        w32(u, owner + 0x34, 0 if next_owner < 0 else owners[next_owner])
        for j, value in enumerate(writers):
            w32(u, owner + 0x20 + 4 * j, 0xFFFFFFFF if value else 0)
        description.extend([kind, next_owner] + writers)
    w32(u, owners[0] + 0x30, 0 if derived < 0 else owners[derived])
    vop, sop = random.randrange(14), random.randrange(8)
    masks = [random.choice([0, 0, 1]) for _ in range(8)]
    u.mem_write(instruction, b'\0' * 0x4C)
    w32(u, instruction + 0x2C, vop)
    w32(u, instruction + 0x30, sop)
    u.mem_write(instruction + 0x20, bytes(masks[:4]))
    u.mem_write(instruction + 0x28, bytes(masks[4:]))
    description.extend([vop, sop] + masks)
    for reader in readers:
        kind, parent = random.randrange(4), random.randrange(4)
        order = [random.randrange(5) for _ in range(4)]
        u.mem_write(reader, b'\0' * 0x2C)
        w32(u, reader + 4, instruction)
        w32(u, reader + 0xC, kind)
        w32(u, reader + 0x10, owners[parent])
        for j, value in enumerate(order):
            w32(u, reader + 0x1C + 4 * j, value)
        call(u, 0x192713, owners[parent] + 0x10, [reader])
        description.extend([kind, parent] + order)
    call(u, 0x193A40, owners[0], [])
    text = ''.join('%08X ' % r32(u, reader + 0x10) for reader in readers) + '|'
    def walk(node):
        return walk(r32(u, node + 0xC)) + [r32(u, node + 4)] + walk(r32(u, node + 0x10)) if node else []
    for owner in owners:
        text += ' %u' % r32(u, owner + 0x1C) + ''.join(',%08X' % value for value in walk(r32(u, owner + 0x18)))
    text += ' |' + ''.join(' %08X' % value for value in deleted)
    lines.append('moveunwritten -1 %d %d %d %s' % (active_kind, derived, reader_count, ' '.join(map(str, description))))
    return text

def run_reparent(u, state, lines, kind, active_kind):
    from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_ESP
    state['fail'] = -1
    owners = [HEAP + 0xD000 + 0x40 * i for i in range(4)]
    operands = [HEAP + 0xE000 + 0x40 * i for i in range(4)]
    active = r32(u, ACTIVE_OWNER)
    parent_at = lambda i: active if i == -2 else 0 if i < 0 else owners[i]
    previous_kind = random.choice([1, 2, 3, 3, 4, 5])
    old_parent = random.choice([-2, -1, 1, 1, 1])
    new_parent = random.choice([-2, -1, 0, 1, 2, 3])
    retain_reader = random.randint(0, 1)
    lazy = int(kind == 'ownerparent' and random.random() < 0.5)
    if lazy:
        old_parent = -2
    deleted = []
    u.mem_map(DESTROY_STUB, 0x1000)
    u.mem_write(DESTROY_STUB, b'\xc2\x04\x00')
    w32(u, 0x2037E8, DESTROY_STUB)
    def destroy(uc, address, size, user):
        node = uc.reg_read(UC_X86_REG_ECX)
        deleted.extend([r32(uc, node + 4), r32(uc, uc.reg_read(UC_X86_REG_ESP) + 4)])
    u.hook_add(UC_HOOK_CODE, destroy, begin=DESTROY_STUB, end=DESTROY_STUB)
    for i in range(4):
        u.mem_write(owners[i], b'\0' * 0x38)
        w32(u, owners[i] + 4, previous_kind if i == 1 else 3)
        w32(u, owners[i] + 0x10, 0x2037B8)
        w32(u, owners[i] + 0xC, operands[i])
        u.mem_write(operands[i], b'\0' * 0x2C)
        w32(u, operands[i] + 8, owners[i])
        w32(u, operands[i] + 0xC, 3)
        w32(u, operands[i] + 0x10, active)
    w32(u, operands[0] + 0x10, parent_at(old_parent))
    w32(u, operands[1] + 0x10, owners[2])
    call(u, 0x192713, owners[2] + 0x10, [operands[1]])
    if old_parent >= 0:
        call(u, 0x192713, parent_at(old_parent) + 0x10, [operands[0]])
    if retain_reader:
        call(u, 0x192713, owners[1] + 0x10, [operands[3]])
    if lazy:
        w32(u, owners[0] + 0xC, 0)
    extra = ''
    text = ''
    if kind in ('clearslot', 'clearslots'):
        instruction = HEAP + 0xF000
        vop = random.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15])
        sop = random.randrange(8)
        masks = [random.choice([0, 0, 1, 2, 255]) for _ in range(8)]
        orders = [random.randrange(5) for _ in range(12)]
        u.mem_write(instruction, b'\0' * 0x4C)
        w32(u, instruction + 0x2C, vop)
        w32(u, instruction + 0x30, sop)
        u.mem_write(instruction + 0x20, bytes(masks[:4]))
        u.mem_write(instruction + 0x28, bytes(masks[4:]))
        for slot in range(3):
            w32(u, instruction + 4 + 4 * slot, operands[slot])
            w32(u, operands[slot] + 4, instruction)
            w32(u, operands[slot] + 0xC, slot)
            for channel in range(4):
                w32(u, operands[slot] + 0x1C + 4 * channel, orders[4 * slot + channel])
        result = call(u, 0x19373D if kind == 'clearslot' else 0x19379C,
            operands[0] if kind == 'clearslot' else instruction, []) & 0xFF
        text = '%d ' % result + ''.join('%u,' % r32(u, operand + 0x1C + 4 * channel)
            for operand in operands[:3] for channel in range(4)) + ' '
        extra = ' ' + ' '.join(map(str, [vop, sop] + masks + orders))
    else:
        call(u, 0x192C9E if kind == 'reparent' else 0x192B20,
            operands[0] if kind == 'reparent' else owners[0], [parent_at(new_parent)])
    for owner in owners:
        operand = r32(u, owner + 0xC)
        text += '%08X:%08X/%u' % (operand, r32(u, operand + 0x10), r32(u, owner + 0x1C))
        def walk(node):
            return walk(r32(u, node + 0xC)) + [r32(u, node + 4)] + walk(r32(u, node + 0x10)) if node else []
        text += ''.join(',%08X' % v for v in walk(r32(u, owner + 0x18))) + ' '
    text += '|%u' % r32(u, TREE + 0xC) + ''.join(' %08X' % v for v in deleted)
    lines.append('%s -1 %d %d %d %d %d %d' % (kind, active_kind, previous_kind, old_parent, new_parent, retain_reader, lazy) + extra)
    return text

def run_dependency_build(u, state, lines, active_kind):
    state['fail'] = -1
    u.mem_write(0x000F7280, b'\xc2\x08\x00')
    u.mem_map(DESTROY_STUB, 0x1000)
    u.mem_write(DESTROY_STUB, b'\xc2\x04\x00')
    w32(u, 0x2037E8, DESTROY_STUB)
    instructions = [call(u, 0x00192DF1, 0, []) for _ in range(4)]
    owners = [HEAP + 0xB000 + 0x40 * j for j in range(4)]
    include = random.choice([0, 1, 2, 255])
    u.mem_write(0x001A4D30, bytes([include]))
    u.mem_write(0x001A4D58, b'\0' * 0x10)
    assignment_kind, query, existing = random.randrange(13), random.choice([0, 1, 2, 255]), random.randrange(2)
    description = [include, assignment_kind, query, existing]
    for index, instruction in enumerate(instructions):
        owner = owners[index]
        u.mem_write(owner, b'\0' * 0x40)
        w32(u, owner + 0x10, 0x2037B8)
        sequence, opcode = R(), random.choice([0, 1, 2, 3, 4, 5, 8])
        reader, chain, writer_mask = random.randrange(2), random.randrange(2), random.randrange(16)
        mask = [random.choice([0, 1, 255]) for _ in range(4)]
        order = [random.randrange(5) for _ in range(12)]
        owner_kind = random.randrange(7)
        description += [sequence, opcode, reader, chain, writer_mask, owner_kind] + mask + order
        w32(u, owner + 4, owner_kind)
        w32(u, instruction + 0x40, sequence)
        w32(u, instruction + 0x2C, opcode)
        w32(u, instruction + 0x34, owner)
        u.mem_write(instruction + 0x18, bytes(mask))
        for slot in range(3):
            operand = r32(u, instruction + 4 + 4 * slot)
            w32(u, operand + 0x10, owner)
            for j in range(4):
                w32(u, operand + 0x1C + 4 * j, order[slot * 4 + j])
        if index < 3:
            if reader:
                node = HEAP + 0xC000 + 0x40 * index
                u.mem_write(node, b'\0' * 0x20)
                w32(u, node, 0x2037E8)
                w32(u, node + 4, r32(u, instructions[index + 1] + 4))
                w32(u, owner + 0x18, node)
                w32(u, owner + 0x1C, 1)
            if chain:
                w32(u, owner + 0x34, owners[index + 1])
            for j in range(4):
                if writer_mask & (1 << j):
                    w32(u, owner + 0x20 + 4 * j, instructions[index + 1])
    text = ''
    for instruction in instructions:
        call(u, 0x00193C44, instruction, [])
        tree = r32(u, instruction + 0x48)
        values = []
        def walk(node):
            if node:
                walk(r32(u, node + 0xC))
                values.append(r32(u, node + 4))
                walk(r32(u, node + 0x10))
        walk(r32(u, tree + 8))
        text += '%08X %u |' % (tree, r32(u, instruction + 0x44)) + ''.join(' %08X' % value for value in values) + ' ; '
    for first in instructions:
        for second in instructions:
            text += '%d%d' % (bool(call(u, 0x00193FE4, first, [second])),
                bool(call(u, 0x00194002, 0, [first, second]) & 255))
    text += ' |'
    for instruction in instructions:
        for slot in range(3):
            operand = r32(u, instruction + 4 + 4 * slot)
            for owner in owners:
                text += str(call(u, 0x001940AC, operand, [owner, 0x203790, 0x01010101]) & 255)
    list_nodes = [HEAP + 0xD000 + 0x20 * j for j in range(4)]
    for j, node in enumerate(list_nodes):
        w32(u, node + 4, instructions[j])
        w32(u, node + 0xC, list_nodes[j + 1] if j < 3 else 0)
    call(u, 0x00194047, 0, [list_nodes[0], 0])
    text += ' |' + ''.join(' %08X' % r32(u, node + 4) for node in list_nodes)
    text += ' M'
    for index in range(1, 4):
        merged = call(u, 0x00194E5E, owners[0], [owners[index], 1, 0x203790,
            r32(u, instructions[0] + 0x18), 0x203790, r32(u, instructions[index] + 0x18)]) & 255
        text += str(merged)
    text += ''.join(',%08X' % r32(u, owner + 0x20 + 4 * channel) for owner in owners for channel in range(4))
    for instruction in instructions:
        text += ''.join(',%u' % r32(u, r32(u, instruction + 4 + 4 * slot) + 0x1C + 4 * channel)
            for slot in range(3) for channel in range(4))
        text += ''.join(',%u' % value for value in u.mem_read(instruction + 0x18, 4))
    w32(u, instructions[0] + 0x2C, 1)
    w32(u, instructions[0] + 0x30, 0)
    u.mem_write(instructions[0] + 0x10, b'\x01')
    w32(u, instructions[0] + 0x38, owners[0])
    u.mem_write(instructions[0] + 0x20, bytes(u.mem_read(instructions[0] + 0x18, 4)))
    u.mem_write(STACK - 0x18, bytes([r32(u, instructions[0] + 0x40) & 255]))
    rewritten = call(u, 0x00193222, instructions[0], []) & 255
    text += ' W%d,%08X' % (rewritten, r32(u, instructions[0] + 0x38))
    for instruction in instructions:
        text += ''.join(',%u' % value for value in u.mem_read(instruction + 0x18, 4))
        text += ''.join(',%u' % value for value in u.mem_read(instruction + 0x20, 4))
        text += ''.join(',%u' % r32(u, r32(u, instruction + 4 + 4 * slot) + 0x1C + 4 * channel)
            for slot in range(3) for channel in range(4))
    w32(u, owners[0] + 4, 1)
    w32(u, owners[3] + 4, 6)
    u.mem_write(owners[0] + 0x20, b'\0' * 16)
    u.mem_write(owners[3] + 0x20, b'\0' * 16)
    w32(u, owners[0] + 0x34, 0)
    w32(u, owners[0] + 0x30, owners[3])
    w32(u, owners[3] + 0x34, owners[0])
    reader = r32(u, instructions[1] + 4)
    if r32(u, owners[0] + 0x1C):
        w32(u, reader + 0x10, owners[0])
    call(u, 0x00194D12, owners[0], [0])
    rotated = call(u, 0x00195CA0, owners[0], []) & 255
    detached = call(u, 0x00195BE1, owners[0], []) & 255
    text += ' | %d %d' % (rotated, detached)
    text += ''.join(' %08X,%08X,%u' % (r32(u, owner + 0x30), r32(u, owner + 0x34), r32(u, owner + 0x1C)) for owner in owners)
    text += ' %08X' % r32(u, reader + 0x10)
    for owner in owners:
        u.mem_write(owner + 0x20, b'\0' * 24)
        w32(u, owner + 0x18, 0)
        w32(u, owner + 0x1C, 0)
    w32(u, owners[0] + 4, 0)
    w32(u, owners[0] + 8, r32(u, instructions[0] + 0x40) % 14)
    w32(u, owners[1] + 4, 1)
    w32(u, owners[1] + 8, assignment_kind)
    assignment_pass = HEAP + 0xA000
    u.mem_write(assignment_pass, b'\0' * 0x54)
    if existing:
        call(u, 0x00192830, assignment_pass + 0x28, [HEAP + 0xA100, assignment_kind, owners[1]])
    empty_readers = HEAP + 0xA200
    u.mem_write(empty_readers, b'\0' * 0x10)
    assigned = call(u, 0x00196814, assignment_pass,
        [assignment_kind, owners[0], empty_readers, instructions[0], 0x203790, 0x01010101, query]) & 255
    chosen = call(u, 0x00196A14, assignment_pass,
        [owners[2], empty_readers, instructions[0], 0x203790, 0x01010101, query])
    if chosen & 0x80000000:
        chosen -= 0x100000000
    scalar_assigned = call(u, 0x001969C0, assignment_pass,
        [owners[3], empty_readers, instructions[0], 0x203790, 0x01010101, query]) & 255
    text += ' | %d %d %d %u' % (assigned, chosen, scalar_assigned, r32(u, assignment_pass + 0x34))
    text += ''.join(' %u,%u,%08X,%08X' % (r32(u, owner + 4), r32(u, owner + 8),
        r32(u, owner + 0x30), r32(u, owner + 0x34)) for owner in owners)
    for instruction in instructions:
        available_pointer = HEAP + 0xA300
        accepted = call(u, 0x00197075, assignment_pass, [instruction, empty_readers, available_pointer]) & 255
        available = r32(u, available_pointer)
        if available & 0x80000000:
            available -= 0x100000000
        text += ' %d,%d' % (accepted, available)
    for instruction in instructions:
        sequence = r32(u, instruction + 0x40)
        w32(u, instruction + 0x38, owners[0] if sequence & 1 else 0)
        w32(u, instruction + 0x3C, owners[3] if sequence & 2 else 0)
        w32(u, instruction + 0x30, sequence % 8)
        u.mem_write(instruction + 0x20, b'\x01' * 4)
        u.mem_write(instruction + 0x28, b'\x01' * 4)
        accepted = call(u, 0x00196DD1, instruction, [assignment_pass, empty_readers, empty_readers]) & 255
        text += ' R%d,%u' % (accepted, r32(u, assignment_pass + 0x34))
    for instruction in instructions:
        w32(u, instruction + 0x34, 0)
    text += ' C'
    for instruction in instructions:
        for other in instructions:
            compatible = call(u, 0x001954F3, instruction, [other]) & 255
            on_pass = call(u, 0x00196C74, instruction, [other, assignment_pass, empty_readers]) & 255
            text += '%d%d' % (compatible, on_pass)
    for owner in owners:
        u.mem_write(owner + 0x20, b'\0' * 24)
        w32(u, owner + 0x18, 0)
        w32(u, owner + 0x1C, 0)
    sequence = r32(u, instructions[0] + 0x40)
    source = 0x100 if sequence & 1 else 0
    w32(u, owners[0] + 8, source)
    w32(u, owners[1] + 8, source)
    w32(u, owners[0] + 0x20 + 4 * (sequence % 4), 0xFFFFFFFF)
    w32(u, owners[1] + 0x20 + 4 * ((sequence >> 2) % 4), 0xFFFFFFFF)
    call(u, 0x00192830, assignment_pass + 0x3C, [HEAP + 0xA100, source, owners[1]])
    w32(u, 0x2037F8, DESTROY_STUB)
    special = call(u, 0x00195D42, assignment_pass, [owners[0]])
    text += ' S%08X,%u,%u' % (special, r32(u, assignment_pass + 0x48), r32(u, assignment_pass + 0x34))
    text += ''.join(',%08X,%08X' % (r32(u, owner + 0x30), r32(u, owner + 0x34)) for owner in owners[:2])
    merge_first = call(u, 0x00192DF1, 0, [])
    merge_second = call(u, 0x00192DF1, 0, [])
    w32(u, merge_first + 0x2C, sequence % 5)
    w32(u, merge_second + 0x2C, sequence % 5)
    w32(u, merge_second + 0x30, (sequence >> 4) % 8)
    w32(u, owners[2] + 4, 1)
    for slot in range(3):
        call(u, 0x00192C9E, r32(u, merge_first + 4 + 4 * slot), [owners[2]])
        call(u, 0x00192C9E, r32(u, merge_second + 4 + 4 * slot), [owners[2]])
    merged = call(u, 0x001956D1, merge_first, [merge_second]) & 255
    text += ' I%d,%u,%u,%u,%u,%u,%u' % (merged, r32(u, merge_first + 0x2C), r32(u, merge_first + 0x30),
        r32(u, merge_first + 0x44), r32(u, 0x001A4D64), r32(u, TREE + 0xC), r32(u, owners[2] + 0x1C))
    call(u, 0x001937D8, merge_first, [1])
    text += ' D%u,%u,%u' % (r32(u, 0x001A4D64), r32(u, TREE + 0xC), r32(u, owners[2] + 0x1C))
    program = HEAP + 0xA400
    packed = HEAP + 0xE000
    w32(u, ACTIVE_OWNER, HEAP + 0x9000)
    u.mem_write(0x001A4D48, b'\0' * 0x10)
    w32(u, 0x2037C4, DESTROY_STUB)
    w32(u, packed, 2)
    for index in range(2):
        value = r32(u, instructions[index] + 0x40)
        words = [(value & 0x0F0FFFFF) | ((value % 12) << 20) | 0x2000F000,
            (value & 0x0FF80000) | (value & 0x1FF0) | (value % 12) | 0x20004000 | (((value >> 4) % 12) << 15),
            (value & 0x01FFFFF0) | (value % 12) | ((value % 14) << 25) | (((value >> 8) % 8) << 29)]
        if r32(u, instructions[0] + 0x40) & 0x40:
            words = [(0x2F000000 if index == 0 else 0x2000F000) | ((value & 7) << 3),
                (value & 0x0FF80000) | (value & 0x1FF0) | (0x20004000 if index == 0 else 0x10004000),
                (value & 0x01FFFFF0) | 0x02000000]
        for j, word in enumerate(words):
            w32(u, packed + 4 + 12 * index + 4 * j, word)
    call(u, 0x00196739, program, [packed])
    text += ' P%u,%u,%u,%u' % (r32(u, program + 0x34), r32(u, program + 0x48), r32(u, program + 0x94), r32(u, 0x001A4D54))
    for index in range(13):
        owner = r32(u, program + 0x54 + 4 * index)
        text += ';%08X,%u,%u,%08X,%08X,%u' % (owner, r32(u, owner + 4), r32(u, owner + 8),
            r32(u, owner + 0x30), r32(u, owner + 0x34), r32(u, owner + 0x1C))
    values = []
    def program_walk(node):
        if node:
            program_walk(r32(u, node + 0xC))
            values.append(r32(u, node + 4))
            program_walk(r32(u, node + 0x10))
    program_walk(r32(u, program + 0x90))
    for instruction in values:
        text += ';%08X,%u,%u,%u,%08X,%08X,%08X' % (instruction, r32(u, instruction + 0x40), r32(u, instruction + 0x2C),
            r32(u, instruction + 0x30), r32(u, instruction + 0x34), r32(u, instruction + 0x38), r32(u, instruction + 0x3C))
        for offset in (0x18, 0x20, 0x28):
            text += ''.join(',%u' % value for value in u.mem_read(instruction + offset, 4))
        for slot in range(3):
            operand = r32(u, instruction + 4 + 4 * slot)
            text += ',%08X,%u' % (r32(u, operand + 0x10), u.mem_read(operand + 0x14, 1)[0])
            text += ''.join(',%u' % r32(u, operand + 0x1C + 4 * channel) for channel in range(4))
    u.mem_write(0x001A4D30, bytes([1, 0, 1, 0]))
    call(u, 0x00197223, program, [])
    text += ' Q%u,%u,%u,%u' % (r32(u, program + 0x94), r32(u, 0x001A4D54), r32(u, program + 0x34), r32(u, program + 0x48))
    u.mem_write(0x001A4D30, bytes([0, 0, 1, 1]))
    scheduled = call(u, 0x001973F1, program, []) & 0xFF
    text += ' T%d,%u' % (scheduled, r32(u, program + 0x10))
    node = r32(u, program + 8)
    while node:
        instruction = r32(u, node + 4)
        text += ';%08X,%u,%u,%u' % (instruction, r32(u, instruction + 0x40), r32(u, instruction + 0x2C), r32(u, instruction + 0x30))
        for slot in range(3):
            operand = r32(u, instruction + 4 + 4 * slot)
            parent = r32(u, operand + 0x10)
            text += ',%u,%u,%u' % (r32(u, parent + 4), r32(u, parent + 8), u.mem_read(operand + 0x14, 1)[0])
        node = r32(u, node + 0xC)
    u.mem_write(0x001A4D38, b'\0' * 0x30)
    w32(u, ACTIVE_OWNER, 0)
    from unicorn.x86_const import UC_X86_REG_ESP
    serializer_calls = [0]
    def initialize_serializer_frame(uc, address, size, user):
        if serializer_calls[0] == 0:
            esp = uc.reg_read(UC_X86_REG_ESP)
            for offset in (0x20, 0x1C, 0x18):
                w32(uc, esp - offset, 0)
        serializer_calls[0] += 1
    u.hook_add(UC_HOOK_CODE, initialize_serializer_frame, begin=0x0018F8B7, end=0x0018F8B7)
    validated = call(u, 0x0019723C, 0, [packed]) & 0xFF
    text += ' V%d,%u,%u,%u,%u' % (validated, r32(u, packed), r32(u, TREE + 0xC), r32(u, 0x001A4D54), r32(u, 0x001A4D64))
    for word in range(1, r32(u, packed) * 3 + 1):
        text += ',%08X' % r32(u, packed + word * 4)
    text += ' | %d %u' % (state['calls'], r32(u, 0x001A4D64))
    lines.append('dependencybuild -1 %d %s' % (active_kind, ' '.join(map(str, description))))
    return text

def run_split(u, state, lines, active_kind, kind):
    state['fail'] = -1
    source = call(u, 0x00192DF1, 0, [])
    vector = random.choice([0, 1, 2, 3, 4, 5, R()])
    scalar = random.choice([0, 1, 2, 3, 4, 7, R()])
    flag = random.choice([0, 1, 2, 255])
    sequence, marker = R(), random.randrange(256)
    order = [random.randrange(5) for _ in range(4)]
    w32(u, source + 0x2C, vector)
    w32(u, source + 0x30, scalar)
    w32(u, source + 0x40, sequence)
    u.mem_write(source + 0x10, bytes([flag]))
    operand = r32(u, source + 0xC)
    u.mem_write(operand + 0x14, bytes([marker]))
    for j, channel in enumerate(order):
        w32(u, operand + 0x1C + 4 * j, channel)
    description = [vector, scalar, flag, sequence, marker] + order
    owners = [HEAP + 0xB000 + 0x40 * j for j in range(3)]
    for owner in owners:
        u.mem_write(owner, b'\0' * 0x40)
    for slot, offset in enumerate((0x34, 0x38, 0x3C)):
        index = random.choice([-1, 0, 1, 2])
        mask = [random.choice([0, 0, 1, 2, 255]) for _ in range(4)]
        owner = owners[index] if index >= 0 else 0
        w32(u, source + offset, owner)
        u.mem_write(source + 0x18 + slot * 8, bytes(mask))
        if owner:
            for j, channel in enumerate(mask):
                if channel:
                    w32(u, owner + 0x20 + 4 * j, source)
        description += [index] + mask
    pass_object = HEAP + 0xA000
    pass_tree = pass_object + 0x88
    if kind == 'split':
        result = call(u, 0x00192F0C, source, [])
    else:
        u.mem_write(pass_tree, b'\0' * 0x10)
        call(u, 0x001927D1, pass_tree, [source])
        if kind == 'splitregister':
            call(u, 0x0019368D, pass_object, [source])
        else:
            call(u, 0x00193F8A, pass_object, [])
        result = 0
        def find_result(node):
            nonlocal result
            if node:
                value = r32(u, node + 4)
                if value != source:
                    result = value
                find_result(r32(u, node + 0xC))
                find_result(r32(u, node + 0x10))
        find_result(r32(u, pass_tree + 8))
    text = '%08X %d' % (result, state['calls'])
    for instruction in (source, result):
        if not instruction:
            continue
        text += ' | %08X %u %u %u %u %u' % (r32(u, instruction), r32(u, instruction + 0x2C),
            r32(u, instruction + 0x30), r32(u, instruction + 0x40), r32(u, instruction + 0x44), u.mem_read(instruction + 0x10, 1)[0])
        text += ''.join(' %08X' % r32(u, instruction + off) for off in (0x34, 0x38, 0x3C))
        text += ''.join(' ' + bytes(u.mem_read(instruction + off, 4)).hex().upper() for off in (0x18, 0x20, 0x28))
        for slot in range(3):
            operand = r32(u, instruction + 4 + 4 * slot)
            text += ' %08X:%08X,%u,%08X,%u,%u,%u,%u,%u' % (operand, r32(u, operand), r32(u, operand + 0xC),
                r32(u, operand + 0x10), u.mem_read(operand + 0x14, 1)[0], *[r32(u, operand + 0x1C + 4 * j) for j in range(4)])
    for owner in owners:
        text += ' |' + ''.join(' %08X' % r32(u, owner + 0x20 + 4 * j) for j in range(4))
    values = []
    def walk(node):
        if node:
            walk(r32(u, node + 0xC))
            values.append(r32(u, node + 4))
            walk(r32(u, node + 0x10))
    walk(r32(u, TREE + 8))
    text += ' | %u' % r32(u, TREE + 0xC) + ''.join(' %08X' % value for value in values)
    if kind != 'split':
        values.clear()
        walk(r32(u, pass_tree + 8))
        text += ' | %u' % r32(u, pass_tree + 0xC) + ''.join(' %08X' % value for value in values)
    lines.append('%s -1 %d %s' % (kind, active_kind, ' '.join(map(str, description))))
    return text

def main():
    random.seed(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    lines, expected = [], []
    for _ in range(int(sys.argv[2]) if len(sys.argv) > 2 else 60):
        kind = sys.argv[3] if len(sys.argv) > 3 and sys.argv[3] in ('dependencybuild', 'destinations', 'moveunwritten', 'split', 'splitregister', 'splitpass') else random.choice(['dependencybuild', 'splitregister', 'splitpass', 'split', 'split', 'ctor', 'owner', 'owner', 'passdtor', 'passdtor', 'readers', 'readers', 'readers', 'negate', 'negate', 'negate',
            'equiv', 'equiv', 'equiv', 'retire', 'retire', 'retire', 'regtree', 'readmask', 'readmask', 'input', 'input',
            'programdtor', 'programdtor', 'programdelete', 'movechannels', 'movechannels', 'dependencies', 'dependencies', 'reorder', 'reorder',
            'moveunwritten', 'moveunwritten', 'destinations', 'destinations', 'destinations', 'clearslot', 'clearslots', 'reparent', 'ownerparent', 'unwritten', 'unuseddest', 'link', 'link', 'select', 'select', 'ownerreader', 'channelreader', 'installreg', 'installreg', 'installpass'])
        u = new_emu_with_runtime()
        state = {'next': BASE, 'calls': 0, 'fail': random.choice([-1, -1, 2, 3, 4, 5, 6, 7, 8])}
        if kind in ('input', 'select'):
            state['fail'] = random.choice([-1, -1, 0, 1, 2, 3, 4])
        def allocate(uc, address, size, user):
            index = state['calls']
            state['calls'] += 1
            if index == state['fail']:
                uc.reg_write(UC_X86_REG_EAX, 0)
            else:
                uc.reg_write(UC_X86_REG_EAX, state['next'])
                state['next'] += 0x100
        u.mem_map(0x000F7000, 0x1000)
        u.mem_write(XMEMALLOC, b'\xc2\x08\x00')
        u.hook_add(UC_HOOK_CODE, allocate, begin=XMEMALLOC, end=XMEMALLOC)
        u.mem_write(BASE, b'\xcd' * 0x40000)
        active_kind = random.randint(0, 1)
        active = HEAP + 0x9000
        w32(u, ACTIVE_OWNER, active if active_kind else 0)
        w32(u, TREE + 8, 0)
        w32(u, TREE + 0xC, 0)
        if kind == 'dependencybuild':
            expected.append(run_dependency_build(u, state, lines, active_kind))
            continue
        if kind in ('split', 'splitregister', 'splitpass'):
            expected.append(run_split(u, state, lines, active_kind, kind))
            continue
        if kind == 'moveunwritten':
            expected.append(run_move_unwritten(u, state, lines, active_kind))
            continue
        if kind in ('reparent', 'ownerparent', 'clearslot', 'clearslots'):
            expected.append(run_reparent(u, state, lines, kind, active_kind))
            continue
        if kind in ('owner', 'input', 'select'):
            source = random.choice([0, 1, 7, R()])
            use_owner = random.randint(0, 1)
            pass_owner_kind = random.choice([0, 1, 2])
            pass_object = HEAP + 0xA000
            pass_owner = HEAP + 0xB000
            u.mem_write(pass_owner, b'\0' * 0x40)
            w32(u, pass_object + 0x50, [0, active if active_kind else 0, pass_owner][pass_owner_kind])
            prefix = ''
            extra = ''
            if kind == 'select':
                selector = random.choice([0, 1, 1, 2, 2, 3, 3, 4, R()])
                input_source = R()
                register_kind = random.choice([0, 1, 12, 13, R()])
                tree = random.randint(0, 1)
                key = random.choice([register_kind, 0x100, R()])
                presence = random.choice([0, 1, 2, 2])
                mask = [random.randrange(256) for _ in range(4)]
                u.mem_write(pass_object + 0x14, b'\0' * 0x3C)
                if presence:
                    node = HEAP + 0xC000
                    u.mem_write(node, b'\0' * 0x1C)
                    w32(u, node + 4, key)
                    w32(u, node + 8, pass_owner if presence == 2 else 0)
                    offset = 0x28 if tree == 0 else 0x3C
                    w32(u, pass_object + offset + 8, node)
                    w32(u, pass_object + offset + 0xC, 1)
                owner = call(u, 0x00193DF4, pass_object, [selector, input_source, source, register_kind,
                    use_owner | (R() & 0xFFFFFF00), 0xDEADBEEF, struct.unpack('<I', bytes(mask))[0]])
                extra = ' %s' % ' '.join(map(str, [selector, input_source, register_kind, tree, key, presence] + mask))
            elif kind == 'input':
                u.mem_write(pass_object + 0x14, b'\0' * 0x14)
                owner = call(u, 0x00193566, pass_object, [source])
                before = state['calls']
                again = call(u, 0x00193566, pass_object, [source])
                prefix = '%d %d %u ' % (again == owner, state['calls'] == before, r32(u, pass_object + 0x20))
            else:
                owner = call(u, 0x001935E6, pass_object, [source, use_owner | (R() & 0xFFFFFF00)])
            if not owner:
                text = '-1 | %u' % r32(u, TREE + 0xC)
            else:
                operand = r32(u, owner + 0xC)
                def parent_code(parent):
                    if parent == 0: return 0
                    if parent == r32(u, ACTIVE_OWNER): return 1
                    if parent == pass_owner: return 2
                    return 3
                text = '%X %08X %u %u %08X %u %u %u %u %d' % (owner, r32(u, owner), r32(u, owner + 4), r32(u, owner + 8), r32(u, owner + 0x10),
                    r32(u, owner + 0x1C), r32(u, owner + 0x20), r32(u, owner + 0x2C), 1 if r32(u, owner + 0x30) == 0 and r32(u, owner + 0x34) == 0 else 0,
                    -1 if operand == 0 else 0)
                if operand:
                    text += ' %X:%08X,%d,%u,%d,%u' % (operand, r32(u, operand), 1 if r32(u, operand + 8) == owner else 0, r32(u, operand + 0xC),
                        parent_code(r32(u, operand + 0x10)), 1 if r32(u, operand + 4) == 0 else 0)
                text += ' %u |' % r32(u, pass_owner + 0x1C)
                values = []
                def walk(node):
                    if node:
                        walk(r32(u, node + 0xC))
                        values.append(r32(u, node + 4))
                        walk(r32(u, node + 0x10))
                walk(r32(u, TREE + 8))
                text += ' %u' % r32(u, TREE + 0xC) + ''.join(' %08X' % v for v in values)
            expected.append(prefix + text)
            lines.append('%s %d %d %d %d %d%s' % (kind, state['fail'], active_kind, source, use_owner, pass_owner_kind, extra))
            continue
        if kind == 'regtree':
            # 0x0019149A on a pre-filled 0x14-byte object.
            words = [R() for _ in range(5)]
            tree = HEAP + 0xC000
            for j, word in enumerate(words):
                w32(u, tree + 4 * j, word)
            returned = call(u, 0x0019149A, tree, [])
            expected.append('%d %s' % (1 if returned == tree else 0, ' '.join('%08X' % r32(u, tree + 4 * j) for j in range(5))))
            lines.append('regtree 0 0 %s' % ' '.join(map(str, words)))
            continue
        if kind in ('destinations', 'readers', 'readmask', 'negate', 'equiv', 'retire', 'movechannels', 'dependencies', 'reorder', 'link',
                    'unwritten', 'unuseddest', 'ownerreader', 'channelreader', 'installreg', 'installpass'):
            text = run_owner_sets(u, state, lines, kind)
            if text is None:
                continue
            expected.append(text)
            continue
        if kind in ('passdtor', 'programdtor', 'programdelete'):
            expected.append(run_pass_destructor(u, state, lines, kind != 'passdtor', kind == 'programdelete'))
            continue
        instruction = call(u, 0x00192DF1, 0, [])
        fmt = lambda v: ('%X' % v) if v else '-1'
        text = '%X %08X' % (instruction, r32(u, instruction))
        for off in (0x14, 0x1C, 0x24):
            text += ' %08X%s' % (r32(u, instruction + off), bytes(u.mem_read(instruction + off + 4, 4)).hex().upper())
        text += ' %u %u %u %u %u %u %d%d%d' % (r32(u, instruction + 0x40), r32(u, instruction + 0x44), 1 if r32(u, instruction + 0x48) else 0,
            r32(u, instruction + 0x2C), r32(u, instruction + 0x30), u.mem_read(instruction + 0x10, 1)[0], 1 if r32(u, instruction + 0x34) else 0,
            1 if r32(u, instruction + 0x38) else 0, 1 if r32(u, instruction + 0x3C) else 0)
        for slot in range(3):
            operand = r32(u, instruction + 4 + 4 * slot)
            text += ' ' + fmt(operand)
            if operand:
                text += ':%08X,%s,%d,%u,%d,%u,%08X,%s' % (r32(u, operand), fmt(r32(u, operand + 4)), 1 if r32(u, operand + 8) else 0, r32(u, operand + 0xC),
                    1 if r32(u, operand + 0x10) == r32(u, ACTIVE_OWNER) else 0, u.mem_read(operand + 0x14, 1)[0], r32(u, operand + 0x18),
                    ''.join('%u' % r32(u, operand + 0x1C + 4 * j) for j in range(4)))
        values = []
        def walk(node):
            if node:
                walk(r32(u, node + 0xC))
                values.append(r32(u, node + 4))
                walk(r32(u, node + 0x10))
        walk(r32(u, TREE + 8))
        text += ' | %u' % r32(u, TREE + 0xC) + ''.join(' %08X' % v for v in values)
        expected.append(text)
        lines.append('ctor %d %d' % (state['fail'], active_kind))

    host = subprocess.run([harness('ctor_harness')], input='\n'.join(lines) + '\n', capture_output=True, text=True)
    got = host.stdout.splitlines()
    bad = 0
    for line, want, have in zip(lines, expected, got):
        if want.strip() != have.strip():
            bad += 1
            if bad < 6:
                print('MISMATCH', line, '\n exp', want, '\n got', have)
    print('cases', len(lines), len(got), 'mismatches', bad)
    if bad or len(got) != len(expected) or host.returncode != 0:
        raise SystemExit(1)

if __name__ == '__main__':
    main()
