"""ML's shop prices as Unbound data (docs/UNBOUND_MAP.md U8), for tools/unbound_layer.py.

Our SoH fork edits En_GirlA's shop table (shopItemEntries) in C (ledger row 67: Bombchus cost 40 for 10 and 80 for
20, not 100 and 180, in all eight Bombchu entries). Unbound takes prices as unbound/shops.json (Unbound SPEC §7.12).
Read from the fork's z_en_girla.c against Unbound's, so the two stay in step: every shop item whose price differs.
ML's own text already names its prices.
"""
import os, sys

from unbound_ml_maps import _num, _read, _source, _table
from unbound_ml_pause import _enum_names

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'pipeline'))
from mspaths import SOH   # engine/soh, or the copy MS_SHIPWRIGHT names (the builder's)
FORK = SOH
GIRLA = os.path.join('src', 'overlays', 'actors', 'ovl_En_GirlA')
PRICE = 3  # ShopItemEntry: objID, giDrawId, hiliteFunc, price, count, ...


def _prices(soh):
    table = _table(_source(_read(os.path.join(soh, GIRLA, 'z_en_girla.c'))), 'shopItemEntries')
    return [_num(row[PRICE]) for row in table]


class MlShops:
    def __init__(self, unbound_src):
        soh = os.path.join(unbound_src, 'soh')
        self.ub = _prices(soh)
        self.fork = _prices(FORK)
        self.names = _enum_names(_read(os.path.join(soh, GIRLA, 'z_en_girla.h')), 'SI_DEKU_NUTS_5,')

    def shops(self):
        if len(self.ub) != len(self.fork):
            raise SystemExit(f"z_en_girla.c: the fork's shop table has {len(self.fork)} items, Unbound's "
                             f'{len(self.ub)}; line them up before writing unbound/shops.json')
        return {'items': {self.names[i]: {'price': theirs}
                          for i, (ours, theirs) in enumerate(zip(self.ub, self.fork)) if ours != theirs}}
