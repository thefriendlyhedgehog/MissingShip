"""What ML's UI looks like, as Unbound data (docs/UNBOUND_MAP.md U10), for tools/unbound_layer.py.

Our SoH fork edits SoH's UI tables in C; Unbound takes them as data. Everything here is read from the fork's own
sources (the engine/ checkout), compared with Unbound's where the fork changed a table, so the two stay in step:
  - item and quest-item names (util.cpp), as English labels;
  - area names for Gameplay Stats (gameplaystats.cpp sMlAreas), as the vanilla scenes' `name`;
  - Gameplay Stats timestamps the item labels don't already rename (SetupDisplayNames), and the count rows ML always
    shows (MlCountShown);
  - Time Splits' list and groups (TimeSplits.cpp) and the countdown timer's icon (TimeDisplay.cpp).
"""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'pipeline'))
from mspaths import SOH   # engine/soh, or the copy MS_SHIPWRIGHT names (the builder's)
FORK = SOH


def _read(path):
    with open(path) as f:
        return f.read()


def _strings(src, var):
    """The string literals of `std::vector<std::string> var = { ... };`."""
    at = src.index(f'std::vector<std::string> {var} = {{')
    return re.findall(r'"((?:[^"\\]|\\.)*)"', src[at:src.index('};', at)])


def _enum(src, member):
    """Names of the enum that contains `member`, with their values."""
    at = src.index(member)
    body = re.sub(r'/\*.*?\*/|//[^\n]*', '', src[src.rfind('{', 0, at) + 1:src.index('}', at)], flags=re.S)
    out, value = {}, -1
    for name, init in re.findall(r'^\s*([A-Z][A-Z0-9_]*)\s*(?:=\s*([^,\n]+))?,?\s*$', body, flags=re.M):
        value = int(init.strip(), 0) if init and re.fullmatch(r'\s*(0x[0-9A-Fa-f]+|\d+)\s*', init) else value + 1
        out[value] = name
    return out


def _function(src, signature):
    at = src.index(signature)
    return src[at:src.index('\n}\n', at)]


# Time Splits' colour names (TimeSplits.cpp), as 0-255 channels.
_COLORS = {
    'COLOR_WHITE': [255, 255, 255], 'COLOR_LIGHT_RED': [255, 13, 0], 'COLOR_RED': [255, 0, 0],
    'COLOR_LIGHT_GREEN': [133, 255, 59], 'COLOR_GREEN': [26, 255, 26], 'COLOR_BLUE': [0, 84, 255],
    'COLOR_PURPLE': [138, 48, 227], 'COLOR_YELLOW': [255, 255, 0], 'COLOR_ORANGE': [255, 171, 28],
    'COLOR_LIGHT_BLUE': [0, 224, 255], 'COLOR_GREY': [199, 199, 199],
}


