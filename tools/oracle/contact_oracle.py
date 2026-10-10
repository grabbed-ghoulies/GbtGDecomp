"""Differential check of the body contact walk against the host build: 0x0013F2C0 with 0x0013ED20,
0x0013EE10, 0x0013EE80, 0x0013F7B0, 0x00122B00, 0x001229F0, 0x000FCC90, 0x000FC6C0, 0x0013FB50,
0x0013EBE0 and 0x00056330, using either a hooked callback or the player callback 0x0001F8C0.
Heap calls, probe/receiver vtables and the callback are hooked. Needs cache/oracle/mem_00011000.bin,
mem_00200000.bin, mem_00203000.bin, mem_00210000.bin and mem_00339000.bin."""
import random, struct, subprocess, sys
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ESP
from retail_cpu import STACK, w32, r32, harness
from update_oracle import Emu, STUBS, FREEZE, DT, SCALE, f32, fb, bf, wf, rand_float

POINT_STUB, CONTACTS_STUB, CALLBACK_STUB, SELF_MESSAGE, OBJECT_MESSAGE = (STUBS + 0x30, STUBS + 0x40, STUBS + 0x50,
    STUBS + 0x60, STUBS + 0x70)
HEAP_ALLOC, BLOCK_SIZE = 0x0014E872, 0x0014DED0
PLAYERS = 0x00511B1C
TOTALS, SESSION = 0x0053E528, 0x00524E2C

