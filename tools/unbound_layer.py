"""ML as an SoH: Unbound mod layer (step 1 of "ML on Unbound", docs/ROADMAP.md).

    python3 tools/unbound_layer.py --unbound <Unbound's soh binary> --oot-o2r <oot.o2r> --mod <MissingLink_Scenes.o2r>
                                   [--soh-o2r <Unbound's soh.o2r>] [--out <MissingLink_Unbound.o2r>]
                                   [--pack [--also-base <another extractor version's oot.o2r>]... | --stock]

Unbound reads vanilla scenes and text only in its own JSON form, converted from oot.o2r at start-up, so ML's
vanilla-format scene overrides are not read there. This converts them with Unbound's own converter
(`--export-unbound`): once for oot.o2r alone, once for oot.o2r with ML's files on top, and keeps what ML adds or changes:
  - scene documents ML changes carry `"$replace": true` (SPEC §3.4), so ML's scene replaces vanilla's instead of
    merging key by key;
  - text/<lang>/messages.json keeps only the ids ML changes or adds (a keyed list merges per id), plus our own lines
    (OUR_MESSAGES below);
  - every other changed or added file is copied whole;
  - ML's changes to a vanilla actor that need no code become actor types that extend it (`base`, Unbound
    SPEC §7.3, ML_TYPES below), and ML's placements of that actor are renamed to the type;
  - ML's changes to a vanilla actor that need code are actors our Unbound branch registers by name
    (ML_CODE_ACTORS below, Unbound's unbound-docs/missinglink-actors.md), and ML's placements are renamed to them;
  - ML's changes to the game's own code (a vanilla actor the game spawns by number, a player hook) are switched on
    in our Unbound branch by the marker file missinglink/mod.json, which the layer ships;
  - ML's changes to a vanilla actor's InitVars, which reach every copy of it, become overrides of that actor
    (Unbound SPEC §7.4, ML_OVERRIDES below);
  - ML's story wiring (entrances, intro cutscenes, cutscene destinations, the new save) becomes unbound/scenes.json,
    unbound/cutscenes.json and unbound/save.json (Unbound SPEC §7.1, §7.5, §7.6; tools/unbound_ml_world.py), and exits
    to the entrances our fork adds use their names;
  - what SoH's UI calls ML's items, areas and events, and ML's Time Splits list, become unbound/labels.json and the
    vanilla scenes' `name` (Unbound SPEC §7.7, §7.1; tools/unbound_ml_ui.py);
  - ML's dungeon minimaps and pause maps (room textures, compass offsets, floors, palettes, chest and boss marks) and
    its missing overworld minimap become unbound/maps.json (Unbound SPEC §7.8; tools/unbound_ml_maps.py);
  - ML's pause menu (age rules, slot and world-map positions, cursor rules, ammo counts, map points, colours) becomes
    unbound/pause.json (Unbound SPEC §7.9; tools/unbound_ml_pause.py);
  - ML's GetItem models (its Deku Nut Milk, the Soul-Charged Kokiri Sword, a red Blue Potion) become
    unbound/getitem.json (Unbound SPEC §7.11; tools/unbound_ml_getitem.py);
  - ML's shop prices (its cheaper Bombchus) become unbound/shops.json (Unbound SPEC §7.12; tools/unbound_ml_shops.py);
  - ML's item rules (its slot layout, capacities, hidden bracelet, bottle slots, 20 Bombchus) become unbound/items.json
    (Unbound SPEC §7.13), and its scene restriction table each scene's `restrictions` in unbound/scenes.json (§7.1)
    (tools/unbound_ml_items.py).
The result goes in Unbound's mods/ folder beside an ordinary oot.o2r. It carries ML's scenes, text and assets, not
the rest of the behaviour our SoH fork adds in C.

With --pack the result is the layer as a pack of deltas (tools/deltapack.py, Unbound SPEC §1.7): ML's own bytes plus
copies from oot.o2r and Unbound's conversion of it, which every player's game has. It carries none of the game's data,
so it is what we distribute; make it from an oot.o2r that Unbound's own extractor wrote, with --also-base for each
other SoH 9.x extractor's oot.o2r of the same ROM (they lay some files out differently).

With --stock the layer is for a stock Unbound, which reads neither packs nor our branch's entries for vanilla scenes:
it ships no unbound/scenes.json, and exits to the entrances ML adds become vanilla entrances (tools/unbound_stock.py),
with a report of what stock Unbound does not read. For ML's code mod (Unbound-modapi's mods/missinglink), which
carries what stock does not read, it also writes the registries again as Blobs, missinglink/<name>, with the game's
names in them as numbers, puts ML's event entrances on vanilla entrance rows ML never uses, missinglink/entrances,
and writes ML's intro cutscenes by entrance, missinglink/intros.
"""
import argparse, glob, json, os, re, shutil, struct, subprocess, sys, tempfile, zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from unbound_ml_getitem import MlGetItem
from unbound_ml_items import MlItems
from unbound_ml_maps import MlMaps
from unbound_ml_pause import MlPause
from unbound_ml_shops import MlShops
from unbound_ml_ui import MlUi
from unbound_ml_world import MlWorld
from unbound_stock import event_entrances, game_names, not_read, numbered, stock_scenes, with_camera_entries
import deltapack

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'pipeline'))
from mspaths import PIPE, SOH   # the fork's files: engine/soh, or the builder's copy
import mspaths
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
    # Ledger row 25: ML draws its grotto hole from its own gameplay_keep, so it shows in any scene; its destinations and
    # return point are unbound/grottos.json (tools/unbound_ml_world.py).
    'ml/grotto': {'replaces': 0x9B, 'type': {
        'name': 'Missing Link: grotto',
        'base': 'Door_Ana',
        'object': 0x01,                                  # OBJECT_GAMEPLAY_KEEP, vanilla's OBJECT_GAMEPLAY_FIELD_KEEP
        'assets': {'objects/gameplay_field_keep/gGrottoDL': 'objects/object_ml_gkeep/gMlGrottoDL'}}},
}

