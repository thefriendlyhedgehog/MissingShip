"""Aligned diff of a code-referenced overlay (player / kaleido), ML vs MQ debug.

Both ROMs keep the overlay's {vromStart, vromEnd, vramStart, vramEnd} at the same offset in `code`
(zero shift against MQ debug), so the entry offset locates each side. Reloc'd words are masked by the
overlay's own relocation table so only real edits are reported. Usage: pldiff.py <code_offset_of_entry>
"""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..', 'pipeline'))
from mspaths import DEBUG_ROM, ML_ROM
import struct, sys
from capstone import *
from mlrom import Rom
ML=Rom(ML_ROM)
DB=Rom(DEBUG_ROM)
md=Cs(CS_ARCH_MIPS, CS_MODE_MIPS32|CS_MODE_BIG_ENDIAN)
def load(rom, codeidx, off):
    c=rom.file(codeidx)
    vs,ve,vas,vae=struct.unpack('>IIII',c[off:off+16])
    d=rom.file_at_vrom(vs)
    tail=struct.unpack('>I',d[-4:])[0]; o=len(d)-tail
    t,da,ro,bs,n=struct.unpack('>IIIII',d[o:o+20])
    rel=struct.unpack(f'>{n}I',d[o+20:o+20+n*4])
    relt={r&0xFFFFFF:(r>>24)&0x3F for r in rel}
    return d,t,da,ro,relt,vas
def diff(off):
    m=load(ML,114,off); b=load(DB,28,off)
    print(f'ML text {m[1]:#x} data {m[2]:#x} rodata {m[3]:#x} | DBG text {b[1]:#x} data {b[2]:#x} rodata {b[3]:#x}')
    out=[]
    for o in range(0,min(m[1],b[1]),4):
        a,c=m[0][o:o+4],b[0][o:o+4]
        if a==c: continue
        if o in m[4] and o in b[4]:
            wa,wc=struct.unpack('>I',a)[0],struct.unpack('>I',c)[0]
            if m[4][o]==4 and b[4][o]==4:   # jal/j local: compare offsets
                if ((wa&0x3FFFFFF)<<2)-(m[5]&0xFFFFFFF)==((wc&0x3FFFFFF)<<2)-(b[5]&0xFFFFFFF): continue
            else: continue
        ia=next(md.disasm(a,o),None); ic=next(md.disasm(c,o),None)
        out.append((o, f'{ia.mnemonic} {ia.op_str}' if ia else a.hex(), f'{ic.mnemonic} {ic.op_str}' if ic else c.hex()))
    return m,b,out
if __name__=='__main__':
    m,b,out=diff(int(sys.argv[1],0))
    for o,x,y in out: print(f'{o:#07x}  ML: {x:<42} DBG: {y}')
    print(len(out),'differing words')
