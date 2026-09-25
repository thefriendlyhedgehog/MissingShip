#!/usr/bin/env python3
"""Rebuild the ML mod archive and install it (MS-082: the cross-platform form of the old ms_cycle.sh).

    python3 pipeline/ms_cycle.py              build, install into mod/, relaunch MissingShip (macOS)
    python3 pipeline/ms_cycle.py --no-launch  build and install only (any OS)

Steps: rebuild the ROM -> name ML's files in the filelist -> generate XMLs -> ZAPD -> the inject_* steps ->
copy pipeline/ml.otr to mod/MissingLink_Scenes.o2r. Paths come from mspaths.py (MS-001), so MS_* env overrides
work as before. Relaunching only exists for the macOS app so far; Windows and Linux packages are MS-080 / MS-081.
"""
import os, shutil, subprocess, sys, time

import mspaths
from mspaths import ROOT, PIPE

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
              352: 'object_ml_prologue',       # object slot 0x04 (ML-only: prologue illustrations, actor 0x01F)
              32: 'object_ml_spot02',         # object slot 0xA1 (object_spot02_objects: ML's own model for Bg_Spot02_Objects)
              51: 'object_ml_slideblock',      # object slot 0xD3 (object_tw: ML's sliding blocks, actor 0xDC)
              380: 'object_ml_elevator',       # object slot 0x154 (object_ka: ML's elevators, actor 0x17B)
              22: 'object_ml_hakalift',        # object slot 0x72 (object_mori_objects: ML's room 2 lift, actor 0x87)
              6: 'object_link_child',          # object slot 0x15 (object_link_child: ML's child Link, shipped under the vanilla names, MS-050)
              26: 'object_ml_bv',              # object slot 0x8C (object_bv; ML's own Shadow Temple boss, actors 0xBA and 0x17)
              30: 'object_ml_du',              # object slot 0x9D (object_du; ML's Bmori1 pillars, actor 0x98)
              38: 'object_ml_nb',              # object slot 0xB3 (object_nb; ML's Bmori1 thieves, actor 0xC3)
              378: 'object_ml_mag',            # object slot 0x14A (object_mag; ML's own title screen layout, actor 0x171)
              379: 'object_ml_ganon2',         # object slot 0x153 (object_ganon2; ML's kakariko3 push block, actor 0x17A)
              159: 'ml_ovl_ending'}            # actor 0x12's overlay (ML's ending card): its text images live in its data
# ML's pause/HUD texture files, same layout as MQ's but redrawn. Named ml_* because dma 7-16 still carry
# the vanilla names over unrelated ML data; inject_ui_textures.py maps the output back to vanilla paths.
ML_OBJECTS.update({82: 'ml_icon_item_static', 83: 'ml_icon_item_24_static', 84: 'ml_icon_item_field_static',
                   87: 'ml_icon_item_nes_static', 90: 'ml_item_name_static', 91: 'ml_map_name_static',
                   125: 'ml_parameter_static'})
# Objects ML only re-textured (same size and layout as MQ debug's): named ml_* so inject_obj_textures.py can
# extract them with texture-only copies of SoH's own XMLs and ship the redrawn textures under vanilla paths (MS-020).
ML_OBJECTS.update({351: 'ml_gameplay_dangeon_keep', 358: 'ml_object_box', 367: 'ml_object_bombf',
                   36: 'ml_object_bdoor', 395: 'ml_object_haka_door', 58: 'ml_object_jya_obj'})


def name_files():
    """Give ML's files the names the XMLs look them up by. ML reorganised dmadata, so the debug ROM's filelist
    names are wrong for most of its files; scenes and rooms come from ml_map, objects from ML_OBJECTS."""
    from ml_map import SCENES
    path = os.path.join(PIPE, 'filelists', 'dbg.txt')
    lines = open(path, encoding='utf-8').read().split('\n')
    while lines and lines[-1] == '':
        lines.pop()
    for i, l in enumerate(lines):
        if l.endswith('_scene') or '_room_' in l:
            lines[i] = f'mlfile_{i}'
    for s, info in SCENES.items():
        lines[s] = info['scene']
        for n, r in enumerate(info['rooms']):
            lines[r] = f"{info['scene'][:-6]}_room_{n}"
    for i, n in ML_OBJECTS.items():
        lines[i] = n
    # dma 114 is ML's `code` file, which holds the audio tables. Audio.xml looks it up BY NAME,
    # so the rewrite above must not leave it as mlfile_114 or ZAPD's ZAudio segfaults on a null file.
    lines[114] = 'code'
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')


# Every step runs with Python's UTF-8 mode, so the scripts read the repo's UTF-8 files the same way on Windows.
ENV = dict(os.environ, PYTHONUTF8='1')


def py(script, log=None, cwd=PIPE, **env):
    """Run a pipeline script with this Python; stop the build if it fails."""
    out = open(os.path.join(PIPE, log), 'w') if log else None
    try:
        subprocess.run([sys.executable, os.path.join(PIPE, script)], cwd=cwd, env=dict(ENV, **env),
                       stdout=out, stderr=subprocess.STDOUT if out else None, check=True)
    finally:
        if out:
            out.close()


def zapd():
    otr = os.path.join(PIPE, 'ml.otr')
    if os.path.exists(otr):
        os.remove(otr)
    out = os.path.join(PIPE, 'out')
    log = os.path.join(PIPE, 'cycle_zapd.log')
    with open(log, 'w') as f:
        subprocess.run([mspaths.ZAPD, 'ed', '-se', 'OTR', '--otrfile', otr, '--portVer', '9.1.1',
                        '-i', os.path.join(PIPE, 'xml', 'scenes'), '-b', os.path.join(PIPE, 'tml_zapd.z64'),
                        '-fl', os.path.join(PIPE, 'filelists'), '-o', out, '-osf', out, '-gsf', '0',
                        '-rconf', mspaths.zapd_config()],
                       cwd=mspaths.ZAPD_CWD, env=ENV, stdout=f, stderr=subprocess.STDOUT, check=True)
    errors = open(log, encoding='utf-8', errors='replace').read().count('texDecl == nullptr')
    print(f'texDecl errors: {errors}', flush=True)


def relaunch():
    if sys.platform != 'darwin':
        print('Built and installed. Launching is macOS-only until the Windows/Linux packages (MS-080/081).')
        return
    subprocess.run(['pkill', '-x', 'MissingShip'], stderr=subprocess.DEVNULL)   # one instance only
    time.sleep(1)
    with open(os.path.join(PIPE, 'ms_run.log'), 'w') as log:
        subprocess.Popen([os.path.join(ROOT, 'MissingShip.sh')], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                         stdin=subprocess.DEVNULL, start_new_session=True)


def main():
    shutil.copy(os.path.join(PIPE, 'build_rom_inline.py'), os.path.join(PIPE, 'build_rom.py'))
    py('build_rom.py', log='cycle_build.log')
    name_files()
    py('gen_xml_inline.py')
    zapd()
    # The inject steps run from ZAPD's working directory, as they always have.
    py('inject_titles.py', cwd=mspaths.ZAPD_CWD, ML_SKIP_SLOTS='')
    for step in ('minimaps', 'messages', 'objects', 'ui_textures', 'obj_textures', 'obj_patches', 'audio'):
        py(f'inject_{step}.py', cwd=mspaths.ZAPD_CWD)
    shutil.copy(os.path.join(PIPE, 'ml.otr'), os.path.join(ROOT, 'mod', 'MissingLink_Scenes.o2r'))
    if '--no-launch' not in sys.argv[1:]:
        relaunch()


if __name__ == '__main__':
    main()