# ML's changes to vanilla actors that need code, and ML's own new actors (docs/UNBOUND_MAP.md, C: owner decision
# 2026-09-28): actors our Unbound branch registers by name (Unbound's unbound-docs/missinglink-actors.md), keyed by
# the number ML's own scenes place them under. Vanilla scenes keep the vanilla actor.
ML_CODE_ACTORS = {
    0x121: 'ML_EventSpawner',                           # ledger rows 26, 54: ML's rewrite of En_Fz (Freezard)
    0xE7: 'ML_Malon',                                   # ledger row 34: ML's rewrite of En_Ma1 (young Malon)
    0x9C: 'ML_Bg_Spot02_Objects',                       # ledger row 60: ML's Kakariko objects (object 0xA1)
    0x16D: 'ML_En_Md',                                  # ledger row 14: ML's Mido, in its Kokiri Forest
    0x005: 'ML_TreeBridge',                             # ledger row 20: ML's own tree bridge (vanilla's unused 0x005)
    0x003: 'ML_Fishpond',                               # ledger row 10: ML's fishing pond scenery (vanilla's unused 0x003)
    0x006: 'ML_Villager',                               # ledger row 10: ML's Kakariko villagers (vanilla's unused 0x006)
    0x174: 'ML_FogWall',                                # ledger row 26: ML's copy of En_Holl in Demo_Gt's slot
    0x192: 'ML_HintNuts',                               # ledger row 26: ML's rewrite of En_Hintnuts (talking scrubs)
    0xBA: 'ML_BossVa',                                  # ledger row 78: ML's rewrite of Boss_Va (the Shadow Temple boss)
    0x17B: 'ML_Elevator',                               # ledger row 71: ML's rewrite of En_Kakasi (HAKAdan's elevators)
    0x040: 'ML_ElevatorSpawner',                        # ledger row 70: ML's rewrite of Bg_Hidan_Dalm
    0x01A: 'ML_RoomClearSwitch',                        # ledger row 69: ML's room-clear switch (holds switch 0x25)
    0x087: 'ML_HakaLift',                               # ledger row 72: ML's rewrite of Bg_Mori_Elevator
    0x0DC: 'ML_SlideBlock',                             # ledger row 68: ML's rewrite of Boss_Tw (sliding blocks)
    0x12C: 'ML_Lift',                                   # ledger row 12: ML's rewrite of Obj_Lift
    0x0E3: 'ML_LiftBlock',                              # ledger row 12: ML's rewrite of Bg_Mori_Hashira4
    0x119: 'ML_DnsShop',                                # ledger row 27: ML's copy of En_Dns (the grotto shop)
    0x17A: 'ML_PushBlock',                              # ledger row 91: ML's copy of Obj_Oshihiki (Library)
    0x093: 'ML_BgmTrigger',                             # ledger row 63: ML's rewrite of Bg_Po_Event (music triggers)
    0x098: 'ML_Pillar',                                 # ledger row 86: ML's rewrite of En_Du (Bmori1's pillars)
    0x0C3: 'ML_Thief',                                  # ledger row 87: ML's rewrite of En_Nb (Bmori1's thieves)
    0x027: 'ML_Torch',                                  # ledger row 85: ML's copy of Obj_Syokudai in Boss_Dodongo's slot
    0x01C: 'ML_BeamHeart',                              # ledger row 30: ML's Blade Beam heart spawner (En_Reeba's slot)
    0x115: 'ML_SkullKid',                               # ledger row 31: ML's rewrite of En_Skj (Deku Road)
    0x18C: 'ML_HoleScrub',                              # ledger row 33: ML's hole-hopping scrub (En_Weiyer's slot)
    0x06A: 'ML_TalkSpot',                               # ledger row 62: ML's talk spot (Bg_Toki_Hikari's slot)
    0x01F: 'ML_Prologue',                               # ledger row 29: ML's illustrated prologue (vanilla's unused 0x01F)
    0x1D1: 'ML_SongBlock',                              # ledger row 21: ML's rewrite of Obj_Timeblock (Saria's house block)
    0x012: 'ML_Ending',                                 # ledger row 92: ML's ending card (En_Dodongo's slot)
}
ML_MARKER = 'missinglink/mod.json'
# Messages that are ours, not ML's: English message id -> the fork's C++ file that builds the same line (its raw bytes
# as C string literals after `std::string text =`), so the two stay in step. 0xF0A2 (ledger row 127): the grotto
# shop's line once its Piece of Heart is sold, which our Unbound branch shows instead of ML's 0x10A2 (MlEnDns.c).
OUR_MESSAGES = {
    '0xF0A2': os.path.join(SOH, 'soh', 'Enhancements', 'MlScrubShopText.cpp'),
}


