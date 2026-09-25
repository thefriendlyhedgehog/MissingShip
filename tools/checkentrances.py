"""Find ML entrances that ported code tells apart but SoH cannot (MS-095).

The pipeline maps ML's entrance table onto SoH's by (scene, spawn) (gen_entrance_map.py), so two ML entrance groups
that differ only in flags land on one SoH entrance. That matters only where ML's code compares the entrance: every such
comparison is ported as `== MlEntrance_ToSoh(0x...)`. For each compared ML entrance L this lists every other route
that arrives at the same SoH entrance: scene exits (every header), cutscene destinations, ML's grotto table, and ported
code that sets an entrance. A route from outside L's group would trigger L's behaviour too.

A collision that is known to be harmless goes in pipeline/entrance_collisions.json with the reason; anything else fails.
It also fails on a vanilla `ENTR_` comparison that an ML route from another group reaches (MS-120), and on any raw
SoH `ENTR_` constant in ported ML code (ovl_Ml_*, code/ml_*.c, lines marked MissingShip): several
ML entrances can share one SoH entrance, so the code must say which ML one it means (MS-148). Documented exceptions go
under "raw" in entrance_collisions.json.
Usage: checkentrances.py [-v]
"""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..', 'pipeline'))
from mspaths import PIPE, SHIPWRIGHT, SOH
import collections, glob, json, re, struct, sys
sys.path.insert(0, PIPE)
import ml_entrance as m
import hack
from ovldiff import load_pair

ML = hack.HACK
src_c = open(SOH + '/src/code/ml_entrance_map.c').read()
TO_SOH = [int(x, 16) for x in re.findall(r'0x([0-9A-F]{3})', src_c[src_c.index('sMlToSoh'):src_c.index('sSohToMl')])]
GROUP = {0x420: 8}                     # entrance groups longer than the usual 4 layers (gen_entrance_map.OVERRIDE)

def same_group(base, e):
    return base <= e < base + GROUP.get(base, 4)

# ---- routes into each SoH entrance: (ML entrance, where it comes from) ------------------------------------------
routes = collections.defaultdict(set)
def add(e, why):
    if 0 <= e < len(TO_SOH):
        routes[TO_SOH[e]].add((e, why))

sm = json.load(open(PIPE + '/ml_scene_map.json'))
populated = {v['slot'] for v in sm.values()} | set(m.SCENE_TRANSLATE.values())
def commands(d, h):
    o = h
    while o + 8 <= len(d) and d[o] != 0x14:
        yield o, d[o], struct.unpack('>I', d[o+4:o+8])[0]
        o += 8
def headers(d):
    out = [0]
    for o, c, w in commands(d, 0):
        if c == 0x18:
            lst = w & 0xFFFFFF
            for k in range(32):
                if lst + 4 * k + 4 > len(d): break
                p = struct.unpack('>I', d[lst+4*k:lst+4*k+4])[0]
                if p and p >> 24 == 2 and (p & 0xFFFFFF) < len(d): out.append(p & 0xFFFFFF)
    return out
