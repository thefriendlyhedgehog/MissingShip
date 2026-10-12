"""ML's scene data for a stock SoH: Unbound, for tools/unbound_layer.py --stock (docs/ROADMAP.md step 5).

Stock Unbound (roborich's, 0.11, and skijer's mod-API branch on it) reads unbound/scenes.json only for new scenes:
an entry keyed by a vanilla scene is refused ("has no scene path"), so the entrances ML adds to vanilla scenes do not
exist there, and a scene whose exits name one fails to load (the game falls back to Dodongo's Cavern). ML has no new
scenes, so the stock layer ships no unbound/scenes.json, and each exit to an entrance ML adds is rewritten into what
stock Unbound can follow:
  - a vanilla entrance into the same scene that lands on the same entrance-list slot, when there is one;
  - else a vanilla entrance group into that scene that ML's exits never use, whose slot no used entrance lands on,
    and which the game's code never names: that slot of the scene's entrance list (each setup's `entrances`, the
    list the entrance table's spawn indexes, as in vanilla) is pointed where ML's entrance lands;
  - else the scene's first used entrance: the scene loads, but Link arrives at that entrance, not ML's.
ML's actors' own collision (objects/*, LOCO resources) has no camera data, as in ML's ROM; vanilla's always has at
least one entry. Standing on such a collision makes stock Unbound's camera read the missing list (z_bgcheck.c's
func_80041A4C) and crash; our branch checks for it (row B1). The stock layer gives each one empty entries (setting 0,
which the camera reads as "none", as our branch's check returns) up to the highest index its surfaces name.
What else the vanilla-scene entries carried is not read by stock Unbound and is counted in the report: scene names,
restrictions, save return points, ML's flags on vanilla entrances (transitions, music, title cards), intro cutscenes,
game-over continue points, and the entrances only ML's other registries name. ML's scene draw configs are not read
either, but a room drawn with the wrong one crashes the game, so ML's code mod sets them; the report lists them.
"""
import glob, os, re, struct
from collections import defaultdict

from unbound_ml_world import FORK_FIRST


def scene_files(unbound_src):
    """Vanilla scene enum -> its scene document's path in the layer (scene_table.h's file name without _scene)."""
    src = open(os.path.join(unbound_src, 'soh', 'include', 'tables', 'scene_table.h')).read()
    return {enum: f'scenes/{name[:-len("_scene")]}/scene.json'
            for name, enum in re.findall(r'DEFINE_SCENE\((\w+_scene),\s*\w+,\s*(SCENE_\w+)', src)}


CODE_NAMES = 'entrance_names_in_code.txt'    # the frozen builder's copy of _code_names (it ships no sources)


def code_names(unbound_src):
    """Every ENTR_ name the game's code uses outside the tables that define them."""
    cached = os.path.join(unbound_src, CODE_NAMES)
    if os.path.exists(cached):
        with open(cached) as f:
            return set(f.read().split())
    names = set()
    for ext in ('c', 'cpp', 'h'):
        for path in glob.glob(os.path.join(unbound_src, 'soh', '**', f'*.{ext}'), recursive=True):
            if os.sep + 'tables' + os.sep in path:
                continue
            with open(path, errors='replace') as f:
                names.update(re.findall(r'\bENTR_\w+', f.read()))
    return names


def _runs(table):
    """The vanilla table as runs of consecutive entries into one scene and slot: (first, last, scene, slot)."""
    runs = []
    for i in sorted(i for i in table if i < FORK_FIRST):
        name, scene, slot = table[i][:3]
        if runs and runs[-1][1] == i - 1 and runs[-1][2:] == (scene, slot):
            runs[-1] = (runs[-1][0], i, scene, slot)
        else:
            runs.append((i, i, scene, slot))
    return runs


