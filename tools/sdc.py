import sys
from capstone import *
from codediff import ML, DB, B
md=Cs(CS_ARCH_MIPS, CS_MODE_MIPS32|CS_MODE_BIG_ENDIAN)
def body(d,a):
    out=[]; ret=False
    for i in md.disasm(d[a-B:a-B+0x2000],a):
        out.append(f'{i.address:#x}  {i.mnemonic} {i.op_str}')
        if ret: break
        if i.mnemonic=='jr' and i.op_str=='$ra': ret=True
    return out
a=int(sys.argv[1],16); which=sys.argv[2] if len(sys.argv)>2 else 'ML'
for l in body(ML if which=='ML' else DB, a): print(l)
