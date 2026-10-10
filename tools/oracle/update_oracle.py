"""Differential check of the player update helpers against the host build:
motion history 0x00025A30, floor tracking 0x00110C90 (with 0x00036AF0, 0x00110BF0, 0x00110EF0 and
0x001108B0), status timers 0x00019520, bounds 0x0002C980, room tracking 0x0002B450 (with 0x001248D0),
and body sphere pushes 0x00025F00/0x00026100 (with 0x00056330, 0x00040FB0, 0x0001FE00, 0x001226C0,
0x00122480, 0x00122C10 and 0x000662A0). Heap calls and virtual probes/receivers are hooked.
Needs cache/oracle/mem_00011000.bin, mem_00210000.bin and mem_00339000.bin."""
import os, random, struct, subprocess, sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_ESI, UC_X86_REG_EBX, UC_X86_REG_EDI
from retail_cpu import ROOT, STOP, STACK, HEAP, w32, r32, harness

ALLOCATE, REALLOCATE, FREE, HEAP_FREE = 0x000F72D0, 0x000F73A0, 0x000663E0, 0x0014D360
STUBS = 0x00F10000
MESSAGE, SWEEP, QUERY = STUBS, STUBS + 0x10, STUBS + 0x20
GRIDS, NAV_COUNT, MIN_Y, MAX_Y, ORIGIN_X, ORIGIN_Z = 0x0050E240, 0x0050E250, 0x0050E254, 0x0050E25C, 0x0050E264, 0x0050E26C
FREEZE, DT, SCALE, BLOCK, DECAY, DECAY_INDEX = 0x004312DC, 0x005096C0, 0x004312C4, 0x0050E768, 0x0030F144, 0x0051248C
BOUNDS = 0x0050EA00
OCCUPANCY, OCCUPANCY_MASK = 0x00512344, 0x00512348
SEGMENT_HEAD = 0x00511AF4

def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]

def fb(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]

def bf(u):
    return struct.unpack('<f', struct.pack('<I', u & 0xffffffff))[0]

def wf(u, a, x):
    w32(u, a, fb(x))

def rand_float(lo, hi):
    return f32(random.uniform(lo, hi))