def stock_scenes(docs, world, unbound_src):
    """Rewrite `docs` (layer path -> scene document) in place for stock Unbound; return the report and the entrances
    whose exits were rewritten."""
    table, files = world.table, scene_files(unbound_src)
    exits = [(setup['exits'], key) for doc in docs.values() for setup in (doc.get('setups') or {}).values()
             for key in (setup.get('exits') or {})]
    used = {e[k] for e, k in exits if isinstance(e[k], int) and e[k] < FORK_FIRST}
    used_slots = {table[i][1:3] for i in used}
    code = code_names(unbound_src)
    runs = _runs(table)
    report = defaultdict(list)
    chosen = {}

    def resolve(target):
        scene, key = target.split('/', 1)
        slot = world.scenes[scene]['entrances'][key].get('spawn', 0)
        same = [r for r in runs if r[2:] == (scene, slot)]
        if same:
            report['same slot'].append(f'{target} -> {table[same[0][0]][0]}')
            return same[0][0]
        doc = docs.get(files.get(scene))
        free = [r for r in runs if r[2] == scene and (scene, r[3]) not in used_slots
                and not any(r[0] <= i <= r[1] for i in used)
                and not any(table[i][0] in code for i in range(r[0], r[1] + 1))]
        if doc is not None and free:
            first, _, _, spare = free[0]
            for setup in (doc.get('setups') or {}).values():
                entrances = setup.get('entrances')
                if entrances and str(slot) in entrances:
                    for gap in range(len(entrances), spare):
                        entrances[str(gap)] = dict(entrances['0'])
                    entrances[str(spare)] = dict(entrances[str(slot)])
            used_slots.add((scene, spare))
            used.update(range(first, free[0][1] + 1))
            report['vanilla entrance reused'].append(f'{target} -> {table[first][0]} (slot {spare} -> {slot})')
            return first
        main = min((i for i in used if table[i][1] == scene), default=None)
        if main is None:
            main = next(r[0] for r in runs if r[2] == scene)
        report['lands at the scene entrance'].append(f'{target} -> {table[main][0]}')
        return main

    for e, k in exits:
        if isinstance(e[k], str):
            if e[k] not in chosen:
                chosen[e[k]] = resolve(e[k])
            e[k] = chosen[e[k]]
    return dict(report), set(chosen)


def not_read(world, rewritten):
    """What ML's entries for vanilla scenes carry that stock Unbound does not read, once the layer has added all of it
    (names, restrictions and intros come after the scene documents); `rewritten` are the entrances exits now reach."""
    report, vanilla = {}, world.scenes
    for field in ('name', 'restrictions', 'saveReturn'):
        report[f'not read: {field}'] = sorted(s for s, v in vanilla.items() if field in v)
    # without ML's draw config some rooms draw through an unset segment and the game crashes on arrival: ML's code mod
    # sets these (skijer's branch, mod-sdk/mods/missinglink/ml_scenes.c, the same table)
    report['draw configs left to the code mod'] = sorted(f'{s} {v["drawConfig"]}' for s, v in vanilla.items()
                                                         if 'drawConfig' in v)
    overrides = [(s, n, v) for s, d in vanilla.items() for n, v in d.get('entrances', {}).items()]
    report['not read: entrance flags'] = [f'{s}/{n}' for s, n, v in overrides if n.startswith('ENTR_')]
    report['not read: intros'] = [f'{s}/{n}' for s, n, v in overrides if 'intro' in v]
    report['not read: continue points'] = [f'{s}/{n}' for s, n, v in overrides if 'continueAt' in v]
    report['not read: entrances only other registries name'] = sorted(
        f'{s}/{n}' for s, n, v in overrides if not n.startswith('ENTR_') and f'{s}/{n}' not in rewritten)
    return report


def with_camera_entries(data):
    """An object collision resource (LOCO) with no camera data, given empty entries up to the highest camera index its
    surfaces name; None for any other file or one that already has camera data. Layout:
    soh/soh/resource/importer/CollisionHeaderFactory.cpp (header 0x40, bounds, vertices, polygons, surface types,
    then the camera data count and entries of u16 setting, s16 camera count, s32 position index)."""
    if len(data) < 0x50 or data[4:8] != b'LOCO':
        return None
    o = 0x40 + 12
    o += 4 + struct.unpack_from('<i', data, o)[0] * 6
    o += 4 + struct.unpack_from('<I', data, o)[0] * 16
    count = struct.unpack_from('<I', data, o)[0]
    top = max((struct.unpack_from('<I', data, o + 4 + i * 8 + 4)[0] & 0xFF for i in range(count)), default=0)
    o += 4 + count * 8
    if struct.unpack_from('<I', data, o)[0] != 0:
        return None
    entries = top + 1
    return data[:o] + struct.pack('<I', entries) + struct.pack('<HhI', 0, 0, 0) * entries + data[o + 4:]

