# The Missing Link as a code mod

MissingShip's first step toward running The Missing Link on stock Unbound with this branch's mod API
(MissingShip `docs/ROADMAP.md`, step 5). ML's world (scenes, objects, text) stays its layer,
`MissingLink_Unbound.o2r`, made from the player's ROM: by this mod's porter in the game (`ml_porter.c`, from the
pack in `assets/missinglink/porter/`, MissingShip `tools/porterpack.py`), or by the MissingShip builder. This mod adds
what ML does in code, one actor at a time, each registered under the name ML's rooms place it by.

| Part | State |
|---|---|
| `ML_TreeBridge` | Ported, 2026-10-11: the same code as MissingShip's Unbound branch (`ovl_ML_TreeBridge`). |
| `ML_En_Md` | Ported, 2026-10-11: Mido, the branch's code (`ml_en_md.c`) over vanilla En_Md's Destroy, Draw and helpers. |
| `ML_SongBlock` | Ported, 2026-10-11: the block before Saria's house, the branch's code (`ml_songblock.c`). |
| `ML_Prologue` | Ported, 2026-10-11: the illustrated prologue, the branch's code (`ml_prologue.c`). |
| `ML_BgmTrigger` | Ported, 2026-10-11: ML's music and sound trigger, the branch's code (`ml_bgmtrigger.c`). |
| `ML_Bg_Spot02_Objects` | Ported, 2026-10-12: ML's Kakariko objects over vanilla's Destroy and Update (`ml_bg_spot02_objects.c`). |
| `ML_TalkSpot` | Ported, 2026-10-12: Kakariko's talk spot (`ml_talkspot.c`). |
| `ML_Villager` | Ported, 2026-10-12: Kakariko's two villagers (`ml_villager.c`). |
| `ML_Fishpond` | Ported, 2026-10-12: the fishing pond's scenery (`ml_fishpond.c`). |
| `ML_Elf` | Ported, 2026-10-12: the fairy the pond spawns, vanilla En_Elf with ML's one change (`ml_elf.c`). |
| `ML_HintNuts` | Ported, 2026-10-12: ML's talking Deku Scrubs, in four scenes (`ml_hintnuts.c`). |
| `ML_HoleScrub` | Ported, 2026-10-12: the Deku Tree's hole-hopping scrub (`ml_holescrub.c`). |
| `ML_Lift`, `ML_LiftBlock` | Ported, 2026-10-12: ML's switch lift and barrier block (`ml_lift.c`, `ml_liftblock.c`). |
| `ML_Elevator`, `ML_ElevatorSpawner` | Ported, 2026-10-12: Hollow Root Keep's elevators (`ml_elevator.c`, `ml_elevatorspawner.c`). The tall one drops while a Magic Hourglass rewind plays nearby. |
| `ML_HakaLift` | Ported, 2026-10-12: Hollow Root Keep's room 2 lift (`ml_hakalift.c`). |
| `ML_RoomClearSwitch` | Ported, 2026-10-12: holds switch 0x25 until its room is cleared (`ml_roomclearswitch.c`). |
| `ML_SlideBlock` | Ported, 2026-10-12: Hollow Root Keep's sliding blocks (`ml_slideblock.c`). |
| `ML_Pillar`, `ML_Thief`, `ML_Torch` | Ported, 2026-10-12: Bmori1's pillars, thieves and torch. The torch is vanilla Obj_Syokudai's Init, Destroy and Draw with its Update copied and ML's four changes made (`ml_torch.c`). |
| `ML_PushBlock` | Ported, 2026-10-12: the Library's bookshelf block, a copy of vanilla Obj_Oshihiki (SoH 9.2.3) with ML's switch check and push record built in, as the branch hooks them (`ml_pushblock.c`). |
| `ML_DnsShop` | Ported, 2026-10-12: the grotto scrub shop, vanilla En_Dns with ML's sale table, line and switch 9 set around it (`ml_dnsshop.c`). |
| `ML_EventSpawner`, `ML_Malon`, `ML_Cucco` | Ported, 2026-10-12: the Epona chase and ML's young Malon and her cucco (`ml_eventspawner.c`, `ml_malon.c`, `ml_cucco.c`). The cucco's counters reset at `OnLoadGame`, as the mod API has no actor reset function. |
| `ML_BossVa`, `ML_BossVaShot`, `ML_Light` | Ported, 2026-10-12: Hollow Root Keep's boss, its shot and its death flames (`ml_bossva.c`, `ml_bossvashot.c`, `ml_light.c`). It holds still while a Magic Hourglass rewind plays. |
| `ML_SkullKid`, `ML_BeamHeart`, `ML_Ending` | Ported, 2026-10-12: the Deku Road's Skull Kids, the Blade Beam's Piece of Heart and the ending card (`ml_skullkid.c`, `ml_beamheart.c`, `ml_ending.c`). |
| `ML_FogWall` | Ported, 2026-10-12: Deep Woods' fog walls and looping corridor (`ml_fogwall.c`). As on the branch, the next two wall push-outs after the loop moves Link are skipped (`VB_PUSH_ACTOR_OUT_OF_WALL`, `ml_player.c`). |
| Blade Beam (En_M_Thunder's slot) | Ported, 2026-10-12: ML's rewrite of actor 0x57, the beam and the spin-charge sparkle, in the slot itself (its ActorDB entry), since the player spawns it by number; the spawner runs at `OnPlayerUpdate` (`ml_mthunder.c`). Params 0xBEEF is Saria's soul, the figure that speaks from the sword in Monktown Valley's and Deku Grove's arrival cutscenes, which hold Link's sword-drawing pose (cue 26) while she talks; ported 2026-10-19, drawn from `gMlChildSoulFigureDL` and its parts, which ML's layer extracts from ML's `object_link_child` (`ml_mthunder.c`; the blade's glow is `ml_player.c`'s). The sword loses its charge on ML's Gerudo Valley event entrance, which stock reaches through the rows `ml_cutscenes.c` gives it. |
| Business Scrubs (En_Dns's slot) | Ported, 2026-10-12: ML's sale table (Piece of Heart 99, Deku Shield 1, Bugs, the garden's 20-rupee fee), the empty-bottle sale and the garden scrub, in the slot itself since the game spawns scrubs by number. Vanilla's Init and Update run inside ML's, which swap in ML's copies of the two sale actions (`ml_en_dns.c`). |
| Fairies (En_Elf's slot) | Ported, 2026-10-12: ML's two edits around vanilla's update and draw, on the slot itself: Navi is never drawn, and once Link has the Soul-Charged sword every other fairy is parked below the level (`ml_en_elf.c`). `ML_Elf` calls vanilla's functions and keeps neither. |
| Push-block colours (Obj_Oshihiki's slot) | Ported, 2026-10-12: ML's colours for the Deku Tree's and the Shadow Temple's rows, set after vanilla's Init, the only place vanilla sets the colour (`ml_obj_oshihiki.c`). |
| ML's small edits to vanilla NPCs | Ported, 2026-10-12, on each actor's slot: the Monktown Valley carpenters (En_Daiku: gone until ML's Bmori1 event, then the tent's lines in Kakariko's slot), En_Hs (gone until the event), the Birth Certificate (En_Hy's offer, Anju's count, line and Piece of Heart), the Kokiri's after-the-sword lines and frown (En_Ko), the Bunny Hood trader paid in nuts (En_Ms, flashing every 52 frames), the shopkeepers (En_Ossan: gone until the event, then shop music; the Bombchu Shop always open; Link stays drawn; a steady cursor), the cursed men's lines and key (En_Ssh) and the readable notes (En_Wonder_Talk) (`ml_vanilla_npcs.c`). |
| ML's small edits to vanilla enemies | Ported, 2026-10-12: the Bombchu rail (En_Bom_Chu; SoH's Controllable Bombchus lets go of a Bombchu on its walkways or in the gap), Deku Babas' nuts, the room-0 Deku Scrub clamp, the Baby Dodongo's dust, the giant Floormaster, nuts reflected by the Hylian Shield only (En_Nutsball) and the Peahat's mini-boss music (`ml_vanilla_enemies.c`). |
| ML's small edits to vanilla objects | Ported, 2026-10-12: Bmori1's sliding platform (Bg_Jya_1flift), the steep shutters' 6-frame wait (Bg_Mizu_Shutter), no boulder camera (Bg_Spot16_Bombstone, refused through `VB_PLAY_ONEPOINT_CS` while the boulder updates), the first blue warps to the Fishing Pond (Door_Warp1), the cows' limb 5, weather tags never removed, the soft soil's Piece of Heart (Obj_Makekinsuta, its slot made bigger for ML's two fields) and no Song of Storms rain (En_Okarina_Effect removed) (`ml_vanilla_objects.c`). |
| ML's title screen (En_Mag's slot) | Ported, 2026-10-12: ML's logo, flame, credits and red PRESS START on `object_ml_mag`, without ML's brown bars as the owner asked, the faster copyright fade and the letterboxed title cutscene (`ml_en_mag.c`). |
| ML's ReDeads (En_Rd's slot) | Ported, 2026-10-12: gone after ML's Bmori1 event, no lock-on or rumble, a scream that only sounds, a body that stays, no drop. These sit deep in vanilla's functions, so this is vanilla's 9.2.3 code with them built in (`ml_en_rd.c`, as `ml_pushblock.c` is for its block). |
| ML's player code (the player's slot) | Ported, 2026-10-12: the Magic Hourglass in the boomerang's slot (no aiming hold, the rewind and the room counter, the cucco kept through a void-out), the triple jump, the horse idle while Epona carries Link off from the Fishing Pond (2026-10-19), the Soul-Charged sword's model and glow, the child's hands, ML's cucco as a cucco to the player (glide, throw), the Bunny Hood's speed, no Navi toggle, no damage on a void-out, no Saria call (text 0xE0), music kept through holes and pit exits, no enemy or moving music, the place-name card at y 80, and no revival fairy (En_Elf's slot). The player's own ActorDB entry wraps vanilla's init and update; the rest is SoH's hooks (`ml_player.c`). What no hook reaches is listed below. |
| ML's code patches outside actors | Ported, 2026-10-12, on SoH's hooks around the code they patch on the branch. The HUD (`ml_hud.c`): the black drop shadows under the B, C and Start buttons and the rupee and key counts, drawn first in the overlay list at the places SoH computes (HUD editor positions included); the Start button's (223, 73, 73), set on its colour command after `Interface_Draw`; the green A note; the Soul-Charged sword's B icon; and ML's title cards (its ribbon, tinted per scene, in only the ten slots ML titles, vanilla's card hidden while the world draws; the cutscene card at y 80). The text (`ml_text.c`): Text Speed 2 while the player has never set it, slowed text two characters a step, and B Cancels Choices; both of ML's options are in Enhancements > Missing Link. The rest (`ml_world.c`): Navi's sounds and the Skull Kid laugh taken out of the request queue, dispersed sparkles deleted before they draw, ML's world-map areas, the title screen's setup (child Link, cutscene 0xFFF0), and a vanilla scene ML lacks refused for Kokiri Forest (in `Play_Init`, at `VB_PLAY_TRANSITION_CS`, whichever way the game got there: a transition, a file load, a warp point or the debug map select). SoH's Remove Spin Attack Darkness no longer swaps the Blade Beam's update (`ml_mthunder.c`). |
| ML's registries | Ported, 2026-10-12: the eight files of ML's layer stock does not read (`unbound/items.json` and the rest; grottos are `ml_grotto.c`'s). The builder's `--stock` layer also writes each as `missinglink/<name>`, the game's names (items, scenes, entrances, ...) turned into numbers from SoH 9.2.3's headers, and this mod reads them (`ml_json.h`). Items, shops, GetItem and the new save (`ml_items.c`): ML's item slots, hidden upgrades and capacities, the 20-Bombchu cap and ML's strength rule (on SoH's VBs), ML's four bottle slots shown to the seven actors that look for an empty bottle while they update, the Bombchu Shop's 40 and 80 rupees, ML's three GetItem models, ML's new save (applied as a new file is first written, since 2026-10-12, then saved again) and Skip Intro's destination (the Temple of Time). ML's entrances and cutscene destinations (`ml_cutscenes.c`): ML's 18 entrances on vanilla rows stock never reaches with ML, and the 21 cutscenes that end somewhere else, replaced on the frame the cutscene runs its destination. Maps (`ml_maps.c`): ten dungeons' maps, marks and room textures, and no minimap in 20 scenes unless SoH's minimap position is set. The pause menu (`ml_pause.c`): ML's age rules, the item, equipment, quest and map points' positions, its cursor rules, ammo counts and arrow colours. Labels (`ml_labels.cpp`): ML's names in SoH's save editor, gameplay stats and Time Splits, with ML's splits list, the counts its stats list and its countdown icon, through the mod API's label entries and `VB_SHOW_GAMEPLAY_STATS_COUNT`. |
| ML's intros | Ported, 2026-10-12: ML's ten arrival cutscenes, which replace vanilla's table (z_demo.c `sEntranceCutsceneTable`); the branch reads them as each entrance's `intro` in `unbound/scenes.json`, and the builder's `--stock` layer ships them by entrance number as `missinglink/intros`, with the 29 vanilla ones ML removes. ML's is set up at `AfterSceneCommands`, just before the game's table runs, under the table's conditions and SoH's Skip Entrance Cutscenes; a vanilla row on an entrance ML set is refused (`VB_PLAY_ENTRANCE_CS`) with its flag put back, and the Requiem, Nocturne and Saria-bridge triggers don't run on an entrance ML set (`VB_PLAY_TRANSITION_CS`), as on the branch (`ml_cutscenes.c`). |
| ML's credits in widescreen | 2026-10-19: ML framed its credits flybys for 4:3, and wider views showed past its scenes' edges. While a scene's cutscene layer is loaded in play, which in ML only the credits chain does, the sides beyond 4:3 are drawn black at `OnPlayDrawEnd` (`ml_cutscenes.c`). |
| The rest of ML's `scenes.json` | Ported, 2026-10-12: everything ML's entries for vanilla scenes carry that stock refuses, shipped by the builder's `--stock` layer by number. ML's 18 entrances (event layers, the grotto's slot 2, `temple_of_time_1` and the rest) on vanilla rows, with every exit to them rewritten to those rows (8 exits in four scenes); ML's flags on 164 vanilla entrances (`missinglink/entrancechanges`, set over the rows' own) and its 9 game-over continue points, taken when Continue is chosen (`VB_BE_ABLE_TO_SAVE` in the game over's prompt) and set at the end of that frame, after vanilla's boss-room rule (`ml_cutscenes.c`). Its scenes' draw configs, save return points (at `OnLoadGame`, on a file load only) and item restrictions (the scene's whole row, on its first frame) from `missinglink/scenes` (`ml_scenes.c`), and its scene names in SoH's windows (`SetLabel`, `ml_labels.cpp`). The grotto's slot 2 takes ML's Deku Tree entrance from the layer (`ml_grotto.c`). |
| The hooks offered to skijer | 2026-10-13, on `ml-codemod-hooks` (this branch: `ml-codemod` with skijer/Shipwright#23 to #27 merged): ML's child-shield rule (`VB_BE_CHILD_WITH_HYLIAN_SHIELD`), the wall push-out skip on a room's first frame, while the Hourglass rewinds and after the fog wall's teleport (`VB_PUSH_ACTOR_OUT_OF_WALL`), the Bunny Hood's water speed factor (`SOH_PLAYER_MOTION_WATER_SPEED`; `ml_player.c`), `countsShown` and every label on Windows too (`ml_labels.cpp`). `ml-codemod` keeps the stock versions until skijer merges them. |
| `ml/grotto` | 2026-10-11: Door_Ana's code with ML's hole model and ML's destination table (`ml_grotto.c`). |
| `ml/crack_wall` | 2026-10-11: Bg_Hidan_Kowarerukabe's code with ML's model and collision (`ml_crack_wall.c`). |
| Vanilla actors | 2026-10-11: ML's flags on seven actors and Bg_Treemouth not spawned (`ml_overrides.c`). 2026-10-19: no Skullwalltula spawned without its object, as on the N64, so ML's large crates (params 0) let none out. |
| Scene draw configs | 2026-10-11: ML's for its 25 scenes, set at `OnSceneInit` (`ml_scenes.c`); read from the layer's `missinglink/scenes` since 2026-10-12. |

MissingShip's Unbound branch reads the middle three from ML's layer (`unbound/actors/`, `unbound/grottos.json`) and
the draw configs from `unbound/scenes.json`. Stock Unbound refuses or ignores those, so the builder's `--stock`
layer leaves the actor files out and this mod carries the same tables (the draw configs since 2026-10-12 from the
layer's `missinglink/scenes`).

Without ML's layer only the porter starts: it asks for the player's OoT USA 1.0 ROM and Kaze's patch (or an ML ROM)
with skijer's file picker (the manifest's `"permissions": ["files"]`), makes the layer, saves it to `mods/` and
restarts. Over an older layer it offers the update, clearing that one first. The rest of the mod starts whenever a
layer is mounted.

Other mods (`ml_othermods.c`, 2026-10-24): ML is tested only with this mod and its layer, and other mods have crashed
it (custom music packs, skijer's MM masks). Each start logs the other mods mounted: code mods by their binaries
(`bin/<platform>/<mod>`), custom music (`custom/`) and texture or model packs (`alt/`), which the game's own archives
don't hold. When that list changes, the first frame tells the player that ML may crash with them; the list told is kept
in `gEnhancements.MissingLink.OtherModsSeen`, and nothing is shown where the porter doesn't ask either
(`gEnhancements.MissingLink.PorterAsks` 0, test homes).

Quests (`ml_quests.c`, 2026-10-28): ML's world replaces the game's for every file, and ML plays only Original Quest.
Randomizer, Master Quest, Boss Rush and the speedrun quests read vanilla's world, items and checks, so while ML's
layer is mounted a new file skips the file select's quest choice and is Original Quest (as the game does when only one
quest is shown), and a file of another quest doesn't open: the error sound plays and the player is told why (only
logged where the porter doesn't ask).
On a host with mod quests (`RegisterQuest`, offered to skijer for his mod API), ML is "The Missing
Link" (label `ML`) on the file select's quest carousel beside Original Quest, Master Quest, Randomizer and Boss Rush,
drawn with ML's title logo, and the guard above is off. The host puts ML's layer on top of the game's files only while
an ML file is played and sinks it under them otherwise, live, with no restart; ML's hooks run only in its world. What
ML writes into the game's own tables (actor slots, entrance rows, item slots and capacities, GetItem models, age rules,
ammo items, map tables, Link's hand display lists, the A note's colour, the run speed limit, the Kokiri frown) is kept
in `ml_patch.c` and swapped with the game's on the host's `OnQuestWorldChange`. Original Quest files made before
become ML's the first time. Built against a host without it (`SOH_MOD_QUEST_ADOPTS_ORIGINAL_FILES` undefined), the
guard above stays.

## Tested, 2026-10-11 (macOS, this branch at 27bb6c445)

- The mod registers `ML_TreeBridge` (actor 472), and ML's Kokiri Forest room spawns it by name with its params and
  position. Its model draws from ML's layer by path.
- With its switch set, as a Blade Beam would, it burns (En_Light), falls under ML's cutscene camera and lies as the
  bridge. Link stood on its collision for 700 frames.
- Its collision carries one camera entry that sets nothing, where ML's has none. `z_bgcheck.c`'s `func_80041A4C`
  reads the list unchecked (MissingShip's Unbound branch guards it, row B1). Standing still did not crash with a
  NULL list either, so the entry is a safeguard rather than a proven fix.

## Tested, 2026-10-11: actor data and draw configs (macOS, the builder's `--stock` layer)

- The seven flag changes land on the ActorDB entries (En_St 0x31, En_Sw 0x11, Obj_Bombiwa 0x10, En_Vm 0x10,
  En_Karebaba 0x1, En_Firefly 0x5001, En_Dekubaba 0x1), and the Lost Woods' Bg_Treemouth placement is not spawned.
- Standing on a Lost Woods grotto (slot 2) takes Link to the Deku Tree. ML's destination there is its own entrance
  (the scene's spawn 2), which stock Unbound does not have, and the Deku Tree has no vanilla entrance group to spare,
  so Link lands at the Deku Tree's entrance. Since 2026-10-12 the layer puts that entrance on vanilla rows and the
  grotto takes it from there: standing on that grotto now lands Link at the Deku Tree's spawn 2 (see the rest of ML's
  `scenes.json` below).
- The Deku Tree's breakable wall (params 0x501) spawns with ML's collision; with its break forced through
  `VB_FIRE_TEMPLE_BOMBABLE_WALL_BREAK` it shatters into debris, with no errors.
- Without ML's draw configs, five scenes crashed on their first frame (Deku Tree, Forest Temple, Desert Colossus,
  Kakariko, Bongo Bongo's room): their rooms call segment 8, which only ML's config sets, so the game ran whatever
  the segment still pointed at (seen under lldb: the game's own path strings). With `ml_scenes.c` all 25 of ML's
  scenes ran 15 s with no critical log; the only errors left are ML actors not yet ported.

## Tested, 2026-10-11: Mido and the song block (Kokiri Forest, the `--stock` layer)

- Mido spawns at ML's position and stays there, offers to talk when Link is beside him ("Speak"), and his line is
  0x1046, switching to 0x1044 once the slingshot has ammo, as in the branch. With this test save (Kokiri Emerald) he
  fades out beyond 100 units, which is vanilla's rule.
- The song block draws ML's model. With its switch (0x0C) set, as the song at its Okarina tag would, it plays the
  camera cut and sinks 2 units a frame (268 to -32) and is gone, leaving the passage open.
- Kokiri Forest now loads with no errors: every actor ML places there is ported.

## Tested, 2026-10-11: the prologue and the music trigger (the `--stock` layer)

- The prologue (link_home slot) draws ML's first illustration over the screen and switches to the second after
  frame 445, as its timer says. A warp-point boot does not run the prologue cutscene, so Link stands in front of it.
- The music trigger's mode 0 (Bombchu shop slot) silences the shop music (0x55) the frame it starts. Mode 3
  (`SCENE_HAIRAL_NIWA2`) plays Link's falling scream once as he drops past it. Modes 1 and 2 were not run: mode 1
  waits on a Forest Temple switch, and mode 2 is in a room the warp did not reach.
- With these, every actor ML places in the link_home and Bombchu shop slots is ported.

## Tested, 2026-10-12: Kakariko and the fishing pond (the `--stock` layer)

- Kakariko: both villagers spawn with their lines (0x6063, 0x605F) once the Forest Temple switch they wait on is set
  (forced for the test); the orange one turns to Link and offers to talk. The three Kakariko objects take ML's
  setup: 0x002 gets ML's collision and sinks 200 units while its switch is clear; 0x202 and 0xF0F skip collision.
- Fishing pond: its scenery draws, and arriving by `ENTR_FISHING_POND_0` (the blue warp's) spawns `ML_Elf` as Navi.
  The pond intro's other branch (Epona via `ML_EventSpawner`) needs ML's own pond entrance, which stock did not
  have, so it did not run there. Since 2026-10-12 stock reaches it through the rows `ml_cutscenes.c` gives it: it
  spawns `ML_EventSpawner` in mode 3 (see the registries' tests below).
- Both scenes load with no errors: every actor ML places in them is ported.
- `ML_PushBlock` (the Library) needs two calls inside vanilla Obj_Oshihiki's code, which a mod cannot add without a
  hook there: it was ported later as a copy of that code (see below).

## Tested, 2026-10-12: the Deku Scrubs (the `--stock` layer)

- The Deku brothers (0x901B/0x901C, Deku Tree room 3) pop up on their flowers and offer line 0x101C.
- The Deep Woods guards (0x1017/0x1018, Lost Woods room 1) draw ML's own scrub model on ML's pedestals and offer to
  talk. This test save holds the Gerudo Card (ML's Deku Pass), so they leave at Init as ML has them; the check was
  bypassed for the test.
- The hole scrub (Deku Tree room 2), hit twice (forced), burrowed and came up on a different flower each time.
- The Deku Tree now loads with no errors: every actor ML places there is ported.

## Tested, 2026-10-12: Hollow Root Keep's lifts and blocks (the `--stock` layer)

- Standing on the room 2 lift crashed stock Unbound at once: ML's own collisions carry no camera data, and stock's
  camera reads the missing list (our branch checks for it, row B1). The builder's `--stock` layer now gives those
  collisions empty camera entries (MissingShip `tools/unbound_stock.py`); this also covers the song block, the
  breakable wall and the Kakariko objects, which no test had stood on.
- After that: the room 2 lift carried Link its full 675 units and waited at the top; the spawner put the tall
  elevator in room 19 and it carried Link 1784 units (-1685 to 99); a sliding block moved when its switch (0x20,
  forced) changed; the room-clear switch held 0x25 while room 20 had enemies, and the lift there (0x125) rose on it.
- Hollow Root Keep and the Gerudo's Fortress slot now load with no errors: every actor ML places there is ported.

## Tested, 2026-10-12: the last twelve (the `--stock` layer)

One boot per scene, with a temporary census of each actor's Init.

- Bmori1: the three pillars, the three thieves and the torch spawn at ML's positions, and the thieves draw. A red
  translucent wedge covered the view from the test's start point (spawn 4, facing away from the room's south end):
  the camera sat inside ML's red ice there (vanilla `Bg_Ice_Shelter`, params 0x3B, two at z 272-276 between the
  blue fires). With the ice not spawned the wedge is gone, and facing it the ice draws as two red crystals. No exit
  leads to spawn 4 (ML's exits into Bmori1 use its entrances 0-2), so a player does not start there.
- Hollow Root Keep's boss room: with Link dropped into the arena, the boss plays its intro, turns to him and spits
  `ML_BossVaShot` (half a heart lost), with its `ML_Elf` fairies circling. The fight beyond was not run.
- The Library: the push block draws as the middle bookshelf; with it unregistered, the shelf is gone and the
  passage behind it shows. Pushing was not run.
- The Gerudo Valley slot by `ENTR_GERUDO_VALLEY_OUTSIDE_TENT`: the chase starts (Epona gallops off, a 40 s timer runs),
  the spawners put `ML_Malon` (by name), Umajump, Deku Babas and Tektites down, and both Skull Kids and the Blade
  Beam heart spawn.
- Lon Lon Ranch: Malon and young Epona draw. The Desert Colossus slot: the shop scrub draws.
- Deep Woods room 5: nine fog walls. With Link set in a wall's fade zone, the loop moved him (x 956 to -247) and he
  stayed there. He stayed there too with the `home.pos` change taken out, so standing still does not show the
  push-out the branch skips; walking through was not run.
- The testroom spawner (params 7) stops the timer and leaves.
- Not run: `ML_Ending` (only in Deep Woods' ending cutscene header), `ML_Cucco` (after Malon's race), `ML_Light`
  (the boss's death), lighting the torch, buying, talking.

## Tested, 2026-10-12: the Blade Beam (the `--stock` layer)

A temporary probe set the Soul-Charged sword's marker (slingshot ammo 40) and called the spawner's beam every 20
frames, as a spin or a full-health swing would; pressing the buttons needs input.

- Deku Road: each call spawned actor 0x57 as the beam (params 0xDEAD), drawn as vanilla's unused blade crescent
  flying ahead of Link. Facing a wall a few units away, the beams ended at once.
- One beam sent through `ML_BeamHeart`'s spot (y 360, above the path) revealed the Piece of Heart.
- Not run: the spin-charge sparkle, a crystal switch (the beam's stand-in explosion), real spins and swings.

## Tested, 2026-10-12: the Business Scrubs (the Gerudo's Fortress slot, the `--stock` layer)

A temporary probe started each sale where vanilla's does once its textbox closes; talking needs input.

- With this save's Magic Bean, the air-bottle scrub (params 9) sells Bugs (40 rupees): the sale filled the empty
  bottle with bugs. With the bean slot cleared it keeps its own row and hands over ML's empty bottle (into a free
  slot) where vanilla gives a stick upgrade.
- The garden scrub (params 10): its sale set the gate's switch 0x27, offered nothing, paid its fee (vanilla's
  nut-upgrade payment ran) and burrowed at once. With 0x27 already set it leaves at Init.
- The other scrub's row is vanilla's (red potion, 40). Not run: buying by talking, the 99-rupee Piece of Heart and
  the 1-rupee shield (no ML scrub sells them on this save's route), and the payments, which wait on the get-item
  text being closed.

## Tested, 2026-10-12: the push-block colours and the fairies (the `--stock` layer)

- The Deku Tree slot's two blocks (room 2) take ML's tan (240, 196, 111; vanilla's is 110, 86, 40) and Hollow Root
  Keep's (room 11) ML's green (154, 240, 111), read back from the blocks after Init and seen on screen.
- Navi, who showed beside Link at the same Library spot before, is gone.
- A small white streak near Link in both dungeon slots shows with the fairy edits unregistered too, so it is not
  theirs. Not run: a healing fairy with the Soul-Charged sword owned.

## Tested, 2026-10-12: the small edits to vanilla actors (the `--stock` layer)

Booted at warp points with a temporary census probe (removed) forcing each case; every run clean of errors.

- Kakariko's slot (Monktown Valley): before ML's Bmori1 event six ReDeads stand and no carpenter; after it no
  ReDead, both carpenters, and next to one the tent's line 0x6063 in Kakariko's slot. Hitting the boulder refuses its
  cutscene 4180 and the camera stays. Bugs held on the soft soil raise a Piece of Heart 40 units after 60 frames and
  settle it. The cow draws through ML's draw; its tail keeps vanilla's. A Baby Dodongo rising runs ML's dust.
- The Bombchu Shop: vanilla's Init no longer removes the keeper; ML removes him before the event (no music) and
  keeps him after it with shop music (0x55), cursor green and blue 255, and its ReDead gone.
- Bmori1: the platform holds at z -197, y 233, y scale 0.15625, and with its switch set slides to 197 at 4 a frame.
  The Peahat's room plays the mini-boss music (0x38).
- Hollow Root Keep: the giant Floormaster starts at scale 0.016 with a 75/120 collider and no partners, and on death
  splits into three full-size Floormasters with six small partners; a Bombchu at either end of the gap is put on the
  rail heading -x or +x; the steep shutter (rot.x 0x4000) waits 6 frames with no cutscene and opens; a cursed man
  (params 6) says 0x28; a dying Deku Baba with its drop flag set drops nuts.
- Deep Woods: a room-0 scrub pushed 100 units is held 30 from home; a nut flies with ML's copy; Anju counts all seven
  cuccos with the certificate (line 0x503B), and none once it is delivered (line 0x503D).
- Deku Grove: the bean seller offers 0x405E and his counter wraps every 52 frames; a Lon Lon Ranch weather tag
  survives the Epona flag. Kokiri Forest with the sword: the frown goes on and a line set to 0x100A becomes 0x101F.
  The Library's notes read 0x306-0x30C with flags 0x31 forced.
- The Shadow boss's blue warp sends the player to the Fishing Pond both the first time (Chamber of Sages) and after
  (the child's Graveyard warp pad). A Song of Storms effect's rain actor is removed. Booting to the title shows ML's
  title screen, letterboxed.
- Not run (they need talking or real input): the Birth Certificate's offer, buying the Bunny Hood, Link kept drawn
  in a shop, the cursed men's key, a nut bounced off the Hylian Shield or burst by swimming up.

## Tested, 2026-10-12: the player (the `--stock` layer)

Booted at warp points with a temporary probe (removed) that fed the player input through `OnPlayerFilterInput` and
logged each frame; every run clean of errors.

- Kokiri Forest, the Hourglass on C-Left: pressed and held, the throw fired at once (ML's aim and throw swapped in for
  vanilla's), set the trigger on its frame 6, and Link, walked 2 units a frame for 180 frames, was rewound 4 entries
  a frame for 24 frames to where he stood 157 frames before (x -390 back to -678), under the Song of Time wipe. The room counter
  then restarted and refilled to 169 at frame 17.
- Running at full stick: 5.50 without the Bunny Hood, 9.21 with it (limit 1100). A jump with the hood started at
  1.25x. Three running jumps in a row played run_jump, then ML's kiruL_end and kiruR_end slots, then back.
- Holding ML's cucco in the air: gravity -0.5 and fall speed -2 (its id is En_Niw's only inside the update).
  Voiding out with it: health stayed at 48, the room counter reset, and one frame into the respawned room ML's cucco
  was spawned beside Link.
- A pressed with nothing in hand: vanilla toggled Navi's flag inside the update and it was put back. naviTextId
  -0xE0 was dropped before the player read it: no talk, no message. A revival fairy was removed at spawn. A hole's
  music stop (func_800F6964(5)), queued inside the update, was taken back out of the queue (18 commands) and the
  scene's music and ambience kept playing. Sequence mode was "still" while running.
- With slingshot ammo 40 the sheath draws ML's soul sheath (its display list now calls ML's by path).
- The Deku Tree, entered through its entrance: the place-name card set up at y 80. The Deku Tree's first-visit
  cutscene sets it again at 120 the next frame, through z_demo.c's own call; `ml_hud.c` now moves that one to 80 as
  well, when it is presented (the branch's 0x80064B44).
- Not run: a real hole or pit exit (the probe's teleports into the Deku Tree's holes fell through them), a roll with
  the hood, the sword's glow pulse.

## Tested, 2026-10-12: ML's code patches outside actors (the `--stock` layer)

Booted at warp points and to the title screen with a temporary probe (removed) that set up each case and logged it;
every run clean of errors.

- Kokiri Forest: the B and C buttons and the rupee count drew their shadows, 2 px and 1 px down and right. A place
  card set up there drew ML's ribbon across the screen under "Kokiri Forest" at y 80, vanilla's card hidden. The
  world-map area went from the scene's 4 to ML's 9, and the visited areas from bit 4 to bit 9 (0x10 to 0x200), on the
  first load and again after a reload.
- Dispersed sparkles spawned for six frames were each deleted before they drew. Navi's "Hello" (0x6841), her flight
  (0x285F) and the fairy dash (0x281B) were dropped from the queue; the cursor sound next to them was not.
- Text with Text Speed never set: 2 characters a frame (3, 5, 7, ...), and the setting was still unset in the saved
  config afterwards. With the text slowed (delay 4): 2 characters a step (5, 7, 9, ...). On a choice prompt, B became
  A on option 1, with B taken out of the input.
- A transition to Death Mountain Trail (entrance 318) was refused, logged, and loaded Kokiri Forest.
- The Shadow Temple's slot (Hollow Root Keep): the key count (12) drew its shadow, the card ML's green ribbon. Paused,
  the Start button drew in (223, 73, 73) over its 1 px shadow. With SoH's Remove Spin Attack Darkness on, an
  En_M_Thunder spawned there got ML's update back before its first update.
- With slingshot ammo 40 the B button showed ML's Soul-Charged sword icon. At the ocarina prompt the A note was
  (80, 255, 150) by the time the staff drew.
- The title screen: spot00 loaded with linkAge 1 and cutscene 0xFFF0, ML's logo and credits over it.
- Not run: the Missing Link page in the menu (it was not opened), a real choice answered by an NPC, the scene refusal
  at file load.

## Tested, 2026-10-12: Controllable Bombchus on the rail (the `--stock` layer)

With SoH's Controllable Bombchus on, a probe (removed) spawned a Bombchu in Hollow Root Keep. Stock took it: Link's
input off, the Bombchu's camera on. Moved onto the rail (x -400, y 700, z 2080), it was let go the next frame (Link's
input back, the main camera back) and kept moving without being taken again until it blew up. No other actor saw the
A press. Moved 10 units off the rail instead, it stayed under control to the end.

## Tested, 2026-10-12: the registries (the `--stock` layer)

Booted at warp points with a temporary probe (removed) that applied ML's new save, logged the tables, gave Link test
items and drove the pause menu; every run clean of errors.

- Load: empty bottles in slots 18, 13, 14 and 21, 20 Bombchus, 3 GetItem models, 10 dungeon maps, 20 scenes without
  a minimap. The new save: item 8 in slot 7, equipment 0x1121, upgrades 0x1040, quest items 0x6000, no magic, B 59 and
  C-left slot 7. Deku Sticks in slot 9, bottles in 13, Nut capacity 20, strength from the bracelet (vanilla's mask
  0), Bombchus held at 20 when 10 were added to 15. GetItem rows 55 and 107 draw ML's Deku Nut Milk and Soul-Charged
  sword. The timestamps read "Arborachnid Beaten:" and "Soul-Charged Sword:".
- Entrances: the Deku Tree's event on row 0x11 (scene 0, spawn 0), the Lost Woods' on 0xE5 (scene 91), Gerudo Valley's
  on 131, each found by name.
- Kokiri Forest: no minimap. Paused, the item page showed ML's layout with its counts (Deku Sticks 7 in slot 9, Nuts
  20 in green, Bombchus 20); the quest page ML's places (Saria's and Epona's notes on the staff, the stones off the
  page); the equipment page ML's one column (sword, Hero's Shield, tunic, boots). From the left arrow the quest cursor
  went to point 21; on the equipment page down went 1, 6, 9 (the sword, the shield, the tunic). The arrow pulsed in
  ML's colours from one end of vanilla's pulse to the other.
- The Deku Tree with its map and compass: the minimap drew ML's room texture and a mark. Sent to its event entrance
  with cutscene 0xFFF0, the scene loaded setup 4 and played ML's credits; when they ran their destination the log read
  "cutscene ends at entrance 229, cutscene 0xfff0", and the Lost Woods (scene 91) loaded setup 4 and played its
  cutscene.
- The cases left over, run the same day with a second probe (removed):
  - The Deku Tree's pause map: its five floors with Link on 1F and the boss on B2, ML's room picture and its chest
    mark (ML's 1F marks are vanilla's, so the builder leaves them as they are).
  - The Bombchu Shop, ML's Bmori1 event set so the keeper stays: the four 10-Bombchu shelves at 40 rupees and the four
    20-Bombchu ones at 80.
  - An empty bottle in ML's slot 13 only: outside En_Ossan's update the game saw no empty bottle; inside it, with
    vanilla's four slots showing ML's (18, 13, 14, 21), it saw one, and the slots were put back after.
  - Two new files, made through the file select's own `Sram_InitSave` and `FileChoose_LoadGame`: with SoH's Skip Intro
    on, the first started in the Temple of Time (entrance 0x53) at noon with ML's new save; with it off, the second
    played ML's opening in Link's house and its destination took Link to the Temple of Time at noon.
  - SoH's windows: the gameplay stats listed "Magic Hourglass:" and "Soul-Charged Sword:" (Gohma's timestamp, which
    ML does not rename, stayed "Gohma Defeated:"); Time Splits drew ML's list with its names and icons (Magic
    Hourglass, Deku Nut Milk with ML's icon). The save editor opened; its item names show only on hover, not checked.
  - The pond's event entrance (with cutscene 0xFFF0, as ML's pond intro ends): `ML_EventSpawner` spawned in mode 3,
    and the cutscenes ran on through ML's event entrances to the Lon Lon Ranch, Gerudo Valley, Gerudo's Fortress,
    Kakariko, Kokiri Forest, the Deku Tree and the Lost Woods, each on its layer 4. The Soul-Charged sword (ammo 40)
    lost its charge on the Gerudo Valley event.
  - Found and fixed there: SoH's CreditsFix runs some credits cases' destination code twice on an NTSC ROM, at the
    start frame and again after its delay, and ML reuses them. The second run sent the pond cutscene to vanilla's
    Gerudo's Fortress; the replaced destination is now put back when that happens.

## Tested, 2026-10-12: the intros (the `--stock` layer)

A temporary probe (removed) walked Link through entrances, each once the scene had been idle for 80 frames, and
logged what played; both runs clean of errors.

- Kokiri Forest (entrance 238, walked in from Hyrule Field): ML's intro played (its letterboxed flyover), flag 0xA3
  set. Booted there at a warp point it does not play, as vanilla's don't: the warp point arrives as a respawn.
- Hyrule Field's wooded exit (389), vanilla's intro there removed by ML: nothing played and its flag (0xA0) stayed
  clear.
- Desert Colossus outside the temple (481): the Requiem trigger did not send Link to the east exit; its flag stayed
  clear.
- Kakariko's front gate (219): ML's intro played and set its flag (0xA9); vanilla's row there (0xA3, still clear) was
  refused with its flag left clear.
- The Fishing Pond by the blue warp's entrance (1119): ML's pond intro played, the Epona flag set as in ML, and its
  destination took Link to the pond's event entrance, from which the cutscenes ran on through ML's event entrances.
  The scene's commands ran twice as it loaded; the intro is set up once per arrival.
- With SoH's Skip Entrance Cutscenes on, the Lost Woods' south exit (286) played nothing and set ML's flag (0xA5), as
  SoH's option does for vanilla's.
- Not run: an adult arrival at Kakariko's front gate (the Nocturne trigger), Link's house's intro (it plays only on
  arrival there with flag 0xA0 clear outside the opening), the Gerudo Valley, Gerudo's Fortress, Lon Lon Ranch, Sacred
  Forest Meadow and Temple of Time intros.

## Tested, 2026-10-12: the rest of ML's `scenes.json` (the `--stock` layer)

A temporary probe (removed) moved Link through exits and entrances, each once the scene had been idle for 80 frames,
died once, loaded a save, and logged the entrance rows and restrictions; the run was clean of errors.

- Kokiri Forest's exit 0 took Link to ML's `temple_of_time_1` (row 817: the Temple of Time's spawn 1), Kakariko's
  exit 1 to `hairal_niwa2_1` (318, Monktown Valley side area spawn 1), that scene's exit 1 to `hairal_niwa2_2` (322,
  spawn 2), and Lon Lon Ranch's exit 2 to `stable_1` (790, the stable's spawn 1). The grotto's slot 2 resolves to
  `deku_tree_2` (row 46).
- A vanilla entrance's flags: row 118 holds ML's 0x4102 (title card, transitions 2 and 2).
- Dying in the Temple of Time after arriving by `temple_of_time_1`, Save? No, Continue? Yes: the Continue resumed at
  ML's 590 (scene 3, spawn 1) instead of the entrance Link arrived by.
- Restrictions on each scene's first frames: Monktown Valley side area hookshot, Farore's and Din's/Nayru's blocked,
  as ML sets; the Temple of Time Farore's only (ML's); scene 3, Kakariko, Lon Lon Ranch and the stable vanilla's (ML
  sets none there).
- A save made in scene 3, loaded through `FileChoose_LoadGame`, resumed at ML's 590 (vanilla's rule gives the Forest
  Temple's entrance), and Link arrived there.
- SoH's scene names: 107 "Monktown Valley side area", 52 "Link's house" and 81 (Hyrule Field's slot) "Title screen",
  ML's names for its 25 scenes.
- Every scene the run reached drew with ML's draw config from the layer, as with the old table.
- Not run in this pass: the other eight continue points, the other save returns, the grotto and how ML's flags look
  on arrival; the next section runs them.

## Tested, 2026-10-12: the rest of ML's `scenes.json`, untested cases (the `--stock` layer)

A temporary probe (removed) again; three runs, clean apart from the refusals noted.

- ML's flags on arrival, through `Scene_SetTransitionForNextEntrance` (what the player's exits call) and the game's
  own readers of the row:
  - the Temple of Time's entrance (83, title card off in ML): transitions 2 out and in, the music cut, no title card;
  - the Lost Woods' north exit (425, music kept and no card in ML): transitions 3 and 3, the music (0x1) kept across
    the transition, no title card;
  - Desert Colossus spawn 2 (485, title card on in ML): the place-name card set up (80 frames);
  - Desert Colossus' east exit (291, music kept and the sandstorm transition in vanilla): transition 2 and the music
    cut, as ML sets.