def our_message(path):
    """The raw message bytes a fork file builds, as Unbound's text JSON writes them (one code point per byte)."""
    with open(path) as f:
        src = f.read()
    block = src[src.index('std::string text ='):]
    block = block[:block.index(';')]
    raw = ''.join(re.findall(r'"((?:[^"\\]|\\.)*)"', block))
    text = raw.encode('latin-1').decode('unicode_escape')
    return {'box': 0, 'ypos': 0, 'text': text}   # TEXTBOX_TYPE_BLACK, TEXTBOX_POS_VARIABLE, as the fork's line
# ML's title-card ribbon (ML custom code +0x3BE0, 160x8 IA16), which our Unbound branch draws by this path while ML is
# mounted (Unbound's soh/soh/missinglink/MlTitleCard.c). Read from the fork's generated copy.
ML_BANNER = 'textures/ml_titlecard/gMlTitleCardBannerTex'
ML_BANNER_INC = os.path.join(SOH, 'src', 'code', 'ml_titlecard_banner.inc')
RENAMES = {**{t['replaces']: name for name, t in ML_TYPES.items()}, **ML_CODE_ACTORS}

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
    'En_Firefly': {'flags': {'clear': HOSTILE}},        # row 27: Keese are not enemies (0x5005 -> 0x5001)
    'En_Dekubaba': {'flags': {'clear': HOSTILE}},       # row 27: nor Deku Babas (5 -> 1)
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
        logs = sorted(glob.glob(os.path.join(home, 'logs', '*.log')))
        tail = (open(logs[-1], errors='replace').read() if logs else r.stdout + r.stderr).splitlines()[-15:]
        sys.exit('\n'.join(tail + [f'Unbound export failed ({r.returncode})']))


def unbound_game(path):
    """(binary, soh.o2r) of an Unbound install: a macOS app (Contents/MacOS/soh, Contents/Resources/soh.o2r), or a
    binary with soh.o2r beside it (a build folder, Windows, Linux)."""
    path = os.path.abspath(path)
    if os.path.isdir(os.path.join(path, 'Contents', 'MacOS')):
        macos = os.path.join(path, 'Contents', 'MacOS')
        binary = next((os.path.join(macos, n) for n in ('soh', 'soh-macos') if os.path.isfile(os.path.join(macos, n))),
                      None)
        if binary is None:
            sys.exit(f'{path} holds no Ship of Harkinian binary')
        return binary, os.path.join(path, 'Contents', 'Resources', 'soh.o2r')
    return path, os.path.join(os.path.dirname(path), 'soh.o2r')


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
    """ML's placements of a vanilla actor ML_TYPES extends or ML_CODE_ACTORS replaces, placed by that name instead."""
    for setup in (doc.get('setups') or {}).values():
        for actor in (setup.get('actors') or {}).values():
            if actor.get('id') in RENAMES:
                actor['id'] = RENAMES[actor['id']]
                stats['actors placed by name'] += 1


