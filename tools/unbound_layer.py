"""ML as an SoH: Unbound mod layer (step 1 of "ML on Unbound", docs/ROADMAP.md).

    python3 tools/unbound_layer.py --unbound <Unbound's soh binary> --oot-o2r <oot.o2r> --mod <MissingLink_Scenes.o2r>
                                   [--soh-o2r <Unbound's soh.o2r>] [--out <MissingLink_Unbound.o2r>]

Unbound reads vanilla scenes and text only in its own JSON form, converted from oot.o2r at start-up, so ML's
vanilla-format scene overrides are not read there. This converts them with Unbound's own converter
(`--export-unbound`): once for oot.o2r alone, once for oot.o2r with ML's files on top, and keeps what ML adds or changes:
  - scene documents ML changes carry `"$replace": true` (SPEC §3.4), so ML's scene replaces vanilla's instead of
    merging key by key;
  - text/<lang>/messages.json keeps only the ids ML changes or adds (a keyed list merges per id);
  - every other changed or added file is copied whole;
  - ML's changes to a vanilla actor that need no code become actor types that extend it (`base`, Unbound
    SPEC §7.3, ML_TYPES below), and ML's placements of that actor are renamed to the type;
  - ML's changes to a vanilla actor's InitVars, which reach every copy of it, become overrides of that actor
    (Unbound SPEC §7.4, ML_OVERRIDES below);
  - ML's story wiring (entrances, intro cutscenes, cutscene destinations, the new save) becomes unbound/scenes.json,
    unbound/cutscenes.json and unbound/save.json (Unbound SPEC §7.1, §7.5, §7.6; tools/unbound_ml_world.py), and exits
    to the entrances our fork adds use their names;
  - what SoH's UI calls ML's items, areas and events, and ML's Time Splits list, become unbound/labels.json and the
    vanilla scenes' `name` (Unbound SPEC §7.7, §7.1; tools/unbound_ml_ui.py).
The result goes in Unbound's mods/ folder beside an ordinary oot.o2r. It carries ML's scenes, text and assets, not
the behaviour our SoH fork adds in C.
"""
import argparse, json, os, shutil, subprocess, sys, tempfile, zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from unbound_ml_ui import MlUi
from unbound_ml_world import MlWorld

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKIP = ('version', 'portVersion', 'unbound.json')     # archive metadata: the base provides it
MANIFEST = {'format': 'unbound', 'formatVersion': 2, 'requires': {'formatVersion': 2}}   # `base` is format 2

# ML's changes to vanilla actors, as types that extend them (docs/UNBOUND_MAP.md, C). Each is placed wherever ML's
# own scenes place the vanilla actor `replaces` (by its vanilla number); vanilla scenes keep the vanilla actor.
HIDAN = 'objects/object_hidan_objects/gFireTemple'
ML_TYPES = {
    # Ledger row 38: ML's breakable wall draws one model and collision of its own for all three wall types, from
    # object 0xA3 (object_ml_crackwall), which ML's rooms load instead of the Fire Temple's objects.
    'ml/crack_wall': {'replaces': 0xCF, 'type': {
        'name': 'Missing Link: breakable wall',
        'base': 'Bg_Hidan_Kowarerukabe',
        'object': 0xA3,
        'assets': {**{f'{HIDAN}{w}DL': 'objects/object_ml_crackwall/gMlCrackWallDL'
                      for w in ('CrackedStoneFloor', 'BombableWall', 'LargeBombableWall')},
                   **{f'{HIDAN}{w}Col': 'objects/object_ml_crackwall/gMlCrackWallCol'
                      for w in ('CrackedStoneFloor', 'BombableWall', 'LargeBombableWall')}}}},
}
RENAMES = {t['replaces']: name for name, t in ML_TYPES.items()}