- The Continue, Save? No, Continue? Yes:
  - from Bongo Bongo's room after arriving by its entrance (1043), Link resumed in the same room, where vanilla sends
    him to the Shadow Temple's entrance.
  - The other seven continue points are on boss rooms of scenes ML lacks (Gohma, King Dodongo, Barinade, Phantom
    Ganon, Morpha, Twinrova, Ganondorf), which the mod refuses, so Link never arrives by them in ML. The probe set the
    arrival entrance before dying: 12 to 590, 141 to 686, 1035 to 205, 1039, 1047 and 1055 to 286, each arriving
    there; 769 to itself, which the refusal then sent to Kokiri Forest.
- Save returns: a save in each of the 21 scenes ML gives one, loaded through `FileChoose_LoadGame`, resumed at ML's
  entrance. A save in Bongo Bongo's room (no return point in ML) resumed at the Shadow Temple's entrance (55), and
  one in Kokiri Forest at Link's house (187), as vanilla's rule gives. That last was wrong for ML (found by the
  scripted playthrough, 2026-10-13): ML nops vanilla's default case, so a save outside the dungeons resumes at the
  entrance it was written at, and Link's house is ML's prologue stub, so the load replayed the prologue. Since then
  `ml_scenes.c` keeps the saved entrance (taken at `OnLoadFile`, before Sram_OpenSave's switch); a save made in
  Kokiri Forest after the tree bridge resumed at its entrance (0x209). Only the Forest Temple's slot (scene 3) is a
  scene ML has; the others are saves made in scenes ML lacks. For those, the refusal at `OnLoadGame` logged vanilla's
  entrance before the save return replaced it; the entrance the file resumed at was ML's either way.
- The grotto's slot 2 (the Lost Woods' room 1): standing on it took Link to the Deku Tree's spawn 2 (row 46), which
  drew normally.

