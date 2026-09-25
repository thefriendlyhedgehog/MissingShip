"""Semantic diff of a vanilla actor overlay, ML vs debug ROM.

ML's overlays were rebuilt with a different compiler, so instruction-aligned diffs drown in
delay-slot noise. Compare meaning instead: engine calls (MQ debug baseline: zero code shift
cancelled), constants present on one side only, and changed data halfwords.
"""
import struct, sys
from collections import Counter
from ovldiff import load_pair
def run(aid):
    m,b=load_pair(aid)
    jb=Counter(s.split()[1] for _,_,s in b[4] if s.startswith('jal '))
    jm=Counter()
    for _,_,s in m[4]:
        if s.startswith('jal '):
            t=int(s.split()[1],0)
            jm[hex(t)]+=1
    print(f'actor {aid:#x}: text ML {m[1]}B vs DBG {b[1]}B | data {m[2]}B vs {b[2]}B | rodata {m[3]}B vs {b[3]}B')
    ch={k:(jm.get(k,0),jb.get(k,0)) for k in sorted(set(jm)|set(jb)) if jm.get(k,0)!=jb.get(k,0)}
    print('  call-count changes (ML, DBG):', ch or 'none')
    def imms(ins):
        c=Counter()
        for _,_,s in ins:
            if s.startswith(('addiu','ori')) and '$zero' in s: c[int(s.split(', ')[-1],0)&0xFFFF]+=1
        return c
    im,ib=imms(m[4]),imms(b[4])
    print('  constants ML-only :', sorted(hex(k) for k in im if k not in ib))
    print('  constants DBG-only:', sorted(hex(k) for k in ib if k not in im))
    return m,b
if __name__=='__main__': run(int(sys.argv[1],0))
