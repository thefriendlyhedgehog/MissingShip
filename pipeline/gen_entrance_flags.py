"""Generate soh/src/code/ml_entrance_flags.c: ML's entrance flags for the SoH entrances ML uses (MS-027).

An entrance's flags word says whether the music keeps playing across the transition, whether the area title card
shows, and which transition plays on the way out and in. ML rebuilt its entrance table with its own flags, but its
exits are remapped onto SoH entrances (ml_entrance.py), so SoH applied its OWN flags. This writes ML's flags onto
those SoH entrances at startup.

Entrances come in scene-layer groups (child day/night, adult day/night...): consecutive entries with the same
(scene, spawn). The engine adds the layer to a group's first entrance, so ML's group starting at e and SoH's group
starting at v = REV[(scene, spawn)] line up entry by entry: ML[e + k] -> SoH[v + k], for k within both groups.
Groups whose destination has no exact SoH entrance (the "lowest entrance of the scene" fallback) are skipped.

Only groups ML can actually reach are applied: scene exits, cutscene destinations, ML's grotto table (Door_Ana data
+0x5FC) and the ML entrance numbers the engine converts with MlEntrance_ToSoh. ML's table also keeps vanilla's
leftover entries (e.g. ML 0x201 is still vanilla's Kakariko entrance), which land on the same SoH entrances as ML's
real ones with vanilla's flags; they are unreachable in ML and are ignored. When two reachable ML groups want
different flags on one SoH entrance, the group referenced most often wins; on a tie, fields they agree on take ML's
value and disputed fields keep SoH's. Either way the case is printed: a known deviation, so far only day-layer fade
colours (black vs white) on 4 entrances.
Rerun after changing entrances (same as gen_entrance_map.py).
"""
from mspaths import ML_ROM, PIPE, ROOT, SOH
import collections, glob, json, os, re, struct, sys
sys.path.insert(0, PIPE)
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import ml_entrance as m
from mlrom import Rom
from yaz0 import get_file

OUT = SOH + '/src/code/ml_entrance_flags.c'
EN = open(SOH + '/include/tables/entrance_table.h').read()
Z64 = open(SOH + '/include/z64.h').read()

def split_args(t):
    out, depth, cur = [], 0, ''
    for c in t:
        if c == ',' and depth == 0:
            out.append(cur.strip()); cur = ''; continue
        depth += (c == '(') - (c == ')'); cur += c
    return out + [cur.strip()]

enums = {}
for blk in re.findall(r'typedef enum \{(.*?)\}', Z64, re.S):
    val = -1
    for name, v in re.findall(r'^\s*(?:/\*.*?\*/)?\s*(T\w+)(?:\s*=\s*(\w+))?,?', blk, re.M):
        try: val = int(v, 0) if v else val + 1
        except ValueError: continue
        enums[name] = val

def trans(x):
    c = re.match(r'TRANS_TYPE_CIRCLE\((\w+),\s*(\w+),\s*(\w+)\)', x)
    if c:
        a, col, sp = (enums[g] for g in c.groups())
        return (1 << 5) | ((col & 3) << 3) | ((a & 3) << 1) | (sp & 1)
    return enums[x]

soh = {}                                   # index -> ((scene enum, spawn), field)
for line in EN.splitlines():
    mm = re.match(r'/\* 0x([0-9A-Fa-f]{3}) \*/ DEFINE_ENTRANCE\((.*)\)\s*$', line)
    if mm:
        _, scene, spawn, bgm, card, end, start = split_args(mm.group(2))
        field = ((bgm == 'true') << 15) | ((card == 'true') << 14) | ((trans(end) << 7) & 0x3F80) | (trans(start) & 0x7F)
        soh[int(mm.group(1), 16)] = ((scene, int(spawn)), field)

populated = {v['slot'] for v in json.load(open(PIPE + '/ml_scene_map.json')).values()} | set(m.SCENE_TRANSLATE.values())
dest = lambda e: (m.SCENE_TRANSLATE.get(m.ML[e][0], m.ML[e][0]), m.ML[e][1])