## Tested, 2026-10-12: ML's new save on a new file (the `--stock` layer)

A temporary probe (removed) booted to the file select and made file 2 as the name entry does (`Sram_InitSave`).

- The mod applied ML's new save after the first write and saved again: `file2.sav` holds item 8 in slot 7,
  equipment and equips 0x1121, upgrades 0x1040, quest items 0x6000, B 59, C-left 8 on slot 7, cutscene 0xFFF1; the
  file select's preview data (`Save_GetSaveMetaInfo`) holds the same, where before it held vanilla's empty start.
- The file then opened at the file select as usual. Vanilla's preview panel shows hearts, rupees, stones and
  medallions only, which ML's start leaves as vanilla's, so the screen looks the same.
- A file made with the debug save is left alone, as on the branch: with the developer tools' debug mode on, file 1
  made the same way kept SoH's debug save on disk and in its preview (every item, equipment 0x7777, cutscene 0) and
  the mod logged nothing for it, while file 2, made next in the same run, got ML's start.

## Tested, 2026-10-13: the countdown timer's icon (the `--stock` layer)

ML's countdown icon is the Kokiri Sword (ML has no Master Sword). SoH's Additional Timers window draws a countdown
with `itemMapping[ITEM_SWORD_MASTER]`'s texture; while a countdown runs, the mod gives that entry the Kokiri Sword's
name, faded name and texture, and puts its own back once the countdown ends (`ml_labels.cpp`, macOS and Linux).

