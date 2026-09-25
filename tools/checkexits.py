"""Validate every ML scene exit against the SoH entrance it is remapped to.

For each ML scene's exit list (scene header command 0x13, main header), the ML entrance is looked up in ML's
entrance table -> (scene slot, spawn); the pipeline maps that to a SoH entrance (ml_entrance.REV). Checks: the
SoH entrance exists, its scene is the same slot and spawn, and the destination ML scene actually has that spawn.
Exit lists have no length field, so entries are read up to the next header data block (bounded by the smallest
following segment offset) and anything >= 0x800 or beyond ML's table is reported as padding, not an error.
"""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..', 'pipeline'))
from mspaths import ML_ROM, PIPE, SOH
import struct, sys, re, json
sys.path.insert(0, PIPE)
from mlrom import Rom
import ml_entrance as m
ml = Rom(ML_ROM)
rows = {}
for l in open(SOH + '/include/tables/entrance_table.h'):
    mm = re.match(r'/\* 0x([0-9A-F]+) \*/ DEFINE_ENTRANCE\((\w+), (\w+), (\d+)', l)
    if mm: rows[int(mm.group(1), 16)] = (mm.group(2), mm.group(3), int(mm.group(4)))
scenes = {}
for l in open(SOH + '/include/tables/scene_table.h'):
    mm = re.match(r'/\* 0x([0-9A-F]+) \*/ DEFINE_SCENE\(\w+, \w+, (\w+)', l)
    if mm: scenes[mm.group(2)] = int(mm.group(1), 16)
sm = json.load(open(PIPE + '/ml_scene_map.json'))
by_slot = {v['slot']: (int(k), v['scene']) for k, v in sm.items()}
def headers(d):
    out = []; p = 0
    while p + 8 <= len(d):
        c, a, w = d[p], d[p+1], struct.unpack('>I', d[p+4:p+8])[0]; out.append((c, a, w))
        if c == 0x14: break
        p += 8
    return out
def spawn_count(slot):
    if slot not in by_slot: return None
    h = headers(ml.file(by_slot[slot][0]))
    return next((a for c, a, w in h if c == 0x00), None)
populated = set(by_slot) | set(m.SCENE_TRANSLATE.values())
problems = 0; checked = 0; zero_gaps = []
for slot, (dma, name) in sorted(by_slot.items()):
    d = ml.file(dma); h = headers(d)
    for c, a, w in h:
        if c != 0x13: continue
        ptr = w & 0xFFFFFF
        # the pipeline's own bound (ml_entrance.remap_exits): stop at a non-ML-scene target, max 32
        end = min([x & 0xFFFFFF for cc, aa, x in h if (x >> 24) == 2 and (x & 0xFFFFFF) > ptr], default=len(d))
        entries = []
        for k in range(min(32, (end - ptr) // 2)):
            e = struct.unpack('>H', d[ptr+k*2:ptr+k*2+2])[0]
            if e >= len(m.ML): break
            sc = m.SCENE_TRANSLATE.get(m.ML[e][0], m.ML[e][0])
            if sc not in populated: break
            entries.append(e)
        # end-to-end: the pipeline's remap_exits must turn every entry into the expected SoH entrance
        out, _, _ = m.remap_exits(bytes(d), populated)
        got = [struct.unpack('>H', out[ptr+k*2:ptr+k*2+2])[0] for k in range(len(entries))]
        want = [m.REV.get((m.SCENE_TRANSLATE.get(m.ML[e][0], m.ML[e][0]), m.ML[e][1])) for e in entries]
        bad = [(k, hex(g), hex(wv) if wv is not None else None) for k, (g, wv) in enumerate(zip(got, want)) if wv is not None and g != wv]
        if bad:
            zero_gaps.append((name, bad))
        for i, e in enumerate(entries):
            checked += 1
            sc, sp, _ = m.ML[e]; sc = m.SCENE_TRANSLATE.get(sc, sc); v = m.REV.get((sc, sp)); r = rows.get(v)
            msg = None
            if v is None: msg = 'no exact SoH entrance -> pipeline falls back to the scene\'s lowest entrance'
            elif scenes.get(r[1]) != sc or r[2] != sp: msg = f'maps to {r} -- wrong scene/spawn'
            else:
                n = spawn_count(sc)
                if n is not None and sp >= n: msg = f'spawn {sp} but destination has only {n} spawns'
            if msg:
                problems += 1
                print(f'{name:22} exit {i}: ML {e:#05x} -> {by_slot.get(sc,(0,hex(sc)))[1]} spawn {sp}: {msg}')
print(f'{checked} exits checked, {problems} problems')
for name, entries in zero_gaps:
    print(f'NOT REMAPPED {name}: (exit, got, want) {entries}')
