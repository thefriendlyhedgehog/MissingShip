"""Per-function diff of a recompiled actor overlay, ML vs debug ROM. Usage: fnpair.py 0xACTOR [-v]

ML rebuilt some vanilla overlays with a different compiler, so an aligned diff (ovldiff.py) drowns in scheduling
noise. Compilers keep function order, though, so split both sides into functions (local jal targets plus code
pointers in .data), pair them in order, and compare each pair as a multiset of normalised instructions (branch
targets and relocated immediates masked). Pairs whose multisets match are the same code reordered; the rest are
listed with what only one side has. -v also prints matching pairs.
"""
import struct, sys, re
from collections import Counter
from ovldiff import load, ML, DB

def functions(side):
    d, t, da, ro, ins, vas = side
    starts = {0}
    for a, s, raw in ins:
        if raw.startswith('jal '):
            tgt = int(raw.split()[1], 16)
            if vas & 0x00FFFFFF <= tgt < (vas & 0x00FFFFFF) + t:
                starts.add(tgt - (vas & 0x00FFFFFF))
    for o in range(t, t + da, 4):                      # code pointers in .data (InitVars, action tables)
        w = struct.unpack('>I', d[o:o + 4])[0]
        if vas <= w < vas + t and w % 4 == 0:
            starts.add(w - vas)
    starts = sorted(starts)
    out = []
    for i, st in enumerate(starts):
        en = starts[i + 1] if i + 1 < len(starts) else t
        body = [x for x in ins if st <= x[0] < en]
        while body and body[-1][2] == 'nop ':          # trailing alignment padding
            body.pop()
        out.append((st, body))
    return out

def norm(s):
    s = re.sub(r'\$(t\d|s\d|a\d|v\d|at|fp)', 'R', s)   # register allocation differs between compilers
    s = re.sub(r'\$f\d+', 'F', s)
    if s.split()[0].startswith('b') or s.startswith('j '):
        s = s.rsplit(', ', 1)[0] if ', ' in s else s.split()[0]
        s += ' <target>'
    return s

def main():
    aid = int(sys.argv[1], 0)
    m, b = load(ML, 114, 0xF9440, aid), load(DB, 28, 0xF9440, aid)
    fm, fb = functions(m), functions(b)
    print(f'actor {aid:#x}: {len(fm)} functions in ML, {len(fb)} in the debug ROM')
    for i in range(max(len(fm), len(fb))):
        if i >= len(fm) or i >= len(fb):
            side, (st, body) = ('ML', fm[i]) if i < len(fm) else ('DBG', fb[i])
            print(f'#{i:<3} only in {side} at +{st:#06x} ({len(body)} insns)')
            continue
        (ms, mb), (bs, bb) = fm[i], fb[i]
        cm, cb = Counter(norm(x[1]) for x in mb), Counter(norm(x[1]) for x in bb)
        if cm == cb:
            if '-v' in sys.argv:
                print(f'#{i:<3} ML +{ms:#06x} = DBG +{bs:#06x} ({len(mb)} insns)')
            continue
        print(f'#{i:<3} ML +{ms:#06x} ({len(mb)}) vs DBG +{bs:#06x} ({len(bb)})')
        for k, v in sorted((cm - cb).items()):
            print(f'       ML  +{v} {k}')
        for k, v in sorted((cb - cm).items()):
            print(f'       DBG +{v} {k}')

if __name__ == '__main__':
    main()