- A temporary probe (removed) started a 90-second countdown in Kokiri Forest and stopped it 15 seconds later, with
  the Additional Timers window's conditional timer on. The entry named `ITEM_SWORD_KOKIRI` from the countdown's
  first frames to its end, and `ITEM_SWORD_MASTER` before and after. The window drew the Kokiri Sword beside the
  countdown.
- While a countdown runs, the other windows that take the Master Sword's icon from `itemMapping` show the Kokiri
  Sword for it too. A second run opened SoH's save editor on its Equipment tab (the probe selected the tab through
  ImGui's tab-bar state; multi-viewports off so it drew in the game window): the Master Sword's slot showed the
  Kokiri Sword during the countdown and the Master Sword again once it stopped. Plandomizer, which needs a
  randomizer spoiler, was not run; it reads the same entry.

## Tested, 2026-10-13: the scene refusal in `Play_Init` (the `--stock` layer)

The refusal moved from `OnLoadGame` and the transition's first frame into `VB_PLAY_TRANSITION_CS`, which `Play_Init`
asks just before it picks the scene's layer and spawns it. A temporary probe (removed) drove each way in:

- The debug map select (boot sequence 3): `Select_LoadGame` with entrance 4 (Dodongo's Cavern) logged the refusal
  for scene 0x1 and loaded Kokiri Forest (scene 85, entrance 238), where the map select had set the entrance after
  `OnLoadGame` and the old refusal never saw it.
- A transition to entrance 40 (Jabu-Jabu, scene 0x2) from Kokiri Forest was refused the same way; one to 238 was not.
- A warp point booting at entrance 4 was refused for Kokiri Forest, and ML's own Kokiri Forest intro then played.
- Entrance 187 with cutscene 0xFFF1 (a new file's opening, cutscene layer 5) loaded Link's house (scene 52) and ML's
  opening ran: the refusal picks the layer as `Play_Init` does.
- The title screen (entrance 205, cutscene 0xFFF0, the title game mode) drew ML's logo over its scene, not refused.
- A file load, run 2026-10-13: file 1, saved as if in the Fire Temple (scene 4, which ML lacks and gives no save
  return), was picked at the file select through `FileChoose_LoadGame`, as the A button does. `Sram_OpenSave` set
  vanilla's return, entrance 357; the refusal logged it for scene 0x4 and the file opened in Kokiri Forest (scene
  85, entrance 238, no respawn point), where ML's Kokiri Forest intro then played.

## Tested, 2026-10-13: Text Speed and the A note on frames without the HUD (the `--stock` layer)

Text Speed's default of 2 and the green A note were set after `Interface_Draw`, which returns early under SoH's "no
UI" effect and isn't called outside the normal game mode (the title screen, the credits), while `Message_Draw` still
runs. Both are now set at `OnPlayDrawBegin`, which comes before `Message_Draw` on every play frame. A temporary probe
(removed) opened text 0x1004 in Kokiri Forest, then again with "no UI" on, then the ocarina staff with "no UI" on:

- With the HUD, the text revealed 2 characters a frame (1, 4, 6, 8, ...); with "no UI", the same. On the old hooks
  the "no UI" box revealed about 1 a frame (1, 3, 4, 5, ...).
- The A note at the "no UI" ocarina prompt was ML's (80, 255, 150); on the old hooks vanilla's (80, 150, 255), as
  `Message_ResetOcarinaNoteState` left it.
- The title screen and the credits, run the same day: ML shows no message text outside the normal game mode. Its
  title screen's credits are `En_Mag`'s pictures, not text, and its credits tour (from the pond's event entrance on
  cutscene 0xFFF0, as the Hollow Root Keep's blue warp sends it, through the ranch, Gerudo Valley, Gerudo's Fortress,
  Kakariko, Kokiri Forest and the Deku Tree to the Lost Woods) stays in the normal game mode. Its nine staff texts
  (0x500 to 0x51E) each appeared whole on their first frame (1, then the full length less 2), with Text Speed 2 at
  `Message_Draw`.

## Tested, 2026-10-13: the HUD shadows on the pause screen's copy frame (the `--stock` layer)

On the frame the pause screen copies the screen for its background, the shadows were left out, as anything drawn at
`OnPlayDrawBegin` would be in that picture. `Play_Draw` adds the copy to the overlay list just before
`OnPlayDrawEnd`, and `Interface_Draw` draws after it, so on that frame the shadows are now drawn at `OnPlayDrawEnd`:
after the copy, still under the buttons, as on the branch, where they are part of `Interface_Draw`. A temporary probe
(removed) pressed Start in Kokiri Forest and listed the overlay list's screen copy and black prim colours (the
shadows) on the next four frames:

