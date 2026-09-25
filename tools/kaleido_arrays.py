"""Find which pause-menu (kaleido) C arrays ML changed, by value rather than by offset.

SoH reorders and extends some of the decomp's kaleido arrays, so an ML data offset cannot be mapped to a SoH
symbol by position. Instead: parse each simple numeric array out of SoH's kaleido sources, pack its vanilla
values big-endian, find that byte string in MQ debug's kaleido data, and read ML's bytes at the same place.
Prints only the arrays whose ML values differ. Usage: kaleido_arrays.py
"""
import re, struct, glob
from pldiff import load, ML, DB
KDIR = '/Volumes/MacMiniExternal/AI_Projects/Shipwright/soh/src/overlays/misc/ovl_kaleido_scope/'
ENUM = {'AGE_REQ_ADULT': 0, 'AGE_REQ_CHILD': 1, 'AGE_REQ_NONE': 9,
        'PAUSE_ITEM': 0, 'PAUSE_MAP': 1, 'PAUSE_QUEST': 2, 'PAUSE_EQUIP': 3}
FMT = {'s8': 'b', 'u8': 'B', 's16': 'h', 'u16': 'H', 's32': 'i', 'u32': 'I', 'f32': 'f'}
m = load(ML, 114, 0x110344); b = load(DB, 28, 0x110344)
mdat, bdat = m[0], b[0]
start = m[1]
def val(tok):
    tok = tok.strip()
    if tok in ENUM: return ENUM[tok]
    tok = re.sub(r'(?<=\d)f$', '', tok)
    return float(tok) if '.' in tok else int(tok, 0)
out = []
for path in sorted(glob.glob(KDIR + '*.c')):
    src = open(path).read()
    for mm in re.finditer(r'(?:static\s+)?(s8|u8|s16|u16|s32|u32|f32)\s+(\w+)\s*((?:\[[^\]]*\])+)\s*=\s*\{(.*?)\};', src, re.S):
        typ, name, _, body = mm.groups()
        body = re.sub(r'//[^\n]*', '', body)
        toks = [t for t in re.split(r'[,{}\s]+', body) if t]
        try: vals = [val(t) for t in toks]
        except Exception: continue
        if len(vals) < 3: continue
        fmt = '>' + FMT[typ] * len(vals)
        bits = {'s8': 8, 's16': 16, 's32': 32}.get(typ)
        if bits:   # hex literals such as 0xFFCF / 0xFE are written unsigned in the C source
            vals = [v - (1 << bits) if isinstance(v, int) and v >= (1 << (bits - 1)) else v for v in vals]
        try: blob = struct.pack(fmt, *vals)
        except struct.error: continue
        hits = [i for i in range(start, len(bdat) - len(blob) + 1) if bdat[i:i+len(blob)] == blob]
        if len(hits) != 1:
            continue
        o = hits[0]
        mv = struct.unpack(fmt, mdat[o:o+len(blob)])
        if list(mv) != [v if typ != 'f32' else struct.unpack('>f', struct.pack('>f', v))[0] for v in vals]:
            out.append((o, name, typ, path.split('/')[-1], vals, list(mv)))
for o, name, typ, f, vals, mv in sorted(out):
    print(f'{o:#07x} {f}:{name} ({typ}[{len(vals)}])')
    print('   MQ:', vals)
    print('   ML:', mv)
