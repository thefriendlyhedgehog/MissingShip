"""Find every place ML's code reaches outside vanilla: jumps and pointers into ML's custom code (MS-006).

Scans `code`, ML's custom code (dma 507) and every relocatable overlay in the ROM (actors, effects, game states,
pause menu, player, map-mark data): anything with a valid relocation header.

- **Jumps:** `j`/`jal` whose target is absolute. An overlay's relocation table marks its own internal jumps (R_MIPS_26),
  so any unrelocated jump in an overlay, and every jump in `code`, leaves the file. Each target is classified: boot,
  `code`, custom code, or unknown.
- **Pointers:** unrelocated 32-bit data words that point into the custom region (function pointers, tables).

Each file is named by the table entry that points at it: the same `code` offset in PAL MQ Debug names the vanilla
file, because ML's `code` has zero shift against it.

Usage: hookscan.py [--md]   (--md prints the Markdown tables the ledger uses)
"""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..', 'pipeline'))
from mspaths import SOH
import struct, sys, collections, re
from ovldiff import ML, DB

BOOT = (0x80000460, 0x80000460 + 0x11F10)
CODE = (0x8001CE60, 0x8001CE60 + 0x13AF30)
CUSTOM = (0x80600000, 0x80608000)          # dma 507, loaded by boot's last loader entry
CODE_DMA, CUSTOM_DMA = 114, 507
RAM_END = 0x80800000                        # 8 MB: a "target" beyond this is data decoded as an instruction
# ML's player overlay (dma 102, text 0x21200) always loads at this RAM address: ML's heap layout is fixed. Derived from
# the Hourglass hook (player +0x17E5C replaces vanilla `jal 0x80848F9C` = player +0x16D8C; the custom routine calls
# 0x804E913C = that function) and confirmed by the two trampoline returns (+0x17E68, +0x20F40).
PLAYER_DMA, PLAYER_RAM, PLAYER_LEN = 102, 0x804D23B0, 0x21200 + 0x1E30 + 0x310

def W(d, o): return struct.unpack('>I', d[o:o+4])[0]

def region(t):
    if BOOT[0] <= t < BOOT[1]: return 'boot'
    if CODE[0] <= t < CODE[1]: return 'code'
    if CUSTOM[0] <= t < CUSTOM[1]: return 'custom'
    if PLAYER_RAM <= t < PLAYER_RAM + PLAYER_LEN: return f'player +{t - PLAYER_RAM:#x}'
    if 0x80000400 <= t < RAM_END: return 'unknown'
    return None                                # not an address: data, not a jump

def overlay(d):
    """(text, data, rodata, relocated word offsets) of a relocatable overlay, or None."""
    if len(d) < 0x20: return None
    tail = W(d, len(d) - 4)
    if not (0x14 <= tail < len(d)) or tail & 3: return None
    o = len(d) - tail
    t, da, ro, bs, n = struct.unpack('>IIIII', d[o:o+20])
    if t + da + ro != o or n > 0x10000 or o + 20 + 4 * n > len(d): return None
    rel = set()
    for r in struct.unpack(f'>{n}I', d[o+20:o+20+4*n]):
        sec = (r >> 30) & 3
        base = {1: 0, 2: t, 3: t + da}.get(sec)
        if base is not None: rel.add(base + (r & 0xFFFFFF))
    return t, da, ro, rel

# ---- name every file by the code-table entries that point at it -----------------------------------------------
dbg_names = open(SOH + '/assets/extractor/filelists/dbg.txt').read().split('\n')
dbg_by_vrom = {e[0]: dbg_names[i] for i, e in enumerate(DB.E) if i < len(dbg_names)}
actor_names = {}
for l in open(SOH + '/include/tables/actor_table.h'):
    m = re.match(r'/\* (0x[0-9A-F]+) \*/ DEFINE_ACTOR\w*\((\w+)', l)
    if m: actor_names[int(m.group(1), 16)] = m.group(2)
mc, dc = ML.file(CODE_DMA), DB.file(28)
refs = collections.defaultdict(set)
for o in range(0, len(mc) - 8, 4):
    v = W(mc, o)
    if v in ML.byvrom and v and W(mc, o + 4) == ML.E[ML.byvrom[v]][1]:   # a {vromStart, vromEnd} table entry
        refs[ML.byvrom[v]].add(o)