class MlUi:
    def __init__(self, unbound_src):
        self.ub = lambda *p: _read(os.path.join(unbound_src, 'soh', *p))
        self.fork = lambda *p: _read(os.path.join(FORK, *p))
        z64item = self.ub('include', 'z64item.h')
        self.item_names = _enum(z64item, 'ITEM_STICK,')
        self.quest_names = _enum(z64item, 'QUEST_MEDALLION_FOREST,')
        actors = self.ub('include', 'tables', 'actor_table.h')
        self.actor_names = dict((e, n) for n, e in re.findall(r'DEFINE_ACTOR\w*\((\w+), (ACTOR_\w+)', actors))

    def _renamed(self, var, names):
        """{enum name: {"eng": fork's name}} where the fork's English table differs from Unbound's."""
        ub, fork = _strings(self.ub('soh', 'util.cpp'), var), _strings(self.fork('soh', 'util.cpp'), var)
        return {names[i]: {'eng': f} for i, (u, f) in enumerate(zip(ub, fork)) if u != f}

    def scene_names(self):
        """SCENE_* -> ML's area name, for Gameplay Stats and SoH's menus (Unbound SPEC §7.1 `name`)."""
        src = self.fork('soh', 'Enhancements', 'gameplaystats.cpp')
        table = src[src.index('sMlAreas = {'):]
        return dict(re.findall(r'\{ (SCENE_\w+), "([^"]*)" \}', table[:table.index('};')]))

    def _timestamp_names(self, src):
        """TIMESTAMP_*/ITEM_* -> the name gameplaystats.cpp's SetupDisplayNames gives it."""
        return dict(re.findall(r'itemTimestampDisplayName\[(\w+)\],\s*"([^"]*)"',
                               _function(src, 'void SetupDisplayNames()')))

    def _timestamps(self, items):
        """Timestamps ML names that the item labels leave as SoH's: events, and items SoH doesn't list."""
        ub = self._timestamp_names(self.ub('soh', 'Enhancements', 'gameplaystats.cpp'))
        fork = self._timestamp_names(self.fork('soh', 'Enhancements', 'gameplaystats.cpp'))
        out = {}
        for key, fork_name in fork.items():
            ub_name = ub.get(key, '')
            shown = ('%-20s' % (items[key]['eng'] + ':')) if key in items and ub_name else ub_name
            if shown.rstrip() != fork_name.rstrip():
                out[key] = {'eng': fork_name.rstrip().rstrip(':')}
        return out

    def _time_splits(self):
        src = self.fork('soh', 'Enhancements', 'timesplits', 'TimeSplits.cpp')
        body = src[src.index('splitObjectList = {'):]
        splits = []
        for kind, ident, label, icon, color in re.findall(
                r'\{ SPLIT_TYPE_(\w+),\s*(\w+),\s*"([^"]*)",\s*"([^"]*)",\s*(\w+) \}', body[:body.index('};')]):
            if kind == 'BOSS':
                ident = self.actor_names[ident]
            splits.append({'type': kind.lower(), 'id': ident, 'label': label, 'icon': icon, 'color': _COLORS[color]})
        popups = src[src.index('popupList = {'):]
        groups = {k: re.findall(r'\w+', v) for k, v in
                  re.findall(r'\{ (ITEM_\w+),\s*\{([^}]*)\} \}', popups[:popups.index('};')])}
        return {'splits': splits, 'groups': groups}

    def stock_labels(self, labels):
        """`labels` for ML's code mod (missinglink/labels): an item's label also names the item's timestamp where the
        stats list it, which the mod can't see in SoH's tables on every platform, so those timestamps are written out;
        and the countdown icon's item again as `countdownTimerItem`, which becomes its number."""
        listed = self._timestamp_names(self.ub('soh', 'Enhancements', 'gameplaystats.cpp'))
        timestamps = {key: label for key, label in labels['items'].items() if key in listed}
        timestamps.update(labels['timestamps'])
        icons = dict(labels['icons'], countdownTimerItem=labels['icons']['countdownTimer'])
        return dict(labels, timestamps=timestamps, icons=icons)

    def labels(self):
        """unbound/labels.json (Unbound SPEC §7.7)."""
        items = self._renamed('itemNamesEng', self.item_names)
        shown = re.findall(r'case (COUNT_\w+):', _function(self.fork('soh', 'Enhancements', 'gameplaystats.cpp'),
                                                          'static bool MlCountShown'))
        icon = re.search(r'TIMER_STATE_DOWN_PREVIEW\) \{.*?itemMapping\[(ITEM_\w+)\]',
                         self.fork('soh', 'Enhancements', 'TimeDisplay', 'TimeDisplay.cpp'), flags=re.S).group(1)
        return {
            'items': items,
            'questItems': self._renamed('questItemNamesEng', self.quest_names),
            'timestamps': self._timestamps(items),
            'countsShown': shown,
            'timeSplits': self._time_splits(),
            'icons': {'countdownTimer': icon},
        }