# ML's changes to vanilla actors' InitVars (docs/UNBOUND_MAP.md, U9). They apply to every copy, including the Skulltulas
# ML's crates and soil patch spawn, so they are overrides of the actor itself rather than types.
HOSTILE, ATTENTION, UPDATE_CULLING_DISABLED = 1 << 2, 1 << 0, 1 << 4    # ACTOR_FLAG_*
ML_OVERRIDES = {
    'En_St': {'flags': {'clear': HOSTILE}},             # row 41: Skulltulas are not enemies (0x35 -> 0x31)
    'En_Sw': {'flags': {'clear': HOSTILE}},             # row 41: nor Skullwalltulas (0x15 -> 0x11)
    'Obj_Bombiwa': {'flags': {'set': UPDATE_CULLING_DISABLED}},   # row 42 (0 -> 0x10)
    'En_Vm': {'flags': {'clear': ATTENTION}},           # row 82: Beamos are never Z-targetable (0x11 -> 0x10)
    'En_Karebaba': {'flags': {'clear': HOSTILE}},       # row 94 (5 -> 1); ML never places it
    'Bg_Treemouth': {'spawn': False},                   # rows 8 and 122: ML guts the Deku Tree's mouth
}


def merged_archive(oot, mod, out):
    """oot.o2r with the mod's files replacing or joining its own."""
    with zipfile.ZipFile(oot) as o, zipfile.ZipFile(mod) as m, zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        ours = set(m.namelist()) - set(SKIP)             # oot.o2r keeps its own version files
        for info in o.infolist():
            if info.filename not in ours:
                z.writestr(info, o.read(info))
        for info in m.infolist():
            if info.filename not in SKIP:
                z.writestr(info, m.read(info))


def export(unbound, soh_o2r, oot, out, work):
    """Unbound's converter run on `oot` in a throwaway SHIP_HOME."""
    home = tempfile.mkdtemp(dir=work)
    shutil.copy2(soh_o2r, os.path.join(home, 'soh.o2r'))
    shutil.copy2(oot, os.path.join(home, 'oot.o2r'))
    r = subprocess.run([unbound, '--export-unbound', out], cwd=home, env=dict(os.environ, SHIP_HOME=home),
                       capture_output=True, text=True, timeout=600)
    if r.returncode != 0 or not os.path.exists(out):
        sys.exit(f'Unbound export failed ({r.returncode}); see {home}/logs')


def _is_entrance(value):
    """A number Unbound takes as an exit: a vanilla entrance, or a dynamic return entrance (SPEC.md §4.2)."""
    return value < 0x614 or 0x7FF9 <= value <= 0x7FFF


def rename_exits(doc, world, stats):
    """Exits to an entrance our fork adds, by the name it has in Unbound (only a group's first entry is named).
    An exit list has no length, so the converter reads it up to the next data it knows of; from the first entry
    that is no entrance on, it is that data, not exits (checkexits.py's padding rule). ML's pond is the one case:
    its list is 0x2E00, then texture data, and 0x2E00 is no entrance in ML either."""
    for setup in (doc.get('setups') or {}).values():
        exits = setup.get('exits') or {}
        for key in sorted(exits, key=int):
            value = exits[key]
            if isinstance(value, int) and value in world.fork_names:
                exits[key] = world.fork_names[value]
                stats['exits named'] += 1
            elif isinstance(value, int) and not _is_entrance(value):
                cut = [k for k in exits if int(k) >= int(key)]
                for k in cut:
                    del exits[k]
                stats['exit padding dropped'] += len(cut)
                break


def rename_actors(doc, stats):
    """ML's placements of a vanilla actor ML_TYPES extends, placed as the type instead."""
    for setup in (doc.get('setups') or {}).values():
        for actor in (setup.get('actors') or {}).values():
            if actor.get('id') in RENAMES:
                actor['id'] = RENAMES[actor['id']]
                stats['actor types placed'] += 1


