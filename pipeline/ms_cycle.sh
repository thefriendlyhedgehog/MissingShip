#!/bin/bash
# Rebuild the ML archive and reload it into the MissingShip.app package. One instance only.
set -e
source "$(dirname "${BASH_SOURCE[0]}")/mspaths.sh"   # MS_ROOT, MS_PIPE, MS_ZAPD, ... (MS-001)
MS="$MS_ROOT"
cd "$MS_PIPE"
cp build_rom_inline.py build_rom.py
python3 build_rom.py > "$MS_PIPE"/cycle_build.log 2>&1
python3 - <<'EOG'
import os, sys; P=os.environ['MS_PIPE']; sys.path.insert(0,P)
from ml_map import SCENES
lines=open(P+'/filelists/dbg.txt').read().split('\n')
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
# Objects ML only re-textured (same size and layout as MQ debug's): named ml_* so inject_obj_textures.py can
# extract them with texture-only copies of SoH's own XMLs and ship the redrawn textures under vanilla paths (MS-020).
ML_OBJECTS.update({351: 'ml_gameplay_dangeon_keep', 358: 'ml_object_box', 367: 'ml_object_bombf',
                   36: 'ml_object_bdoor', 395: 'ml_object_haka_door', 58: 'ml_object_jya_obj'})
for _i, _n in ML_OBJECTS.items(): lines[_i] = _n
# dma 114 is ML's `code` file, which holds the audio tables. Audio.xml looks it up BY NAME,
# so the rewrite above must not leave it as mlfile_114 or ZAPD's ZAudio segfaults on a null file.
lines[114]='code'
open(P+'/filelists/dbg.txt','w').write('\n'.join(lines)+'\n')
EOG
python3 "$MS_PIPE"/gen_xml_inline.py
cd "$MS_ZAPD_CWD"
rm -f "$MS_PIPE"/ml.otr
"$MS_ZAPD" ed -se OTR --otrfile "$MS_PIPE"/ml.otr --portVer 9.1.1 \
  -i "$MS_PIPE"/xml/scenes -b "$MS_PIPE"/tml_zapd.z64 -fl "$MS_PIPE"/filelists \
  -o "$MS_PIPE"/out -osf "$MS_PIPE"/out -gsf 0 -rconf "$(python3 "$MS_PIPE"/mspaths.py --zapd-config)" \
  > "$MS_PIPE"/cycle_zapd.log 2>&1
echo "texDecl errors: $(grep -c 'texDecl == nullptr' "$MS_PIPE"/cycle_zapd.log || true)"
ML_SKIP_SLOTS="" python3 "$MS_PIPE"/inject_titles.py
python3 "$MS_PIPE"/inject_minimaps.py
python3 "$MS_PIPE"/inject_messages.py
python3 "$MS_PIPE"/inject_objects.py
python3 "$MS_PIPE"/inject_ui_textures.py
python3 "$MS_PIPE"/inject_obj_textures.py
python3 "$MS_PIPE"/inject_audio.py
cp "$MS_PIPE"/ml.otr "$MS"/mod/MissingLink_Scenes.o2r
pkill -x MissingShip 2>/dev/null || true
sleep 1
cd "$MS" && (nohup ./MissingShip.sh > "$MS_PIPE"/ms_run.log 2>&1 &)
