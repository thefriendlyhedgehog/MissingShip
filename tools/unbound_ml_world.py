"""ML's story wiring as Unbound data (docs/UNBOUND_MAP.md U1-U4), for tools/unbound_layer.py.

Our SoH fork does these in C; Unbound takes them as data. Everything here is read from the fork's own sources (the
engine submodule), so the two stay in step:
  - U1 entrances: the entrances the fork adds to vanilla scenes (entrance_table.h from 0x614) become entrances in
    those scenes, the eight-entry event groups with a cutscene layer 4, and ML's flags on the vanilla entrances it
    uses (ml_entrance_flags.c, ledger row 50) become overrides of those entrances;
  - U3 intro cutscenes: ML's eleven (z_demo.c sEntranceCutsceneTable) on their entrances, and vanilla's (read from
    Unbound's z_demo.c) removed, which also removes the Requiem and Nocturne triggers (row 114);
  - U2 cutscene destinations: every ML cutscene whose destination command the pipeline rewrote or ML patched;
  - U4 new save: ML's starting loadout (z_sram.c) and where Skip Intro lands (SkipIntro.cpp).
"""
import json, os, re, struct

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENGINE = os.path.join(ROOT, 'engine', 'soh')
FORK_FIRST = 0x614                  # the fork's entrances start after vanilla's table
TRANS_FADE_BLACK, TRANS_FADE_WHITE = 2, 3
EVENT_LAYER = 4                     # cutsceneIndex 0xFFF0: the scene's first cutscene setup
EVENTCHKINF_EPONA_OBTAINED = 0x18


def _read(path):
    with open(path) as f:
        return f.read()


def _entrance_table(path):
    """index -> (name, scene enum, spawn, continueBgm, titleCard, endTrans, startTrans) from an entrance_table.h.
    A transition is its TRANS_TYPE_* name, or the macro text for the circle wipes (TRANS_TYPE_CIRCLE(...))."""
    out = {}
    src = _read(path)
    for m in re.finditer(r'/\* 0x([0-9A-Fa-f]+) \*/ DEFINE_ENTRANCE\(', src):
        depth, args, cur = 1, [], ''
        for c in src[m.end():]:
            depth += (c == '(') - (c == ')')
            if depth == 0:
                break
            if c == ',' and depth == 1:
                args.append(cur.strip()); cur = ''
            else:
                cur += c
        name, scene, spawn, bgm, card, end, start = args + [cur.strip()]
        out[int(m.group(1), 16)] = (name, scene, int(spawn), bgm == 'true', card == 'true', end, start)
    return out


TRANS = {'TRANS_TYPE_FADE_BLACK': 2, 'TRANS_TYPE_FADE_WHITE': 3, 'TRANS_TYPE_FADE_BLACK_FAST': 4,
         'TRANS_TYPE_FADE_WHITE_FAST': 5, 'TRANS_TYPE_FADE_BLACK_SLOW': 6, 'TRANS_TYPE_FADE_WHITE_SLOW': 7,
         'TRANS_TYPE_FADE_WHITE_CS_DELAYED': 8, 'TRANS_TYPE_FADE_WHITE_INSTANT': 9, 'TRANS_TYPE_FADE_GREEN': 10,
         'TRANS_TYPE_FADE_BLUE': 11}


def _fields(continue_bgm, title_card, end_trans, start_trans):
    return {'continueBgm': continue_bgm, 'showTitleCard': title_card, 'endTransition': end_trans,
            'startTransition': start_trans}


def _unpack(field):
    return _fields(bool(field & 0x8000), bool(field & 0x4000), (field >> 7) & 0x7F, field & 0x7F)