def layer(vanilla, ml, out, world, ui):
    stats = dict.fromkeys(('added', 'changed', 'scene documents', 'messages', 'actor types placed', 'exits named',
                           'exit padding dropped'), 0)
    with zipfile.ZipFile(vanilla) as v, zipfile.ZipFile(ml) as m, zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        base = {i.filename: i.CRC for i in v.infolist()}
        for info in m.infolist():
            n = info.filename
            if n in SKIP or base.get(n) == info.CRC:
                continue
            data = m.read(info)
            if n in base and n.startswith('text/') and n.endswith('/messages.json'):
                old = json.loads(v.read(n))['messages']
                doc = json.loads(data)
                doc['messages'] = {k: val for k, val in doc['messages'].items() if old.get(k) != val}
                stats['messages'] += len(doc['messages'])
                data = json.dumps(doc, indent=1)
            elif n.startswith('scenes/') and n.endswith('.json'):
                doc = json.loads(data)
                rename_actors(doc, stats)
                rename_exits(doc, world, stats)
                if n in base:
                    doc = {'$replace': True, **doc}
                    stats['scene documents'] += 1
                else:
                    stats['added'] += 1
                data = json.dumps(doc, indent=1)
            else:
                stats['changed' if n in base else 'added'] += 1
            z.writestr(n, data)
        z.writestr('unbound.json', json.dumps(MANIFEST, indent=1))
        z.writestr('unbound/actors.json',
                   json.dumps({**ML_OVERRIDES, **{k: t['type'] for k, t in ML_TYPES.items()}}, indent=1))
        world.intros(m.namelist())
        for scene, name in ui.scene_names().items():
            world.scenes.setdefault(scene, {})['name'] = name
        destinations = world.destinations(m)
        z.writestr('unbound/scenes.json', json.dumps(world.scenes, indent=1))
        z.writestr('unbound/cutscenes.json', json.dumps(destinations, indent=1))
        z.writestr('unbound/save.json', json.dumps(world.new_save(), indent=1))
        labels = ui.labels()
        z.writestr('unbound/labels.json', json.dumps(labels, indent=1))
        stats['vanilla scenes changed'] = len(world.scenes)
        stats['cutscene destinations'] = len(destinations)
        stats['item labels'] = len(labels['items'])
        stats['time splits'] = len(labels['timeSplits']['splits'])
    return stats


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--unbound', required=True, help="Unbound's soh binary (build-*/soh/soh-macos, soh.elf, soh.exe)")
    ap.add_argument('--soh-o2r', help="Unbound's soh.o2r (default: next to the binary)")
    ap.add_argument('--unbound-src', help="Unbound's source tree (default: the tree the binary was built in, "
                                          "build-*/soh/<binary>)")
    ap.add_argument('--oot-o2r', required=True, help='the oot.o2r ML was built against')
    ap.add_argument('--mod', default=os.path.join(ROOT, 'mod', 'MissingLink_Scenes.o2r'))
    ap.add_argument('--out', default=os.path.join(ROOT, 'mod', 'MissingLink_Unbound.o2r'))
    args = ap.parse_args()
    # the converter runs in a temporary folder, so every path must be absolute
    args.unbound, args.oot_o2r, args.mod, args.out = map(os.path.abspath, (args.unbound, args.oot_o2r, args.mod,
                                                                           args.out))
    soh_o2r = args.soh_o2r or os.path.join(os.path.dirname(os.path.abspath(args.unbound)), 'soh.o2r')
    unbound_src = args.unbound_src or os.path.dirname(os.path.dirname(os.path.dirname(args.unbound)))
    world, ui = MlWorld(unbound_src), MlUi(unbound_src)
    work = tempfile.mkdtemp(prefix='missingship-unbound-')
    try:
        merged = os.path.join(work, 'oot-ml.o2r')
        merged_archive(args.oot_o2r, args.mod, merged)
        export(args.unbound, soh_o2r, args.oot_o2r, os.path.join(work, 'vanilla.o2r'), work)
        export(args.unbound, soh_o2r, merged, os.path.join(work, 'ml.o2r'), work)
        stats = layer(os.path.join(work, 'vanilla.o2r'), os.path.join(work, 'ml.o2r'), args.out, world, ui)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print(f'{args.out}: ' + ', '.join(f'{v} {k}' for k, v in stats.items()))


if __name__ == '__main__':
    main()