- The copy frame: nothing before the copy, the five shadow colours after it, as on the frames after. On the old hooks
  that frame had none.
- Rechecked as out of reach, the same day: the wall push-out skip for every actor on a room's first two frames and
  while the Hourglass rewinds (the copy is inside `Actor_UpdateBgCheckInfo`, called from each actor's own update; the
  fog-wall teleport's part is covered by `ml_fogwall.c`), `countsShown` (rows inside the stats window's own ImGui
  table) and the Bunny Hood's water factor (a static of `z_player.c`).

## Tested, 2026-10-13: B Cancels Choices and the gameplay stats (the `--stock` layer)

On a choice prompt the mod turns a B press into an A press on the cancel option and takes B out of the frame's
input; the branch changes only what `Message_ShouldAdvance` sees. The only other code reading that input on a prompt
is SoH's gameplay stats, which count the frame's presses in `Play_Update`: they counted the press as A, or not at all
where B does nothing. The mod now counts it as B there (and takes back the A), when `Play_Update` will count the
frame's presses (`FrameAdvance_Update`'s answer, worked out ahead). A temporary probe (removed) opened two choice
texts in Kokiri Forest and pressed B on each:

- 0x703A ("OK / I don't believe you"): B picked option 1 and the text went on; the counts went A +0, B +1. On the old
  hooks A +1, B +0.