class Emu:
    def __init__(self):
        u = Uc(UC_ARCH_X86, UC_MODE_32)
        u.mem_map(0, 0x1000)  # Empty goal rings read their kind from address zero.
        u.mem_map(0x00010000, 0x150000)
        u.mem_write(0x00011000, open(os.path.join(ROOT, 'cache', 'oracle', 'mem_00011000.bin'), 'rb').read())
        u.mem_map(0x00200000, 0x10000)
        u.mem_write(0x00200000, open(os.path.join(ROOT, 'cache', 'oracle', 'mem_00200000.bin'), 'rb').read()[:0x3000])
        u.mem_write(0x00203000, open(os.path.join(ROOT, 'cache', 'oracle', 'mem_00203000.bin'), 'rb').read()[:0xD000])
        u.mem_map(0x00210000, 0x130000)
        u.mem_write(0x00210000, open(os.path.join(ROOT, 'cache', 'oracle', 'mem_00210000.bin'), 'rb').read())
        u.mem_write(0x00339000, open(os.path.join(ROOT, 'cache', 'oracle', 'mem_00339000.bin'), 'rb').read())
        u.mem_map(0x00400000, 0x200000)
        u.mem_map(STOP, 0x1000); u.mem_write(STOP, b'\xf4')
        u.mem_map(STUBS, 0x1000); u.mem_write(STUBS, b'\xc3' * 0x1000)
        u.mem_map(HEAP, 0x400000)
        u.mem_map(STACK - 0x10000, 0x20000)
        self.u = u
        self.next = HEAP + 0x200000
        self.sizes = {}
        self.freed = []
        self.messages = []
        self.calls = []
        self.probes = {}
        for a in (ALLOCATE, REALLOCATE, FREE):
            u.mem_write(a, b'\xc3')
        u.hook_add(UC_HOOK_CODE, self.allocate, begin=ALLOCATE, end=ALLOCATE)
        u.hook_add(UC_HOOK_CODE, self.reallocate, begin=REALLOCATE, end=REALLOCATE)
        u.hook_add(UC_HOOK_CODE, self.free, begin=FREE, end=FREE)
        u.mem_write(HEAP_FREE, bytes([0xC2, 0x0C, 0x00]))  # XAPI HeapFree(heap, flags, block), stdcall.
        u.hook_add(UC_HOOK_CODE, self.heap_free, begin=HEAP_FREE, end=HEAP_FREE)
        u.hook_add(UC_HOOK_CODE, self.message, begin=MESSAGE, end=MESSAGE)
        u.hook_add(UC_HOOK_CODE, self.sweep, begin=SWEEP, end=SWEEP)
        u.hook_add(UC_HOOK_CODE, self.query, begin=QUERY, end=QUERY)

    def block(self, n, fill=b'\xcd'):
        b = self.next
        self.next += (n + 0xF) & ~0xF
        self.sizes[b] = n
        self.u.mem_write(b, fill * n)
        return b

    def allocate(self, uc, address, size, user):
        uc.reg_write(UC_X86_REG_EAX, self.block(r32(uc, uc.reg_read(UC_X86_REG_ESP) + 4)))

    def reallocate(self, uc, address, size, user):
        old = uc.reg_read(UC_X86_REG_EAX)
        n = r32(uc, uc.reg_read(UC_X86_REG_ESP) + 4)
        b = self.block(n)
        uc.mem_write(b, bytes(uc.mem_read(old, min(n, self.sizes[old]))))
        uc.reg_write(UC_X86_REG_EAX, b)

    def free(self, uc, address, size, user):
        # 0x000663E0 receives the block in EAX.
        self.freed.append(uc.reg_read(UC_X86_REG_EAX))

    def heap_free(self, uc, address, size, user):
        self.freed.append(r32(uc, uc.reg_read(UC_X86_REG_ESP) + 12))

    def message(self, uc, address, size, user):
        esp = uc.reg_read(UC_X86_REG_ESP)
        self.messages.append(r32(uc, r32(uc, esp + 8)))

    # kind_offset 1 is vtable +0x20 (0x001226C0, with the extra param word); 0 is +0x1C (0x00122480).
    def probe_call(self, uc, kind_offset):
        esp = uc.reg_read(UC_X86_REG_ESP)
        self_ = r32(uc, esp + 4)
        candidate, kept = r32(uc, esp + 8), r32(uc, esp + 12)
        base = esp + 16 + (4 if kind_offset else 0)
        kind, mask, record = r32(uc, base), r32(uc, base + 4), r32(uc, base + 8)
        p = self.probes[self_]
        cy = bf(r32(uc, candidate + 4))
        kx = bf(r32(uc, kept))
        hit = (mask & p['mask']) != 0 and f32(f32(kx * p['a']) + cy) < p['h']
        self.calls.append('%d %d %08x %08x %08x %08x %08x %08x %d' % (p['id'], kind_offset, r32(uc, candidate), r32(uc, candidate + 4),
            r32(uc, candidate + 8), r32(uc, kept), r32(uc, kept + 4), r32(uc, kept + 8), kind))
        if hit:
            for i, n in enumerate(p['normal']):
                wf(uc, record + 0xC + 4 * i, n)
            uc.mem_write(record + 0x26, bytes([p['flag'], p['id']]))
            if p['clamp']:
                wf(uc, kept + 4, p['h'])
        uc.reg_write(UC_X86_REG_EAX, 1 if hit else 0)

    def sweep(self, uc, address, size, user):
        self.probe_call(uc, 0)

    def query(self, uc, address, size, user):
        self.probe_call(uc, 1)

    def call(self, fn, args, regs=None):
        u = self.u
        esp = STACK
        w32(u, esp, STOP)
        for i, a in enumerate(args):
            w32(u, esp + 4 + 4 * i, a)
        u.reg_write(UC_X86_REG_ESP, esp)
        for r, v in (regs or {}).items():
            u.reg_write(r, v)
        u.emu_start(fn, STOP)
        return u.reg_read(UC_X86_REG_EAX)

def clock(e, line):
    u = e.u
    freeze = random.choice([0.0, 0.0, 0.0, 1.0, -1.0])
    dt, scale = rand_float(0.0, 0.05), random.choice([1.0, 1.0, rand_float(0.0, 2.0)])
    block = random.choice([0, 0, 0, 1])
    index = random.randint(0, 3)
    enabled = [random.choice([0, 1, 1]) for _ in range(4)]
    wf(u, FREEZE, freeze); wf(u, DT, dt); wf(u, SCALE, scale); w32(u, BLOCK, block); w32(u, DECAY_INDEX, index)
    u.mem_write(DECAY, bytes(enabled))
    line += [fb(freeze), fb(dt), fb(scale), block, index] + enabled

