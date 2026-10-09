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

def run_pass_destructor(u, state, lines):
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
    used = (state['next'] - base) // 0x80
    for i in range(used):
        node = base + 0x80 * i
        owners[node] = ('registry', i)
        w32(u, node, vtable)
    call(u, 0x001934A5, pass_object, [])
    text = '%08X %08X %08X %08X %u %u %u %u %u %u' % (r32(u, pass_object), r32(u, pass_object + 0x14), r32(u, pass_object + 0x28),
        r32(u, pass_object + 0x3C), r32(u, pass_object + 0x20), r32(u, pass_object + 0x34), r32(u, pass_object + 0x48), r32(u, pass_object + 0x10),
        r32(u, EXPRESSIONS + 0xC), r32(u, TREE + 0xC))
    text += ''.join(' e%d' % e for e in events)
    lines.append('passdtor %d 0 %s %s' % (list_count, ' '.join(description), ' '.join(registries)))
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
        owner_kind = random.choice([0, 1, 1, 2, 3]) if kind == 'negate' else random.choice([1, 3, 3]) if kind == 'equiv' else 1
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
    if kind == 'retire':
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
        text += ''.join(' f%u' % e for e in events)
        lines.append('retire %d %d %s %d %d %s %d %s %d' % (n, m, ' '.join(description), target, len(members), ' '.join(map(str, members)),
            len(keys), ' '.join('%d %d' % kd for kd in zip(keys, datas)), retire_kind if retire_kind < 0x80000000 else retire_kind - 0x100000000))
        return text
    # The retail functions compare operand and instruction pointers; the harness maps them to 0x3000+j / 0x2000+k,
    # which preserves order because both are allocated in ascending address order.
    if kind == 'readers':
        registry = HEAP + 0x2800
        w32(u, registry + 8, 0)
        w32(u, registry + 0xC, 0)
        state['next'], state['calls'], state['fail'] = HEAP + 0x40000, 0, -1
        members = random.sample(range(3 * m), random.randint(0, 3 * m))
        for j in members:
            call(u, 0x00192655, registry, [operands[j]])
        mask = [random.choice([0, 1]) for _ in range(4)]
        instruction = random.choice([-1] + list(range(m)))
        climb_derived, climb_channel = random.randint(0, 1), random.randint(0, 1)
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
        events.append(((pointer - base) // 0x80) * 10 + 1)
    hook = u.hook_add(UC_HOOK_CODE, free, begin=0x000F7280, end=0x000F7280)
    result = call(u, 0x00192B84, owners[target], []) & 0xFF
    u.hook_del(hook)
    used = (state['next'] - base) // 0x80
    text = '%d %d' % (result, used)
    text += ''.join(' d%d' % e for e in events)
    text += ''.join(' %u' % u.mem_read(op + 0x14, 1)[0] for op in operands)
    lines.append('negate %d %d %s %d' % (n, m, ' '.join(description), target))
    return text

def main():
    random.seed(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    lines, expected = [], []
    for _ in range(int(sys.argv[2]) if len(sys.argv) > 2 else 60):
        kind = random.choice(['ctor', 'owner', 'owner', 'passdtor', 'passdtor', 'readers', 'readers', 'readers', 'negate', 'negate', 'negate',
            'equiv', 'equiv', 'equiv', 'retire', 'retire', 'retire', 'regtree'])
        u = new_emu_with_runtime()
        state = {'next': BASE, 'calls': 0, 'fail': random.choice([-1, -1, 2, 3, 4, 5, 6, 7, 8])}
        def allocate(uc, address, size, user):
            index = state['calls']
            state['calls'] += 1
            if index == state['fail']:
                uc.reg_write(UC_X86_REG_EAX, 0)
            else:
                uc.reg_write(UC_X86_REG_EAX, state['next'])
                state['next'] += 0x80
        u.mem_map(0x000F7000, 0x1000)
        u.mem_write(XMEMALLOC, b'\xc2\x08\x00')
        u.hook_add(UC_HOOK_CODE, allocate, begin=XMEMALLOC, end=XMEMALLOC)
        u.mem_write(BASE, b'\xcd' * 0x2000)
        active_kind = random.randint(0, 1)
        active = HEAP + 0x9000
        w32(u, ACTIVE_OWNER, active if active_kind else 0)
        w32(u, TREE + 8, 0)
        w32(u, TREE + 0xC, 0)
        if kind == 'owner':
            source = random.choice([0, 1, 7, R()])
            use_owner = random.randint(0, 1)
            pass_owner_kind = random.choice([0, 1, 2])
            pass_object = HEAP + 0xA000
            pass_owner = HEAP + 0xB000
            u.mem_write(pass_owner, b'\0' * 0x40)
            w32(u, pass_object + 0x50, [0, active if active_kind else 0, pass_owner][pass_owner_kind])
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
            expected.append(text)
            lines.append('owner %d %d %d %d %d' % (state['fail'], active_kind, source, use_owner, pass_owner_kind))
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
        if kind in ('readers', 'negate', 'equiv', 'retire'):
            text = run_owner_sets(u, state, lines, kind)
            if text is None:
                continue
            expected.append(text)
            continue
        if kind == 'passdtor':
            expected.append(run_pass_destructor(u, state, lines))
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

    got = subprocess.run([harness('ctor_harness')], input='\n'.join(lines) + '\n', capture_output=True, text=True).stdout.splitlines()
    bad = 0
    for line, want, have in zip(lines, expected, got):
        if want.strip() != have.strip():
            bad += 1
            if bad < 6:
                print('MISMATCH', line, '\n exp', want, '\n got', have)
    print('cases', len(lines), len(got), 'mismatches', bad)

if __name__ == '__main__':
    main()