def ml_scenes():
    """The vanilla scene slots ML's ROM has (pipeline/ml_scene_map.json), for the marker file: with ML mounted, our
    Unbound branch refuses an entrance into any other vanilla scene (ledger row 119), since oot.o2r still holds
    vanilla's scenes there."""
    with open(os.path.join(PIPE, 'ml_scene_map.json')) as f:
        scenes = sorted({entry['enum'] for entry in json.load(f).values()})
    if len(scenes) != 25:
        raise SystemExit(f'pipeline/ml_scene_map.json names {len(scenes)} scene slots; ML has 25')
    return scenes


def ml_banner(vanilla):
    """ML's ribbon as a texture resource: a vanilla texture's resource header with LUS's GrayscaleAlpha16bpp (9)."""
    src = '\n'.join(l for l in open(ML_BANNER_INC).read().splitlines() if not l.lstrip().startswith('//'))
    pixels = bytes(int(b, 16) for b in re.findall(r'0x([0-9A-Fa-f]{2})\b', src))
    assert len(pixels) == 160 * 8 * 2, len(pixels)
    header = vanilla.read('textures/g_pn_31/gKokiriForestTitleCardENGTex')[:0x40]
    return header + struct.pack('<IIII', 9, 160, 8, len(pixels)) + pixels


def blob(data):
    """`data` as an LUS Blob resource (libultraship BlobFactory): a 64-byte OTR header (little-endian, custom, type
    'OBLB', version 0), the size, the bytes. The resource manager hands its bytes to C as they are, where a .json file
    in an archive would be parsed as a resource of its own."""
    header = struct.pack('<BBxxIIQ', 0, 1, 0x4F424C42, 0, 0).ljust(0x40, b'\0')
    return header + struct.pack('<I', len(data)) + data


# The registries a stock Unbound does not read (only our branch does), which ML's code mod reads as Blobs instead,
# missinglink/<name>, with the names in them as numbers (tools/unbound_stock.py numbered).
STOCK_REGISTRIES = ('cutscenes', 'getitem', 'items', 'labels', 'maps', 'pause', 'save', 'shops')