# ---- the ML entrances ML can reach ----
ml = Rom(ML_ROM)
by_slot = {v['slot']: (int(k), v['scene']) for k, v in json.load(open(PIPE + '/ml_scene_map.json')).items()}
used = collections.Counter()
for slot, (dma, name) in by_slot.items():
    d = ml.file(dma)
    hdr, p = [], 0
    while p + 8 <= len(d):
        hdr.append((d[p], struct.unpack('>I', d[p+4:p+8])[0])); p += 8
        if hdr[-1][0] == 0x14: break
    starts = {w & 0xFFFFFF for c, w in hdr if w >> 24 == 2}
    for c, w in hdr:
        if c == 0x13:                                   # exit list (same bounds as ml_entrance.remap_exits)
            ptr = w & 0xFFFFFF
            end = min([x for x in starts if x > ptr], default=len(d))
            for k in range(min(32, (end - ptr) // 2)):
                e = struct.unpack('>H', d[ptr+k*2:ptr+k*2+2])[0]
                if e >= len(m.ML) or dest(e)[0] not in populated: break
                used[e] += 1
    offs = ({o for o, _ in m.ENTRANCE_CS.get(name, []) + m.ACTOR_CS.get(name, [])}
            | {w & 0xFFFFFF for c, w in hdr if c == 0x17})
    for cs in offs:                                     # cutscene destinations (ml_entrance.fix_cutscene_destinations)
        if cs and cs + 8 <= len(d):
            for at in m._cutscene_terminators(d, cs):
                code = struct.unpack('>H', d[at:at+2])[0]
                if code in m.ML_PATCHED_CODES: continue  # ML's own case, reproduced in the engine
                e = m.cs_destination(code)
                if e is not None: used[e] += 1
rom = open(ML_ROM, 'rb').read()
code = get_file(rom, *struct.unpack('>IIII', rom[0x12F70 + 16 * 114:0x12F70 + 16 * 114 + 16]))   # ML's `code` (dma 114)
dma = [struct.unpack('>IIII', rom[0x12F70 + 16 * i:0x12F70 + 16 * i + 16]) for i in range(1600)]
vrom = struct.unpack('>I', code[0xF9440 + 0x9B * 0x20:0xF9440 + 0x9B * 0x20 + 4])[0]   # Door_Ana (actor 0x9B)
door_ana = get_file(rom, *next(e for e in dma if e[0] == vrom))
o = 0x5FC
while struct.unpack('>h', door_ana[o:o+2])[0] != -1:                                  # ML's grotto table
    used[struct.unpack('>h', door_ana[o:o+2])[0]] += 1; o += 2
for f in glob.glob(SOH + '/src/**/*.c', recursive=True):                              # engine literals
    used.update(int(x, 16) for x in re.findall(r'MlEntrance_ToSoh\((0x[0-9A-Fa-f]+)\)', open(f, errors='ignore').read()))

want = collections.defaultdict(dict)       # soh index -> {flags: [ml entrances]}
e = 0
while e < len(m.ML):
    g = e
    while g + 1 < len(m.ML) and dest(g + 1) == dest(e):
        g += 1
    v = m.REV.get(dest(e))
    if dest(e)[0] in populated and v is not None and any(used[x] for x in range(e, g + 1)):
        for k in range(g - e + 1):
            if v + k not in soh or soh[v + k][0] != soh[v][0]:
                break
            want[v + k].setdefault(m.ML[e + k][2], []).append((e + k, sum(used[x] for x in range(e, g + 1))))
    e = g + 1

FIELDS = (0x8000, 0x4000, 0x3F80, 0x007F)      # continue BGM, title card, end transition, start transition
def merge(v, flags):
    """Per field: ML's value where every candidate agrees, SoH's where they don't."""
    out = 0
    for mask in FIELDS:
        vals = {f & mask for f in flags}
        out |= vals.pop() if len(vals) == 1 else soh[v][1] & mask
    return out
chosen, notes = {}, []
for v, w in sorted(want.items()):
    ranked = sorted(((sum(r for _, r in es), f, es) for f, es in w.items()), reverse=True)
    if len(ranked) > 1 and ranked[0][0] == ranked[1][0]:
        tied = [f for n, f, _ in ranked if n == ranked[0][0]]
        f = merge(v, tied)
        notes.append(f'SoH {v:#05x}: tie between {[hex(t) for t in tied]} (ML {[hex(es[0][0]) for n, _, es in ranked]}), '
                     f'merged to {f:#06x} (disputed fields keep SoH\'s)')
        chosen[v] = (f, ranked[0][2][0][0])
        continue
    if len(ranked) > 1:
        notes.append(f'SoH {v:#05x}: {ranked[0][1]:#06x} (ML {ranked[0][2][0][0]:#05x}, {ranked[0][0]} refs) over '
                     + ', '.join(f'{f:#06x} (ML {es[0][0]:#05x}, {n} refs)' for n, f, es in ranked[1:]))
    chosen[v] = (ranked[0][1], ranked[0][2][0][0])
for n in notes:
    print('conflict:', n)
override = {v: f for v, (f, _) in chosen.items() if f != soh[v][1]}
rows = '\n'.join(f'    {{ 0x{v:03X}, 0x{f:04X} }}, // was 0x{soh[v][1]:04X}, ML 0x{chosen[v][1]:03X}'
                 for v, f in sorted(override.items()))
open(OUT, 'w').write(f'''// GENERATED by MissingShip/pipeline/gen_entrance_flags.py -- do not edit by hand.
// MissingShip: ML's entrance flags (music continuity, title card, transitions) for the SoH entrances ML's exits and
// entrances land on (MS-027). {len(override)} of the {len(want)} SoH entrances ML uses differ from SoH's table.
#include "ml_entrance_flags.h"

static const struct {{
    u16 index;
    u16 field;
}} sMlEntranceFlags[] = {{
{rows}
}};

void MlEntranceFlags_Apply(void) {{
    for (s32 i = 0; i < ARRAY_COUNT(sMlEntranceFlags); i++) {{
        gEntranceTable[sMlEntranceFlags[i].index].field = sMlEntranceFlags[i].field;
    }}
}}
''')
print(f'wrote {OUT}: {len(used)} ML entrances reachable; {len(override)} of the {len(want)} SoH entrances they '
      f'land on take ML\'s flags')
