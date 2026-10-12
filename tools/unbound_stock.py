"""ML's scene data for a stock SoH: Unbound, for tools/unbound_layer.py --stock (docs/ROADMAP.md step 5).

Stock Unbound (roborich's, 0.11, and skijer's mod-API branch on it) reads unbound/scenes.json only for new scenes:
an entry keyed by a vanilla scene is refused ("has no scene path"), so the entrances ML adds to vanilla scenes do not
exist there, and a scene whose exits name one fails to load (the game falls back to Dodongo's Cavern). ML has no new
scenes, so the stock layer ships no unbound/scenes.json. Each entrance ML adds goes on a run of vanilla entrance rows
a stock Unbound never reaches with ML (rows into a scene ML lacks, which ML's code mod refuses, that the game's code
never names and that none of ML's data names), built from ML's definition the way our branch builds its table; ML's
code mod points those rows at ML's scene, spawn and flags (missinglink/entrances), and every exit to the entrance is
rewritten to the run's first row.
ML's actors' own collision (objects/*, LOCO resources) has no camera data, as in ML's ROM; vanilla's always has at
least one entry. Standing on such a collision makes stock Unbound's camera read the missing list (z_bgcheck.c's
func_80041A4C) and crash; our branch checks for it (row B1). The stock layer gives each one empty entries (setting 0,
which the camera reads as "none", as our branch's check returns) up to the highest index its surfaces name.
What else the vanilla-scene entries carried is not read by stock Unbound either; the layer ships it by number for ML's
code mod and the report counts it: scene names, draw configs, save return points and restrictions
(missinglink/scenes), ML's flags on vanilla entrances (transitions, music, title cards) and its game-over continue
points (missinglink/entrancechanges), and intro cutscenes (missinglink/intros). A room drawn with the wrong draw
config crashes the game, so without the code mod the stock layer does not run.
"""
import glob, os, re, struct
from collections import defaultdict

from unbound_ml_world import FORK_FIRST


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


def stock_scenes(docs, world, donors):
    """Point each exit in `docs` (layer path -> scene document, changed in place) that names an entrance ML adds at that
    entrance's donor rows (custom_entrances); return the report and the entrances exits now reach."""
    report, chosen = defaultdict(list), set()
    for doc in docs.values():
        for setup in (doc.get('setups') or {}).values():
            exits = setup.get('exits') or {}
            for key, target in exits.items():
                if isinstance(target, str) and '/' in target:
                    if target not in donors:
                        raise SystemExit(f'stock layer: an exit names {target}, which has no donor rows')
                    exits[key] = donors[target][0]
                    chosen.add(target)
    report["exits to ML's entrances, on donor rows"] = sorted(f'{t} -> {donors[t][0]}' for t in chosen)
    return dict(report), chosen


def not_read(world, rewritten):
    """What ML's entries for vanilla scenes carry that stock Unbound does not read and ML's code mod carries instead,
    counted for the report; `rewritten` are ML's entrances that exits now reach."""
    report, vanilla = {}, world.scenes
    for field in ('name', 'restrictions', 'saveReturn'):
        report[f'{field} left to the code mod'] = sorted(s for s, v in vanilla.items() if field in v)
    # without ML's draw config some rooms draw through an unset segment and the game crashes on arrival: ML's code mod
    # sets these (skijer's branch, mod-sdk/mods/missinglink/ml_scenes.c, the same table)
    report['draw configs left to the code mod'] = sorted(f'{s} {v["drawConfig"]}' for s, v in vanilla.items()
                                                         if 'drawConfig' in v)
    overrides = [(s, n, v) for s, d in vanilla.items() for n, v in d.get('entrances', {}).items()]
    report['entrance flags left to the code mod'] = [f'{s}/{n}' for s, n, v in overrides
                                                     if n.startswith('ENTR_') and any(k in v for k in ENTRANCE_FLAGS)]
    report['intros left to the code mod'] = [f'{s}/{n}' for s, n, v in overrides if 'intro' in v]
    report['continue points left to the code mod'] = [f'{s}/{n}' for s, n, v in overrides if 'continueAt' in v]
    # the event entrances are reached by cutscene destinations, deku_tree_2 by a grotto
    report["ML's entrances no scene exit reaches"] = sorted(
        f'{s}/{n}' for s, n, v in overrides if not n.startswith('ENTR_') and f'{s}/{n}' not in rewritten)
    return report


def _enums(text):
    """Every enumerator of every enum in C source `text`, with its value (an explicit value is a number or an earlier
    enumerator, plus or minus a number)."""
    values = {}
    for body in re.findall(r'\benum\b[^{;]*\{([^}]*)\}', re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S)):
        n = -1
        for entry in body.split(','):
            m = re.fullmatch(r'\s*([A-Za-z_]\w*)\s*(?:=\s*(.+?))?\s*', entry)
            if not m:
                continue
            if m[2] is None:
                n += 1
            else:
                e = re.fullmatch(r'(\w+)\s*(?:([+-])\s*(\w+))?', m[2].strip('() '))
                if not e:
                    n = None
                    continue
                n = values.get(e[1]) if e[1] in values else int(e[1], 0) if e[1][0].isdigit() else None
                if n is not None and e[2]:
                    n += int(e[3], 0) * (1 if e[2] == '+' else -1)
            if n is not None:
                values[m[1]] = n
    return values


# The headers whose enums ML's registries name (items, slots, upgrades, quest items, GetItem models, shop items,
# timestamps and counts)
ENUM_HEADERS = (os.path.join('include', 'z64item.h'),
                os.path.join('src', 'overlays', 'actors', 'ovl_En_GirlA', 'z_en_girla.h'),
                os.path.join('soh', 'Enhancements', 'gameplaystats.h'))