class MlWorld:
    def __init__(self, unbound_src):
        self.table = _entrance_table(os.path.join(ENGINE, 'include', 'tables', 'entrance_table.h'))
        self.unbound_demo = _read(os.path.join(unbound_src, 'soh', 'src', 'code', 'z_demo.c'))
        self.fork_demo = _read(os.path.join(ENGINE, 'src', 'code', 'z_demo.c'))
        self.scenes = {}                 # vanilla scene enum -> {"entrances": {...}}
        self.fork_names = {}             # fork entrance index (group start) -> registered entrance name
        self.fork_groups = []            # (start, end, scene, key) of each registered fork entrance
        self._fork_entrances()
        self._entrance_flags()

    def _entrances_of(self, scene):
        return self.scenes.setdefault(scene, {'entrances': {}})['entrances']

    # ---- U1 ----

    def _fork_entrances(self):
        """The fork's entrances past vanilla's table, one registered entrance per run of identical entries."""
        rows = sorted(i for i in self.table if i >= FORK_FIRST)
        groups = []
        for i in rows:
            name, scene, spawn, *fields = self.table[i]
            if groups and groups[-1]['end'] == i and groups[-1]['row'][1:] == self.table[i][1:]:
                groups[-1]['end'] = i + 1
            else:
                groups.append({'start': i, 'end': i + 1, 'row': self.table[i]})
        for g in groups:
            name, scene, spawn, bgm, card, end, start = g['row']
            key = 'ml/' + re.sub(r'_0$', '', name[len('ENTR_MISSINGSHIP_'):]).lower()
            entrance = {'spawn': spawn, **_fields(bgm, card, TRANS[end], TRANS[start])}
            if g['end'] - g['start'] > EVENT_LAYER:           # an event group: ML lands on its layer 4
                entrance['layers'] = {str(EVENT_LAYER): {}}
            self._entrances_of(scene)[key] = entrance
            self.fork_names[g['start']] = f'{scene}/{key}'
            self.fork_groups.append((g['start'], g['end'], scene, key))

    def _entrance_flags(self):
        """ML's flags on the entrances it uses (ledger row 50): overrides of the vanilla ones, and over the fork's own
        (Play_Init applies them to those too, so they are what a player sees there): the entrance's fields where its
        whole group agrees, per layer where it does not (the Temple of Time's white fades by day)."""
        src = _read(os.path.join(ENGINE, 'src', 'code', 'ml_entrance_flags.c'))
        fork = {}
        for index, field in re.findall(r'\{ 0x([0-9A-Fa-f]+), 0x([0-9A-Fa-f]+) \}', src):
            index, field = int(index, 16), int(field, 16)
            if index < FORK_FIRST:
                name, scene = self.table[index][:2]
                self._entrances_of(scene).setdefault(name, {}).update(_unpack(field))
            else:
                fork[index] = field
        for start, end, scene, key in self.fork_groups:
            fields = {i - start: fork[i] for i in range(start, end) if i in fork}
            if not fields:
                continue
            entrance = self._entrances_of(scene)[key]
            if len(fields) == end - start and len(set(fields.values())) == 1:
                entrance.update(_unpack(fields[0]))
            else:
                layers = entrance.setdefault('layers', {})
                for layer, field in fields.items():
                    layers.setdefault(str(layer), {}).update(_unpack(field))

    def entrance_name(self, index):
        """The name an exit or a destination may use for an entrance index: vanilla's, or the fork's group."""
        if index < FORK_FIRST:
            return self.table[index][0]
        return self.fork_names.get(index)

    # ---- U3 ----

    def intros(self, layer_names):
        """ML's intro cutscenes on their entrances; vanilla's removed everywhere else they were."""
        def table(src):
            body = src[src.index('sEntranceCutsceneTable[] = {'):]
            body = body[:body.index('};')]
            return re.findall(r'\{ (ENTR_\w+), (\d+), (0x[0-9A-Fa-f]+|\d+), (\w+) \}', body)
        by_scene = {self.table[i][0]: self.table[i][1] for i in self.table}
        for entr, _age, _flag, _cs in table(self.unbound_demo):
            self._entrances_of(by_scene[entr]).setdefault(entr, {})['intro'] = False
        # the Requiem trigger's entrance (ledger row 114); the Nocturne one gets ML's Monktown Valley intro below
        self._entrances_of('SCENE_DESERT_COLOSSUS').setdefault('ENTR_DESERT_COLOSSUS_OUTSIDE_TEMPLE', {})['intro'] = False
        for entr, age, flag, cs in table(self.fork_demo):
            path = next(n for n in layer_names if n.endswith('/' + cs))
            intro = {'cutscene': path, 'flag': int(flag, 0)}
            if intro['flag'] == EVENTCHKINF_EPONA_OBTAINED:
                # vanilla's loop plays a row with this flag on every arrival; ML's pond intro is one
                intro['repeats'] = True
            if age != '2':
                intro['age'] = 'child' if age == '1' else 'adult'
            self._entrances_of(by_scene[entr]).setdefault(entr, {})['intro'] = intro

    # ---- U2 ----

    def _patched_cases(self):
        """Destination code -> the fork's case: the MissingShip entrance it goes to (ML patched these cases)."""
        fn = self.fork_demo.index('void Cutscene_Command_Terminator')
        body = self.fork_demo[fn:self.fork_demo.index('\nvoid ', fn + 10)]
        seg = body[[m.start() for m in re.finditer(r'switch \(cmd->base\)', body)][1]:]
        parts = re.split(r'((?:\s*case \d+:)+)', seg)
        index = {row[0]: i for i, row in self.table.items()}
        out = {}
        for i in range(1, len(parts) - 1, 2):
            block = parts[i + 1].split('break;')[0]
            m = re.search(r'nextEntranceIndex = (ENTR_MISSINGSHIP_\w+);', block)
            target = m.group(1) if m else None
            if 'MlEntrance_ToSoh(0x420)' in block:
                # ML 0x420-0x427, the pond intro's row: the pipeline maps them to the pond event group (ledger row 84)
                target = 'ENTR_MISSINGSHIP_FISHING_POND_EVENT'
            if target:
                for label in re.findall(r'case (\d+):', parts[i]):
                    out[int(label)] = index[target]
        return out

    def destinations(self, layer):
        """unbound/cutscenes.json: every ML cutscene whose destination the fork's C decides."""
        patched = self._patched_cases()
        out = {}
        for n in layer.namelist():
            d = layer.read(n)
            if len(d) < 0x48 or d[4:8] != b'TVCO':          # a Cutscene resource
                continue
            count = struct.unpack('<I', d[0x40:0x44])[0]
            words = struct.unpack(f'<{count}I', d[0x44:0x44 + 4 * count])
            for i in range(len(words) - 2):
                if words[i] == 0x3E8 and words[i + 1] == 1:   # CS_CMD_DESTINATION with one entry
                    code = words[i + 2] & 0xFFFF
                    if code & 0x8000:                        # the pipeline's rewrite: a literal vanilla entrance
                        dest = {'entrance': self.entrance_name(code & 0x7FFF), 'transition': TRANS_FADE_BLACK}
                        if n.endswith('/gMlLinkHomeIntroCs'):
                            # the opening: vanilla code 119, which ML leaves as it is (noon, white fade); the
                            # rewrite lost both (pipeline/ml_entrance.py "Known loss")
                            dest.update(time=0x8000, transition=TRANS_FADE_WHITE)
                    elif code in patched:                    # ML's own case: its event group, layer 4, black fade
                        dest = {'entrance': self.fork_names[patched[code]], 'setup': EVENT_LAYER,
                                'transition': TRANS_FADE_BLACK}
                    else:
                        continue
                    out[n] = {'destination': dest}
        return out

    # ---- U4 ----

    @staticmethod
    def new_save():
        """ML's new-save template (ledger row 13) and Skip Intro's destination (row 115). The fork's QoL option for
        where the Ocarina goes (MissingShip.StartOcarinaButton) is ours, not ML's, so this is ML's own C-Left."""
        SLOT_OCARINA, ITEM_OCARINA_TIME, ITEM_SWORD_KOKIRI = 7, 0x08, 0x3B
        return {
            'items': {str(SLOT_OCARINA): ITEM_OCARINA_TIME},
            'equipment': 0x1121,          # Kokiri Sword, Hylian Shield, Kokiri Tunic, Kokiri Boots
            'equipped': 0x1121,           # and all four worn
            'magic': 0,
            'upgrades': 0x1040,           # Goron's Bracelet, Adult's Wallet
            'questItems': 0x6000,         # Epona's Song, Saria's Song
            'buttonItems': {'0': ITEM_SWORD_KOKIRI, '1': ITEM_OCARINA_TIME},   # B, and C-Left as ML's template has it
            'cButtonSlots': {'0': SLOT_OCARINA},
            'skipIntro': {'entrance': 'ENTR_TEMPLE_OF_TIME_ENTRANCE', 'time': 0x8000},   # ML 0x5F0 at noon
        }
