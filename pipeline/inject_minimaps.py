"""Port ML's overworld minimap textures.

ML's minimap TABLES are byte-identical to vanilla (verified: owMinimapPosX/PosY/Width/Height and
the entrance-icon arrays all match exactly), so there is nothing to port there -- the minimap was
wrong purely because the TEXTURES were still vanilla's.

ML's map_grand_static is **dma 111** (58192 B), not dma 25: the debug-ROM filelist name at index 25
is misleading, ML replaced that slot with something else. The blob is sliced with the vanilla
owMinimapTexOffset/Size arrays (IA4, dimensions from owMinimapWidth/Height) and each area is written
over the matching vanilla resource, reusing vanilla's header so format and dimensions are identical.

MapData struct: code 0x108998, VRAM base **0x8001ce60** (derived from the struct's own internal
offset signature; the 0x8000b2e0 base recorded earlier does not resolve these pointers).
"""
from mspaths import ML_ROM, OOT_O2R, PIPE
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

shutil.copy(MOD, MOD + '.tmp')
with zipfile.ZipFile(MOD + '.tmp', 'a', zipfile.ZIP_DEFLATED) as z:
    have = set(z.namelist())
    for path, data in added.items():
        if path not in have:
            z.writestr(path, data)
os.replace(MOD + '.tmp', MOD)
print(f"injected {len(added)} overworld minimap textures")
for p_ in sorted(added): print("   ", p_.split('/')[-1])