def name(i):
    if i == CODE_DMA: return 'code'
    if i == CUSTOM_DMA: return 'ML custom code (dma 507)'
    labels = set()
    for o in refs.get(i, ()):
        if 0xF9440 <= o < 0xF9440 + 0x20 * 0x1D7 and (o - 0xF9440) % 0x20 == 0:
            a = (o - 0xF9440) // 0x20
            labels.add(f'actor {a:#05x} {actor_names.get(a, "?")}')
        else:
            dn = dbg_by_vrom.get(W(dc, o))
            if dn: labels.add(dn)
    return ', '.join(sorted(labels)) or f'dma {i} (unreferenced)'

# ---- scan ------------------------------------------------------------------------------------------------------
jumps = collections.defaultdict(list)     # dma -> [(offset, insn, target, region)]
ptrs = collections.defaultdict(list)      # dma -> [(offset, value)]
def scan(i, d, text_len, rel, vanilla=None):
    for o in range(0, text_len, 4):
        w = W(d, o)
        if vanilla is not None and o + 4 <= len(vanilla) and W(vanilla, o) == w: continue
        if w >> 26 in (2, 3) and o not in rel:
            t = 0x80000000 | ((w & 0x3FFFFFF) << 2)
            reg = region(t)
            if reg not in (None, 'code', 'boot'):
                jumps[i].append((o, 'jal' if w >> 26 == 3 else 'j', t, reg))
    if vanilla is not None: return           # `code`'s pointers into the custom region show up as jumps above
    for o in range(text_len, len(d) - 4, 4):
        v = W(d, o)
        if o not in rel and CUSTOM[0] <= v < CUSTOM[1] and v & 3 == 0:
            ptrs[i].append((o, v))

# `code` lines up word for word with MQ Debug's (zero shift), so only words ML changed can be hooks. That also
# drops the RSP microcode and data inside `code`, which decode as nonsense jumps.
scan(CODE_DMA, mc, len(mc), set(), vanilla=dc)
for i in range(len(ML.E)):
    if i in (CODE_DMA, CUSTOM_DMA) or ML.E[i][0] == ML.E[i][1]: continue
    try: d = ML.file(i)
    except Exception: continue
    ov = overlay(d)
    if ov: scan(i, d, ov[0], ov[3])

# ML's custom code: which of its own calls leave it (into code is normal; anything else is a lead)
cc = ML.file(CUSTOM_DMA)
out_of_custom = []          # (offset, insn, target, region) for targets outside code/boot/custom
for o in range(0, len(cc), 4):
    w = W(cc, o)
    if w >> 26 in (2, 3):
        t = 0x80000000 | ((w & 0x3FFFFFF) << 2)
        r = region(t)
        if r not in (None, 'code', 'boot', 'custom'):
            out_of_custom.append((o, 'jal' if w >> 26 == 3 else 'j', t, r))
live = {i for i in jumps if refs.get(i) or i == CODE_DMA}

def fmt(i):
    return ', '.join(f'+{o:#x} `{ins} {t:#x}`' + ('' if r == 'custom' else f' ({r})') for o, ins, t, r in jumps[i])

if '--md' in sys.argv:
    print('| File | Hooks (offset: jump → target) |\n|---|---|')
    for i in sorted(live, key=lambda i: (i != CODE_DMA, name(i))):
        print(f'| {name(i)} | {fmt(i)} |')
    dead = sorted(set(jumps) - live)
    if dead:
        print('\nNever loaded (no table entry points at them), listed for completeness:\n')
        print('| File | Hooks |\n|---|---|')
        for i in dead: print(f'| dma {i} | {fmt(i)} |')
    print('\nCustom code jumping outside `code`, boot and itself:\n')
    for o, ins, t, r in out_of_custom: print(f'- +{o:#x} `{ins} {t:#x}` → {r}')
    print('\nData pointers into the custom region: ' + ('none' if not ptrs else
          '; '.join(f'{name(i)}: ' + ', '.join(f'+{o:#x}={v:#x}' for o, v in ptrs[i]) for i in ptrs)))
else:
    for i in sorted(jumps, key=lambda i: (i != CODE_DMA, name(i))):
        print(f'{name(i)}  [dma {i}]' + ('' if i in live else '  (never loaded)'))
        for o, ins, t, r in jumps[i]: print(f'   +{o:#07x}  {ins:3} {t:#x}  {r}')
    print('\ndata pointers into the custom region:', 'none' if not ptrs else '')
    for i in sorted(ptrs): print(f'  {name(i)} [dma {i}]: ' + ', '.join(f'+{o:#x}={v:#x}' for o, v in ptrs[i]))
    print('\ncustom code jumping outside code/boot/itself:')
    for o, ins, t, r in out_of_custom: print(f'   +{o:#06x}  {ins:3} {t:#x}  {r}')