def actor_rates(e, actor, line):
    pause = random.choice([0.0, 0.0, 0.5, -0.5])
    rate = random.choice([1.0, rand_float(0.0, 3.0), 0.0, -1.0])
    wf(e.u, actor + 0x930, pause); wf(e.u, actor + 0x92C, rate)
    line += [fb(pause), fb(rate)]

def new_actor(e, line, vtable=True):
    actor = e.block(0xD00, b'\x00')
    vt = e.block(0x40, b'\x00')
    w32(e.u, actor, vt)
    w32(e.u, vt + 0xC, MESSAGE)
    position = [rand_float(-100, 100) for _ in range(3)]
    for i, p in enumerate(position):
        wf(e.u, actor + 8 + 4 * i, p)
    line += [fb(p) for p in position]
    actor_rates(e, actor, line)
    return actor

def run_motion(e, line):
    clock(e, line)
    owner = new_actor(e, line)
    history = e.block(4 + 0x50 * 12, b'\x00')
    index = random.randint(0, 0x4F)
    w32(e.u, history, index)
    delta = e.block(12)
    values = [rand_float(-10, 10) for _ in range(3)]
    for i, d in enumerate(values):
        wf(e.u, delta + i * 4, d)
    line += [index] + [fb(d) for d in values]
    from unicorn.x86_const import UC_X86_REG_EDX
    result = e.call(0x00025A30, [], {UC_X86_REG_ECX: history, UC_X86_REG_EDX: delta, UC_X86_REG_EAX: owner})
    sample = history + 4 + index * 12
    return '%d %d %08x %08x %08x' % (result, r32(e.u, history), r32(e.u, sample), r32(e.u, sample + 4), r32(e.u, sample + 8))

def nav_grids(e, line, count):
    u = e.u
    ox, oz = random.randint(-50, 50), random.randint(-50, 50)
    w32(u, ORIGIN_X, ox); w32(u, ORIGIN_Z, oz); w32(u, NAV_COUNT, count)
    line += [ox & 0xffffffff, oz & 0xffffffff, count]
    for b in range(2):
        lo = rand_float(-20, 20); hi = f32(lo + rand_float(0, 30))
        wf(u, MIN_Y + 4 * b, lo); wf(u, MAX_Y + 4 * b, hi)
        line += [fb(lo), fb(hi)]
    grids = []
    for g in range(4):
        if g >= count and random.random() < 0.7:
            w32(u, GRIDS + 4 * g, 0); line.append(0); grids.append(None); continue
        width, depth, cell = random.randint(1, 6), random.randint(1, 6), random.choice([4, 8, 16])
        grid = e.block(0x20, b'\x00')
        cells = e.block(0x12 * width * depth, b'\x00')
        w32(u, grid, width); w32(u, grid + 4, depth); w32(u, grid + 8, cell); w32(u, grid + 0x10, cells)
        w32(u, GRIDS + 4 * g, grid)
        line += [1, width, depth, cell]
        for i in range(width * depth):
            word = random.choice([0, 0x200, random.getrandbits(16), 0x1F, 0x1E0])
            u.mem_write(cells + 0x12 * i + 6, struct.pack('<H', word))
            line.append(word)
        grids.append((grid, cells, width, depth))
    return grids, ox, oz

