"""Set each ML-shipped slot's draw config in soh/include/tables/scene_table.h to ML's own (ml_scene_map.json 'config').

The draw config sets per-scene segments (8/9/... scroll and texture DLs), fog and animated materials. ML changed it
for most of its slots (its grottos use SDC_GROTTOS, Deep Woods SDC_DEKU_TREE, ...); with vanilla's config a scene DL
that calls segment 8 runs whatever an actor last put there (a rupee texture -> crash entering the Colossus slot).
Idempotent; rerun after ml_map changes.
"""
from mspaths import PIPE, SOH
import json, re
T = SOH + '/include/tables/scene_table.h'
E = SOH + '/include/z64scene.h'
sdc = [re.search(r'SDC_(\w+)', l).group(0) for l in open(E) if re.search(r'/\* +\d+ \*/ SDC_', l)]
want = {v['slot']: sdc[v['config']] for v in json.load(open(PIPE + '/ml_scene_map.json')).values()}
out = []; n = 0
for l in open(T):
    m = re.match(r'(/\* 0x([0-9A-F]+) \*/ DEFINE_SCENE\(\w+, \w+, \w+, )(\w+)(, .*?\))(.*)$', l.rstrip('\n'))
    if m and int(m.group(2), 16) in want:
        new = want[int(m.group(2), 16)]
        tail = re.sub(r'\s*// MissingShip: ML draw config.*', '', m.group(5))
        if new != m.group(3) or 'MissingShip: ML draw config' in l:
            orig = re.search(r'was (SDC_\w+)', l).group(1) if 'was SDC_' in l else m.group(3)
            l = f'{m.group(1)}{new}{m.group(4)}{tail}' + (f' // MissingShip: ML draw config (was {orig})' if new != orig else '')
            n += new != m.group(3)
    out.append(l.rstrip('\n'))
open(T, 'w').write('\n'.join(out) + '\n')
print(f'{n} draw configs changed')
