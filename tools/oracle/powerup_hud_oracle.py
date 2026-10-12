"""Differential check for the powerup award helpers below 0x00123E70: scoreboard messages
0x001316F0 (add), 0x001319C0 (remove), 0x00131BD0 (icon), 0x00132450 (powerup icon), 0x00132540
(caption), 0x0012D7E0 (bonus counters), table cancel 0x00124320, timer extension 0x0004A370,
challenge lookup 0x000D0650 and stream restart 0x00038880.

The retail HUD counter 0x0012F990 and memmove 0x00165AF0 run as retail code. Resource, text-pool,
pulse and stream-slot callees are logging stubs on both sides.
Usage: powerup_hud_oracle.py [seed] [count] [entry]"""
import random, struct, subprocess, sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_XMM0)
from retail_cpu import STOP, STACK, HEAP, w32, r32, harness
from imp_strategy_oracle import SECTIONS, r16, w16, fb, cstr

BOARD, BOARDCELL, OUTCELL = HEAP, HEAP + 0x200, HEAP + 0x204
POOL = HEAP + 0x1000
ICONS = HEAP + 0x3000
TEX = HEAP + 0x6000
TEXTPOOL = HEAP + 0x7000
NAMES = HEAP + 0x8000
TIMERS, CHALLENGES = HEAP + 0x9000, HEAP + 0xA000
GROWN = HEAP + 0x20000
TABLE = 0x00512140
STREAMS, STATES, IDS, TIMES = 0x004DFFA0, 0x004E02A0, 0x004E02B8, 0x004E02D0
SCRATCH = 0x00507E78
ICON_ROWS = 0x50
STREAM_NAMES = ['XACT_A', 'XACT_B', 'XACT_C']
CALLEES = {
    0x00035E90: 'acquire', 0x00035C00: 'release', 0x00032700: 'error', 0x000662A0: 'grow',
    0x000FF0A0: 'text', 0x0005F390: 'textpool', 0x0005F5C0: 'record', 0x00122C70: 'pulse',
    0x00038780: 'streamrelease', 0x00038720: 'streamrequest',
}
ENTRIES = ['icon', 'add', 'remove', 'powerup', 'caption', 'bonus', 'cancel', 'timers', 'challenge', 'restart']


def icon_is_aid(index, seed):
    # Message icons must name a texture: retail reads a null texture's width word and faults.
    if index < 0x10 or 0x3A <= index <= 0x4B:
        return True
    return (index * 7 + seed) % 5 != 0


