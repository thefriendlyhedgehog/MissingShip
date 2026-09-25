"""Disassemble a routine in the hack's custom code (ML: dma 507, loaded at 0x80600000). Usage: mlcustom.py 0xADDR [maxinsns]"""
import sys
from capstone import *
import hack
md = Cs(CS_ARCH_MIPS, CS_MODE_MIPS32 | CS_MODE_BIG_ENDIAN)
if not hack.CUSTOM: sys.exit(f'{hack.NAME} has no custom code (config: {hack.CONFIG})')
DMA, BASE, _ = hack.custom_region()
C = hack.HACK.file(DMA)
def dis(a, n=400):
    rets = 0
    for i in md.disasm(C[a - BASE:a - BASE + n * 4], a):
        print(f'{i.address:#x}  {i.mnemonic} {i.op_str}')
        if i.mnemonic == 'jr' and i.op_str == '$ra':
            rets += 1
            if rets and len(sys.argv) < 4: 
                d = next(md.disasm(C[i.address + 4 - BASE:i.address + 8 - BASE], i.address + 4), None)
                print(f'{i.address+4:#x}  ' + (f'{d.mnemonic} {d.op_str}' if d else '?') + '   (delay slot)'); break
if __name__ == '__main__':
    dis(int(sys.argv[1], 16), int(sys.argv[2]) if len(sys.argv) > 2 else 400)