# Keys whose values are names the code mod reads as names: a time split's icon (TimeSplits.cpp's splitImage)
KEEP_NAMES = ('icon', 'countdownTimer')


def game_names(world, unbound_src):
    """Name -> number for what ML's registries name: the enums of ENUM_HEADERS, vanilla's entrances and scenes."""
    names = {}
    for header in ENUM_HEADERS:
        with open(os.path.join(unbound_src, 'soh', header)) as f:
            names.update(_enums(f.read()))
    for i in sorted(world.table, reverse=True):       # a group's first entry is its name's index
        if i < FORK_FIRST:
            names[world.table[i][0]] = i
    with open(os.path.join(unbound_src, 'soh', 'include', 'tables', 'scene_table.h')) as f:
        names.update({enum: i for i, enum in enumerate(re.findall(r'DEFINE_SCENE\(\w+,\s*\w+,\s*(SCENE_\w+)',
                                                                       f.read()))})
    return names


def numbered(doc, names):
    """`doc` with every key and string that is a name in `names` replaced by its number, for ML's code mod, which reads
    the registries on a stock Unbound and has no tables of names (its keys stay strings: "13"); KEEP_NAMES' values
    stay names."""
    if isinstance(doc, dict):
        return {str(names.get(k, k)): v if k in KEEP_NAMES else numbered(v, names) for k, v in doc.items()}
    if isinstance(doc, list):
        return [numbered(v, names) for v in doc]
    if isinstance(doc, str):
        return names.get(doc, doc)
    return doc


def _pack(d, base):
    """An entrance row's packed field (z64.h ENTRANCE_INFO_*), as our branch's PackEntranceField: `d`'s fields over
    `base`'s, with SceneDB::RegisterEntrance's defaults (no music carried over, no title card, black fades)."""
    get = lambda k, default: d.get(k, base.get(k, default))
    return ((0x8000 if get('continueBgm', False) else 0) | (0x4000 if get('showTitleCard', False) else 0) |
            ((get('endTransition', 2) << 7) & 0x3F80) | (get('startTransition', 2) & 0x7F))


def entrance_rows(scene, d):
    """A custom entrance's rows as our branch registers them (SceneDB::RegisterEntrance): its own fields on each of the
    four layers, and each layer it lists over those (an event's is cutscene layer 4, the fifth row). Our branch pads a
    group to whole fours with the entrance's own rows; ML lands on none past the ones listed, so they are left off."""
    own = {'scene': scene, 'spawn': d.get('spawn', 0), 'field': _pack(d, {})}
    rows = [dict(own) for _ in range(4)]
    for key, layer in sorted((d.get('layers') or {}).items(), key=lambda kv: int(kv[0])):
        while len(rows) <= int(key):
            rows.append(dict(own))
        rows[int(key)] = {'scene': scene, 'spawn': layer.get('spawn', own['spawn']), 'field': _pack(layer, d)}
    return rows


def custom_entrances(world, unbound_src, ml_scenes, docs, referenced):
    """Every entrance ML adds to a vanilla scene, on vanilla entrance rows a stock Unbound never reaches with ML: each
    takes as many consecutive rows as it has layers, all leading into scenes ML does not have (ML's code mod refuses
    those), never named by the game's code (its intro table, its triggers, its game-over rules) and never named by ML's
    data (`referenced`, and an exit's number). ML's code mod points the rows at ML's scene, spawn and flags
    (missinglink/entrances), as our branch's table does. Scene/entrance name -> (first row, its rows)."""
    table, code = world.table, code_names(unbound_src)
    used = {e for doc in docs.values() for setup in (doc.get('setups') or {}).values()
            for e in (setup.get('exits') or {}).values() if isinstance(e, int)}
    free = lambda i: (i in table and table[i][1] not in ml_scenes and table[i][0] not in code
                      and table[i][0] not in referenced and i not in used)
    groups = sorted((f'{scene}/{key}', entrance_rows(scene, d)) for scene, sd in world.scenes.items()
                    for key, d in sd.get('entrances', {}).items() if not key.startswith('ENTR_'))
    out, i = {}, 0
    for name, rows in groups:
        while i + len(rows) <= FORK_FIRST and not all(free(j) for j in range(i, i + len(rows))):
            i += 1
        if i + len(rows) > FORK_FIRST:
            raise SystemExit(f'stock layer: no free run of {len(rows)} entrance rows left for {name}')
        out[name] = (i, rows)
        i += len(rows)
    return out


ENTRANCE_FLAGS = ('continueBgm', 'showTitleCard', 'endTransition', 'startTransition')


def entrance_changes(world):
    """What ML changes on entrances besides their rows, by name: a vanilla entrance's flags (only the fields it sets,
    which the code mod puts over the row's own, as SceneDB::OverrideEntrance does), and the game-over continue point
    after arriving by any entrance (`continueAt`). A custom entrance's flags are in its rows (custom_entrances)."""
    out = {}
    for scene, sd in world.scenes.items():
        for name, d in sd.get('entrances', {}).items():
            entry = {k: d[k] for k in ENTRANCE_FLAGS if k in d and name.startswith('ENTR_')}
            if 'continueAt' in d:
                entry['continueAt'] = d['continueAt']
            if entry:
                out[name if name.startswith('ENTR_') else f'{scene}/{name}'] = entry
    return out


SCENE_FIELDS = ('name', 'drawConfig', 'saveReturn', 'restrictions')


def scene_data(world):
    """ML's name, draw config, save return point and item restrictions for each vanilla scene it sets them on."""
    return {scene: {k: d[k] for k in SCENE_FIELDS if k in d}
            for scene, d in world.scenes.items() if any(k in d for k in SCENE_FIELDS)}


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

