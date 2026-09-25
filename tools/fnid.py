"""Identify ML code functions by the debug strings they reference (lui/addiu pairs -> C strings)."""
import struct, sys
from capstone import *
from mlrom import Rom
ML=Rom('/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/tml_2_0.z64')
code=ML.file(114); BASE=0x8001CE60
md=Cs(CS_ARCH_MIPS, CS_MODE_MIPS32|CS_MODE_BIG_ENDIAN); md.detail=True
def cstr(addr):
    o=addr-BASE
    if not (0<=o<len(code)): return None
    e=code.find(b'\0',o)
    s=code[o:e]
    if len(s)<4 or len(s)>120: return None
    try: t=s.decode('euc-jp')
    except: return None
    return t if sum(ch.isprintable() for ch in t)>=len(t)-1 else None
def strings_in(fn, maxn=600):
    off=fn-BASE; hi={}; out=[]; n=0; saw_ret=False
    for ins in md.disasm(code[off:off+maxn*4], fn):
        n+=1
        ops=ins.op_str.replace('$','').split(', ')
        if ins.mnemonic=='lui': hi[ops[0]]=int(ops[1],0)<<16
        elif ins.mnemonic in ('addiu','ori') and len(ops)==3 and ops[1] in hi:
            imm=int(ops[2],0)
            a=(hi[ops[1]]|imm) if ins.mnemonic=='ori' else (hi[ops[1]]+ (imm if imm<0x8000 else imm-0x10000))&0xFFFFFFFF
            s=cstr(a)
            if s and s not in out: out.append(s)
        if saw_ret: break
        if ins.mnemonic=='jr' and ops[0]=='ra': saw_ret=True
    return n,out
for a in sys.argv[1:]:
    fn=int(a,16); n,s=strings_in(fn)
    print(f'{fn:#010x} ({n} insns): ' + (' | '.join(s[:3]) if s else '(no strings)'))