- 0x1035 (a question menu, where B does nothing): the prompt stayed; A +0, B +1. On the old hooks nothing was counted.
- Mod hooks on the same event run in the reverse order of their registration (the probe, registered last, ran before
  `ml_text.c`'s).

## Tested, 2026-10-13: the hooks offered to skijer (the `--stock` layer, a host with skijer/Shipwright#23 to #27)

ML's side of the five hooks, on `ml-codemod-hooks`. A temporary probe (removed), registered before ML's hooks so it saw
their answers, loaded Kokiri Forest from the map select, then Lost Woods, with the Gameplay Stats window open:

- The child-shield rule: with the Hylian Shield, false (vanilla's rule gives true); with no shield, true (since dropped,
  below). The Kokiri
  Sword's model group is sword-and-shield. A second run held R with the shield equipped: child Link holds the Hylian
  Shield up in ML's fist-and-shield hand, and wears it on his back otherwise (screenshots). Stock never draws that hand.
- The water speed factor: 1.00, then 1.25 with the Bunny Hood on, out of water.
- The wall push-out skip: Link put on a wall on a room's first frame (the room counter at 1) was not pushed, and
  pushed the next frame; put there later, pushed at once. The fog wall's skip in Kokiri Forest refused nothing; in
  Lost Woods, the next two push-outs (both Link's) were refused and the third pushed him out.
- The labels: ML's names for items 14 and 63, the timestamps of the Magic Hourglass, Hero's Shield, Deku Pass,
  Birth Certificate and the Arborachnid, ML's 28 splits and 3 groups; the Master Sword's icon was the Kokiri Sword's
  only while a countdown ran. The layer now writes the timestamps of ML's items the stats list into
  `missinglink/labels`, and the countdown icon's item number, since the mod can't read SoH's tables on Windows.
- The stats: the Enemy, Ammo and Buttons details listed the 22 counts ML names and any count above 0 (Keese, set to
  2); the other 51 were hidden.

After skijer's review, the same day: #23 also sends Equipment Always Visible's two sheath checks through the rule, and
ML's answer is now always false. The no-shield half was a side effect of ML's nop, shown only once the shield is taken
off, so it's left out. #25 ignores a factor of 0 or less. Run again from a save in Monktown Valley (the scripted
playthrough's), with Equipment Always Visible off and on: holding R, Link was shielding (`stateFlags1` 0x00400000) with
the shield in his hand (right hand `RH_SHIELD`, sheath type 16, the sword alone) and the rule false. The stats window
listed ML's 18 enemies.

## Built, 2026-10-13: Windows and Linux

`.github/workflows/build-missinglink.yml` builds this mod alone on Windows (MSVC, linked against `soh.lib` from
skijer's NEI Unbound modder release, the build players run it on) and Linux (Ubuntu 22.04, GCC), on each push to
`ml-codemod` or `ml-codemod-hooks` that touches it; macOS is built locally. `mod-sdk/tools/merge_mod_packages.py`
merges the three into one package (`bin/windows_x64`, `bin/linux_x64`, `bin/darwin`).

- Windows: the link resolves every function and global the mod takes from the game; the DLL imports only `soh.exe`
  and the C runtime. Six globals that the game's headers declare without `HOST_DATA` (`gAmmoItems`, the three age
  tables, the two map-mark tables) are declared as imports in `ml_pause.c` and `ml_maps.c`. On `ml-codemod` the
  labels in SoH's windows stay SoH's on Windows (C++ globals); `ml-codemod-hooks` sets them through the API there too.
- The merged package ran on macOS with no error (57 ML log lines, Kokiri Forest). The macOS binary is arm64 only.
- Linux, the same day, on skijer's own release (`soh-unbound-unsafe-linux.zip` from `nei-unbound-modders`, the
  AppImage unpacked), CachyOS: the unsigned mod was pre-approved by its content hash in
  `mods/.unbound-mod-approvals.json` (BLAKE2b-512 over each file's path and bytes, as `ModTrust.cpp` computes it), so
  no prompt appeared. Booted into each of ML's 25 scenes (one entrance each, warp point at the origin) and the title
  screen for 20 s each: every run loaded the Linux binary (57 ML log lines), logged no error and was still running at
  the end. Windows was first run on 2026-10-16 (below).

## Tested, 2026-10-16: Windows

On a Windows test machine with skijer's 9.3.0 Windows build (the merged #33). The scripted playthrough (`mlplay`,
14 segments from the prologue to The End) plays every segment with the same results as on macOS. Segments 01, 04
and 11 played again on files made by the Windows builder. One thing was Windows-only: in a Windows mod, a game
function's plain name is the mod's import thunk, not the game's function. So every compare of an `actionFunc` with
a game function failed, and every `actionFunc` the mod set was one the game's own compares never match. That broke
the Bunny Hood's speed, the Magic Hourglass, shops and Floormasters. `ML_HOST_FN` in `missinglink.h` now takes the
game's own address (`__imp_<name>`) for those compares and stores; calls by the plain name are fine (`badd23105`,
`dd9a7e4f3`).

