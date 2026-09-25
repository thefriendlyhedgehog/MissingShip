import struct, sys
from capstone import *
import hack
def disasm(dma, label):
    ml=hack.HACK
    d=ml.file(dma)
    tail=struct.unpack('>I',d[-4:])[0]; o=len(d)-tail
    t,da,ro,bs,n=struct.unpack('>IIIII',d[o:o+20])
    md=Cs(CS_ARCH_MIPS, CS_MODE_MIPS32|CS_MODE_BIG_ENDIAN)
    md.detail=False
    print(f"=== {label}: text {t}B ===")
    for i in md.disasm(d[:t], 0x0):
        print(f"  {i.address:04x}:  {i.mnemonic:<10s} {i.op_str}")
if __name__=='__main__':
    disasm(int(sys.argv[1]), sys.argv[2])
