#!/bin/bash
# One test cycle: rebuild ROM (honouring ML_SKIP_FMT), regen XML from the manifest,
# repack the O2R, install into ml_profile, relaunch, and report liveness + assertions.
set -e
cd /Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline
cp build_rom_inline.py build_rom.py
python3 build_rom.py > /Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/cycle_build.log 2>&1
python3 - <<'EOG'
import sys; sys.path.insert(0,'/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline')
from ml_map import SCENES
lines=open('/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/filelists/dbg.txt').read().split('\n')
while lines and lines[-1]=='': lines.pop()
for i,l in enumerate(lines):
    if l.endswith('_scene') or '_room_' in l: lines[i]=f'mlfile_{i}'
for s,info in SCENES.items():
    lines[s]=info['scene']
    for n,r in enumerate(info['rooms']): lines[r]=f"{info['scene'][:-6]}_room_{n}"
# ML's custom objects must be reachable by name for xml/objects/*.xml. ML reorganised dmadata,
# so these indices carry object data under unrelated vanilla names.
ML_OBJECTS = {12: 'object_ml_fishpond',    # object slot 0x40 (OBJECT_JJ in vanilla)
              356: 'object_ml_villager',   # object slot 0x0C (OBJECT_DODONGO in vanilla)
              75: 'object_ml_lift',        # object slot 0x11D (OBJECT_D_LIFT in vanilla)
              # GetItem models ML refilled (see docs/ENGINE_CHANGES.md, "pickup models")
              52: 'object_ml_gi_dekupass',     # object slot 0xD7  (object_gi_gerudo: Gerudo Card -> Deku Pass)
              56: 'object_ml_gi_hourglass',    # object slot 0xE8  (object_gi_boomerang: Boomerang -> Magic Hourglass)
              359: 'object_ml_gi_nutmilk',     # object slot 0x11  (UNSET in vanilla: Red Potion -> Deku Nut Milk)
              376: 'object_ml_gi_certificate', # object slot 0x146 (object_gi_prescription: Stone of Agony -> Birth Certificate)
              391: 'object_ml_gi_soulsword',   # object slot 0x17B (object_gi_dekupouch: Bullet Bag 40 -> Soul-Charged Kokiri Sword)
              24: 'object_ml_treebridge',      # object slot 0x7C (object_wood02; ML appended the tree-bridge model)
              399: 'object_ml_songblock',      # object slot 0x18F (object_ouke_haka: ML's song-opened block, actor 0x1D1)
              354: 'object_ml_cucco',          # object slot 0x09 (object_poh: ML's recoloured cucco, actor 0x0D)
              55: 'object_ml_ma1',             # object slot 0xE0 (object_ma1: ML's redrawn young Malon, actor 0xE7)
              33: 'object_ml_crackwall',       # object slot 0xA3 (object_ru1: ML's breakable wall for Bg_Hidan_Kowarerukabe)
              349: 'object_ml_gkeep',          # object slot 0x01 (gameplay_keep: ML's grotto hole + redrawn bomb drop)
              353: 'object_ml_scrub',          # object slot 0x05 (ML-only: Deep Woods hint scrubs)
              352: 'object_ml_prologue'}       # object slot 0x04 (ML-only: prologue illustrations, actor 0x01F)
# ML's pause/HUD texture files, same layout as MQ's but redrawn. Named ml_* because dma 7-16 still carry
# the vanilla names over unrelated ML data; inject_ui_textures.py maps the output back to vanilla paths.
ML_OBJECTS.update({82: 'ml_icon_item_static', 83: 'ml_icon_item_24_static', 84: 'ml_icon_item_field_static',
                   87: 'ml_icon_item_nes_static', 90: 'ml_item_name_static', 91: 'ml_map_name_static',
                   125: 'ml_parameter_static'})
for _i, _n in ML_OBJECTS.items(): lines[_i] = _n
# dma 114 is ML's `code` file, which holds the audio tables. Audio.xml looks it up BY NAME,
# so the rewrite above must not leave it as mlfile_114 or ZAPD's ZAudio segfaults on a null file.
lines[114]='code'
open('/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/filelists/dbg.txt','w').write('\n'.join(lines)+'\n')
EOG
python3 /Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/gen_xml_inline.py
cd /Volumes/MacMiniExternal/AI_Projects/SOHCombo/soh
rm -f /Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/ml.otr
../build-macos-sdl2/ZAPD/ZAPD.out ed -se OTR --otrfile /Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/ml.otr --portVer 9.1.1 \
  -i /Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/xml/scenes -b /Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/tml_zapd.z64 -fl /Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/filelists \
  -o /Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/out -osf /Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/out -gsf 0 -rconf /Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/Config_ML.xml \
  > /Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/cycle_zapd.log 2>&1
echo "texDecl errors: $(grep -c 'texDecl == nullptr' /Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/cycle_zapd.log || true)"
pkill -f "soh-macos" 2>/dev/null || true
pkill -f "${ML_PGREP:-SOH/soh.app/Contents/MacOS/soh}" 2>/dev/null || true
cp /Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/ml.otr /Volumes/MacMiniExternal/AI_Projects/SOH/ml_profile/mods/MissingLink_Scenes.o2r
cd /Volumes/MacMiniExternal/AI_Projects/SOH
rm -f /Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/run.log
(nohup ${ML_LAUNCHER:-./run_ml.sh} > /Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/run.log 2>&1 &)
# Wait for a decisive outcome: the attract demo loads spot00 (sceneNum 0x51) ~2-3 min in,
# and that is when the texture crash fires. Poll for the assertion, or for the scene to load
# and then survive 25s, rather than sampling at a fixed time (which reported false OKs).
n=0; VERDICT="TIMEOUT"
while [ $n -lt 90 ]; do
  n=$((n+1))
  if grep -qi assertion /Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/run.log 2>/dev/null; then VERDICT="CRASH"; break; fi
  if ! pgrep -f "${ML_PGREP:-SOH/soh.app/Contents/MacOS/soh}" >/dev/null; then VERDICT="EXITED"; break; fi
  if grep -q "sceneNum: 0x51" /Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/run.log 2>/dev/null || [ $n -gt 18 ]; then
    sleep 25
    if grep -qi assertion /Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/run.log; then VERDICT="CRASH"; else VERDICT="OK"; fi
    break
  fi
  sleep 4
done
P=$(pgrep -f "${ML_PGREP:-SOH/soh.app/Contents/MacOS/soh}" | head -1)
echo "RESULT: $VERDICT  pid=${P:-none} skip_range=${ML_SKIP_RANGE:-none}"
echo "inlined: $(grep -o 'inlined_tex=[0-9]*' /Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/cycle_build.log | awk -F= '{s+=$2} END{print s+0}')"
