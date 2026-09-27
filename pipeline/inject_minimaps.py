"""Port ML's overworld minimap textures, its dungeon minimaps and pause dungeon maps (MS-022), and its skyboxes (MS-025).

ML's minimap TABLES are byte-identical to vanilla (verified: owMinimapPosX/PosY/Width/Height and
the entrance-icon arrays all match exactly), so there is nothing to port there -- the minimap was
wrong purely because the TEXTURES were still vanilla's.

ML's map_grand_static is **dma 111** (58192 B), not dma 25: the debug-ROM filelist name at index 25
is misleading, ML replaced that slot with something else. The blob is sliced with the vanilla
owMinimapTexOffset/Size arrays (IA4, dimensions from owMinimapWidth/Height) and each area is written
over the matching vanilla resource, reusing vanilla's header so format and dimensions are identical.

MapData struct: code 0x108998, VRAM base **0x8001ce60** (derived from the struct's own internal
offset signature; the 0x8000b2e0 base recorded earlier does not resolve these pointers).

Same-layout files: ML's map_i_static (dma 113, minimaps), map_48x85_static (dma 112, pause maps) and the vr_fine1 /
vr_cloud2 skyboxes and palettes (dma 128/129, 138/139) have the same size and layout as the MQ debug ROM's, so SoH's own
XMLs give every texture's offset and size, and stock stores the raw N64 bytes behind a 0x50-byte header. A texture ships only if ML's
bytes differ from the debug ROM's (ML redrew it) and from stock's, the same baseline rule as inject_obj_textures.py.
The engine side (minimap index room + 190, ML's map tables) is in z_map_exp.c / z_map_data.c.
"""
from mspaths import ML_ROM, OOT_O2R, PIPE, SOH
import re
import struct, sys, zipfile, shutil, os
sys.path.insert(0, PIPE)
from yaz0 import get_file

OOT = OOT_O2R
MOD = PIPE + '/ml.otr'
BASE, T, BLOB_DMA = 0x8001ce60, 0x108998, 111

AREA_TEX = {
    0:['gHyruleFieldMinimapTex'], 1:['gKakarikoVillageMinimapTex'],
    2:['gGraveyardMinimapTex','gExploredShadowGraveyardMinimapTex'],
    3:['gZorasRiverMinimapTex'], 4:['gKokiriForestMinimapTex'],
    5:['gSacredMeadowMinimapTex'],
    6:['gLakeHyliaMinimapTex','gDrainedLakeHyliaMinimapTex'],
    7:['gZorasDomainMinimapTex'], 8:['gZorasFountainMinimapTex'],
    9:['gGerudoValleyMinimapTex','gGerudoValleyWithBrokenBridgeMinimapTex'],
    10:['gLostWoodsMinimapTex'], 11:['gDesertColossusMinimapTex'],
    12:['gGerudosFortressMinimapTex','gGerudosFortessMinimapTex'],
    13:['gHauntedWastelandMinimapTex'], 14:['gHyruleCastleAreaMinimapTex'],
    15:['gDeathMountainTrailMinimapTex'], 16:['gDeathMountainCraterMinimapTex'],
    17:['gGoronCityMinimapTex'], 18:['gLonLonRanchMinimapTex'],
    19:['gOutsideGanonsCastleMinimapTex'],
}

rom = open(ML_ROM, 'rb').read()
E = []; p = 0x12f70
while True:
    a, b, c, d = struct.unpack('>IIII', rom[p:p+16])
    if a == 0 and b == 0 and c == 0 and d == 0: break
    E.append((a, b, c, d)); p += 16
blob = get_file(rom, *E[BLOB_DMA])

code = open(PIPE + '/ml_code.bin', 'rb').read()
ptrs = [struct.unpack('>I', code[T+i*4:T+i*4+4])[0] for i in range(28)]
def arr(idx, n, signed=True):
    o = ptrs[idx] - BASE
    return [struct.unpack('>h' if signed else '>H', code[o+k*2:o+k*2+2])[0] for k in range(n)]
size, off = arr(9, 24), arr(10, 24, False)

