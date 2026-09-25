"""Diff one engine function between ML and the debug ROM, masking relocation-dependent immediates."""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..', 'pipeline'))
from mspaths import DEBUG_ROM, ML_ROM
import struct, sys
from capstone import *
from mlrom import Rom
B=0x8001CE60
md=Cs(CS_ARCH_MIPS, CS_MODE_MIPS32|CS_MODE_BIG_ENDIAN)
ML=Rom(ML_ROM).file(114)
DB=Rom(DEBUG_ROM).file(28)
def body(code, fn, maxn=800):
    out=[]; ret=False
    for i in md.disasm(code[fn-B:fn-B+maxn*4], fn):
        out.append(i)
        if ret: break
        if i.mnemonic=='jr' and i.op_str=='$ra': ret=True
    return out
def norm(i, fn):
    m=i.mnemonic; o=i.op_str
    if m in ('jal','j'): return f'{m} <call>'
    if m.startswith('b'):                       # branches: keep relative target
        parts=o.split(', '); tgt=int(parts[-1],0)-fn
        return f'{m} {", ".join(parts[:-1])}, +{tgt:#x}'
    if m=='lui': return f'lui {o.split(", ")[0]}, <hi>'
    return f'{m} {o}'
def find_by_string(code, s):
    so=code.find(s); sv=B+so; hi=(sv+0x8000)>>16; lo=sv&0xFFFF
    for o in range(0,len(code)-4,4):
        w=struct.unpack('>I',code[o:o+4])[0]
        if (w>>26)==0x0F and (w&0xFFFF)==hi:
            for q in range(o+4,o+48,4):
                x=struct.unpack('>I',code[q:q+4])[0]
                if (x>>26)==0x09 and (x&0xFFFF)==lo:
                    st=q
                    while st>0 and not ((struct.unpack('>I',code[st:st+4])[0]>>16)==0x27BD and (code[st+2]&0x80)): st-=4
                    return B+st
if __name__=='__main__':
    s=sys.argv[1].encode()
    mf=find_by_string(ML,s); df=find_by_string(DB,s)
    a=[norm(i,mf) for i in body(ML,mf)]; b=[norm(i,df) for i in body(DB,df)]
    print(f'{sys.argv[1]!r}: ML {mf:#010x} ({len(a)} insns)  DBG {df:#010x} ({len(b)} insns)')
    diffs=[(k,x,y) for k,(x,y) in enumerate(zip(a,b)) if x!=y]
    print(f'   differing instructions: {len(diffs)}' + ('' if len(a)==len(b) else f'  (LENGTH DIFFERS by {len(a)-len(b)})'))
    for k,x,y in diffs[:25]: print(f'   +{k*4:#05x}  ML: {x:40s} DBG: {y}')
