"""The hack under analysis and its baseline ROM, from a per-hack config (MS-090).

The config is `hacks/<name>/hack.json`: `MS_HACK` names it (a directory or the file), default `hacks/ml`. The tools
assume the hack keeps `code` at zero shift against the baseline (true of ML against PAL MQ Debug), so every `code`
offset and address below is the baseline's and holds for both.

    HACK, BASE        the two Roms (loaded on first use)       CODE_VRAM, CODE_LEN, BOOT     address ranges
    H, B              each side's dma indices (code_dma, ...)   ACTOR_TABLE, ACTOR_STRIDE, N_ACTORS
    CUSTOM            [(dma, vram)] of the hack's custom code   SPAWN, SPAWN_CHILD    Actor_Spawn / _SpawnAsChild
"""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '..', 'pipeline'))
import json, types
import mspaths
from mlrom import Rom

_path = _os.environ.get('MS_HACK', _os.path.join(mspaths.ROOT, 'hacks', 'ml'))
CONFIG = _os.path.abspath(_os.path.join(_path, 'hack.json') if _os.path.isdir(_path) else _path)
_cfg = json.load(open(CONFIG, encoding='utf-8'))
NAME = _cfg['name']

def _side(d):
    return types.SimpleNamespace(**{k: int(v, 0) if isinstance(v, str) and k not in ('rom', 'name') else v
                                    for k, v in d.items()})

H, B = _side(_cfg['hack']), _side(_cfg['baseline'])
HACK_ROM = _os.path.join(mspaths.ROOT, H.rom) if hasattr(H, 'rom') else mspaths.ML_ROM
BASE_ROM = _os.path.join(mspaths.ROOT, B.rom) if hasattr(B, 'rom') else mspaths.DEBUG_ROM
CUSTOM = [(int(c['dma'], 0), int(c['vram'], 0)) for c in _cfg['hack'].get('custom_code', [])]

CODE_VRAM, ACTOR_TABLE, ACTOR_STRIDE, N_ACTORS = B.code_vram, B.actor_table, B.actor_stride, B.actor_count
SPAWN, SPAWN_CHILD = B.actor_spawn, B.actor_spawn_as_child

_roms = {}
def __getattr__(name):
    """HACK / BASE and the ranges measured from them load lazily, so importing this module stays cheap."""
    if name == 'HACK':
        return _roms.setdefault(name, Rom(HACK_ROM, dma=H.dmadata))
    if name == 'BASE':
        return _roms.setdefault(name, Rom(BASE_ROM, dma=B.dmadata))
    if name == 'CODE_LEN':
        return len(__getattr__('BASE').file(B.code_dma))
    if name == 'BOOT':
        return (B.boot_vram, B.boot_vram + len(__getattr__('BASE').file(B.boot_dma)))
    raise AttributeError(name)

def custom_region(i=0):
    """(dma, vram, end) of the hack's i-th custom code file."""
    dma, vram = CUSTOM[i]
    return dma, vram, vram + len(__getattr__('HACK').file(dma))

if __name__ == '__main__':
    print(f'{NAME}  ({_os.path.relpath(CONFIG, mspaths.ROOT)})')
    print(f'  hack ROM  {HACK_ROM}' + ('' if _os.path.exists(HACK_ROM) else '   (missing)'))
    print(f'  baseline  {BASE_ROM}' + ('' if _os.path.exists(BASE_ROM) else '   (missing)'))
    print(f'  code dma {H.code_dma} / {B.code_dma} at {CODE_VRAM:#x}, actor table +{ACTOR_TABLE:#x}; custom code: '
          + (', '.join(f'dma {d} at {v:#x}' for d, v in CUSTOM) or 'none'))
