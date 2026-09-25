"""Print every differing word of a vanilla actor overlay that ML edited in place (same section sizes).
Text words are shown disassembled, data words raw with their section offset. Usage: smalldiff.py 0xID [...]"""
import sys, struct
from capstone import *
from ovldiff import load, ML, DB
md = Cs(CS_ARCH_MIPS, CS_MODE_MIPS32 | CS_MODE_BIG_ENDIAN)
def dis(w, a):
    l = list(md.disasm(struct.pack('>I', w), a))
    return f'{l[0].mnemonic} {l[0].op_str}' if l else f'.word {w:#010x}'
for arg in sys.argv[1:]:
    aid = int(arg, 0)
    m = load(ML, 114, 0xF9440, aid); b = load(DB, 28, 0xF9440, aid)
    md_, bd = m[0], b[0]; t = m[1]
    print(f'== {aid:#05x} text {t:#x} data {m[2]:#x} rodata {m[3]:#x}')
    for o in range(0, t + m[2] + m[3], 4):
        x = struct.unpack('>I', md_[o:o+4])[0]; y = struct.unpack('>I', bd[o:o+4])[0]
        if x == y: continue
        if o < t:
            print(f'  text +{o:#06x}  ML {dis(x, o):34s} DB {dis(y, o)}')
        else:
            print(f'  data +{o:#06x} (d+{o-t:#x})  ML {x:#010x}  DB {y:#010x}')
