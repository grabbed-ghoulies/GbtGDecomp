"""Differential check: validator mask/order/instruction/owner/pass-tree accessors."""
import random, struct, subprocess, sys
from retail_cpu import *
from unicorn.x86_const import *
R=lambda: random.getrandbits(32)
def rd(u,a,n): return bytes(u.mem_read(a,n))
def ch(): return random.choice([0,1,2,3,4,4,R(),5])
cases=[]; exp=[]
u=new_emu()
for _ in range(400):
    k=random.choice(['mask','order','ord','ins','own','tree'])
    if k=='mask':
        a=[random.getrandbits(8) for _ in range(4)]; M=HEAP
        u.mem_write(M,b'\x11'*4+b'\x09'*4)
        r=call(u,0x0018E414,M,[x|(R()&0xffffff00) for x in a])
        cases.append('mask '+' '.join(map(str,a))); exp.append('%08X %s %d'%(r32(u,M),rd(u,M+4,4).hex().upper(),r==M))
    elif k=='order':
        a=[R() for _ in range(4)]; M=HEAP; u.mem_write(M,b'\0'*20)
        r=call(u,0x0018E595,M,a)
        cases.append('order '+' '.join(map(str,a))); exp.append(' '.join('%08X'%r32(u,M+4*i) for i in range(5))+' %d'%(r==M))
    elif k=='ord':
        c=[ch() for _ in range(4)]; v=random.choice([0,1,2,3,4,R()]+c); idx=random.randint(0,4); M=HEAP
        w32(u,M,0x203794); [w32(u,M+4+4*i,c[i]) for i in range(4)]
        a=call(u,0x0018E5C3,M,[v])&0xff; b=call(u,0x0018E67C,M,[idx]); d=call(u,0x0018E6BE,M,[v])
        cases.append('ord %s %d %d'%(' '.join(map(str,c)),v,idx)); exp.append('%d %u %u'%(a,b,d))
    elif k=='ins':
        flag=random.choice([0,0,1,0x80]); m=[random.choice([0,0,1,0xff]) for _ in range(12)]
        vop=random.choice([0,1,2,3,4,5,6,7,8,0xD,R(),0xFFFFFFFF,0x80000005]); sop=random.choice([0,1,2,R()])
        I=HEAP; u.mem_write(I,b'\xab'*0x50); u.mem_write(I+0x10,bytes([flag]))
        for j,off in enumerate([0x14,0x1C,0x24]): w32(u,I+off,0x203790); u.mem_write(I+off+4,bytes(m[4*j:4*j+4]))
        w32(u,I+0x2C,vop); w32(u,I+0x30,sop)
        out=[]
        for j,f in enumerate([0x0018E833,0x0018E84E,0x0018E869]):
            Rr=HEAP+0x100+8*j; u.mem_write(Rr,b'\0'*8); r=call(u,f,I,[Rr]); out.append('%08X %s'%(r32(u,Rr),rd(u,Rr+4,4).hex().upper()))
        s=' '.join(out)+' %d |'%(r==Rr)
        for i in range(4): s+=' %u%u'%(call(u,0x0018E884,I,[i]),call(u,0x0018E8A5,I,[i]))
        s+=' | %u %u %u'%(call(u,0x0018E8E4,I,[]),call(u,0x0018E8C6,I,[]),call(u,0x0018E8F8,I,[]))
        cases.append('ins %d %s %d %d'%(flag,' '.join(map(str,m)),vop,sop)); exp.append(s)
    elif k=='own':
        n=random.randint(1,5); O=[HEAP+0x40*i for i in range(n)]; desc=[]
        for i in range(n):
            chs=[random.choice([0,0,R(),5]) for _ in range(4)]; d=random.choice([-1]+list(range(n))); c=random.choice([-1]+list(range(n)))
            for j in range(4): w32(u,O[i]+0x20+4*j,chs[j])
            w32(u,O[i]+0x30,0 if d<0 else O[d]); w32(u,O[i]+0x34,0 if c<0 else O[c]); desc+=chs+[d,c]
        ops=[]; res=''
        for _ in range(random.randint(1,6)):
            kind=random.randint(0,2); a=random.randrange(n)
            if kind<2:
                b=random.choice([-1]+list(range(n))); call(u,[0x0018E76F,0x0018F4F7][kind],O[a],[0 if b<0 else O[b]])
            else:
                # avoid infinite cycles of zero channels: retail would loop forever too
                b=random.randint(0,3)
                seen=set(); x=O[a]; cyc=False
                while x:
                    if r32(u,x+0x20+4*b): break
                    if x in seen: cyc=True; break
                    seen.add(x); x=r32(u,x+0x34)
                if cyc: continue
                res+='%08X '%call(u,0x0018E7AF,O[a],[b])
            ops.append('%d %d %d'%(kind,a,b))
        st=''
        for i in range(n):
            d=r32(u,O[i]+0x30); c=r32(u,O[i]+0x34)
            st+='%d %d '%(O.index(d) if d else -1, O.index(c) if c else -1)
        cases.append('own %d %s %d %s'%(n,' '.join(map(str,desc)),len(ops),' '.join(ops))); exp.append((res+st).rstrip())
    else:
        n=random.randint(0,7); N=[HEAP+0x20*i for i in range(n)]; desc=[]
        for i in range(n):
            key=random.choice([R(),random.randint(-5,5)&0xffffffff,random.randint(0,10)])
            f=random.choice([-1]+list(range(i+1,n))); s=random.choice([-1]+list(range(i+1,n)))
            w32(u,N[i]+4,key); w32(u,N[i]+0x10,0 if f<0 else N[f]); w32(u,N[i]+0x14,0 if s<0 else N[s]); desc.append('%d %d %d'%(key,f,s))
        root=0 if n else -1; T=HEAP+0x800; w32(u,T+8,N[0] if n else 0)
        qs=[random.choice([R(),random.randint(-5,5)&0xffffffff]) for _ in range(5)]
        if n: qs+= [r32(u,N[random.randrange(n)]+4) for _ in range(3)]
        rr=[]
        for q in qs:
            r=call(u,0x00190370,T,[q,R()]); rr.append(str(N.index(r) if r else -1))
        cases.append('tree %d %d %s %d %s'%(n,root,' '.join(desc),len(qs),' '.join(map(str,qs)))); exp.append(' '.join(rr))
got=subprocess.run([harness('accessor_harness')],input='\n'.join(cases)+'\n',capture_output=True,text=True).stdout.splitlines()
bad=0
for c,a,b in zip(cases,exp,got):
    if a.strip()!=b.strip():
        bad+=1
        if bad<6: print('MISMATCH',c,'\n exp',a,'\n got',b)
print('cases',len(cases),len(got),'mismatches',bad)