def run_floor(e, line):
    u = e.u
    count = random.choice([1, 2, 2])
    grids, ox, oz = nav_grids(e, line, count)
    floor = random.randint(0, count - 1)
    point = e.block(12)
    p = [f32(ox + random.uniform(-10, 60)), rand_float(-30, 40), f32(oz + random.uniform(-10, 60))]
    for i, v in enumerate(p):
        wf(u, point + 4 * i, v)
    preferred = random.choice([-1, 0, 1, floor, 7])
    check = random.choice([0, 1])
    line += [fb(v) for v in p] + [floor, preferred & 0xffffffff, check]
    floor_slot = e.block(4); w32(u, floor_slot, floor)
    records_slot = e.block(4)
    rows = []
    if grids[floor] is not None and random.random() < 0.7:
        g = grids[floor]
        n = random.randint(0, 4)
        header = e.block(0x10 + 0xC * max(n, 1))
        u.mem_write(header, struct.pack('<HHIII', 0xC, 4, header + 0x10, header + 0x10 + 0xC * n, header + 0x10 + 0xC * max(n, 1)))
        for r in range(n):
            rows.append((random.randrange(g[2] * g[3]), random.choice([0, 1])))
            u.mem_write(header + 0x10 + 0xC * r, struct.pack('<II', *rows[-1]))
        w32(u, records_slot, header)
        line += [1, n] + [v for row in rows for v in row]
    else:
        w32(u, records_slot, 0)
        line.append(0)
    search = 0
    if random.random() < 0.6:
        search = e.block(0x34)
        nodes = e.block(0x14 * 40, b'\x00')
        scratch = random.choice([0, e.block(8)])
        words = [random.getrandbits(32) for _ in range(13)]
        line += [1, 1 if scratch else 0] + [w for i, w in enumerate(words) if i not in (3, 5)]
        words[3] = nodes; words[5] = scratch
        u.mem_write(search, struct.pack('<13I', *words))
    else:
        line.append(0)
    result = e.call(0x00110C90, [point, floor_slot, records_slot, search], {UC_X86_REG_ECX: preferred & 0xffffffff, UC_X86_REG_EAX: check})
    new_floor = r32(u, floor_slot)
    text = '%d %d %s |' % (result, new_floor, 'null' if r32(u, records_slot) == 0 else 'kept')
    for g in grids:
        if g:
            text += ' ' + ' '.join('%u' % struct.unpack('<H', u.mem_read(g[1] + 0x12 * i + 6, 2))[0] for i in range(g[2] * g[3]))
    text += ' |'
    if search:
        w = struct.unpack('<13I', u.mem_read(search, 0x34))
        grid_index = [g[0] if g else None for g in grids].index(w[4]) if w[4] in [g[0] for g in grids if g] else -1
        text += ' %u %d %u %d %d %u %u %u %u %u' % (w[2], grid_index, 1 if w[5] else 0, w[6] - (1 << 32) if w[6] >> 31 else w[6],
            w[7] - (1 << 32) if w[7] >> 31 else w[7], w[0], w[1], w[8], w[9], w[10])
        text += ' ' + ' '.join('%u' % struct.unpack('<H', u.mem_read(w[3] + 0x14 * i + 0xA, 2))[0] for i in range(min(w[2], 40)))
        text += ' freed %d' % len(e.freed)
    return text

def run_timers(e, line):
    clock(e, line)
    actor = new_actor(e, line)
    timers = [random.choice([0.0, rand_float(0, 0.05), rand_float(0, 2), -1.0, float('nan')]) for _ in range(2)]
    for i, t in enumerate(timers):
        wf(e.u, actor + 0x974 + 4 * i, t)
    line += [fb(t) for t in timers]
    e.call(0x00019520, [], {UC_X86_REG_ESI: actor})
    return '%08x %08x | %s' % (r32(e.u, actor + 0x974), r32(e.u, actor + 0x978), ' '.join('%x' % m for m in e.messages))

def goal_ring(e, line):
    u = e.u
    choice = random.choice([0, 1, 2])
    if choice == 0:
        line.append(0)
        return 0
    rows = e.block(0x40, b'\x00')
    ring = e.block(0xC, b'\x00')
    head = random.randint(0, 3)
    empty = choice == 1
    kind = random.choice([0, 0, 5])
    u.mem_write(ring, struct.pack('<HHHHI', 8, 4, head, 4 if empty else 1, rows))
    u.mem_write(rows + 8 * head, struct.pack('<H', kind))
    line += [1, head, 1 if empty else 0, kind]
    return ring

def run_bounds(e, line):
    u = e.u
    grids, ox, oz = nav_grids(e, line, 2)
    actor = new_actor(e, line)
    enabled = random.choice([0, 1, 1])
    values = [rand_float(-120, 0), rand_float(-120, 0), rand_float(-120, 0), rand_float(0, 120), rand_float(-50, 50), rand_float(0, 120)]
    u.mem_write(BOUNDS, bytes([enabled]))
    for i, v in enumerate(values):
        wf(u, BOUNDS + 4 + 4 * i, v)
    mode = random.choice([0, 1, 1])
    floor = random.choice([0, 1])
    bitbyte = random.choice([0, 0x10])
    body = e.block(0x200, b'\x00'); bits = e.block(0x10, b'\x00'); bitbytes = e.block(8, b'\x00')
    w32(u, actor + 0x95C, body); w32(u, body + 0x15C, bits); w32(u, bits + 8, bitbytes)
    u.mem_write(bitbytes + 4, bytes([bitbyte]))
    u.mem_write(actor + 0xCDE, struct.pack('<H', mode)); w32(u, actor + 0xCEC, floor)
    ring = goal_ring(e, line)
    w32(u, actor + 0x980, ring)
    line += [enabled] + [fb(v) for v in values] + [mode, floor, bitbyte]
    e.call(0x0002C980, [], {UC_X86_REG_EAX: actor})
    return ' '.join('%x' % m for m in e.messages)

