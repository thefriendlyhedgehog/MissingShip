"""ML's item rules as Unbound data (docs/UNBOUND_MAP.md U11), for tools/unbound_layer.py.

Our SoH fork edits the game's item tables in C (ENGINE_CHANGES "ML's inventory layout" and "Item restrictions"): the
slot each item goes in (gItemSlots), the bomb bag and nut capacities without an upgrade, the strength mask ML clears
so the bracelet is not shown, the four slots Inventory_HasEmptyBottle looks in, the Bombchu cap, and the scene
restriction table (sRestrictionFlags). Unbound takes the tables as unbound/items.json (Unbound SPEC §7.13) and the
restrictions as each scene's `restrictions` in unbound/scenes.json (§7.1). Everything is read from the fork's sources
against Unbound's, so the two stay in step: only what differs is written.
"""
import os, re, sys

from unbound_ml_maps import _num, _read, _source, _table
from unbound_ml_pause import _enum_names

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'pipeline'))
from mspaths import SOH   # engine/soh, or the copy MS_SHIPWRIGHT names (the builder's)
FORK = SOH
INVENTORY = os.path.join('src', 'code', 'z_inventory.c')
PARAMETER = os.path.join('src', 'code', 'z_parameter.c')
VANILLA_BOTTLES = ['SLOT_BOTTLE_1', 'SLOT_BOTTLE_2', 'SLOT_BOTTLE_3', 'SLOT_BOTTLE_4']
VANILLA_BOMBCHUS = 50
# sRestrictionFlags packs twelve 2-bit fields into three bytes, high bits first (z_parameter.c
# Interface_SetSceneRestrictions); the names are Unbound's (SPEC §7.1 `restrictions`)
RESTRICTIONS = ['hGauge', 'bButton', 'aButton', 'bottles', 'tradeItems', 'hookshot', 'ocarina', 'warpSongs',
                'sunsSong', 'farores', 'dinsNayrus', 'all']


def _function(text, name):
    at = text.index(name + '(')
    return text[at:text.index('\n}\n', at)]


def _restriction_rows(src):
    """Scene -> its (flags1, flags2, flags3): the first row naming it, as the lookup takes the first match."""
    rows = {}
    for row in _table(src, 'sRestrictionFlags'):
        if row and row[0] != '0xFF':
            rows.setdefault(row[0], tuple(_num(v) for v in row[1:4]))
    return rows


def _fields(flags):
    fields = {}
    for n, name in enumerate(RESTRICTIONS):
        level = (flags[n // 4] >> (6 - 2 * (n % 4))) & 3
        if level:
            fields[name] = level
    return fields


class MlItems:
    def __init__(self, unbound_src):
        soh = os.path.join(unbound_src, 'soh')
        header = _read(os.path.join(soh, 'include', 'z64item.h'))
        self.items = _enum_names(header, 'ITEM_STICK,')
        self.slot_names = _enum_names(header, 'SLOT_STICK,')
        self.upgrades = _enum_names(header, 'UPG_QUIVER,')
        self.ub_inv = _source(_read(os.path.join(soh, INVENTORY)))
        self.fork_inv = _source(_read(os.path.join(FORK, INVENTORY)))
        self.ub_par = _source(_read(os.path.join(soh, PARAMETER)))
        self.fork_par = _source(_read(os.path.join(FORK, PARAMETER)))

    def _slots(self, src):
        return [self.slot_names.index(v) if v in self.slot_names else _num(v) for v in _table(src, 'gItemSlots')]

    def _bottles(self):
        body = _function(self.fork_par, 's32 Inventory_HasEmptyBottle')
        return re.findall(r'items\[(SLOT_\w+)\] == ITEM_BOTTLE', body)

    def _bombchus(self):
        body = _function(self.fork_par, 'u8 Item_Give')
        caps = {int(v) for v in re.findall(r'AMMO\(ITEM_BOMBCHU\) > (\d+)', body)}
        if len(caps) != 1:
            raise SystemExit(f"z_parameter.c: the fork's Item_Give caps Bombchus at {sorted(caps)}; expected one value")
        return caps.pop()

    def items_doc(self):
        """unbound/items.json: every entry where the fork's table differs from Unbound's."""
        doc = {}
        ub, fork = self._slots(self.ub_inv), self._slots(self.fork_inv)
        if len(ub) != len(fork):
            raise SystemExit(f"z_inventory.c: gItemSlots has {len(fork)} items in the fork, {len(ub)} in Unbound")
        # numbers, not SLOT_* names: ML reuses slots for other items (its Deku Sticks sit in vanilla's Hookshot slot)
        slots = {self.items[i]: s for i, (a, s) in enumerate(zip(ub, fork)) if a != s}
        if slots:
            doc['slots'] = slots
        ub_caps = [[_num(v) for v in row] for row in _table(self.ub_inv, 'gUpgradeCapacities')]
        fork_caps = [[_num(v) for v in row] for row in _table(self.fork_inv, 'gUpgradeCapacities')]
        caps = {self.upgrades[u]: fork_caps[u] for u in range(len(ub_caps)) if ub_caps[u] != fork_caps[u]}
        if caps:
            doc['capacities'] = caps
        ub_masks = [_num(v) for v in _table(self.ub_inv, 'gUpgradeMasks')]
        fork_masks = [_num(v) for v in _table(self.fork_inv, 'gUpgradeMasks')]
        hidden = [self.upgrades[u] for u, (a, b) in enumerate(zip(ub_masks, fork_masks)) if a != b]
        if any(fork_masks[self.upgrades.index(u)] != 0 for u in hidden):
            raise SystemExit("z_inventory.c: the fork changes an upgrade mask to something other than 0")
        if hidden:
            doc['hiddenUpgrades'] = hidden
        bottles = self._bottles()
        if len(bottles) != 4:
            raise SystemExit(f"z_parameter.c: the fork's Inventory_HasEmptyBottle checks {bottles}; expected four")
        if bottles != VANILLA_BOTTLES:
            doc['emptyBottleSlots'] = [self.slot_names.index(b) for b in bottles]
        bombchus = self._bombchus()
        if bombchus != VANILLA_BOMBCHUS:
            doc['bombchuCapacity'] = bombchus
        return doc

    def restrictions(self):
        """Vanilla scene -> its `restrictions` object, for every scene whose row differs: ML's table read the same way
        (an unlisted scene is all zeros, so a row ML dropped becomes an empty object)."""
        ub, fork = _restriction_rows(self.ub_par), _restriction_rows(self.fork_par)
        out = {}
        for scene in sorted(set(ub) | set(fork)):
            a, b = ub.get(scene, (0, 0, 0)), fork.get(scene, (0, 0, 0))
            if a != b:
                out[scene] = _fields(b)
        return out
