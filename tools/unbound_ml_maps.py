"""ML's minimap and pause-map tables as Unbound data (docs/UNBOUND_MAP.md U7a), for tools/unbound_layer.py.

Our SoH fork edits vanilla's map tables in C (ledger rows 5, 48, 49, 138); Unbound takes them as unbound/maps.json
(Unbound SPEC §7.8). Everything is read from the fork's sources compared with Unbound's, so the two stay in step:
  - gMapData's dungeon rows (z_map_data.c): room palettes, compass offsets, floors, palette rooms, the boss floor
    and the room switches, where the fork's differ;
  - the dungeon minimaps: ML draws every dungeon's from the Shadow Temple's block, index 190 + room (row 48);
  - the minimap and pause-map marks: ML's are MQ debug's with ML's edits, whatever SoH's MQ mode (row 49), so each
    room or floor whose marks differ from either of Unbound's tables is written;
  - `minimap: false` on the overworld scenes: ML never draws an overworld minimap (row 5), and the dungeon minimap
    stays as vanilla shows it (row 138).
"""
import os, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FORK = os.path.join(ROOT, 'engine', 'soh')

DUNGEONS = ['SCENE_DEKU_TREE', 'SCENE_DODONGOS_CAVERN', 'SCENE_JABU_JABU', 'SCENE_FOREST_TEMPLE',
            'SCENE_FIRE_TEMPLE', 'SCENE_WATER_TEMPLE', 'SCENE_SPIRIT_TEMPLE', 'SCENE_SHADOW_TEMPLE',
            'SCENE_BOTTOM_OF_THE_WELL', 'SCENE_ICE_CAVERN']
ML_MINIMAP_BASE = 190  # ML code 0x80080D20: every dungeon's minimap is index 190 + room
FLOOR_NAMES = ['8F', '7F', '6F', '5F', '4F', '3F', '2F', '1F', 'B1', 'B2', 'B3', 'B4', 'B5', 'B6', 'B7', 'B8']
MARK_TYPES = {'MAP_MARK_CHEST': 'chest', 'MAP_MARK_BOSS': 'boss', 'PAUSE_MAP_MARK_CHEST': 'chest',
              'PAUSE_MAP_MARK_BOSS': 'boss'}


def _read(path):
    with open(path) as f:
        return f.read()


def _source(text):
    """The file without comments, with its object-like #defines expanded."""
    text = re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S)
    for name, body in re.findall(r'^#define (\w+) (.+)$', text, flags=re.M):
        text = re.sub(r'\b%s\b' % name, body, text.replace('#define %s %s' % (name, body), ''))
    return text


def _parse(text, at):
    """The brace initializer starting at text[at] == '{', as nested lists of atom strings."""
    stack, atom, i = [[]], '', at + 1
    while stack:
        c = text[i]
        if c == '{':
            stack.append([])
        elif c in ',}':
            if atom.strip():
                stack[-1].append(atom.strip())
            atom = ''
            if c == '}':
                done = stack.pop()
                if not stack:
                    return done
                stack[-1].append(done)
        else:
            atom += c
        i += 1


def _table(text, name):
    m = re.search(r'\b%s\s*(\[[^=]*\])?\s*=\s*\{' % re.escape(name), text)
    return _parse(text, m.end() - 1)


def _num(atom):
    atom = atom.strip()
    if re.fullmatch(r'-?\d+\.\d*f?', atom):
        return float(atom.rstrip('f'))
    return int(atom, 0) if re.fullmatch(r'-?(0x[0-9A-Fa-f]+|\d+)', atom) else atom


def _grid(rows, *shape):
    """A C array padded to its declared shape, as C pads it (zeros)."""
    if len(shape) == 1:
        return [_num(v) for v in rows] + [0] * (shape[0] - len(rows))
    rows = rows + [[]] * (shape[0] - len(rows))
    return [_grid(r, *shape[1:]) for r in rows]


class _Maps:
    """One source tree's map tables."""
    def __init__(self, soh):
        data = _source(_read(os.path.join(soh, 'src', 'code', 'z_map_data.c')))
        self.room_palette = _grid(_table(data, 'sRoomPalette'), 10, 32)
        self.max_palettes = _grid(_table(data, 'sMaxPaletteCount'), 10)
        self.palette_room = _grid(_table(data, 'sPaletteRoom'), 10, 8, 14)
        self.compass_x = _grid(_table(data, 'sRoomCompassOffsetX'), 10, 44)
        self.compass_y = _grid(_table(data, 'sRoomCompassOffsetY'), 10, 44)
        self.compass_info = _grid(_table(data, 'sDgnCompassInfo'), 10, 4)
        self.floor_y = _grid(_table(data, 'sFloorCoordY'), 10, 8)
        ids = {('F_' + n): i + 1 for i, n in enumerate(FLOOR_NAMES)}
        self.floor_id = [[ids.get(v, v) for v in row] for row in _grid(_table(data, 'sFloorID'), 10, 8)]
        self.boss_floor = _grid(_table(data, 'sBossFloor'), 8)
        self.switch_count = _grid(_table(data, 'sSwitchEntryCount'), 10)
        self.switches = [list(zip(*(r[:n] for r in rows)))
                         for n, *rows in zip(self.switch_count, _grid(_table(data, 'sSwitchFromRoom'), 10, 51),
                                             _grid(_table(data, 'sSwitchFromFloor'), 10, 51),
                                             _grid(_table(data, 'sSwitchToRoom'), 10, 51))]
        self.minimap_count = _grid(_table(data, 'sDgnMinimapCount'), 12)
        self.minimap_offset = _grid(_table(data, 'sDgnMinimapTexIndexOffset'), 10)
        self.tex_base = _grid(_table(data, 'sDgnMinimapTexIndexBase'), 10)
        self.floor_tex = _grid(_table(data, 'sFloorTexIndexOffset'), 10, 8)

        marks = _source(_read(os.path.join(soh, 'src', 'overlays', 'misc', 'ovl_map_mark_data', 'z_map_mark_data.c')))
        self.minimap_marks = {kind: [[self._marks(room) for room in _table(marks, name)]
                                     for name in _table(marks, 'gMapMarkDataTable' + kind)]
                              for kind in ('Vanilla', 'Mq')}
        pause = _source(_read(os.path.join(soh, 'src', 'overlays', 'misc', 'ovl_kaleido_scope', 'z_lmap_mark_data.c')))
        self.pause_marks = {kind: [self._marks(entry, pause=True) for entry in _table(pause, name)]
                            for kind, name in (('Vanilla', 'gPauseMapMarkDataTable'),
                                               ('Mq', 'gPauseMapMarkDataTableMasterQuest'))}

    @staticmethod
    def _marks(groups, pause=False):
        """A room's or floor's mark groups as the flat list maps.json takes."""
        out = []
        for group in groups:
            kind = MARK_TYPES.get(group[0])
            if kind is None:
                break
            count, points = (_num(group[4]), group[5]) if pause else (_num(group[1]), group[2])
            for flag, x, y in (map(_num, p) for p in points[:count]):
                mark = {'type': kind, 'flag': flag, 'x': x, 'y': y}
                if kind == 'boss':
                    del mark['flag']
                out.append(mark)
        return out

    def pause_entry(self, dungeon, slot):
        return (self.tex_base[dungeon] + self.floor_tex[dungeon][slot]) >> 1