class ContactEmu(Emu):
    def __init__(self):
        super().__init__()
        u = self.u
        u.mem_write(HEAP_ALLOC, bytes([0xC2, 0x0C, 0x00]))
        u.mem_write(BLOCK_SIZE, bytes([0xC2, 0x0C, 0x00]))
        u.hook_add(UC_HOOK_CODE, self.heap_alloc, begin=HEAP_ALLOC, end=HEAP_ALLOC)
        u.hook_add(UC_HOOK_CODE, self.block_size, begin=BLOCK_SIZE, end=BLOCK_SIZE)
        u.hook_add(UC_HOOK_CODE, self.point_probe, begin=POINT_STUB, end=POINT_STUB)
        u.hook_add(UC_HOOK_CODE, self.contacts_probe, begin=CONTACTS_STUB, end=CONTACTS_STUB)
        u.hook_add(UC_HOOK_CODE, self.callback, begin=CALLBACK_STUB, end=CALLBACK_STUB)
        u.hook_add(UC_HOOK_CODE, self.self_message, begin=SELF_MESSAGE, end=SELF_MESSAGE)
        u.hook_add(UC_HOOK_CODE, self.object_message, begin=OBJECT_MESSAGE, end=OBJECT_MESSAGE)
        self.objects = {}
        self.log = []
        self.callback_results = []
        self.state = 0
        self.actor = 0

    def heap_alloc(self, uc, address, size, user):
        uc.reg_write(UC_X86_REG_EAX, self.block(r32(uc, uc.reg_read(UC_X86_REG_ESP) + 12)))

    def block_size(self, uc, address, size, user):
        uc.reg_write(UC_X86_REG_EAX, self.sizes[r32(uc, uc.reg_read(UC_X86_REG_ESP) + 12)])

    def name(self, address):
        if address == self.actor:
            return 'A'
        if address in self.objects:
            return 'O%d' % self.objects[address]['id']
        return '?'

    def pool_text(self, pool):
        u = self.u
        begin, cursor = r32(u, pool + 4), r32(u, pool + 8)
        stride = struct.unpack('<H', u.mem_read(pool, 2))[0]
        return '%d:%s' % ((cursor - begin) // 0x54, bytes(u.mem_read(begin, cursor - begin)).hex()) if stride == 0x54 else 'bad'

    def point_probe(self, uc, address, size, user):
        esp = uc.reg_read(UC_X86_REG_ESP)
        obj, candidate, kept, radius, mask, record = [r32(uc, esp + 4 * i) for i in range(1, 7)]
        p = self.objects[obj]
        kx, cy = bf(r32(uc, kept)), bf(r32(uc, candidate + 4))
        hit = (mask & p['mask']) != 0 and f32(f32(kx * p['a']) + cy) < p['h']
        self.log.append('P%d %s %08x' % (p['id'], ' '.join('%08x' % r32(uc, a + 4 * i) for a in (candidate, kept) for i in range(3)), radius))
        if hit:
            uc.mem_write(record, p['record'])
            if p['clamp']:
                wf(uc, kept + 4, p['h'])
        uc.reg_write(UC_X86_REG_EAX, 1 if hit else 0)

    def contacts_probe(self, uc, address, size, user):
        esp = uc.reg_read(UC_X86_REG_ESP)
        obj, origin, radius, mask, slot = [r32(uc, esp + 4 * i) for i in range(1, 6)]
        p = self.objects[obj]
        oy = bf(r32(uc, origin + 4))
        hit = (mask & p['mask']) != 0 and f32(f32(oy * p['a']) + bf(radius)) < p['h2']
        self.log.append('C%d %s %08x %x' % (p['id'], ' '.join('%08x' % r32(uc, origin + 4 * i) for i in range(3)), radius, mask))
        if hit:
            pool = r32(uc, slot)
            for record in p['contacts']:
                cursor = r32(uc, pool + 8)
                uc.mem_write(cursor, record)
                w32(uc, pool + 8, cursor + 0x54)
        uc.reg_write(UC_X86_REG_EAX, 1 if hit else 0)

    def callback(self, uc, address, size, user):
        esp = uc.reg_read(UC_X86_REG_ESP)
        actor, obj, point, copy, record, velocity, word = [r32(uc, esp + 4 * i) for i in range(1, 8)]
        k = len(self.callback_results)
        result, clear = self.plan[k % len(self.plan)]
        self.callback_results.append(result)
        self.log.append('K %s %s %s %s %s %s %04x' % (self.name(actor), self.name(obj),
            ' '.join('%08x' % r32(uc, point + 4 * i) for i in range(3)), self.pool_text(copy),
            bytes(uc.mem_read(record, 3)).hex() + ' ' + ' '.join('%08x' % r32(uc, record + 4 + 4 * i) for i in range(4)),
            ' '.join('%08x' % r32(uc, velocity + 4 * i) for i in range(3)), word & 0xffff))
        if clear:
            w32(uc, self.state + 0x10, 0)
        uc.reg_write(UC_X86_REG_EAX, result)

    def object_message(self, uc, address, size, user):
        esp = uc.reg_read(UC_X86_REG_ESP)
        obj, message = r32(uc, esp + 4), r32(uc, esp + 8)
        w = [r32(uc, message + 4 * i) for i in range(10)]
        self.log.append('M%d %x %x %s %s %s %s' % (self.objects[obj]['id'], w[0], w[1], self.name(w[2]),
            ' '.join('%08x' % v for v in w[3:6]), self.pool_text(w[6]), ' '.join('%08x' % v for v in w[7:10])))
        w32(uc, message + 4, self.objects[obj]['reply'])

    def self_message(self, uc, address, size, user):
        esp = uc.reg_read(UC_X86_REG_ESP)
        message = r32(uc, esp + 8)
        w = [r32(uc, message + 4 * i) for i in range(11)]
        self.log.append('S %x %d %s %s %s %s %x' % (w[0], w[1] - (1 << 32) if w[1] >> 31 else w[1],
            ' '.join('%08x' % v for v in w[2:5]), self.name(w[5]), self.pool_text(w[6]),
            ' '.join('%08x' % v for v in w[7:10]), w[10]))

def contact_record(centre, radius, kind, tag):
    data = bytearray(random.getrandbits(8) for _ in range(0x54))
    data[0x26] = 1
    data[0x28] = kind
    data[0x29] = tag
    data[0x2C:0x38] = struct.pack('<fff', *centre)
    data[0x38:0x3C] = struct.pack('<f', radius)
    return bytes(data)

def run_case(e, line):
    u = e.u
    freeze = random.choice([0.0, 0.0, 0.0, 1.0])
    dt, scale = rand_float(0.0, 0.05), 1.0
    wf(u, FREEZE, freeze); wf(u, DT, dt); wf(u, SCALE, scale)
    line += [fb(freeze), fb(dt), fb(scale)]
    actor = e.block(0xD00, b'\x00'); e.actor = actor
    vt = e.block(0x40, b'\x00'); w32(u, actor, vt); w32(u, vt + 0xC, SELF_MESSAGE)
    pause, rate = random.choice([0.0, 0.0, 1.0]), random.choice([1.0, 1.0, 0.0])
    wf(u, actor + 0x930, pause); wf(u, actor + 0x92C, rate)
    filt = random.choice([0, 0x10, 0x3])
    w32(u, actor + 0x1F0, filt)
    tag = random.randint(0, 5)
    u.mem_write(actor + 6, struct.pack('<H', tag))
    xform = [1, 0, 0, rand_float(-20, 20), 0, 1, 0, rand_float(-5, 5), 0, 0, 1, rand_float(-20, 20)]
    if random.random() < 0.5:
        xform = [rand_float(-1.5, 1.5) if i % 4 != 3 else rand_float(-20, 20) for i in range(12)]
    for i, v in enumerate(xform):
        wf(u, actor + 0x2AC + 4 * i, v)
    body = e.block(0x800, b'\x00'); w32(u, actor + 0x95C, body)
    count = random.choice([0, 0, 1, 2])
    w32(u, body + 0x2CC, count)
    line += [fb(pause), fb(rate), filt, tag] + [fb(v) for v in xform] + [count]
    state = e.block(0x20, b'\x00'); e.state = state
    w32(u, body + 0x2C8, state)
    forced = random.choice([0, 0, 1])
    w32(u, state + 0x1C, forced)
    line.append(forced)
    # Hit actors.
    nobjects = random.randint(0, 3)
    line.append(nobjects)
    objects = []
    links = []
    players = [PLAYERS]
    for k in range(nobjects):
        obj = e.block(0xA00, b'\x00')
        ovt = e.block(0x40, b'\x00')
        w32(u, obj, ovt); w32(u, ovt + 0xC, OBJECT_MESSAGE); w32(u, ovt + 0x2C, POINT_STUB); w32(u, ovt + 0x30, CONTACTS_STUB)
        pos = [rand_float(-20, 20) for _ in range(3)]
        for i, v in enumerate(pos):
            wf(u, obj + 8 + 4 * i, v)
        ofilt = random.choice([0, 7, 7, 0x20])
        child = random.choice([0, 0, 0, 1])
        player = random.random() < 0.15
        w32(u, obj + 0x1F0, ofilt); w32(u, obj + 0x908, child)
        reply = random.choice([0, 1, 1, 5])
        record = bytearray(random.getrandbits(8) for _ in range(0x54)); record[0x26] = random.choice([0, 1])
        contacts = []
        for c in range(random.choice([0, 1, 1, 2])):
            contacts.append(contact_record([rand_float(-20, 20) for _ in range(3)], rand_float(0, 4), random.choice([1, 1, 0]), k))
        p = {'id': k + 1, 'mask': random.choice([0x220002, 0x40, 0x100, 0x220142, 0]), 'a': rand_float(-1, 1),
             'h': rand_float(-10, 10), 'h2': rand_float(-10, 20), 'clamp': random.choice([0, 1]), 'record': bytes(record),
             'contacts': contacts, 'reply': reply}
        e.objects[obj] = p
        objects.append(obj)
        links.append(obj + 0x1E0)
        if player:
            players.append(obj + 0x934)
        line += [fb(v) for v in pos] + [ofilt, child, 1 if player else 0, p['mask'], fb(p['a']), fb(p['h']), fb(p['h2']), p['clamp'], reply]
        line += list(record) + [len(contacts)] + [b for c in contacts for b in c]
    for i, link in enumerate(links):
        w32(u, link + 4, links[i + 1] if i + 1 < len(links) else 0)
    w32(u, 0x00511AF4, links[0] if links else 0)
    for i, node in enumerate(players):
        w32(u, node + 4, players[i + 1] if i + 1 < len(players) else 0)
    # Overrides pool.
    rows = []
    if random.random() < 0.6 and objects:
        n = random.randint(0, 3)
        pool = e.block(0x10 + 0x10 * max(n, 4))
        u.mem_write(pool, struct.pack('<HHIII', 0x10, 4, pool + 0x10, pool + 0x10 + 0x10 * n, pool + 0x10 + 0x10 * max(n, 4)))
        for r in range(n):
            index = random.randrange(len(objects))
            row = (index, random.getrandbits(32), fb(rand_float(0, 3)), random.choice([0, 0, 1]))
            rows.append(row)
            u.mem_write(pool + 0x10 + 0x10 * r, struct.pack('<IIII', objects[index], row[1], row[2], row[3]))
        w32(u, state + 8, pool)
        line += [1, n] + [v for row in rows for v in row]
    else:
        line.append(0)
    # Removed list.
    if random.random() < 0.3 and objects:
        n = random.randint(0, 3)
        removed = e.block(0x10 + 4 * max(n, 1))
        picks = [random.randrange(len(objects)) for _ in range(n)]
        u.mem_write(removed, struct.pack('<HHIII', 4, 4, removed + 0x10, removed + 0x10 + 4 * n, removed + 0x10 + 4 * n))
        for i, index in enumerate(picks):
            w32(u, removed + 0x10 + 4 * i, objects[index])
        w32(u, state + 0x18, removed)
        line += [1, n] + picks
    else:
        line.append(0)
    # Active shape records.
    nshapes = random.choice([0, 1, 2, 3])
    line.append(nshapes)
    records = []
    for s in range(nshapes):
        rec = e.block(0x20, b'\x00')
        node = e.block(0x24, b'\x00')
        word = random.getrandbits(16)
        u.mem_write(node + 2, struct.pack('<H', word)); u.mem_write(node + 8, struct.pack('<H', 0xFFFF))
        values = [random.choice([rand_float(0, 2), 0.0])] + [rand_float(-2, 2) for _ in range(3)]
        for i, v in enumerate(values):
            wf(u, node + 0x10 + 4 * i, v)
        prev = [rand_float(-20, 20) for _ in range(3)]
        for i, v in enumerate(prev):
            wf(u, rec + 8 + 4 * i, v)
        flags = random.getrandbits(3)
        scale_value = rand_float(0.5, 3)
        u.mem_write(rec + 0x17, bytes([flags])); wf(u, rec + 0x18, scale_value); w32(u, rec + 0x1C, node)
        records.append(rec)
        line += [word] + [fb(v) for v in values] + [fb(v) for v in prev] + [flags, fb(scale_value)]
    for i, rec in enumerate(records):
        w32(u, rec + 4, records[i + 1] if i + 1 < len(records) else 0)
    w32(u, state + 0x10, records[0] if records else 0)
    mode = random.choice([0, 0, 1])
    e.plan = [(random.choice([0, 0, 0, 1]), random.random() < 0.1) for _ in range(6)]
    line += [mode] + [v for plan in e.plan for v in (plan[0], 1 if plan[1] else 0)]
    callback = 0x0001F8C0 if mode else CALLBACK_STUB
    u.reg_write(UC_X86_REG_EBX, state)
    e.call(0x0013F2C0, [actor, callback], {UC_X86_REG_EBX: state})
    out = ['%08x %08x %08x %08x' % (r32(u, actor + 0x1F0), 1 if r32(u, state + 0x10) else 0, r32(u, state + 0x14), 1 if r32(u, state + 0x18) else 0)]
    pool = r32(u, state + 8)
    if pool:
        begin, cursor = r32(u, pool + 4), r32(u, pool + 8)
        out.append('rows ' + ' '.join('%s:%08x:%08x:%x' % (e.name(r32(u, a)), r32(u, a + 4), r32(u, a + 8), r32(u, a + 12))
            for a in range(begin, cursor, 0x10)))
    else:
        out.append('rows none')
    out.append('prev ' + ' '.join('%08x' % r32(u, rec + 8 + 4 * i) for rec in records for i in range(3)))
    out.append('filters ' + ' '.join('%08x' % r32(u, obj + 0x1F0) for obj in objects))
    out.append('body %u %s %s' % (r32(u, body + 0x2CC), ' '.join('%u' % r32(u, TOTALS + 4 * i) for i in range(0, 0x3D * 6, 0x3D)),
        ' '.join('%u' % r32(u, SESSION + 4 * i) for i in range(0, 0x3D * 6, 0x3D))))
    out.append('log ' + ' ; '.join(e.log))
    return ' | '.join(out)

def main():
    random.seed(int(sys.argv[1]) if len(sys.argv) > 1 else 1)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 200
    lines, expected = [], []
    for _ in range(count):
        e = ContactEmu()
        line = []
        expected.append(run_case(e, line))
        lines.append(' '.join('%u' % (v & 0xffffffff) for v in line))
    got = subprocess.run([harness('contact_harness')], input=str(count) + '\n' + '\n'.join(lines) + '\n',
        capture_output=True, text=True).stdout.splitlines()
    bad = 0
    for i, (want, have) in enumerate(zip(expected, got)):
        if want.split() != have.split():
            bad += 1
            if bad < 5:
                print('MISMATCH', i, '\n exp', want[:900], '\n got', have[:900])
    print('cases', len(expected), len(got), 'mismatches', bad)

if __name__ == '__main__':
    main()
