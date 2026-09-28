"""ML's pause-menu layout and rules as Unbound data (docs/UNBOUND_MAP.md U7b), for tools/unbound_layer.py.

Our SoH fork edits the pause menu (kaleido) in C (ledger row 24); Unbound takes it as unbound/pause.json (Unbound SPEC
§7.9). Everything is read from the fork's sources compared with Unbound's, so the two stay in step:
  - the age rules (gSlotAgeReqs, gItemAgeReqs, gEquipAgeReqs) ML opens to either age;
  - the item grid's X, the ammo counts ML shows (slots 8-10, Sticks and Nuts placed over slots 9 and 10) and the
    Bombchu count drawn full at 20;
  - the equipment page's column X and ML's vertical cursor (its remap, stopping on any slot);
  - the quest page's slot positions (ML parks the slots it does not use off the page), the slot the page is entered on
    from the right, and ML's cursor redirects;
  - the world map's piece positions and ML's point rules (points 0 and 1 hidden, six following its story flags);
  - the info panel's colours.
The fork also edits sPageVtxColor*, but SoH draws the pages from its own cosmetics defaults (pageColors), so those
edits never showed; `colors.pages` is not written, and the pages keep the colours the fork shows (the owner's choice
over ML's parchment, 2026-09-28).
"""
import os, re

from unbound_ml_maps import _num, _read, _source, _table

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FORK = os.path.join(ROOT, 'engine', 'soh')
KALEIDO = os.path.join('src', 'overlays', 'misc', 'ovl_kaleido_scope')

AGES = {'AGE_REQ_NONE': 'any', 'AGE_REQ_CHILD': 'child', 'AGE_REQ_ADULT': 'adult'}
EQUIP_CELLS = [['ITEM_QUIVER_30', 'ITEM_SWORD_KOKIRI', 'ITEM_SWORD_MASTER', 'ITEM_SWORD_BGS'],
               ['ITEM_BOMB_BAG_20', 'ITEM_SHIELD_DEKU', 'ITEM_SHIELD_HYLIAN', 'ITEM_SHIELD_MIRROR'],
               ['ITEM_BRACELET', 'ITEM_TUNIC_KOKIRI', 'ITEM_TUNIC_GORON', 'ITEM_TUNIC_ZORA'],
               ['ITEM_SCALE_SILVER', 'ITEM_BOOTS_KOKIRI', 'ITEM_BOOTS_IRON', 'ITEM_BOOTS_HOVER']]
AMMO_ITEMS = ['ITEM_STICK', 'ITEM_NUT', 'ITEM_BOMB', 'ITEM_BOW', 'ITEM_SLINGSHOT', 'ITEM_BOMBCHU', 'ITEM_BEAN']
EDGES = {'PAUSE_CURSOR_PAGE_LEFT': 'left', 'PAUSE_CURSOR_PAGE_RIGHT': 'right'}


def _enum_names(header, first):
    body = header[header.rfind('{', 0, header.index(first)) + 1:]
    return re.findall(r'^\s*(?:/\*[^*]*\*/\s*)?([A-Z][A-Z0-9_]*)', body[:body.index('}')], flags=re.M)


def _s16(values):
    """C's s16 arrays may be written in hex (0xFFCF is -49)."""
    return [v - 0x10000 if isinstance(v, int) and v > 0x7FFF else v for v in map(_num, values)]


def _flat(rows):
    return [v for r in rows for v in (_flat(r) if isinstance(r, list) else [r])]


class _Kaleido:
    def __init__(self, soh):
        read = lambda name: _read(os.path.join(soh, KALEIDO, name))
        self.scope_raw = read('z_kaleido_scope_PAL.c')
        scope = _source(self.scope_raw)
        self.slot_ages = _table(scope, 'gSlotAgeReqs')
        self.item_ages = _table(scope, 'gItemAgeReqs')
        self.equip_ages = _table(scope, 'gEquipAgeReqs')
        self.info_panel = [_num(v) for v in _flat(_table(scope, 'D_8082ADF0'))]
        self.pieces_x = _s16(_table(scope, 'D_8082AEC0'))
        self.pieces_y = _s16(_table(scope, 'D_8082AF78'))
        self.ammo_digits = _s16(_table(scope, 'D_8082B11C'))
        self.columns_x = _s16(_table(scope, 'D_8082B12C'))
        self.quest_x = _s16(_table(scope, 'D_8082B138'))
        self.quest_y = _s16(_table(scope, 'D_8082B198'))
        self.grid_x = int(re.search(r'for \(phi_t1 = (?:Unbound_PauseItemGridX\()?(-?\d+)', scope).group(1))
        item = _source(read('z_kaleido_item.c'))
        self.full = {i: int(n) for i, n in re.findall(r'\(item == (ITEM_BOMBCHU|ITEM_BEAN) && ammo == (\d+)\)', item)}
        m = re.search(r'for \(i = 0; i < (\d+); i\+\+\) \{\s*u8 item = gSaveContext\.inventory\.items\[(\d+) \+ i\]',
                      item)
        self.ammo_slots = list(range(int(m.group(2)), int(m.group(2)) + int(m.group(1)))) if m else None
        self.equipment = _source(read('z_kaleido_equipment.c'))
        self.collect = _source(read('z_kaleido_collect.c'))