class MlMaps:
    def __init__(self, unbound_src):
        self.ub = _Maps(os.path.join(unbound_src, 'soh'))
        self.fork = _Maps(FORK)
        exp = _source(_read(os.path.join(unbound_src, 'soh', 'src', 'code', 'z_map_exp.c')))
        self.minimap_textures = _table(exp, 'minimapTableDangeon')
        header = _read(os.path.join(unbound_src, 'soh', 'assets', 'textures', 'map_i_static', 'map_i_static.h'))
        self.texture_paths = dict(re.findall(r'#define d(\w+) "__OTR__([^"]+)"', header))
        draw = exp[exp.index('void Minimap_Draw('):]
        overworld = draw[draw.index('case SCENE_HYRULE_FIELD:'):]
        self.overworld = re.findall(r'case (SCENE_\w+):', overworld[:overworld.index('if (')])

    def _dungeon(self, d):
        ub, fork = self.ub, self.fork
        rooms, floors, out = {}, {}, {}

        def room(r):
            return rooms.setdefault(str(r), {})

        for r in range(44):
            if r < 32 and fork.room_palette[d][r] != ub.room_palette[d][r]:
                room(r)['palette'] = fork.room_palette[d][r]
            if (fork.compass_x[d][r], fork.compass_y[d][r]) != (ub.compass_x[d][r], ub.compass_y[d][r]):
                room(r)['compass'] = [fork.compass_x[d][r], fork.compass_y[d][r]]
            # ML's minimap for the room, where it is another texture than vanilla's
            index = ML_MINIMAP_BASE + r
            if index < len(self.minimap_textures) and index != ub.minimap_offset[d] + r:
                room(r)['texture'] = self.texture_paths[self.minimap_textures[index]]
            ml = fork.minimap_marks['Mq'][d][r] if r < len(fork.minimap_marks['Mq'][d]) else []
            if any(ml != (t[d][r] if r < len(t[d]) else []) for t in ub.minimap_marks.values()):
                room(r)['marks'] = ml

        def floor(s):
            return floors.setdefault(str(s), {})

        for s in range(8):
            if fork.floor_id[d][s] != ub.floor_id[d][s]:
                floor(s)['id'] = FLOOR_NAMES[fork.floor_id[d][s] - 1] if fork.floor_id[d][s] else ''
            if fork.floor_y[d][s] != ub.floor_y[d][s]:
                floor(s)['y'] = fork.floor_y[d][s]
            if not fork.floor_id[d][s]:
                continue  # not a floor in ML: nothing else in the slot is read
            if fork.palette_room[d][s] != ub.palette_room[d][s]:
                floor(s)['paletteRooms'] = fork.palette_room[d][s]
            entry = fork.pause_entry(d, s)
            ml = fork.pause_marks['Mq'][entry]
            if any(ml != t[entry] for t in ub.pause_marks.values()):
                floor(s)['marks'] = ml

        if rooms:
            out['rooms'] = rooms
        if floors:
            out['floors'] = floors
        if fork.compass_info[d] != ub.compass_info[d]:
            out['compass'] = {'scale': fork.compass_info[d][:2], 'offset': fork.compass_info[d][2:]}
        if fork.max_palettes[d] != ub.max_palettes[d]:
            out['maxPalettes'] = fork.max_palettes[d]
        if d < 8 and fork.boss_floor[d] != ub.boss_floor[d]:
            out['bossFloor'] = fork.boss_floor[d]
        # A switch from room 255 never matches a room, so ML's all-255 rows are no switches at all.
        ml_switches = [s for s in fork.switches[d] if s[0] != 255]
        if ml_switches != [s for s in ub.switches[d] if s[0] != 255]:
            out['roomSwitches'] = [{'fromRoom': a, 'fromFloor': b, 'toRoom': c} for a, b, c in ml_switches]
        return out

    def maps(self):
        """unbound/maps.json (Unbound SPEC §7.8)."""
        doc = {scene: {'minimap': False} for scene in self.overworld}
        for d, scene in enumerate(DUNGEONS):
            dungeon = self._dungeon(d)
            if dungeon:
                doc[scene] = dungeon
        return doc
