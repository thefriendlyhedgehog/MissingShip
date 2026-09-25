"""Word-level diff of the hack's `code` vs the baseline's: zero shift, so any differing word is a hack edit.
Groups differing words by containing function (prologue addiu sp,sp,-N). Usage: codediff.py [lo hi]"""
import struct, sys
import hack
B = hack.CODE_VRAM
ML = hack.HACK.file(hack.H.code_dma)
DB = hack.BASE.file(hack.B.code_dma)
def W(d, o): return struct.unpack('>I', d[o:o+4])[0]
def fn_start(o):
    while o > 0:
        w = W(DB, o)
        if (w >> 16) == 0x27BD and (w & 0x8000): return o
        o -= 4
    return 0
if __name__ == '__main__':
    lo = int(sys.argv[1], 16) - B if len(sys.argv) > 2 else 0
    hi = int(sys.argv[2], 16) - B if len(sys.argv) > 2 else min(len(ML), len(DB))
    groups = {}
    for o in range(lo, hi, 4):
        if ML[o:o+4] != DB[o:o+4]:
            groups.setdefault(fn_start(o), []).append(o)
    for f, offs in sorted(groups.items()):
        print(f'{B+f:#010x}  {len(offs):4d} words  first +{offs[0]-f:#x}')
