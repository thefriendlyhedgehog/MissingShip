"""Aligned instruction diff of one actor overlay, ML vs debug ROM, relocations masked."""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..', 'pipeline'))
from mspaths import DEBUG_ROM, ML_ROM
import struct, sys, difflib
from capstone import *
from mlrom import Rom, entry
md=Cs(CS_ARCH_MIPS, CS_MODE_MIPS32|CS_MODE_BIG_ENDIAN)
ML=Rom(ML_ROM)
DB=Rom(DEBUG_ROM)
def load(rom, codeidx, base, aid):
    vs,ve,vas,vae,_,ini=entry(rom.file(codeidx), base, 0x20, aid)
    d=rom.file_at_vrom(vs)
    tail=struct.unpack('>I',d[-4:])[0]; o=len(d)-tail
    t,da,ro,bs,n=struct.unpack('>IIIII',d[o:o+20])
    rel=struct.unpack(f'>{n}I',d[o+20:o+20+n*4])
    relt={r&0xFFFFFF:(r>>24)&0x3F for r in rel if (r>>30)&3==1}
    ins=[]
    for i in md.disasm(d[:t], 0):
        typ=relt.get(i.address)
        s=f'{i.mnemonic} {i.op_str}'
        if typ==4: s=f'{i.mnemonic} <reloc26>'
        elif typ in (5,6): s=s.rsplit(', ',1)[0]+', <reloc16>'
        elif i.mnemonic in ('jal','j'): s=f'{i.mnemonic} {i.op_str}'   # absolute into code: keep (ML code shifts 0x20 though)
        ins.append((i.address,s,f'{i.mnemonic} {i.op_str}'))
    return d,t,da,ro,ins,vas
if __name__=='__main__':
    aid=int(sys.argv[1],0)
    m=load(ML,114,0xF9440,aid); b=load(DB,28,0xF9440,aid)
    def key(x):   # also neutralise the uniform 0x20 code shift in jal targets
        s=x[1]
        if s.startswith('jal 0x'): 
            return 'jal <code>'
        return s
    A=[key(x) for x in m[4]]; B=[key(x) for x in b[4]]
    sm=difflib.SequenceMatcher(None,A,B,autojunk=False)
    print(f'actor {aid:#x}: ML text {m[1]}B ({len(A)} insns)  DBG text {b[1]}B ({len(B)} insns)')
    for tag,i1,i2,j1,j2 in sm.get_opcodes():
        if tag=='equal': continue
        print(f'\n--- {tag}: ML +{m[4][i1][0] if i1<len(A) else -1:#06x}..  DBG +{b[4][j1][0] if j1<len(B) else -1:#06x}..')
        for k in range(max(i1-3,0), min(i2+3,len(A))): print(f'   ML  +{m[4][k][0]:#06x}  {m[4][k][2]}' + ('   <==' if i1<=k<i2 else ''))
        print('   ...')
        for k in range(max(j1-3,0), min(j2+3,len(B))): print(f'   DBG +{b[4][k][0]:#06x}  {b[4][k][2]}' + ('   <==' if j1<=k<j2 else ''))
