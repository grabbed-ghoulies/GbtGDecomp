"""Call-sequence differential check for the player mind request update 0x0007D360 (mind vtable +0x14).
The flag lookup 0x0001EF70, action selection 0x00027650, body vtable +0x38/+0x3C, control update
0x0007D130, navigation request 0x00136620 and parent update 0x000295E0 are logging stubs with scripted
results. Needs cache/oracle/mem_00011000.bin and mem_00210000.bin."""
import os, random, struct, subprocess, sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ESP
from retail_cpu import ROOT, STOP, STACK, HEAP, w32, r32, harness

MIND, ACTOR, BODY, BITS, REQUEST, DEVICE, VTABLE = (HEAP, HEAP + 0x1000, HEAP + 0x2000, HEAP + 0x3000,
    HEAP + 0x4000, HEAP + 0x5000, HEAP + 0x6000)
CALLEES = {0x0001EF70: 'flag', 0x00027650: 'select', 0x0007D130: 'control', 0x00136620: 'navigate',
           0x000295E0: 'parent', 0x00F20038: 'effectkey', 0x00F2003C: 'contactkey'}

def main():
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 2000
    rng = random.Random(seed)
    u = Uc(UC_ARCH_X86, UC_MODE_32)
    u.mem_map(0x00010000, 0x150000)
    u.mem_write(0x00011000, open(os.path.join(ROOT, 'cache', 'oracle', 'mem_00011000.bin'), 'rb').read())
    u.mem_map(0x00210000, 0x130000)
    u.mem_write(0x00210000, open(os.path.join(ROOT, 'cache', 'oracle', 'mem_00210000.bin'), 'rb').read())
    u.mem_map(STOP, 0x1000); u.mem_write(STOP, b'\xf4')
    u.mem_map(0x00F20000, 0x1000); u.mem_write(0x00F20000, b'\xc3' * 0x1000)
    u.mem_map(HEAP, 0x100000)
    u.mem_map(STACK - 0x10000, 0x20000)
    state = {'log': [], 'values': {}}
    def hook(uc, address, size, name):
        log, values = state['log'], state['values']
        esp = u.reg_read(UC_X86_REG_ESP)
        if name == 'flag':
            name_text = bytes(u.mem_read(u.reg_read(UC_X86_REG_EBX), 8)).split(b'\0')[0].decode()
            log.append('flag %s' % name_text); u.reg_write(UC_X86_REG_EAX, values['flag'])
        elif name == 'select':
            log.append('select %x %d' % (u.reg_read(UC_X86_REG_EAX) & 0xffff, u.reg_read(UC_X86_REG_EBX) == BODY))
            u.reg_write(UC_X86_REG_EAX, 1)
        elif name == 'control':
            log.append('control %d' % (u.reg_read(UC_X86_REG_EAX) == MIND))
        elif name == 'navigate':
            log.append('navigate %d %d' % (u.reg_read(UC_X86_REG_EAX) == REQUEST, r32(u, esp + 4) == DEVICE))
            u.reg_write(UC_X86_REG_EAX, values['navigate'])
        elif name == 'parent':
            log.append('parent %d %d' % (r32(u, esp + 4) == MIND, r32(u, esp + 8) == REQUEST))
            u.reg_write(UC_X86_REG_EAX, values['parent'])
        else:
            log.append('%s %d' % (name, r32(u, esp + 4) == BODY))
            u.reg_write(UC_X86_REG_EAX, values[name])
    for address, name in CALLEES.items():
        u.mem_write(address, b'\xc3')
        u.hook_add(UC_HOOK_CODE, hook, begin=address, end=address, user_data=name)
    lines, expected = [], []
    for case in range(count):
        kind = rng.choice([7, 8, 8, 0x16, 0x2E, 0x31, 0x10, 0x30, 0x40, 6])
        flag = rng.randrange(64)
        bits = [rng.getrandbits(8) for _ in range(8)]
        values = {'flag': flag, 'navigate': rng.choice([0, 1, 7]), 'parent': rng.choice([0, 1, 0xFFFFFFFF]),
                  'effectkey': rng.randrange(0x10000), 'contactkey': rng.randrange(0x10000)}
        state['log'], state['values'] = [], values
        w32(u, MIND + 8, ACTOR); w32(u, ACTOR + 0x95C, BODY); w32(u, ACTOR + 0x960, DEVICE)
        w32(u, BODY, VTABLE); w32(u, VTABLE + 0x38, 0x00F20038); w32(u, VTABLE + 0x3C, 0x00F2003C)
        w32(u, BODY + 0x15C, BITS); w32(u, BITS + 8, BITS + 0x10); u.mem_write(BITS + 0x10, bytes(bits))
        u.mem_write(REQUEST + 8, struct.pack('<H', kind))
        u.mem_write(REQUEST + 0x10, b'flagname\0')
        w32(u, STACK, STOP); w32(u, STACK + 4, MIND); w32(u, STACK + 8, REQUEST)
        u.reg_write(UC_X86_REG_ESP, STACK)
        u.emu_start(0x0007D360, STOP, count=10000)
        expected.append('%x | %s' % (u.reg_read(UC_X86_REG_EAX), ' ; '.join(state['log'])))
        lines.append(' '.join(str(v) for v in [kind, flag] + bits + [values['navigate'], values['parent'],
                                                                     values['effectkey'], values['contactkey']]))
    text = str(count) + '\n' + '\n'.join(lines) + '\n'
    result = subprocess.run([harness('mind_request_update_harness')], input=text, capture_output=True, text=True)
    if result.returncode != 0:
        print('harness exit', result.returncode, result.stderr[-400:])
    got = result.stdout.splitlines()
    bad = abs(len(expected) - len(got))
    for i, (want, have) in enumerate(zip(expected, got)):
        if want.split() != have.split():
            bad += 1
            if bad <= 5:
                print('MISMATCH', i, lines[i], '\n exp', want, '\n got', have)
    print('mind request update: cases', len(expected), len(got), 'mismatches', bad)
    return int(bad != 0)

if __name__ == '__main__':
    raise SystemExit(main())
