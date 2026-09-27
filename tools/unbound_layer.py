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
    SPEC §7.3, ML_TYPES below), and ML's placements of that actor are renamed to the type.
The result goes in Unbound's mods/ folder beside an ordinary oot.o2r. It carries ML's scenes, text and assets, not
the behaviour our SoH fork adds in C.
"""
import argparse, json, os, shutil, subprocess, sys, tempfile, zipfile

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


def rename_actors(doc, stats):
    """ML's placements of a vanilla actor ML_TYPES extends, placed as the type instead."""
    for setup in (doc.get('setups') or {}).values():
        for actor in (setup.get('actors') or {}).values():
            if actor.get('id') in RENAMES:
                actor['id'] = RENAMES[actor['id']]
                stats['actor types placed'] += 1


def layer(vanilla, ml, out):
    stats = dict.fromkeys(('added', 'changed', 'scene documents', 'messages', 'actor types placed'), 0)
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
        z.writestr('unbound/actors.json', json.dumps({k: t['type'] for k, t in ML_TYPES.items()}, indent=1))
    return stats


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--unbound', required=True, help="Unbound's soh binary (build-*/soh/soh-macos, soh.elf, soh.exe)")
    ap.add_argument('--soh-o2r', help="Unbound's soh.o2r (default: next to the binary)")
    ap.add_argument('--oot-o2r', required=True, help='the oot.o2r ML was built against')
    ap.add_argument('--mod', default=os.path.join(ROOT, 'mod', 'MissingLink_Scenes.o2r'))
    ap.add_argument('--out', default=os.path.join(ROOT, 'mod', 'MissingLink_Unbound.o2r'))
    args = ap.parse_args()
    # the converter runs in a temporary folder, so every path must be absolute
    args.unbound, args.oot_o2r, args.mod, args.out = map(os.path.abspath, (args.unbound, args.oot_o2r, args.mod,
                                                                           args.out))
    soh_o2r = args.soh_o2r or os.path.join(os.path.dirname(os.path.abspath(args.unbound)), 'soh.o2r')
    work = tempfile.mkdtemp(prefix='missingship-unbound-')
    try:
        merged = os.path.join(work, 'oot-ml.o2r')
        merged_archive(args.oot_o2r, args.mod, merged)
        export(args.unbound, soh_o2r, args.oot_o2r, os.path.join(work, 'vanilla.o2r'), work)
        export(args.unbound, soh_o2r, merged, os.path.join(work, 'ml.o2r'), work)
        stats = layer(os.path.join(work, 'vanilla.o2r'), os.path.join(work, 'ml.o2r'), args.out)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print(f'{args.out}: ' + ', '.join(f'{v} {k}' for k, v in stats.items()))


if __name__ == '__main__':
    main()
