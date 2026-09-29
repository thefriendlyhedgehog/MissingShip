"""ML's GetItem models as Unbound data (docs/UNBOUND_MAP.md U6), for tools/unbound_layer.py.

Our SoH fork edits the GetItem draw table in C (ledger row 16): the red potion's row draws ML's Deku Nut Milk, the
blue potion's takes the red potion's colours, and the Bullet Bag 40's draws the Soul-Charged Kokiri Sword. Unbound
takes these as unbound/getitem.json (Unbound SPEC §7.11). Everything is read from the fork's z_draw.c against
Unbound's, so the two stay in step: each row whose draw function or display lists differ becomes an entry, its
function named by the first Unbound row that draws with it (`drawAs`, left out when it is the row's own), its display
lists by the asset paths their headers define (Unbound's objects, and ML's in the fork's ml_assets.h).
"""
import glob, os, re

from unbound_ml_maps import _read, _source, _table
from unbound_ml_pause import _enum_names

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FORK = os.path.join(ROOT, 'engine', 'soh')
PATH_DEFINE = re.compile(r'^#define dg(\w+) "__OTR__([^"]+)"', flags=re.M)


def _rows(soh):
    """sDrawItemTable as (draw function, [display-list symbols]) per row."""
    table = _table(_source(_read(os.path.join(soh, 'src', 'code', 'z_draw.c'))), 'sDrawItemTable')
    return [(row[0], list(row[1]) if len(row) > 1 else []) for row in table]


def _asset_paths(headers):
    paths = {}
    for header in headers:
        for name, path in PATH_DEFINE.findall(_read(header)):
            paths['g' + name] = path  # dgFooDL defines gFooDL's path
    return paths


class MlGetItem:
    def __init__(self, unbound_src):
        soh = os.path.join(unbound_src, 'soh')
        self.ub = _rows(soh)
        self.fork = _rows(FORK)
        self.gids = _enum_names(_read(os.path.join(soh, 'include', 'z64item.h')), 'GID_BOTTLE,')
        self.paths = _asset_paths(glob.glob(os.path.join(soh, 'assets', 'objects', '*', '*.h')) +
                                  [os.path.join(FORK, 'assets', 'ml_assets.h')])

    def getitem(self):
        if len(self.ub) != len(self.fork):
            raise SystemExit(f"z_draw.c: the fork's GetItem table has {len(self.fork)} rows, Unbound's "
                             f'{len(self.ub)}; line them up before writing unbound/getitem.json')
        draws = {}
        for row, (ours, theirs) in enumerate(zip(self.ub, self.fork)):
            if ours == theirs:
                continue
            func, dlists = theirs
            entry = {}
            if func != ours[0]:
                entry['drawAs'] = self.gids[[f for f, _ in self.ub].index(func)]
            missing = [d for d in dlists if d not in self.paths]
            if missing:
                raise SystemExit(f'z_draw.c: no asset path for {", ".join(missing)} (row {self.gids[row]})')
            entry['dlists'] = [self.paths[d] for d in dlists]
            draws[self.gids[row]] = entry
        return {'draws': draws}