def run_room(e, line):
    u = e.u
    actor = new_actor(e, line)
    pool = e.block(0x10, b'\x00')
    n = random.randint(0, 4)
    records = e.block(0x38 * max(n, 1), b'\x00')
    u.mem_write(pool, struct.pack('<HHIII', 0x38, 4, records, records + 0x38 * n, records + 0x38 * n))
    w32(u, OCCUPANCY, pool if random.random() < 0.9 else 0)
    w32(u, OCCUPANCY_MASK, random.choice([4, 6, 2, 0xFF]))
    line += [r32(u, OCCUPANCY) != 0 and 1 or 0, r32(u, OCCUPANCY_MASK), n]
    position = [bf(r32(u, actor + 8 + 4 * i)) for i in range(3)]
    for r in range(n):
        rec = records + 0x38 * r
        mask = random.choice([4, 4, 2, 6])
        group = random.choice([0, 0, 3, 5])
        center = [f32(position[i] + random.uniform(-20, 20)) for i in range(3)]
        radius = rand_float(1, 30)
        w32(u, rec + 4, mask); u.mem_write(rec + 8, struct.pack('<h', 0)); u.mem_write(rec + 0x1C, struct.pack('<h', group))
        for i, c in enumerate(center):
            wf(u, rec + 0x20 + 4 * i, c)
        wf(u, rec + 0x2C, radius)
        line += [mask, group & 0xffff] + [fb(c) for c in center] + [fb(radius)]
    tag = random.choice([0, 3, 5])
    allowed = random.choice([0, 0, records])
    latch = random.choice([0, 0, 1])
    body = e.block(0x40, b'\x00')
    body_word = random.choice([0, 0, 1])
    body_float = random.choice([0.0, 0.0, 1.0, -0.0])
    strategy = random.choice([0, 1])
    mode = random.choice([1, 1, 2])
    linked = random.choice([0, 0, 1])
    u.mem_write(actor + 6, struct.pack('<H', tag)); w32(u, actor + 0xCA4, allowed); w32(u, actor + 0xCB8, latch)
    w32(u, actor + 0x95C, body); u.mem_write(body + 0x30, struct.pack('<H', body_word)); wf(u, body + 0x34, body_float)
    w32(u, actor + 0x954, strategy); u.mem_write(actor + 0xCDC, struct.pack('<H', mode)); w32(u, actor + 0x93C, linked)
    line += [tag, 1 if allowed else 0, latch, body_word, fb(body_float), strategy, mode, linked]
    e.call(0x0002B450, [], {UC_X86_REG_EAX: actor})
    current = r32(u, actor + 0xCA8)
    index = -1 if current == 0 else (current - records) // 0x38
    return '%d %u %08x %08x %08x | %s' % (index, r32(u, actor + 0xCB8), r32(u, actor + 0xCAC), r32(u, actor + 0xCB0), r32(u, actor + 0xCB4),
        ' '.join('%x' % m for m in e.messages))

