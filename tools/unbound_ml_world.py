"""ML's story wiring as Unbound data (docs/UNBOUND_MAP.md U1-U4), for tools/unbound_layer.py.

Our SoH fork does these in C; Unbound takes them as data. Everything here is read from the fork's own sources (the
engine/ checkout), so the two stay in step:
  - U1 entrances: the entrances the fork adds to vanilla scenes (entrance_table.h from 0x614) become entrances in
    those scenes, the eight-entry event groups with a cutscene layer 4, and ML's flags on the vanilla entrances it
    uses (ml_entrance_flags.c, ledger row 50) become overrides of those entrances;
  - U3 intro cutscenes: ML's ten (z_demo.c sEntranceCutsceneTable) on their entrances, and vanilla's (read from
    Unbound's z_demo.c) removed, which also removes the Requiem and Nocturne triggers (row 114);
  - U2 cutscene destinations: every ML cutscene whose destination command the pipeline rewrote or ML patched;
  - U4 new save: ML's starting loadout (z_sram.c) and where Skip Intro lands (SkipIntro.cpp);
  - row 28: where a loaded save and the game-over Continue resume. Vanilla's code names those entrances by number,
    and the fork reads each number through ML's entrance table (ml_entrance_map.c), so they become the scenes'
    `saveReturn` and the arrival entrances' `continueAt` where ML's meaning differs from vanilla's;
  - ML's scene draw configs (ENGINE_CHANGES "Scene draw configs" row, pipeline/ml_scene_map.json): each scene ML
    ships gets `drawConfig`, its draw config in ML's scene table. ML's config 0 runs the Deku Tree's handler (ML nops
    the default one, code 0x80099550), so it is written as that handler's number;
  - U5 grottos (row 25): ML's grotto table (z_door_ana.c sGrottoEntrances) where it differs from vanilla's, and no
    return point on any slot, since ML's grottos are ordinary scenes left through their own exits.
"""
import json, os, re, struct, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'pipeline'))
from mspaths import SOH   # engine/soh, or the copy MS_SHIPWRIGHT names (the builder's)
ENGINE = SOH
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
        self.unbound_src = unbound_src
        self.table = _entrance_table(os.path.join(ENGINE, 'include', 'tables', 'entrance_table.h'))
        self.unbound_demo = _read(os.path.join(unbound_src, 'soh', 'src', 'code', 'z_demo.c'))
        self.fork_demo = _read(os.path.join(ENGINE, 'src', 'code', 'z_demo.c'))
        self.scenes = {}                 # vanilla scene enum -> {"entrances": {...}}
        self.fork_names = {}             # fork entrance index (group start) -> registered entrance name
        self.fork_groups = []            # (start, end, scene, key) of each registered fork entrance
        self._fork_entrances()
        self._entrance_flags()
        self.save_returns, self.continue_points = self._returns(unbound_src)
        self.draw_configs = self._draw_configs(unbound_src)

    def _entrances_of(self, scene):
        return self.scenes.setdefault(scene, {}).setdefault('entrances', {})

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

    # ---- row 28 ----

    @staticmethod
    def _ml_entrance_map():
        """ML's entrance table as the fork reads it: ML number -> SoH index, and SoH index -> ML number."""
        src = _read(os.path.join(ENGINE, 'src', 'code', 'ml_entrance_map.c'))

        def table(name):
            body = src[src.index(name):]
            return [int(v, 16) for v in re.findall(r'0x[0-9A-Fa-f]+', body[body.index('{') + 1:body.index('};')])]

        return table('sMlToSoh'), table('sSohToMl')

    @staticmethod
    def _cases(block):
        """A C switch on an entrance or scene as {case name: the entrance name it assigns, or None to keep it}."""
        out, pending = {}, []
        for m in re.finditer(r'case (\w+):|gSaveContext\.entranceIndex = ([^;]+);|break;', block):
            if m.group(1):
                pending.append(m.group(1))
                continue
            for case in pending:
                out[case] = m.group(2)
            pending = []
        return out

    def _returns(self, unbound_src):
        """Ledger row 28 (see the module doc): `saveReturn` and `continueAt` where the fork's differ from vanilla's."""
        ml_to_soh, soh_to_ml = self._ml_entrance_map()
        vanilla = {row[0]: i for i, row in self.table.items() if i < FORK_FIRST}
        fork_sram = _read(os.path.join(ENGINE, 'src', 'code', 'z_sram.c'))
        fork_kaleido = _read(os.path.join(ENGINE, 'src', 'overlays', 'misc', 'ovl_kaleido_scope',
                                          'z_kaleido_scope_PAL.c'))
        assert 'MlEntrance_ToSoh(sDungeonEntrances' in fork_sram and 'MlEntrance_FromSoh(sohEntrance)' in fork_kaleido

        # Sram_OpenSave: the saved scene's entrance, ML's number for it
        sram = _read(os.path.join(unbound_src, 'soh', 'src', 'code', 'z_sram.c'))
        dungeons = dict((s, e) for e, s in re.findall(r'(ENTR_\w+),\s*// (SCENE_\w+)', sram))
        body = sram[sram.index('void Sram_OpenSave'):]
        body = body[body.index('switch (gSaveContext.savedSceneNum)'):body.index('default:')]
        save_returns = 0
        for scene, entrance in self._cases(body).items():
            entrance = dungeons[scene] if entrance.startswith('sDungeonEntrances') else entrance
            ml = ml_to_soh[vanilla[entrance]]
            if ml != vanilla[entrance]:
                self.scenes.setdefault(scene, {})['saveReturn'] = self.entrance_name(ml)
                save_returns += 1

        # the game-over Continue: the arrival entrance as ML's number, through vanilla's switch, and back
        kaleido = _read(os.path.join(unbound_src, 'soh', 'src', 'overlays', 'misc', 'ovl_kaleido_scope',
                                     'z_kaleido_scope_PAL.c'))
        body = kaleido[kaleido.index('case 0x10:'):]
        body = body[body.index('switch (gSaveContext.entranceIndex)'):body.index('} else {')]
        redirects = {vanilla[a]: vanilla[b] for a, b in self._cases(body).items() if b is not None}
        continue_points = 0
        for x in range(max(self.table) + 1):
            m = soh_to_ml[x] if x < len(soh_to_ml) else x
            ml = ml_to_soh[redirects[m]] if m in redirects else x
            if ml == redirects.get(x, x):
                continue
            if x < FORK_FIRST:
                name, scene = self.table[x][:2]
                entrance = self._entrances_of(scene).setdefault(name, {})
            else:
                start, end, scene, key = next(g for g in self.fork_groups if g[0] == x)
                entrance = self._entrances_of(scene)[key]
            entrance['continueAt'] = self.entrance_name(ml)
            continue_points += 1
        return save_returns, continue_points

    # ---- scene draw configs ----

    def _draw_configs(self, unbound_src):
        """`drawConfig` on every scene ML ships whose draw config in ML differs from Unbound's (see the module doc)."""
        z64scene = _read(os.path.join(unbound_src, 'soh', 'include', 'z64scene.h'))
        sdc = re.findall(r'/\* +\d+ \*/ (SDC_\w+)', z64scene)
        table = _read(os.path.join(unbound_src, 'soh', 'include', 'tables', 'scene_table.h'))
        vanilla = dict(re.findall(r'DEFINE_SCENE\(\w+, \w+, (SCENE_\w+), (SDC_\w+)', table))
        fork_table = _read(os.path.join(ENGINE, 'src', 'code', 'z_scene_table.c'))
        default = re.search(r'void func_80099550\(PlayState\* play\) \{\s*(func_\w+)\(play\);', fork_table)
        handlers = re.findall(r'func_\w+', fork_table[fork_table.index('sSceneDrawHandlers[]'):][:2000])
        with open(os.path.join(ROOT, 'pipeline', 'ml_scene_map.json')) as f:
            ml = json.load(f)
        changed = 0
        for entry in ml.values():
            config = entry['config']
            if config == 0 and default:                 # ML's default handler runs another's code
                config = handlers.index(default.group(1))
            if sdc[config] != vanilla[entry['enum']]:
                self.scenes.setdefault(entry['enum'], {})['drawConfig'] = config
                changed += 1
        return changed

    # ---- U5 ----

    def grottos(self):
        """unbound/grottos.json: ML's destination for each grotto slot where the fork's table differs from Unbound's;
        every slot without the return point when the fork leaves it out (ML nops Play_SetupRespawnPoint there)."""
        path = os.path.join('src', 'overlays', 'actors', 'ovl_Door_Ana', 'z_door_ana.c')

        def table(src):
            body = src[src.index('sGrottoEntrances[] = {'):]
            return re.findall(r'ENTR_\w+', body[:body.index('};')])

        fork_src = _read(os.path.join(ENGINE, path))
        fork, vanilla = table(fork_src), table(_read(os.path.join(self.unbound_src, 'soh', path)))
        assert len(fork) == len(vanilla)
        index = {row[0]: i for i, row in self.table.items()}
        return_point = 'Play_SetupRespawnPoint(play, RESPAWN_MODE_RETURN' in fork_src
        destinations = {}
        for slot, (ml, ub) in enumerate(zip(fork, vanilla)):
            d = {}
            if ml != ub:
                d['entrance'] = self.entrance_name(index[ml])
            if not return_point:
                d['returnPoint'] = False
            if d:
                destinations[str(slot)] = d
        return {'destinations': destinations}

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
        where the Ocarina goes (MissingShip.StartOcarinaButton) is ours, not ML's, so this is ML's own C-Left (the owner's
        choice for Unbound, 2026-09-28)."""
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
