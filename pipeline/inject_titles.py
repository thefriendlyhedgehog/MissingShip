"""Port ML's place-name title cards over the vanilla ones.

ML ships its own title cards (scene table's titleFile) as IA8 **144x16** = 2304 bytes; vanilla's are
IA8 **144x24** = 3456. The engine draws the card with hardcoded 144x24
(z_player.c: TitleCard_InitPlaceName(..., 144, 24, 20)), so rather than patch the engine we pad
ML's image into a 144x24 canvas (IA8 0x00 = transparent) and reuse VANILLA's resource header
verbatim -- same format and dimensions, only the pixels change.
Resources live at textures/g_pn_XX/<Name>ENGTex and ...JPNTex; the g_pn_XX name for a scene id comes
from scene_table.h's second DEFINE_SCENE argument.
"""
import struct, sys, json, re, zipfile, shutil, os
sys.path.insert(0, '/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline')
from yaz0 import get_file

OOT = '/Volumes/MacMiniExternal/AI_Projects/MissingShip/home/oot.o2r'
MOD = '/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/ml.otr'
HDR = 0x50           # resource header size; payload follows

rom = open('/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/tml_2_0.z64', 'rb').read()
E = []; p = 0x12f70
while True:
    a, b, c, d = struct.unpack('>IIII', rom[p:p+16])
    if a == 0 and b == 0 and c == 0 and d == 0: break
    E.append((a, b, c, d)); p += 16

sc = open('/Volumes/MacMiniExternal/AI_Projects/SOHCombo/soh/include/tables/scene_table.h').read()
titles = {int(m.group(1), 16): m.group(3)
          for m in re.finditer(r'/\* 0x([0-9A-Fa-f]{2}) \*/ DEFINE_SCENE\((\w+),\s*(\w+),', sc)}

st = json.load(open('/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/ml_scenetable.json'))
oot = zipfile.ZipFile(OOT)
by_pn = {}
for n in oot.namelist():
    m = re.match(r'textures/(g_pn_\d+)/', n)
    if m: by_pn.setdefault(m.group(1), []).append(n)

added = {}
blanked = []
for e in st:
    if e['scene_dma'] is None: continue
    pn = titles.get(e['slot'])
    if not pn or pn == 'none' or pn not in by_pn: continue
    pad = bytearray(144 * 24)                        # 0x00 = transparent
    if e['title_dma'] is None:
        # ML gives this scene no title card, so ML shows nothing on entry. Stock SoH would still draw
        # vanilla's card for the slot (e.g. "Sacred Forest Meadow" over ML's Saria's house), so ship a
        # fully transparent one.
        blanked.append(e['slot'])
    else:
        ml = get_file(rom, *E[e['title_dma']])      # 144x16 IA8
        pad[144*4 : 144*4 + len(ml)] = ml            # centre the 16 rows vertically
    for path in by_pn[pn]:
        vres = oot.read(path)
        fmt, w, h, ds = struct.unpack('<IIII', vres[0x40:0x50])
        if (w, h) != (144, 24) or ds != len(pad): continue
        added[path] = vres[:HDR] + bytes(pad)

# append into the mod archive (mod entries override the base oot.o2r by path)
shutil.copy(MOD, MOD + '.tmp')
with zipfile.ZipFile(MOD + '.tmp', 'a', zipfile.ZIP_DEFLATED) as z:
    existing = set(z.namelist())
    for path, data in added.items():
        if path not in existing:
            z.writestr(path, data)
os.replace(MOD + '.tmp', MOD)
print(f"injected {len(added)} title-card resources for {len(added)//2} scenes "
      f"({len(blanked)} blanked because ML has no card: slots {sorted(blanked)})")