class Retail:
    def __init__(self, text_results):
        u = Uc(UC_ARCH_X86, UC_MODE_32)
        u.mem_map(0, 0x1000)
        u.mem_map(0x00010000, 0x5F0000)
        for va, data in SECTIONS:
            if va + len(data) <= 0x00600000:
                u.mem_write(va, data)
        u.mem_map(STOP, 0x1000); u.mem_write(STOP, b'\xf4')
        u.mem_map(HEAP, 0x100000)
        u.mem_map(STACK - 0x10000, 0x20000)
        self.u, self.log, self.text_results, self.bump = u, [], list(text_results), GROWN
        for address, name in CALLEES.items():
            u.mem_write(address, b'\xc3')
            u.hook_add(UC_HOOK_CODE, self.dispatch, begin=address, end=address, user_data=name)

    def arg(self, k):
        return r32(self.u, self.u.reg_read(UC_X86_REG_ESP) + 4 + 4 * k)

    def reg(self, r):
        return self.u.reg_read(r)

    def resource(self, a):
        if a == 0:
            return '0'
        if a == ICONS:
            return 'icons'
        return 'T%02x' % ((a - TEX) // 0x10)

    def dispatch(self, uc, address, size, name):
        u, log = self.u, self.log
        if name == 'acquire':
            text, out = cstr(u, self.arg(0)), self.arg(1)
            log.append('acquire %s' % text)
            if text == 'aid_aidlist_ghoulies_scoreboard_icons':
                w32(u, out, ICONS)
            else:
                w32(u, out, TEX + 0x10 * int(text[-2:], 16))
        elif name == 'release':
            log.append('release %s' % self.resource(self.reg(UC_X86_REG_EBX)))
        elif name == 'error':
            log.append('error %d' % self.arg(0))
        elif name == 'grow':
            pool = self.reg(UC_X86_REG_EAX)
            stride, grow = r16(u, pool), r16(u, pool + 2)
            begin, cursor = r32(u, pool + 4), r32(u, pool + 8)
            used = (cursor - begin) // stride
            new = self.bump
            self.bump += 0x10 + stride * (used + grow) + 0x40
            u.mem_write(new, bytes(u.mem_read(pool, 4)))
            u.mem_write(new + 0x10, bytes(u.mem_read(begin, cursor - begin)))
            w32(u, new + 4, new + 0x10)
            w32(u, new + 8, new + 0x10 + stride * used)
            w32(u, new + 0xC, new + 0x10 + stride * (used + grow))
            u.reg_write(UC_X86_REG_EAX, new)
        elif name == 'text':
            log.append('text %s %s %s %s' % (cstr(u, self.arg(0)), cstr(u, self.arg(1)), cstr(u, self.arg(2)),
                'scratch' if self.arg(3) == SCRATCH else '%x' % self.arg(3)))
            u.reg_write(UC_X86_REG_EAX, self.text_results.pop(0) if self.text_results else 0)
        elif name == 'textpool':
            log.append('textpool %x %x %x %s' % (self.arg(0), self.arg(1), self.arg(2),
                'cell' if self.reg(UC_X86_REG_ESI) == 0x00545AB8 else '?'))
            w32(u, self.reg(UC_X86_REG_ESI), TEXTPOOL)
        elif name == 'record':
            log.append('record %s %s %08x %08x %08x %08x %08x %x %x %x' % (
                'pool' if self.arg(0) == TEXTPOOL else '%x' % self.arg(0),
                'scratch' if self.arg(1) == SCRATCH else '%x' % self.arg(1),
                self.arg(2), self.arg(3), self.arg(4), self.arg(5), self.arg(6), self.arg(7), self.arg(8),
                self.reg(UC_X86_REG_EAX)))
        elif name == 'pulse':
            m = self.reg(UC_X86_REG_EDI)
            log.append('pulse %x %x %x active %x' % (r32(u, m), r32(u, m + 4), r32(u, m + 8),
                r32(u, TABLE + 0x18 * (r32(u, m + 8) % 20))))
        elif name == 'streamrelease':
            log.append('streamrelease %s' % cstr(u, self.arg(0)))
        elif name == 'streamrequest':
            log.append('streamrequest %s %x' % (cstr(u, self.reg(UC_X86_REG_EAX)), self.arg(0)))


def call(r, entry, regs, args=()):
    u = r.u
    w32(u, STACK, STOP)
    for i, a in enumerate(args):
        w32(u, STACK + 4 + 4 * i, a)
    u.reg_write(UC_X86_REG_ESP, STACK)
    for reg, value in regs.items():
        u.reg_write(reg, value)
    u.emu_start(entry, STOP, count=2000000)
    return u.reg_read(UC_X86_REG_EAX)


def random_float(rng):
    return fb(rng.choice([0.0, 1.0, 0.5, rng.uniform(-3, 3), float('nan')]))


def message_words(rng, textures):
    words = [rng.getrandbits(32) for _ in range(37)]
    words[0] = rng.choice([1, 2, 3, 5, 7, 0x11])
    words[34] = rng.choice(textures)
    return words


def setup_board(r, rng, line, seed):
    u = r.u
    for i in range(ICON_ROWS):
        name = ('aid_icon_%02x' if icon_is_aid(i, seed) else 'tex_icon_%02x') % i
        u.mem_write(ICONS + i * 0x80, name.encode() + b'\0')
    for i in range(0x80):
        w32(u, TEX + 0x10 * i + 8, 0x10 + i * 3)
    count = rng.choice([0, 1, 2, 3])
    capacity = count + rng.choice([0, 1, 2])
    w16(u, POOL, 0x94)
    w16(u, POOL + 2, 2)
    w32(u, POOL + 4, POOL + 0x10)
    w32(u, POOL + 8, POOL + 0x10 + 0x94 * count)
    w32(u, POOL + 0xC, POOL + 0x10 + 0x94 * capacity)
    line.append(count)
    for k in range(count):
        words = message_words(rng, [0, 0, 0x3A, 0x41])
        for i, w in enumerate(words):
            value = w
            if i == 34:
                value = TEX + 0x10 * w if w else 0
            w32(u, POOL + 0x10 + 0x94 * k + 4 * i, value)
        line += words
    next_id = rng.choice([1, 9, 0xFFFFFFFE, rng.getrandbits(16)])
    icons = [rng.choice([0, 0, 1 + 0x30]), rng.choice([0, 1 + 0x31])]
    scale = random_float(rng)
    w32(u, BOARD, POOL)
    w32(u, BOARD + 4, next_id)
    w32(u, BOARD + 8, TEX + 0x10 * (icons[0] - 1) if icons[0] else 0)
    w32(u, BOARD + 0xC, TEX + 0x10 * (icons[1] - 1) if icons[1] else 0)
    w32(u, BOARD + 0x48, scale)
    w32(u, BOARDCELL, BOARD)
    line += [next_id] + icons + [scale]


def board_dump(r):
    u = r.u
    pool = r32(u, BOARD)
    begin, cursor = r32(u, pool + 4), r32(u, pool + 8)
    out = ['n%d' % ((cursor - begin) // 0x94)]
    for row in range(begin, cursor, 0x94):
        for i in range(37):
            w = r32(u, row + 4 * i)
            out.append(r.resource(w) if i == 34 else '%x' % w)
    out.append('%x' % r32(u, BOARD + 4))
    out += [r.resource(r32(u, BOARD + 8)), r.resource(r32(u, BOARD + 0xC))]
    return ' '.join(out)


def main():
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 400
    only = sys.argv[3] if len(sys.argv) > 3 else None
    rng = random.Random(seed)
    names = [only] if only else ENTRIES
    lines, expected = [], []
    for case in range(count):
        entry = names[case % len(names)]
        text_result = rng.choice([0, 1, 1])
        r = Retail([text_result])
        u = r.u
        aid_seed = rng.randrange(5)
        line = [ENTRIES.index(entry), aid_seed, text_result]
        setup_board(r, rng, line, aid_seed)
        out = ''
        if entry == 'icon':
            index = rng.choice([1, 2, 0x3A, 0x41, 0x4A, rng.randrange(ICON_ROWS)])
            line.append(index)
            w32(u, OUTCELL, 0x12345678)
            call(r, 0x00131BD0, {UC_X86_REG_EAX: index}, (OUTCELL,))
            out = r.resource(r32(u, OUTCELL))
        elif entry == 'add':
            args = [rng.choice([3, 4, 7]), rng.choice([0, 0x3A, 0x4A, 0x4B, rng.randrange(0x10)]),
                rng.getrandbits(32), rng.choice([-2, 0, 1, 7, 123456, 99999999]), rng.getrandbits(32),
                rng.choice([0, 1, 5]), rng.getrandbits(32)]
            line += [a & 0xffffffff for a in args]
            got = call(r, 0x001316F0, {UC_X86_REG_EBX: BOARD}, [a & 0xffffffff for a in args])
            out = '%x %s' % (got, board_dump(r))
        elif entry == 'remove':
            ident = rng.choice([1, 2, 3, 5, 7, 0x11, 0])
            line.append(ident)
            got = call(r, 0x001319C0, {UC_X86_REG_EAX: BOARD, UC_X86_REG_ECX: ident})
            out = '%x %s' % (got, board_dump(r))
        elif entry == 'powerup':
            kind = rng.randrange(0x15)
            line.append(kind)
            w32(u, 0x00545ABC, BOARD)
            got = call(r, 0x00132450, {UC_X86_REG_ECX: kind})
            out = '%x %s' % (got, board_dump(r))
        elif entry == 'caption':
            kind = rng.randrange(0x14)
            exists = rng.choice([0, 1])
            width = rng.choice([640, 720, rng.getrandbits(16)])
            bottom = random_float(rng)
            origin = [rng.getrandbits(32) for _ in range(3)]
            line += [kind, exists, width, bottom] + origin
            w32(u, 0x00545AB8, TEXTPOOL if exists else 0)
            for i, w in enumerate(origin):
                w32(u, TEXTPOOL + 0x14 + 4 * i, w)
            w16(u, 0x005096E8, width)
            w32(u, 0x0050974C, bottom)
            w16(u, SCRATCH, 0x4141)
            call(r, 0x00132540, {UC_X86_REG_EAX: kind})
            out = '%x %s %s' % (r16(u, SCRATCH), 'pool' if r32(u, 0x00545AB8) == TEXTPOOL else '0',
                ' '.join('%x' % r32(u, TEXTPOOL + 0x14 + 4 * i) for i in range(3)))
        elif entry == 'bonus':
            slot = rng.choice([0, 1])
            amount = rng.choice([0, 1, 5, 10, 3, 7, -5])
            arrays = [rng.choice([0, 0, 1, 2, 3, 0x11]) for _ in range(2)] + [rng.getrandbits(32) for _ in range(4)]
            line += [slot, amount & 0xffffffff] + arrays
            w32(u, 0x005459F8, BOARD)
            for i, w in enumerate(arrays):
                w32(u, 0x005459FC + 4 * i, w)
            call(r, 0x0012D7E0, {UC_X86_REG_EDI: slot}, (amount & 0xffffffff,))
            out = '%s %s' % (' '.join('%x' % r32(u, 0x005459FC + 4 * i) for i in range(6)), board_dump(r))
        elif entry == 'cancel':
            kind = rng.randrange(20)
            present = rng.choice([0, 1, 1])
            rows = [rng.getrandbits(32) for _ in range(20 * 6)]
            for k in range(20):
                rows[k * 6 + 5] = rng.choice([0, 1, 2, 3, 5, 7])
            line += [kind, present] + rows
            for i, w in enumerate(rows):
                w32(u, TABLE + 4 * i, w)
            w32(u, 0x00545ABC, BOARD if present else 0)
            call(r, 0x00124320, {UC_X86_REG_EAX: kind})
            out = '%s %s' % (' '.join('%x' % r32(u, TABLE + 4 * i) for i in range(20 * 6)), board_dump(r))
        elif entry == 'timers':
            present = rng.choice([0, 1, 1, 1])
            n = rng.randrange(5)
            kinds = [rng.choice([0x10, 0x11, 0xF, 0x12, 0]) for _ in range(n)]
            times = [random_float(rng) for _ in range(n)]
            amount = random_float(rng)
            line += [present, n] + kinds + times + [amount]
            w16(u, TIMERS, 0x14)
            w32(u, TIMERS + 4, TIMERS + 0x10)
            w32(u, TIMERS + 8, TIMERS + 0x10 + 0x14 * n)
            for i in range(n):
                w32(u, TIMERS + 0x10 + 0x14 * i, kinds[i])
                w32(u, TIMERS + 0x10 + 0x14 * i + 8, times[i])
            w32(u, 0x004E17D8, TIMERS if present else 0)
            u.reg_write(UC_X86_REG_XMM0, amount)
            call(r, 0x0004A370, {})
            out = ' '.join('%x' % r32(u, TIMERS + 0x10 + 0x14 * i + 8) for i in range(n))
        elif entry == 'challenge':
            present = rng.choice([0, 1, 1, 1])
            n = rng.randrange(5)
            kinds = [rng.choice([4, 5, 6, 0xF]) for _ in range(n)]
            kind = rng.choice([4, 5, 6, 0xF, 1])
            line += [present, n] + kinds + [kind]
            w32(u, CHALLENGES + 4, CHALLENGES + 0x10)
            w32(u, CHALLENGES + 8, CHALLENGES + 0x10 + 0x88 * n)
            for i in range(n):
                w32(u, CHALLENGES + 0x10 + 0x88 * i, kinds[i])
            w32(u, 0x005064E0, CHALLENGES if present else 0)
            got = call(r, 0x000D0650, {UC_X86_REG_EDX: kind})
            out = '%d' % ((got - CHALLENGES - 0x10) // 0x88 if got else -1)
        elif entry == 'restart':
            slots = []
            for i in range(6):
                name = rng.choice([0, 1, 2, 3])
                state = rng.choice([0, 1, 2, 2, 3])
                time = fb(rng.choice([0.0, 0.1, 0.24, 0.2399, 0.25, 1.0, float('nan')]))
                slots += [name, state, time]
                u.mem_write(STREAMS + 0x80 * i, (STREAM_NAMES[name - 1].encode() if name else b'') + b'\0')
                w32(u, STATES + 4 * i, state)
                w32(u, TIMES + 4 * i, time)
            name = rng.randrange(3)
            ident = rng.choice([0x12, 0xA, 6])
            line += slots + [name, ident]
            u.mem_write(NAMES, STREAM_NAMES[name].encode() + b'\0')
            call(r, 0x00038880, {UC_X86_REG_EBX: NAMES}, (ident,))
        lines.append(' '.join('%x' % (v & 0xffffffff) for v in line))
        expected.append('%s | %s' % (out, ' ; '.join(r.log)))
    text = '\n'.join(lines) + '\n'
    result = subprocess.run([harness('powerup_hud_harness')], input=text, capture_output=True, text=True)
    if result.returncode != 0:
        print('harness exit', result.returncode, result.stderr[-600:])
    got = result.stdout.splitlines()
    bad = 0
    for i, (want, have) in enumerate(zip(expected, got)):
        if want.split() != have.split():
            bad += 1
            if bad <= 6:
                print('MISMATCH', i, names[i % len(names)], '\n in ', lines[i][:300], '\n exp', want[:1500],
                    '\n got', have[:1500])
    bad += abs(len(expected) - len(got))
    print('cases', len(expected), len(got), 'mismatches', bad)


if __name__ == '__main__':
    main()
