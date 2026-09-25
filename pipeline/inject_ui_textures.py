"""Port ML's redrawn pause-menu / HUD textures (item icons, item names, area names, HUD).

ML keeps vanilla's layout for these files (each is byte-for-byte the size of its MQ debug counterpart),
so SoH's own MQ XMLs describe them. xml/ui/ holds copies renamed ml_<file> -- dma 7-16 still carry the
vanilla names over unrelated ML data -- and this step maps the output back to textures/<vanilla file>/.
Only textures whose pixels differ from stock SoH's are shipped, so the mod overrides exactly what ML
redrew. Pure data: drop-in compatible.
"""
import subprocess, zipfile, os, sys

P    = '/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline'
ZAPD = '/Volumes/MacMiniExternal/AI_Projects/SOHCombo/build-macos-sdl2/ZAPD/ZAPD.out'
CWD  = '/Volumes/MacMiniExternal/AI_Projects/SOHCombo/soh'
OOT  = '/Volumes/MacMiniExternal/AI_Projects/MissingShip/home/oot.o2r'
OUT  = P + '/ml_ui.otr'
MOD  = P + '/ml.otr'

if os.path.exists(OUT):
    os.remove(OUT)
r = subprocess.run([ZAPD, 'ed', '-se', 'OTR', '--otrfile', OUT, '--portVer', '9.1.1',
                    '-i', P + '/xml/ui', '-b', P + '/tml_zapd.z64',
                    '-fl', P + '/filelists', '-o', P + '/out',
                    '-osf', P + '/out', '-gsf', '0',
                    '-rconf', P + '/Config_ML.xml'],
                   cwd=CWD, capture_output=True, text=True)
if r.returncode != 0 or not os.path.exists(OUT):
    print(f"UI texture extraction FAILED (exit {r.returncode})")
    print(r.stdout[-2000:], r.stderr[-2000:])
    sys.exit(1)

src, oot = zipfile.ZipFile(OUT), zipfile.ZipFile(OOT)
stock = set(oot.namelist())
keep, same, missing = {}, 0, 0
for n in src.namelist():
    parts = n.split('/')           # ZAPD names them ml_<file>/<texture> (no textures/ prefix)
    if len(parts) != 2 or not parts[0].startswith('ml_'):
        continue
    van = f'textures/{parts[0][3:]}/{parts[1]}'
    if van not in stock:
        missing += 1            # e.g. a language stock SoH does not ship
        continue
    mine = src.read(n)
    if mine[4:8] != b'XETO':    # textures only: DLs/vertices would carry ml_* paths that do not exist
        continue
    if mine[0x40:] == oot.read(van)[0x40:]:
        same += 1
        continue
    keep[van] = mine

ui_dirs = {n.split('/')[0][3:] for n in src.namelist() if n.startswith('ml_')}
with zipfile.ZipFile(MOD) as old, zipfile.ZipFile(MOD + '.tmp', 'w', zipfile.ZIP_DEFLATED) as z:
    for info in old.infolist():
        parts = info.filename.split('/')
        if parts[0] == 'textures' and len(parts) == 3 and parts[1] in ui_dirs:
            continue            # REPLACE semantics
        z.writestr(info, old.read(info.filename))
    for van, data in keep.items():
        z.writestr(van, data)
os.replace(MOD + '.tmp', MOD)
by = {}
for v in keep: by[v.split('/')[1]] = by.get(v.split('/')[1], 0) + 1
print(f"UI textures: {len(keep)} redrawn by ML shipped ({by}), {same} identical to stock skipped, "
      f"{missing} with no stock counterpart")
