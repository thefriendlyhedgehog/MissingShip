"""Word-level diff of ML's code (dma 114) vs MQ debug code (dma 28): zero shift, so any differing word is an ML edit.
Groups differing words by containing function (prologue addiu sp,sp,-N). Usage: codediff.py [lo hi]"""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..', 'pipeline'))
from mspaths import DEBUG_ROM, ML_ROM
import struct, sys
from mlrom import Rom
B = 0x8001CE60
ML = Rom(ML_ROM).file(114)
DB = Rom(DEBUG_ROM).file(28)
TEXT_END = 0xF9440 - 0x40000 if False else None
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
