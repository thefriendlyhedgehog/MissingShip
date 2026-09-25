"""Disassemble a routine in ML's custom code (dma 507, loaded at 0x80600000). Usage: mlcustom.py 0xADDR [maxinsns]"""
import sys
from capstone import *
from mlrom import Rom
md = Cs(CS_ARCH_MIPS, CS_MODE_MIPS32 | CS_MODE_BIG_ENDIAN)
C = Rom('/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/tml_2_0.z64').file(507)
def dis(a, n=400):
    rets = 0
    for i in md.disasm(C[a - 0x80600000:a - 0x80600000 + n * 4], a):
        print(f'{i.address:#x}  {i.mnemonic} {i.op_str}')
        if i.mnemonic == 'jr' and i.op_str == '$ra':
            rets += 1
            if rets and len(sys.argv) < 4: 
                nxt = C[i.address + 8 - 0x80600000:i.address + 12 - 0x80600000]
                print(f'{i.address+4:#x}  (delay)'); break
if __name__ == '__main__':
    dis(int(sys.argv[1], 16), int(sys.argv[2]) if len(sys.argv) > 2 else 400)