def run_spheres(e, line):
    u = e.u
    clock(e, line)
    owner = new_actor(e, line)
    euler = [rand_float(-3, 3) for _ in range(3)]
    scales = [rand_float(0.5, 2) for _ in range(3)] + [rand_float(0.5, 2)]
    for i, v in enumerate(euler):
        wf(u, owner + 0x14 + 4 * i, v)
    for i, v in enumerate(scales[:3]):
        wf(u, owner + 0x30 + 4 * i, v)
    wf(u, owner + 0x200, scales[3])
    line += [fb(v) for v in euler + scales]
    flag = random.choice([0, 0, 1]); mode = random.choice([0, 1, 2, 3, 4])
    w32(u, owner + 0x93C, flag); u.mem_write(owner + 0xCDC, struct.pack('<H', mode))
    line += [flag, mode]
    body = e.block(0x800, b'\x00'); bits = e.block(0x10, b'\x00'); bitbytes = e.block(8, b'\x00')
    w32(u, body + 8, owner); w32(u, body + 0x15C, bits); w32(u, bits + 8, bitbytes)
    bitbyte = random.choice([0, 2]); u.mem_write(bitbytes + 5, bytes([bitbyte]))
    position = [f32(bf(r32(u, owner + 8 + 4 * i)) + random.uniform(-3, 3)) for i in range(3)]
    for i, v in enumerate(position):
        wf(u, body + 0x18 + 4 * i, v)
    count = random.randint(0, 3)
    enable = random.choice([0, 1, 1, 1])
    kind = random.choice([0, 1, 1, 2])
    spheres = e.block(0x58 * max(count, 1), b'\x00') if random.random() < 0.95 else 0
    u.mem_write(body + 0x714, bytes([count, enable, 0x77, kind]))
    w32(u, body + 0x710, spheres)
    line += [bitbyte] + [fb(v) for v in position] + [count, enable, kind, 1 if spheres else 0]
    for s in range(count if spheres else 0):
        node = e.block(0x24, b'\x00')
        u.mem_write(node + 8, struct.pack('<H', 0xFFFF))
        values = [rand_float(0, 2)] + [rand_float(-2, 2) for _ in range(3)]
        for i, v in enumerate(values):
            wf(u, node + 0x10 + 4 * i, v)
        w32(u, spheres + 0x58 * s + 0x54, node)
        line += [fb(v) for v in values]
    probes = random.randint(0, 3)
    previous = SEGMENT_HEAD - 4
    w32(u, SEGMENT_HEAD, 0)
    line.append(probes)
    links = []
    for p in range(probes):
        obj = e.block(0x200, b'\x00'); vt = e.block(0x40, b'\x00')
        w32(u, obj, vt); w32(u, vt + 0x1C, SWEEP); w32(u, vt + 0x20, QUERY)
        params = {'id': p + 1, 'mask': random.choice([0xC4000, 0x4000, 0x80000, 0x40000]), 'a': rand_float(-1, 1),
                  'h': f32(position[1] + random.uniform(-4, 6)), 'normal': [rand_float(-1, 1) for _ in range(3)],
                  'flag': random.choice([1, 1, 0]), 'clamp': random.choice([0, 1])}
        e.probes[obj] = params
        links.append(obj + 0x1E0)
        line += [params['mask'], fb(params['a']), fb(params['h'])] + [fb(n) for n in params['normal']] + [params['flag'], params['clamp']]
    for i, link in enumerate(links):
        w32(u, link + 4, links[i + 1] if i + 1 < len(links) else 0)
    if links:
        w32(u, SEGMENT_HEAD, links[0])
    e.call(0x00025F00, [], {UC_X86_REG_ESI: body})
    text = '%08x %08x %08x %u |' % (r32(u, body + 0x18), r32(u, body + 0x1C), r32(u, body + 0x20), u.mem_read(body + 0x716, 1)[0])
    for s in range(count if spheres else 0):
        text += ' ' + bytes(u.mem_read(spheres + 0x58 * s, 0x54)).hex()
    text += ' | ' + ' ; '.join(e.calls)
    return text

CASES = {'motion': run_motion, 'floor': run_floor, 'timers': run_timers, 'bounds': run_bounds, 'room': run_room, 'spheres': run_spheres}

def main():
    random.seed(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 200
    only = sys.argv[3] if len(sys.argv) > 3 else None
    lines, expected = [], []
    names = [only] if only else list(CASES)
    for i in range(count):
        name = names[i % len(names)]
        e = Emu()
        line = []
        expected.append(CASES[name](e, line))
        lines.append(name + ' ' + ' '.join('%u' % (v & 0xffffffff) for v in line))
    got = subprocess.run([harness('update_harness')], input='\n'.join(lines) + '\n', capture_output=True, text=True).stdout.splitlines()
    bad = 0
    for i, (want, have) in enumerate(zip(expected, got)):
        if want.strip() != have.strip():
            bad += 1
            if bad < 6:
                print('MISMATCH', i, lines[i].split()[0], '\n exp', want[:600], '\n got', have[:600])
    print('cases', len(expected), len(got), 'mismatches', bad)

if __name__ == '__main__':
    main()