class MlPause:
    def __init__(self, unbound_src):
        self.ub = _Kaleido(os.path.join(unbound_src, 'soh'))
        self.fork = _Kaleido(FORK)
        z64item = _read(os.path.join(unbound_src, 'soh', 'include', 'z64item.h'))
        self.slots = _enum_names(z64item, 'SLOT_STICK,')
        self.items = _enum_names(z64item, 'ITEM_STICK,')

    def _ages(self):
        ub, fork = self.ub, self.fork
        out = {}
        slots = {self.slots[i]: AGES[f] for i, (u, f) in enumerate(zip(ub.slot_ages, fork.slot_ages)) if u != f}
        items = {self.items[i]: AGES[f] for i, (u, f) in enumerate(zip(ub.item_ages, fork.item_ages)) if u != f}
        equipment = {EQUIP_CELLS[r][c]: AGES[fork.equip_ages[r][c]]
                     for r in range(4) for c in range(4) if ub.equip_ages[r][c] != fork.equip_ages[r][c]}
        for key, value in (('slots', slots), ('items', items), ('equipment', equipment)):
            if value:
                out[key] = value
        return out

    def _items(self):
        ub, fork = self.ub, self.fork
        out, ammo = {}, {}
        if fork.grid_x != ub.grid_x:
            out['gridX'] = fork.grid_x
        if fork.ammo_slots is not None:
            ammo['slots'] = fork.ammo_slots
        digits = {AMMO_ITEMS[i]: f // 4 for i, (u, f) in enumerate(zip(ub.ammo_digits, fork.ammo_digits)) if u != f}
        if digits:
            ammo['digits'] = digits
        full = {i: n for i, n in fork.full.items() if ub.full.get(i) != n}
        if full:
            ammo['full'] = full
        if ammo:
            out['ammo'] = ammo
        return out

    def _equipment(self):
        ub, fork = self.ub, self.fork
        out, cursor = {}, {}
        if fork.columns_x != ub.columns_x:
            out['columnsX'] = fork.columns_x
        remap = re.search(r'sRemap\[\]\[2\] = \{(.*?)\};', fork.equipment, flags=re.S)
        if remap:
            cursor['vertical'] = {a: int(b) for a, b in re.findall(r'\{ (\d+), (\d+) \}', remap.group(1))}
        # ML drops vanilla's "is this equipment owned" test from the vertical moves
        test = 'gBitFlags[pauseCtx->cursorPoint[PAUSE_EQUIP] - 1]'
        if fork.equipment.count(test) < ub.equipment.count(test):
            cursor['anySlot'] = True
        if cursor:
            out['cursor'] = cursor
        return out

    def _quest(self):
        ub, fork = self.ub, self.fork
        out, cursor = {}, {}
        slots = {str(i): [fork.quest_x[i], fork.quest_y[i]] for i in range(len(fork.quest_x))
                 if (fork.quest_x[i], fork.quest_y[i]) != (ub.quest_x[i], ub.quest_y[i])}
        if slots:
            out['slots'] = slots

        def entry(src):
            return int(re.search(r'if \(\(pauseCtx->stickRelX < -30\)\) \{\s*pauseCtx->cursorPoint\[PAUSE_QUEST\] = '
                                 r'(?:Unbound_PauseQuestEntryFromRight\()?(\d+)', src).group(1))

        if entry(fork.collect) != entry(ub.collect):
            cursor['fromRight'] = entry(fork.collect)
        table = re.search(r'sMlQuestCursorRedirects\[\]\[4\] = \{(.*?)\};', fork.collect, flags=re.S)
        if table:
            redirects = []
            for except_, to, field, value in re.findall(r'\{ (\d+), (\d+), (\d+), (\w+) \}', table.group(1)):
                r = {'to': int(to), 'except': int(except_)}
                if field == '28':
                    r['edge'] = EDGES[value]
                else:
                    r['slot'] = int(value)
                redirects.append(r)
            cursor['redirects'] = redirects
        if cursor:
            out['cursor'] = cursor
        return out

    def _world_map(self):
        ub, fork = self.ub, self.fork
        out = {}
        pieces = {str(i): [fork.pieces_x[i], fork.pieces_y[i]] for i in range(len(fork.pieces_x))
                  if (fork.pieces_x[i], fork.pieces_y[i]) != (ub.pieces_x[i], ub.pieces_y[i])}
        if pieces:
            out['pieces'] = pieces
        src = fork.scope_raw
        if 'sMlMapPoints[]' in src:
            points = {}
            hidden = re.search(r'((?:pauseCtx->worldMapPoints\[\d+\] = )+)0;', src[src.index('sMlMapPoints[]'):])
            for p in re.findall(r'worldMapPoints\[(\d+)\]', hidden.group(1)):
                points[p] = False
            word = int(re.search(r'gSaveContext\.eventChkInf\[(\d+)\] & sMlMapPoints', src).group(1))
            table = src[src.index('sMlMapPoints[] = {'):]
            for p, mask in re.findall(r'\{ (\d+), (0x[0-9A-Fa-f]+) \}', table[:table.index('};')]):
                points[p] = {'flag': word * 16 + int(mask, 16).bit_length() - 1}
            out['points'] = points
        return out

    def pause(self):
        """unbound/pause.json (Unbound SPEC §7.9)."""
        doc = {}
        for key, value in (('ageRequirements', self._ages()), ('items', self._items()),
                           ('equipment', self._equipment()), ('quest', self._quest()),
                           ('worldMap', self._world_map())):
            if value:
                doc[key] = value
        if self.fork.info_panel != self.ub.info_panel:
            doc['colors'] = {'infoPanel': [self.fork.info_panel[0:4], self.fork.info_panel[4:8]]}
        return doc