## What stock Unbound lacks for ML (seen in the same runs)

- `unbound/scenes.json` entries for vanilla scenes (renames, custom entrances on vanilla scenes) are refused. ML's
  Kokiri Forest exits to `SCENE_TEMPLE_OF_TIME/ml/temple_of_time_1`, so the scene failed to load until that exit was
  pointed at a vanilla entrance (test copy only). Four ML scenes use such entrances. This mod now carries all of
  these entries (`missinglink/entrances`, `intros`, `entrancechanges` and `scenes`); a scene's name reaches SoH's
  windows on macOS and Linux only (`sceneNames` is a C++ global), and `SCENE_HAIRAL_NIWA2/ml/6b_3`, which nothing in
  ML leads to, sits on its rows unused.
- Actor types named after vanilla actors (`Bg_Treemouth`, `En_Dekubaba`, ...) and the `assets` key are refused
  (this mod now carries them).
- The other registries ML's layer carries (`cutscenes`, `getitem`, `grottos`, `items`, `labels`, `maps`, `pause`,
  `save`, `shops`) are not read at all (this mod now carries them). What it can't reach on stock: which counts the
  gameplay stats list when they are 0 (`countsShown`: the stats window builds its rows in its own draw code and lists
  every count); every label on Windows (SoH keeps them in C++ globals, and the
  Windows host exports C names only). `ml-codemod-hooks` reaches both through skijer/Shipwright#26 and #27. A new file's save and file-select preview held vanilla's start until the
  file was loaded; since 2026-10-12 ML's new save is applied at `OnSaveFile`, right after the file select first
  writes the file, and the file saved again.
