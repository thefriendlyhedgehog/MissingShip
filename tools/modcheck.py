"""Instruction-level check of which vanilla actor overlays the hack modified (relocations masked).
Semantic diffs miss in-place patches such as a single hooked `j`; this compares every word.
Usage: modcheck.py 0xNNN [0xNNN ...]"""
import sys, struct
from ovldiff import load_pair, ML, DB
import hack
def check(aid):
    from mlrom import entry
    empty = [side for side, rom, dma in (('hack', ML, hack.H.code_dma), ('baseline', DB, hack.B.code_dma))
             if not entry(rom.file(dma), hack.ACTOR_TABLE, hack.ACTOR_STRIDE, aid)[0]]
    if empty:
        return f'{aid:#05x}: empty slot ({" and ".join(empty)})'
    try:
        m, b = load_pair(aid)
    except Exception as e:
        return f'{aid:#05x}: unreadable ({e.__class__.__name__})'
    if (m[1], m[2], m[3]) != (b[1], b[2], b[3]):
        return f'{aid:#05x}: REWRITTEN (text {m[1]:#x} vs {b[1]:#x})'
    t = sum(1 for (x, s1, _), (_, s2, _) in zip(m[4], b[4]) if s1 != s2)
    d = 0
    for o in range(m[1], m[1] + m[2] + m[3], 4):
        w1, w2 = struct.unpack('>I', m[0][o:o+4])[0], struct.unpack('>I', b[0][o:o+4])[0]
        if w1 != w2 and not (w1 >> 24 == 0x80 and w2 >> 24 == 0x80):
            d += 1
    regions = [hack.custom_region(i)[1:] for i in range(len(hack.CUSTOM))]
    hooks = sorted({hex(0x80000000 | ((w & 0x3FFFFFF) << 2)) for o in range(0, m[1], 4)
                    for w in [struct.unpack('>I', m[0][o:o+4])[0]]
                    if (w >> 26) in (2, 3) and any(lo <= (0x80000000 | ((w & 0x3FFFFFF) << 2)) < hi for lo, hi in regions)})
    return f'{aid:#05x}: ' + ('vanilla' if not (t or d) else f'{t} text / {d} data words differ') + (f'  hooks {hooks}' if hooks else '')
if __name__ == '__main__':
    for a in sys.argv[1:]: print(check(int(a, 0)))