oot = zipfile.ZipFile(OOT)
paths = {n.split('/')[-1]: n for n in oot.namelist() if 'map_grand_static' in n}

added = {}
for area, texnames in AREA_TEX.items():
    o, s = off[area], size[area]
    if o + s > len(blob): continue
    data = blob[o:o+s]
    for tn in texnames:
        path = paths.get(tn)
        if not path: continue
        v = oot.read(path)
        fmt, w, h, ds = struct.unpack('<IIII', v[0x40:0x50])
        if ds != len(data):       # only replace when the slot is byte-compatible
            continue
        added[path] = v[:0x50] + data

# Texture files with the MQ debug ROM's layout: dungeon minimaps and pause dungeon maps (MS-022), and the day and
# overcast-sunset skyboxes with their palettes (MS-025). SoH texture file -> (ML dma index, SoH XML holding its File).
SAME_LAYOUT = {'map_i_static': (113, 'map_i_static.xml'), 'map_48x85_static': (112, 'map_48x85_static.xml'),
               'vr_fine1_static': (128, 'skyboxes.xml'), 'vr_fine1_pal_static': (129, 'skyboxes.xml'),
               'vr_cloud2_static': (138, 'skyboxes.xml'), 'vr_cloud2_pal_static': (139, 'skyboxes.xml')}
BPP = {'i4': 0.5, 'ci4': 0.5, 'ci8': 1, 'rgba16': 2}
import baseline                                   # ML-vs-debug answers, recorded for players' builds (MS-154)
baseline.begin('inject_minimaps')
stock = set(oot.namelist())
same = {}
for name, (dma, xml_name) in SAME_LAYOUT.items():
    ml = get_file(rom, *E[dma])
    if baseline.RECORD:
        dbg = baseline.debug_file(name)
        if len(ml) != len(dbg):
            sys.exit(f'{name}: ML dma {dma} is {len(ml)} bytes, debug is {len(dbg)}: layout assumption broken')
    xml = open(os.path.join(SOH, 'assets', 'xml', 'GC_MQ_PAL_F', 'textures', xml_name)).read()
    xml = re.search(rf'<File Name="{name}"[^>]*>(.*?)</File>', xml, re.S).group(1)
    for m in re.finditer(r'<Texture Name="(\w+)"[^>]*Format="(\w+)"[^>]*Width="(\d+)"[^>]*Height="(\d+)"'
                         r'[^>]*Offset="(0x[0-9A-Fa-f]+)"', xml):
        tn, fmt, w, h, o = m.group(1), m.group(2), int(m.group(3)), int(m.group(4)), int(m.group(5), 16)
        n = int(w * h * BPP[fmt])
        path = f'textures/{name}/{tn}'
        if not baseline.value(path, lambda: ml[o:o + n] != dbg[o:o + n]) or path not in stock:
            continue                              # ML didn't redraw it
        v = oot.read(path)
        if struct.unpack('<I', v[0x4C:0x50])[0] != n:
            sys.exit(f'{path}: stock holds {struct.unpack("<I", v[0x4C:0x50])[0]} bytes, expected {n}')
        if v[0x50:] != ml[o:o + n]:
            same[path] = v[:0x50] + ml[o:o + n]
baseline.save()
added.update(same)
# Owner's request: ML's skies without the face seams (pipeline/sky_seams.py).
from sky_seams import fix_skyboxes
print('seamless skyboxes:', fix_skyboxes(added))

shutil.copy(MOD, MOD + '.tmp')
with zipfile.ZipFile(MOD + '.tmp', 'a', zipfile.ZIP_DEFLATED) as z:
    have = set(z.namelist())
    for path, data in added.items():
        if path not in have:
            z.writestr(path, data)
os.replace(MOD + '.tmp', MOD)
by = {}
for p_ in same:
    by[p_.split('/')[1]] = by.get(p_.split('/')[1], 0) + 1
print(f"injected {len(added) - len(same)} overworld minimap textures, {len(same)} same-layout textures {by}")
for p_ in sorted(added): print("   ", '/'.join(p_.split('/')[-2:]))
