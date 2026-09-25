"""Regenerate docs/ML_ACTOR_INVENTORY.md from the ROMs (MS-005).

For every actor slot it works out, straight from the ROMs:
  - the change: `modcheck.check` against PAL MQ Debug, or "ML custom" when the debug slot is empty;
  - where it is placed: actor lists and transition actors of every ML scene and room, alternate headers
    included (an entry repeated across headers counts once);
  - who spawns it: `jal Actor_Spawn` / `Actor_SpawnAsChild` in `code`, the player overlay, ML's custom code
    (dma 507) and every actor overlay, taking the last immediate loaded into the actor-id register before the call.

The hand-written Status and Verified columns are carried over from the existing inventory by actor slot, so
regenerating never loses them. Usage: survey_actors.py [--write]   (without --write it prints to stdout)
"""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..', 'pipeline'))
from mspaths import ROOT, SOH
import re, struct, sys, collections
from ml_map import SCENES
from modcheck import check
from ovldiff import ML, DB
from mlrom import entry

ACTOR_TABLE = 0xF9440
N_ACTORS = 0x1D7
SPAWN, SPAWN_CHILD = 0x80031F50, 0x80032458   # Actor_Spawn: id in $a2; Actor_SpawnAsChild: id in $a3
INV = _os.path.join(ROOT, 'docs', 'ML_ACTOR_INVENTORY.md')

# ---- names ---------------------------------------------------------------------------------------------------
NAME, BY_NAME = {}, {}
for l in open(SOH + '/include/tables/actor_table.h'):
    m = re.match(r'/\* (0x[0-9A-F]+) \*/ DEFINE_ACTOR\w*\((\w+)(?:, (ACTOR_\w+))?', l)
    if not m: continue
    slot = int(m.group(1), 16); NAME[slot] = m.group(2)
    BY_NAME[m.group(2)] = slot
    if m.group(3):   # the vanilla name behind a repointed slot: ACTOR_EN_REEBA -> En_Reeba
        BY_NAME['_'.join(p.capitalize() if not p[-1].isdigit() or p.isalpha() else p.capitalize()
                         for p in m.group(3)[6:].split('_'))] = slot
        BY_NAME[m.group(3)[6:]] = slot

def vanilla_name(slot):
    """The vanilla actor behind a slot we repointed to an ML port (None when the slot keeps its own actor)."""
    for n, s in BY_NAME.items():
        if s == slot and n.lower() != NAME.get(slot, '').lower() and not n.isupper() and not n.startswith('Ml_'):
            return n
    return None

# ---- placements ----------------------------------------------------------------------------------------------
def W(d, o): return struct.unpack('>I', d[o:o+4])[0]

def headers(d):
    """Offsets of the main header and every alternate header in a scene/room file."""
    out, o = [0], 0
    while o + 8 <= len(d) and d[o] != 0x14:
        if d[o] == 0x18:
            lst = W(d, o + 4) & 0xFFFFFF
            for k in range(32):
                if lst + 4 * k + 4 > len(d): break
                p = W(d, lst + 4 * k)
                if p == 0: continue
                if p >> 24 not in (2, 3) or (p & 0xFFFFFF) >= len(d): break
                out.append(p & 0xFFFFFF)
        o += 8
    return out

def commands(d, h):
    o = h
    while o + 8 <= len(d) and d[o] != 0x14:
        yield d[o], d[o + 1], W(d, o + 4) & 0xFFFFFF
        o += 8

placed = collections.defaultdict(lambda: collections.Counter())
for sdma, info in SCENES.items():
    sname = info['scene'][:-6]
    seen = set()
    sd = ML.file(sdma)
    for h in headers(sd):
        for cmd, n, p in commands(sd, h):
            if cmd == 0x0E:   # transition actors: 16 bytes, id at +4
                for k in range(n):
                    e = sd[p + 16 * k: p + 16 * k + 16]
                    key = ('s', e)
                    if len(e) < 16 or key in seen: continue
                    seen.add(key); placed[struct.unpack('>h', e[4:6])[0] & 0xFFFF][sname] += 1
    for rdma in info['rooms']:
        rd = ML.file(rdma)
        for h in headers(rd):
            for cmd, n, p in commands(rd, h):
                if cmd == 0x01:   # actor list: 16 bytes, id at +0
                    for k in range(n):
                        e = rd[p + 16 * k: p + 16 * k + 16]
                        key = (rdma, e)
                        if len(e) < 16 or key in seen: continue
                        seen.add(key); placed[struct.unpack('>h', e[0:2])[0] & 0xFFFF][sname] += 1

# ---- spawners ------------------------------------------------------------------------------------------------
spawned = collections.defaultdict(set)
def text_of(d):
    """An overlay's text section (the relocation header at its end gives the size); data must not be read as code."""
    tail = W(d, len(d) - 4)
    return d[:W(d, len(d) - tail)]

