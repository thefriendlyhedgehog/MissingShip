import os as _os, sys as _sys
_sys.path.insert(0, _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..', 'pipeline'))
from mspaths import ML_ROM
import struct, sys
from capstone import *
from mlrom import Rom, entry
def disasm(dma, label):
    ml=Rom(ML_ROM)
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