def layer(vanilla, ml, out, world, ui, maps, pause, getitem, shops, items, unbound_src=None, stock=False):
    stats = dict.fromkeys(('added', 'changed', 'scene documents', 'messages', 'actors placed by name', 'exits named',
                           'exit padding dropped'), 0)
    report = None
    with zipfile.ZipFile(vanilla) as v, zipfile.ZipFile(ml) as m, zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        names = game_names(world, unbound_src) if stock else {}

        def registry(name, doc):
            z.writestr(f'unbound/{name}.json', json.dumps(doc, indent=1))
            if stock and name in STOCK_REGISTRIES:
                z.writestr(f'missinglink/{name}', blob(json.dumps(numbered(doc, names), indent=1).encode()))

        base = {i.filename: i.CRC for i in v.infolist()}
        scene_docs = {}                 # written after the loop: --stock rewrites exits across scenes
        camera_padded = []
        for info in m.infolist():
            n = info.filename
            if n in SKIP or base.get(n) == info.CRC:
                continue
            data = m.read(info)
            if n in base and n.startswith('text/') and n.endswith('/messages.json'):
                old = json.loads(v.read(n))['messages']
                doc = json.loads(data)
                doc['messages'] = {k: val for k, val in doc['messages'].items() if old.get(k) != val}
                if n == 'text/eng/messages.json':
                    doc['messages'].update({k: our_message(f) for k, f in OUR_MESSAGES.items()})
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
                scene_docs[n] = doc
                continue
            else:
                stats['changed' if n in base else 'added'] += 1
                if stock and with_camera_entries(data) is not None:
                    data = with_camera_entries(data)
                    camera_padded.append(n)
            z.writestr(n, data)
        if stock:
            report, rewritten = stock_scenes(scene_docs, world, unbound_src)
            report['collisions given empty camera entries'] = camera_padded
        for n, doc in scene_docs.items():
            z.writestr(n, json.dumps(doc, indent=1))
        z.writestr('unbound.json', json.dumps(MANIFEST, indent=1))
        # Tells our Unbound branch that ML is mounted, which switches on ML's changes to the game's own code (Unbound's
        # unbound-docs/missinglink-actors.md): ML's En_M_Thunder in actor slot 0x57, the Blade Beam spawner, ...
        z.writestr(ML_MARKER, json.dumps({'name': 'The Missing Link', 'scenes': ml_scenes()}, indent=1))
        z.writestr(ML_BANNER, ml_banner(v))
        # One file per actor (Unbound SPEC §7.2): a vanilla actor's name is an override, any other name a type. Stock
        # Unbound refuses both (a vanilla name, `base`, `assets`): there ML's code mod applies the same table and
        # registers the types under these names (skijer's branch, mod-sdk/mods/missinglink).
        actors = {**ML_OVERRIDES, **{k: t['type'] for k, t in ML_TYPES.items()}}
        if stock:
            stock_actors = {'actors left to the code mod': sorted(actors)}
        else:
            for name, doc in actors.items():
                z.writestr(f'unbound/actors/{name}.json', json.dumps(doc, indent=1))
        world.intros(m.namelist())
        for scene, name in ui.scene_names().items():
            world.scenes.setdefault(scene, {})['name'] = name
        destinations = world.destinations(m)
        if stock:
            # ML's event entrances (the cutscene destinations' and the code mod's), on vanilla rows the code mod
            # points at them
            named = {d['destination']['entrance'] for d in destinations.values()} | \
                    {n for n in world.fork_names.values() if n.endswith('_event')}
            events = event_entrances(world, unbound_src, set(ml_scenes()), named - set(names))
            names.update({name: donor for name, (donor, _) in events.items()})
            z.writestr('missinglink/entrances', blob(json.dumps(
                {name: {'index': donor, 'rows': numbered(rows, names)} for name, (donor, rows) in events.items()},
                indent=1).encode()))
            report['event entrances on unused vanilla rows'] = [f'{n} -> {world.table[d][0]}'
                                                                for n, (d, _) in sorted(events.items())]
            # ML's intro cutscenes and the vanilla ones it removes, by entrance number, which the code mod plays
            intros = numbered({n: v['intro'] for d in world.scenes.values()
                               for n, v in d.get('entrances', {}).items() if 'intro' in v}, names)
            unnamed = sorted(n for n in intros if not n.isdigit())
            if unnamed:
                raise SystemExit(f'stock layer: intros on entrances with no number: {unnamed}')
            z.writestr('missinglink/intros', blob(json.dumps(intros, indent=1).encode()))
        restrictions = items.restrictions()
        for scene, fields in restrictions.items():
            world.scenes.setdefault(scene, {})['restrictions'] = fields
        if stock:
            report.update(stock_actors)
            report.update(not_read(world, rewritten))
        else:
            z.writestr('unbound/scenes.json', json.dumps(world.scenes, indent=1))
        registry('cutscenes', destinations)
        registry('save', world.new_save())
        labels = ui.labels()
        registry('labels', labels)
        stats['vanilla scenes changed'] = len(world.scenes)
        stats['save returns'] = world.save_returns
        stats['draw configs'] = world.draw_configs
        stats['game-over continue points'] = world.continue_points
        stats['cutscene destinations'] = len(destinations)
        stats['item labels'] = len(labels['items'])
        stats['time splits'] = len(labels['timeSplits']['splits'])
        doc = maps.maps()
        registry('maps', doc)
        stats['dungeon maps'] = sum(1 for v in doc.values() if v != {'minimap': False})
        registry('pause', pause.pause())
        grottos = world.grottos()
        registry('grottos', grottos)
        stats['grotto destinations'] = sum(1 for d in grottos['destinations'].values() if 'entrance' in d)
        draws = getitem.getitem()
        registry('getitem', draws)
        stats['GetItem models'] = len(draws['draws'])
        prices = shops.shops()
        registry('shops', prices)
        stats['shop prices'] = len(prices['items'])
        rules = items.items_doc()
        registry('items', rules)
        stats['item slots'] = len(rules.get('slots', {}))
        stats['scene restrictions'] = len(restrictions)
    return stats, report


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--unbound', required=True, help="Unbound: its macOS app, or its binary (build-*/soh/soh-macos, "
                                                     "soh.elf, soh.exe)")
    ap.add_argument('--soh-o2r', help="Unbound's soh.o2r (default: the app's, or next to the binary)")
    ap.add_argument('--unbound-src', help="Unbound's source tree at the fork's SoH version (default: "
                                          "mspaths.UNBOUND_SRC, checked against mspaths.UNBOUND_COMPARE)")
    ap.add_argument('--oot-o2r', required=True, help='the oot.o2r ML was built against')
    ap.add_argument('--mod', default=os.path.join(ROOT, 'mod', 'MissingLink_Scenes.o2r'))
    ap.add_argument('--out', default=os.path.join(ROOT, 'mod', 'MissingLink_Unbound.o2r'))
    ap.add_argument('--pack', action='store_true', help="write the layer as a pack of deltas against the player's "
                                                        "oot.o2r and Unbound's conversion of it (what we distribute)")
    ap.add_argument('--stock', action='store_true', help='for a stock Unbound: no unbound/scenes.json or actor files '
                                                         '(ML\'s code mod carries those), exits to the entrances ML '
                                                         'adds rewritten (tools/unbound_stock.py)')
    ap.add_argument('--also-base', action='append', default=[], metavar='OOT_O2R',
                    help='with --pack, an oot.o2r another SoH extractor version wrote from the same ROM: the pack '
                         'then works over it too (Unbound takes any 9.x oot.o2r)')
    args = ap.parse_args()
    if args.stock and args.pack:
        ap.error('--stock and --pack: stock Unbound does not read packs')
    # the converter runs in a temporary folder, so every path must be absolute
    args.oot_o2r, args.mod, args.out = map(os.path.abspath, (args.oot_o2r, args.mod, args.out))
    args.unbound, soh_o2r = unbound_game(args.unbound)
    # Without its extractor assets Unbound stops at start-up behind a popup that waits for a click (OTRGlobals.cpp
    # "Extractor assets not found"), so an install without them is refused before anything runs.
    assets = os.path.join(os.path.dirname(soh_o2r), 'assets')
    soh_o2r = os.path.abspath(args.soh_o2r) if args.soh_o2r else soh_o2r
    if not os.path.isfile(soh_o2r):
        sys.exit(f"Unbound's soh.o2r not found: {soh_o2r}")
    if not os.path.isdir(assets):
        sys.exit(f"Unbound's assets/ folder not found ({assets}): reinstall Unbound")
    unbound_src = args.unbound_src or mspaths.UNBOUND_SRC
    if not args.unbound_src and mspaths.unbound_src_problem(unbound_src):
        sys.exit(mspaths.unbound_src_problem(unbound_src))
    if not os.path.isfile(os.path.join(unbound_src, 'soh', 'include', 'z64item.h')):
        sys.exit(f"Unbound's sources not found in {unbound_src}: give --unbound-src")
    world, ui, maps, pause = MlWorld(unbound_src), MlUi(unbound_src), MlMaps(unbound_src), MlPause(unbound_src)
    getitem, shops, items = MlGetItem(unbound_src), MlShops(unbound_src), MlItems(unbound_src)
    work = tempfile.mkdtemp(prefix='missingship-unbound-')
    try:
        merged = os.path.join(work, 'oot-ml.o2r')
        merged_archive(args.oot_o2r, args.mod, merged)
        export(args.unbound, soh_o2r, args.oot_o2r, os.path.join(work, 'vanilla.o2r'), work)
        export(args.unbound, soh_o2r, merged, os.path.join(work, 'ml.o2r'), work)
        out = os.path.join(work, 'layer.o2r') if args.pack else args.out
        stats, report = layer(os.path.join(work, 'vanilla.o2r'), os.path.join(work, 'ml.o2r'), out, world, ui, maps,
                              pause, getitem, shops, items, unbound_src, args.stock)
        if args.pack:             # vanilla.o2r is what the player's game converts oot.o2r to (oot-unbound.o2r)
            also = []
            for i, other in enumerate(args.also_base):
                converted = os.path.join(work, f'vanilla-{i}.o2r')
                export(args.unbound, soh_o2r, os.path.abspath(other), converted, work)
                also.append([os.path.abspath(other), converted])
            deltapack.make(out, args.out, [args.oot_o2r, os.path.join(work, 'vanilla.o2r')], also=also)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print(f'{args.out}: ' + ', '.join(f'{v} {k}' for k, v in stats.items()))
    for kind, items in (report or {}).items():
        print(f'stock Unbound, {kind}: {len(items)}' + (''.join(f'\n  {i}' for i in items) if kind[:9] != 'not read:'
                                                        else ''))


if __name__ == '__main__':
    main()
