"""Find ML entrances that ported code tells apart but SoH cannot (MS-095).

The pipeline maps ML's entrance table onto SoH's by (scene, spawn) (gen_entrance_map.py), so two ML entrance groups
that differ only in flags land on one SoH entrance. That matters only where ML's code compares the entrance: every such
comparison is ported as `== MlEntrance_ToSoh(0x...)`. For each compared ML entrance L this lists every other route
that arrives at the same SoH entrance: scene exits (every header), cutscene destinations, ML's grotto table, and ported
code that sets an entrance. A route from outside L's group would trigger L's behaviour too.

A collision that is known to be harmless goes in pipeline/entrance_collisions.json with the reason; anything else fails.
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
                    e = 0x420 if code in m.ML_PATCHED_CODES else m.CODE_TO_ENTRANCE.get(code)
                    if e is not None: add(e, f'cutscene destination {code} in {name}')
    for cs, _ in m.ENTRANCE_CS.get(name, []) + m.ACTOR_CS.get(name, []):
        for at in m._cutscene_terminators(d, cs):
            code = struct.unpack('>H', d[at:at+2])[0]
            e = 0x420 if code in m.ML_PATCHED_CODES else m.CODE_TO_ENTRANCE.get(code)
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
print(f'{len(set(b for b, _ in compares))} compared entrances, {bad} undocumented collisions')
sys.exit(1 if bad else 0)