def scan(d, label):
    ws = [W(d, i * 4) for i in range(len(d) // 4)]
    for i, w in enumerate(ws):
        if w >> 26 != 3: continue
        t = 0x80000000 | ((w & 0x3FFFFFF) << 2)
        if t not in (SPAWN, SPAWN_CHILD): continue
        reg = 6 if t == SPAWN else 7
        val = None
        for j in list(range(max(0, i - 14), i)) + [i + 1]:   # last write before the call, then the delay slot
            x = ws[j] if j < len(ws) else 0
            op, rs, rt, rd = x >> 26, (x >> 21) & 31, (x >> 16) & 31, (x >> 11) & 31
            if op in (9, 13) and rt == reg:                   # addiu/ori: an immediate only if the base is $zero
                val = (x & 0xFFFF) if rs == 0 else None
            elif (op in (8, 10, 11, 12, 14, 15, 32, 33, 35, 36, 37) and rt == reg) or (op == 0 and rd == reg):
                val = None                                    # any other write: id not known statically
        if val is not None and val < N_ACTORS: spawned[val].add(label)

mc = ML.file(114)
scan(mc, 'code'); scan(text_of(ML.file(102)), 'player'); scan(ML.file(507), 'custom code')
for a in range(N_ACTORS):
    vs = entry(mc, ACTOR_TABLE, 0x20, a)[0]
    if vs and vs in ML.byvrom: scan(text_of(ML.file(ML.byvrom[vs])), NAME.get(a, hex(a)))

# ---- changes -------------------------------------------------------------------------------------------------
dc = DB.file(28)
def change(a):
    mvs = entry(mc, ACTOR_TABLE, 0x20, a)[0]; dvs = entry(dc, ACTOR_TABLE, 0x20, a)[0]
    if not mvs and not dvs: return 'in code' if a in (0x15, 0x39) else None
    if not mvs: return 'deleted by ML'
    if not dvs: return f'ML custom ({ML.E[ML.byvrom[mvs]][1] - mvs:#x} bytes)'
    r = check(a).split(': ', 1)[1]
    return None if r == 'vanilla' else r

# ---- carry hand-written columns over -------------------------------------------------------------------------
old = {}
if _os.path.exists(INV):
    for l in open(INV):
        cells = [c.strip() for c in l.strip().strip('|').split('|')]
        if len(cells) < 4 or not l.startswith('|') or cells[0] in ('Actor', '---') or set(cells[0]) <= set('-'): continue
        m = re.match(r'`?(0x[0-9A-Fa-f]+)`?(?:\s|$)', cells[0])   # this script's own format: `0x0e7` Ml_Malon
        key = cells[0].split(' ')[0].strip('`')
        slot = int(m.group(1), 16) if m else BY_NAME.get(key)
        if slot is None:
            m2 = re.search(r'\((0x[0-9A-Fa-f]+)\)', cells[0]); slot = int(m2.group(1), 16) if m2 else None
        if slot is None: continue
        status = cells[-2] if len(cells) >= 6 else cells[-1]
        verified = cells[-1] if len(cells) >= 6 else 'no'
        old[slot] = (status, verified)

# ---- assemble ------------------------------------------------------------------------------------------------
rows = {'placed': [], 'spawned': [], 'orphan': []}
warn = []
for a in range(1, N_ACTORS):
    c = change(a)
    p = placed.get(a); s = spawned.get(a)
    if c == 'deleted by ML':
        if p or s: warn.append(f'{NAME.get(a, hex(a))} ({a:#05x}) is deleted in ML but placed/spawned: {dict(p or {})} {sorted(s or [])}')
        continue
    if c in (None, 'in code'): continue
    status, verified = old.get(a, ('todo', 'no'))
    vn = vanilla_name(a)
    name = f'`{a:#05x}` {NAME.get(a, "?")}' + (f' (was {vn})' if vn and vn != NAME.get(a) else '')
    where = ', '.join(f'{k} ×{v}' if v > 1 else k for k, v in sorted(p.items())) if p else '—'
    who = ', '.join(sorted(x for x in (s or []) if x != NAME.get(a))) or '—'
    row = f'| {name} | {c} | {where} | {who} | {status} | {verified} |'
    rows['placed' if p else 'spawned' if s else 'orphan'].append(row)

HEAD = '| Actor | ML change vs MQ Debug | Placed in (unique entries) | Spawned by | Status | Verified |\n|---|---|---|---|---|---|'
out = f'''# ML actor inventory

Every actor slot whose ML overlay differs from PAL MQ (Debug), or that ML added in a slot vanilla leaves empty.
**Generated by `tools/survey_actors.py --write`**, from the ROMs:

- **Change:** `modcheck` per instruction with relocations masked. "REWRITTEN" means the section sizes differ; "N text / M data words differ" with equal sizes is usually a handful of edits (a shift makes one edit count many words); "hooks" are jumps into ML's custom code at 0x80600000.
- **Placed in:** ML's scene and room actor lists, alternate headers included, each entry counted once.
- **Spawned by:** overlays or code that call `Actor_Spawn` / `Actor_SpawnAsChild` with this id as an immediate. Ids computed at runtime are not seen.

The **Status** and **Verified** columns are hand-maintained and carried over by slot on every regeneration.
- **Status:** `ported` means built and shipped.
- **Verified:** `yes` means seen working in the game, with the date.

Actors living in `code` (En_Item00, En_A_Obj) and slots ML deleted are left out; ML's `code` edits are tracked in `ENGINE_CHANGES.md`.

## Placed in ML scenes ({len(rows["placed"])})

{HEAD}
''' + '\n'.join(rows['placed']) + f'''

## Not placed, spawned at runtime ({len(rows["spawned"])})

{HEAD}
''' + '\n'.join(rows['spawned']) + f'''

## Changed, but no placement or spawner found ({len(rows["orphan"])})

These may be spawned with a computed id, from a cutscene, or not at all. Check before porting.

{HEAD}
''' + '\n'.join(rows['orphan']) + '\n'
if warn:
    out += '\n## Warnings\n\nSlots ML deleted that some surviving code still spawns. These are vanilla paths ML should never reach; the static scan can also report false positives from ML\'s custom code, which mixes code and data.\n\n' + '\n'.join('- ' + w for w in warn) + '\n'

if '--write' in sys.argv:
    open(INV, 'w').write(out); print(f'wrote {INV}: {len(rows["placed"])} placed, {len(rows["spawned"])} spawned, {len(rows["orphan"])} orphan, {len(warn)} warnings')
else:
    print(out)
