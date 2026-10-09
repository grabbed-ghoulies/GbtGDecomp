"""Differential check: 0x0018F8B7 / 0x001914E8 against shader_validation_serializer.cpp."""
import random, struct, subprocess, sys
from retail_cpu import *
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_ECX, UC_X86_REG_EAX
R = lambda: random.getrandbits(32)
def gen():
    n = random.randint(1,6); owners=[]
    for i in range(n):
        kind = random.choice([0,1,2,3,3,2,4,R()])
        owners.append([kind, random.choice([R(), random.randint(0,20), random.randint(0,300)]), -1])
    m = random.randint(1,8); ops=[]
    for i in range(m):
        parent = random.choice([-1,-2]+list(range(n))*3)
        ops.append([parent, random.getrandbits(8)] + [random.choice([0,1,2,3,4,4,4,R(),5]) for _ in range(4)])
    for o in owners: o[2] = random.choice([-1]+list(range(m)))
    active = random.choice([-1,-2]+list(range(n)))
    k = random.randint(0,5); ins=[]
    for i in range(k):
        ins.append([random.randrange(m),random.randrange(m),random.randrange(m),random.getrandbits(8)]
                   +[random.choice([0,0,1,0x80,0xff]) for _ in range(12)]
                   +[random.choice([0,0xD,0xD,random.randint(0,15),R()]), random.choice([0,random.randint(0,7),R()]),
                     random.choice([-1]+list(range(n))),random.choice([-1]+list(range(n))),random.choice([-1]+list(range(n)))])
    frame=[R(),R(),R()]
    return active, owners, ops, ins, frame
def text(mode, c):
    active, owners, ops, ins, frame = c
    s=[mode, active, len(owners)]
    for o in owners: s+=o
    s.append(len(ops))
    for o in ops: s+=o
    s.append(len(ins))
    for x in ins: s+=x
    s+=frame
    return ' '.join(str(v) for v in s)
def emulate(mode, c):
    active, owners, ops, ins, frame = c
    u = new_emu()
    OW = [HEAP + 0x40*i for i in range(len(owners))]
    OP = [HEAP + 0x1000 + 0x40*i for i in range(len(ops))]
    IN = [HEAP + 0x2000 + 0x50*i for i in range(len(ins))]
    ND = [HEAP + 0x3000 + 0x10*i for i in range(len(ins))]
    PROG = HEAP + 0x4000; DUMMY = HEAP + 0x5000
    act = 0 if active == -1 else DUMMY if active == -2 else OW[active]
    w32(u, 0x001A4D34, act)
    for i,o in enumerate(owners):
        w32(u,OW[i]+4,o[0]); w32(u,OW[i]+8,o[1]); w32(u,OW[i]+0xC, 0 if o[2]<0 else OP[o[2]])
    for i,o in enumerate(ops):
        p=o[0]; w32(u,OP[i]+0x10, 0 if p==-1 else act if p==-2 else OW[p]); u.mem_write(OP[i]+0x14, bytes([o[1]]))
        w32(u,OP[i]+0x18,0x203794)
        for j in range(4): w32(u,OP[i]+0x1C+4*j,o[2+j])
    for i,x in enumerate(ins):
        b=IN[i]
        for j in range(3): w32(u,b+4+4*j,OP[x[j]])
        u.mem_write(b+0x10, bytes([x[3]]))
        for mi,off in enumerate([0x14,0x1C,0x24]):
            w32(u,b+off,0x203790); u.mem_write(b+off+4, bytes(x[4+4*mi:8+4*mi]))
        w32(u,b+0x2C,x[16]); w32(u,b+0x30,x[17])
        for j,off in enumerate([0x34,0x38,0x3C]):
            v=x[18+j]; w32(u,b+off,0 if v<0 else OW[v])
        w32(u,ND[i]+4,b); w32(u,ND[i]+0xC, ND[i+1] if i+1<len(ins) else 0)
    lines=[]
    if mode == 0:
        T = STACK
        for a,v in zip([T-0x20,T-0x1C,T-0x18],frame): w32(u,a,v)
        OUT = HEAP+0x6000
        for i in range(len(ins)):
            w32(u,T,STOP); w32(u,T+4,OUT)
            u.reg_write(UC_X86_REG_ESP,T); u.reg_write(UC_X86_REG_ECX,IN[i])
            u.emu_start(0x0018F8B7, STOP)
            o=[r32(u,OUT+4*j) for j in range(3)]; f=[r32(u,a) for a in [T-0x20,T-0x1C,T-0x18]]
            lines.append('%08X %08X %08X | %08X %08X %08X %d' % (*o,*f, u.reg_read(UC_X86_REG_EAX)==OUT))
    else:
        S = STACK
        for a,v in zip([S-0x48,S-0x44,S-0x40],frame): w32(u,a,v)
        PASS = HEAP+0x7000; w32(u,PASS+8, ND[0] if ins else 0)
        for j in range(1+3*len(ins)+8): w32(u,PROG+4*j,0xCCCCCCCC)
        w32(u,S,STOP); w32(u,S+4,PROG)
        u.reg_write(UC_X86_REG_ESP,S); u.reg_write(UC_X86_REG_ECX,PASS)
        u.emu_start(0x001914E8, STOP)
        lines.append(('%d' % (u.reg_read(UC_X86_REG_EAX)&0xff)) + ''.join(' %08X' % r32(u,PROG+4*j) for j in range(1+3*len(ins))))
    for i in range(len(ops)):
        lines.append(' '.join('%08X' % r32(u,OP[i]+0x1C+4*j) for j in range(4)))
    lines.append('--')
    return lines
if __name__ == '__main__':
    random.seed(int(sys.argv[1]) if len(sys.argv)>1 else 1)
    N = int(sys.argv[2]) if len(sys.argv)>2 else 500
    cases=[(mode,gen()) for mode in (0,1) for _ in range(N)]
    inp='\n'.join(text(m,c) for m,c in cases)+'\n'
    got=subprocess.run([harness('serializer_harness')],input=inp,capture_output=True,text=True).stdout.splitlines()
    exp=[]
    for m,c in cases: exp+=emulate(m,c)
    bad=0
    for i,(a,b) in enumerate(zip(exp,got)):
        if a!=b:
            bad+=1
            if bad<6: print('MISMATCH line',i,'\n exp',a,'\n got',b)
    print('lines',len(exp),len(got),'mismatches',bad)