- The ammo counts on ML's moved slots (Bombs 8, Deku Sticks 9, Nuts 10), which MissingShip's Unbound branch skipped
  (its `items.ammo.slots` loop checked `gAmmoItems[SLOT(item)]`), are drawn there since branch commit `a1580ef45`: at
  the count's vanilla place, moved by ML's digit table, as this mod and MissingShip's first fork (`engine/soh`) draw
  them. Branch and mod now agree.
- ML's player edits that no hook reaches: the wall push-out skip after a room change (`Actor_UpdateBgCheckInfo`,
  which the host calls directly for the player), the child-shield rule (`Player_IsChildWithHylianShield`, called
  directly; in ML a child holds the Hylian Shield like any shield, so ML's fist-and-shield hand never draws on stock;
  faking `currentShield` instead would let the shield burn or be dropped as a Deku Shield), and the Bunny Hood's water
  speed factor in its other uses (turn rate, ledge heights, door walk: a static of `z_player.c`, not exported). Two
  edits once listed here are no gap: `Player_StartTalking` keeping `talkActor` matters only for text 0xE0, which this
  mod never lets start (it clears Navi's -0xE0), and LOD 0 changes nothing for ML's child model, whose limbs draw the
  same display list at both LODs (the layer's far hand lists are copies of the near ones). The
  Bunny Hood's target speed is ML's at full stick (x1.4); ML's float-bits trick gives x1.33 to x1.5 by speed. `ml-codemod-hooks` reaches the push-out skip, the child-shield rule and the water
  factor through skijer/Shipwright#23 to #25.
- ML's code patches outside actors, where the hooks fall short of the branch: the scene refusal covers the debug
  map select too since 2026-10-13 (it moved into `Play_Init`, see the tested section), and Text Speed's default of 2
  and the green A note reach the frames `Interface_Draw` skips since the same day (set at `OnPlayDrawBegin`). The HUD shadows are drawn on the frame the
  pause screen copies the screen too, after the copy, since 2026-10-13. The B Cancels Choices input change is made at
  the start of the frame, for the whole frame; SoH's only other reader of it on a choice prompt, the gameplay stats'
  button counts, counts the press as B since 2026-10-13, as on the branch.
- Every actor ML's rooms place by name is now in this mod (2026-10-12).
