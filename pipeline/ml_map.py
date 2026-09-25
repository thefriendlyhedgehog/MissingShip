"""Single source of truth for the ML scene port.

ML's own scene table (code offset 0x10cbb0, 110 slots x 0x14) says which vanilla scene ID each ML
scene belongs to. Because SoH indexes its compiled-in scene table by that same ID, naming each ML
scene file after the vanilla scene occupying the SAME slot makes SoH load ML's scene wherever the
game asks for that ID -- the scene table is ported by renaming, with no engine change at all.
Room lists come from each scene's header command 0x04, resolved back to dma indices via dmadata,
so nothing here is guessed.
"""
from mspaths import PIPE
import json
_M = json.load(open(PIPE + '/ml_full_map.json'))
# dma index of scene -> {slot, scene ('xxx_scene'), enum, config, title_dma, rooms[dma...]}
import os
# Scenes 0..9 (+ Gerudo Training, Inside Ganon's Castle) are "non-shared": OTRPlay_SpawnScene looks
# them up under scenes/nonmq|mq/, but ZAPD always exports to scenes/shared/, so overriding them
# yields a nullptr scene. ML_SKIP_SLOTS excludes them until they are placed under nonmq/.
# Default is now EMPTY: slots 0/3/7 were only skipped while MQ-variant dungeons were being
# exported to scenes/shared. gen_xml_inline.py now routes them through xml/scenes/dungeons/ so
# ZAPD emits scenes/nonmq, which is where OTRPlay_SpawnScene looks for them. Keeping the old
# default silently shipped 21 scenes instead of 25.
_SKIP = {int(x) for x in os.environ.get('ML_SKIP_SLOTS','').split(',') if x.strip()}
SCENES = {int(k): v for k, v in _M.items() if v['slot'] not in _SKIP}

# --- new-game start fix -------------------------------------------------------------------
# SoH hardcodes the new-game spawn to ENTR_LINKS_HOUSE_CHILD_SPAWN (0xbb -> scene 52 spawn 0), and
# ML's entrance table agrees. But ML's scene 52 is a 128-byte STAGING STUB: its intro cutscene runs
# 4000 frames and then CS_DESTINATIONs to entrance 119 = scene 54 (malon_stable), which is where the
# game really begins. Vanilla's Cutscene_ProcessCommands hangs on ML's script (and on a hand-built
# minimal replacement), so the transition can't be relied on.
# Fix declaratively instead: put ML's real starting area AT the scene id the new game spawns into.
# malon_stable has no cutscene at all and a real 27KB room, so the start is clean.
# Cost: vanilla's Stable occupies slot 54. The only ML exits that targeted scene 54 were spawns
# 1-3, which already had no vanilla entrance equivalent, so nothing remappable is lost.
_START_DMA = 457   # malon_stable
_STUB_DMA  = 455   # the empty Link's House staging stub
# OFF BY DEFAULT. This remap was a workaround from when ML's opening cutscene could not run: it
# moved the real starting area onto the new-game spawn slot. The cutscene now works (the
# CS_DESTINATION sentinel plus the TRANS_MODE_CS_BLACK_FILL escape in z_demo.c), so ML's real
# layout is correct and this must stay off -- enabling it also silently switches on
# SCENE_TRANSLATE={54:52} in ml_entrance.py, which misroutes every scene-54 exit.
if os.environ.get('ML_START_FIX'):
    if _START_DMA in SCENES:
        SCENES[_START_DMA] = dict(SCENES[_START_DMA], slot=52, scene='link_home_scene')
    SCENES.pop(_STUB_DMA, None)
# short name used for resource paths, e.g. 'spot00_scene' -> 'spot00'
NAMES = {k: v['scene'][:-6] for k, v in SCENES.items()}
TARGETS = {k: v['rooms'] for k, v in SCENES.items()}