for k, v in sm.items():
    d = ML.file(int(k)); name = v['scene'][:-6]
    for h in headers(d):
        cmds = list(commands(d, h))
        ptrs = sorted({w & 0xFFFFFF for _, _, w in cmds if w >> 24 == 2})
        for _, c, w in cmds:
            if c == 0x13:                                          # exit list, bounded like ml_entrance.remap_exits
                p = w & 0xFFFFFF; end = min([x for x in ptrs if x > p], default=len(d))
                for i in range(min(32, (end - p) // 2)):
                    e = struct.unpack('>H', d[p+2*i:p+2*i+2])[0]
                    if e >= len(m.ML) or m.SCENE_TRANSLATE.get(m.ML[e][0], m.ML[e][0]) not in populated: break
                    add(e, f'exit {i} of {name}')
            if c == 0x17:                                          # header cutscene
                for at in m._cutscene_terminators(d, w & 0xFFFFFF):
                    code = struct.unpack('>H', d[at:at+2])[0]
                    e = m.cs_destination(code)
                    if e is not None: add(e, f'cutscene destination {code} in {name}')
    for cs, _ in m.ENTRANCE_CS.get(name, []) + m.ACTOR_CS.get(name, []):
        for at in m._cutscene_terminators(d, cs):
            code = struct.unpack('>H', d[at:at+2])[0]
            e = m.cs_destination(code)
            if e is not None: add(e, f'cutscene destination {code} in {name} @{cs:#x}')

d = load_pair(0x9B)[0][0]                    # ML's Door_Ana overlay: its grotto table is at file offset 0x5FC, ends at -1
for i in range(32):
    e = struct.unpack('>h', d[0x5FC + 2*i:0x5FE + 2*i])[0]
    if e < 0: break
    add(e, f'grotto table entry {i}')

sets, compares = [], []
for f in glob.glob(SOH + '/src/**/*.c', recursive=True):
    for n, l in enumerate(open(f, errors='replace'), 1):
        for mm in re.finditer(r'(==|!=|=)\s*MlEntrance_ToSoh\((0x[0-9A-Fa-f]+)\)', l):
            e = int(mm.group(2), 16); where = f'{_os.path.relpath(f, SOH)}:{n}'
            if mm.group(1) == '=': sets.append(e); add(e, f'set by {where}')
            else: compares.append((e, where))

# ---- raw SoH ENTR_ constants in ML-ported code (MS-148) ------------------------------------------------------
# ML's code means its own entrance numbers; a raw SoH constant in ported code is only right where SoH's entrance and
# the ML entrance that maps to it are one and the same group. Vanilla code's literals are checked separately below
# (MS-120); ported code is anything in ovl_Ml_* or code/ml_*.c, and lines marked "MissingShip".
allowed = json.load(open(PIPE + '/entrance_collisions.json'))
ENTR = {}
for l in open(SOH + '/include/tables/entrance_table.h'):
    mm = re.match(r'/\* (0x[0-9A-F]+) \*/ DEFINE_ENTRANCE\((\w+)', l)
    if mm: ENTR[mm.group(2)] = int(mm.group(1), 16)
raw_bad = []
for f in glob.glob(SOH + '/src/**/*.c', recursive=True):
    ported = '/ovl_Ml_' in f or _os.path.basename(f).startswith('ml_')
    for n, l in enumerate(open(f, errors='replace'), 1):
        if not (ported or 'MissingShip' in l) or 'MlEntrance_' in l or l.lstrip().startswith('//'):
            continue
        for mm in re.finditer(r'\bENTR_\w+', l):
            v = ENTR.get(mm.group(0))
            if v is None or mm.group(0).startswith('ENTR_MISSINGSHIP'): continue
            where = f'{_os.path.relpath(f, SOH)}:{n}'
            if f'{where} {mm.group(0)}' in allowed.get('raw', {}): continue
            ml = [e for e, s in enumerate(TO_SOH) if s == v]
            raw_bad.append(f'{where}: {mm.group(0)} ({v:#05x}) is where ML ' + (', '.join(f'{e:#05x}' for e in ml[:6])
                           + ('...' if len(ml) > 6 else '') if ml else 'nothing') + ' lands -- say which ML entrance'
                           ' with MlEntrance_ToSoh, or document it under "raw" in entrance_collisions.json')
for r in raw_bad: print('RAW ENTRANCE', r)

# ---- vanilla code's ENTR_ comparisons (MS-120) -----------------------------------------------------------------
# Vanilla code runs in ML too, where the same literal compared ML's own entrance number. The port maps ML entrances
# onto SoH's by (scene, spawn), so ML routes from other groups can now reach a vanilla check that ML's never did: ML's
# Deku Grove upper grotto (0x120) landed on SoH 0x1E1 and froze Link under vanilla's Requiem trigger. A comparison is
# flagged when an ML route from outside the literal's own group arrives at it; document harmless ones under "vanilla"
# (keyed by file and constant) in entrance_collisions.json. Code under #if 0 and randomizer code are skipped.
vanilla_bad = []
for f in sorted(glob.glob(SOH + '/src/**/*.c', recursive=True) + glob.glob(SOH + '/soh/**/*.cpp', recursive=True)):
    rel = _os.path.relpath(f, SOH)
    if 'andomizer' in rel or '/ovl_Ml_' in f or _os.path.basename(f).startswith('ml_'):
        continue
    skip = 0
    for n, l in enumerate(open(f, errors='replace'), 1):
        s = l.strip()
        if skip:
            skip += s.startswith('#if') - s.startswith('#endif')
            continue
        if s.startswith('#if 0'):
            skip = 1; continue
        if s.startswith('//') or 'MlEntrance_' in l or not re.search(r'==|!=|\bcase\b', l):
            continue
        for mm in re.finditer(r'\bENTR_\w+', l):
            v = ENTR.get(mm.group(0))
            if v is None or mm.group(0).startswith('ENTR_MISSINGSHIP'): continue
            others = sorted(e for e, _ in routes.get(v, ()) if not same_group(v, e))
            if others and f'{rel} {mm.group(0)}' not in allowed.get('vanilla', {}):
                vanilla_bad.append(f'{rel}:{n}: {mm.group(0)} ({v:#05x}) is also reached by ML '
                                   + ', '.join(f'{e:#05x}' for e in others[:6]) + ' -- ML compared its own '
                                   f'{v:#05x}; guard it or document it under "vanilla" in entrance_collisions.json')
for r in vanilla_bad: print('VANILLA ENTRANCE', r)

if '-v' in sys.argv:
    kinds = collections.Counter(why.split()[0] for s in routes.values() for _, why in s)
    print(f'routes into {len(routes)} SoH entrances: ' + ', '.join(f'{n} {k}' for k, n in kinds.most_common()))
# ---- report -------------------------------------------------------------------------------------------------
allowed = json.load(open(PIPE + '/entrance_collisions.json'))
bad = 0
for base, where in sorted(set(compares)):
    soh = TO_SOH[base]
    others = sorted((e, why) for e, why in routes[soh] if not same_group(base, e))
    status = 'ok' if not others else ('documented' if f'{base:#05x}' in allowed else 'COLLISION')
    if status == 'COLLISION': bad += 1
    if others or '-v' in sys.argv:
        print(f'ML {base:#05x} (SoH {soh:#05x}) compared at {where}: {status}')
        for e, why in others: print(f'    also arrives: ML {e:#05x} via {why}')
        if status == 'documented': print(f'    documented: {allowed[f"{base:#05x}"]}')
bad += len(raw_bad) + len(vanilla_bad)
print(f'{len(set(b for b, _ in compares))} compared entrances, {bad} undocumented collisions')
sys.exit(1 if bad else 0)
