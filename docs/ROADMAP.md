# MissingShip roadmap

Status, how we work, what is left, and the history of every issue.
Last updated **2026-10-17**. Issues live on **GitHub Issues** (title prefix `MS-nnn`); the history below is generated from them.

- **Detail lives elsewhere:**
  - `ENGINE_CHANGES.md`: every engine change and its drop-in path (the ledger).
  - `ML_ACTOR_INVENTORY.md`: per-actor status.
  - `DROPIN_PROPOSALS.md`: what each engine change needs to move upstream or into data.
  - `PORTING_ACTORS.md`: how to port an actor.
  - `WORKING_NOTES.md`: handoff notes and key addresses.

## Status

**Pre-release: ML for SoH: Unbound, built by players with our builder.**
- **What players get** (`unbound-930-builder-3`, 2026-10-17): ML for the stock SoH: Unbound they already have (SoH
  9.3.0 or later, with skijer's code mods). The builder is a window that works offline on macOS, Windows and Linux.
  It takes the player's OoT USA 1.0 ROM and Kaze's v2.0 patch, then writes ML's layer and ML's code mod into
  Unbound's `mods/` folder. No package carries game data (MS-156, `tools/nodata.py`). The scripted playthrough
  plays all 14 segments, prologue to The End: on macOS and Windows on the builder's files, on Linux through
  segment 04.
- ML's changes were first worked out in a private SoH 9.2.3 fork (`engine/`), played start to finish there (MS-010,
  2026-09-26; milestones M0 to M5). It is a development reference only, never distributed.
- The owner is comparing the port against a reference playthrough of ML on N64. Any difference becomes a new issue
  with the next free ID (the last used is MS-156).
- **Left before a public release:**
  - Testing: from 2026-10-18 players test the pre-release, with no more scripted playthroughs. The owner has played
    ML through in full on the port. A helper still gets the script past three spots (rooms 14, 24 and 1), and
    Linux was scripted only through segment 04; testers' issues take over from here.
  - Public hosting: done 2026-10-18, MissingShip is public for testing; the forks stay private. Its history was
    rewritten twice. On 2026-10-17 git-filter-repo took out the ROMs, `oot.o2r`, the mod and pipeline dumps, `home/`,
    the engine binaries and `soh.o2r`, and the title-card banner (ML's art) from old `docs/soh-missingship.patch`
    versions: 469 MB became 2.8 MB, with the same files at HEAD. On 2026-10-18 the tool attribution lines left the
    commit messages. `tools/nodata.py --git` passes, no secrets were found, and the docs' commit IDs were remapped
    both times. The `test-build-1` release (2026-09-26, an early test build), which carried
    `mods/MissingLink_Scenes.o2r`, was deleted on 2026-10-17.
  - An official skijer release with #33: until then the README points players at the safe builds in the Build
    Artifacts list of skijer/Shipwright#11, his permalink, rebuilt with each merge and every Monday (skijer,
    2026-10-19; the unsafe builds are for modders). Our signed mods load in its 2026-10-17 build (7376c9f).
  - Code mod signing: done 2026-10-19, by skijer's sign-mod workflow from `codemods/` (skijer/Shipwright#39).
    `childshield-2` is signed. ML's signed code mod reaches players with `unbound-930-builder-4`, whose Windows and
    Linux builders wait for Actions minutes (2026-11-01); until then builder-3's unsigned one asks to "Always load".
  - One file to install (owner, 2026-10-20): ML's code mod makes the layer itself in the game (its porter,
    `ml_porter.c`, on skijer's file API). The player puts `missinglink.o2r` in `mods/` and starts the game. It asks
    for their OoT ROM and Kaze's patch, writes `MissingLink_Unbound.o2r` to `mods/` and restarts. On an update it
    clears the old layer first. The code mod carries the layer as deltas against the player's own files
    (`tools/porterpack.py`, USA 1.0 `oot.o2r` by skijer's build and by SoH 9.2.3; `tools/nodata.py` passes it). It
    refuses an `oot.o2r` from Rev A or B with what to do, since covering those would put OoT's bytes in the pack. In
    skijer's Mac build (7376c9f) the porter writes the stock layer file for file (5,873 files), and ML starts on it
    after the restart. Windows and Linux make the same layer. The layer keeps the game's files it covers, so an
    update is one question, one file pick and one restart. Left: the owner's run through the dialogs, signing the new
    code mod, and a release that ships only `missinglink.o2r` (the builder stays for testers).
  - Upstream offers: roborich's libultraship #2 and #3 are open; the rest are held until the project is done.
- **No game data** (owner decision, 2026-09-27): public builds carry no Nintendo data, and players build ML from
  their own ROM. Kaze Emanuar has given permission for the port, which covers ML's own work; Nintendo's assets are
  still never distributed.

- **How we got here: ML on SoH: Unbound.** Owner decision, 2026-09-27: The Missing Link becomes a mod for
  [SoH: Unbound](https://github.com/roborich/Shipwright/tree/unbound) (roborich's fork; on SoH 9.3.0 since Unbound 0.11, while our fork stays on 9.2.3). What
  Unbound lacks, we build ourselves in its style, so it can be offered to its author. The work lives in the private repo
  our Unbound fork (branch `missinglink`; `unbound` tracks roborich's). Our SoH fork gets no new
  features; it keeps the current beta running. Players still bring the OoT ROM and Kaze's patch or ML ROM. Order:
  1. ML's world as an Unbound mod: scenes, text and entrances, without ML's custom behaviour.
  2. Map every engine-ledger row to Unbound: covered, a data feature to add, or behaviour (`base`, later `script`).
     Done 2026-09-27: [UNBOUND_MAP.md](UNBOUND_MAP.md). Step 1 is done too (`tools/unbound_layer.py`).
  3. Build the features one at a time, starting with `base` (a vanilla actor's code under a new type, with asset
     swaps and text remaps). Prelude of Light may be used for manual scene fixes.
     Done 2026-09-29: every ledger row is built, carried or covered on our Unbound branch (UNBOUND_MAP.md, "Step 3
     is complete"); what remains is the owner's (section D: packaging and the builder, menu curation, debug aids).
  4. Packaging, owner decision 2026-09-29: the pack route. Players drop one file, `MissingLink_Unbound.o2r`, into the
     `mods/` folder of their own Unbound; no ML ROM, no builder. Our Unbound branch reads delta files, and
     `build_mod.py --unbound --pack` makes the pack (ML's own bytes plus copies from the player's `oot.o2r` and
     `oot-unbound.o2r`, 4.96 MB); `tools/package.py unbound-pack` zips it with a README (3.3 MB). Verified in game on
     macOS over an `oot.o2r` from roborich's extractor and over our fork's. Players get it inside the game: Unbound
     loads mods from the app's own folder (our branch), and `tools/package.py unbound-app-macos` makes a DMG of
     Unbound with the pack inside (22 MB; a first-time player setup converts its base and plays ML, 2026-09-29).
     Packs work over several extractor versions (`--also-base`): the owner's own SoH 9.1.1 `oot.o2r` refused the
     first pack, and the same DMG now plays ML over it and over a 9.2.3 one (2026-09-29). Owner decision 2026-09-29:
     support the latest SoH and later, not 9.0; a new extractor version joins with `--also-base`, and players with an
     older `oot.o2r` extract again (the READMEs say how). A refused pack is named at startup in a popup that says
     why and what to do, and marked *(not loaded)* in the Mod Menu (our branch, 2026-09-29). A clean install from the
     DMG with only a USA 1.0 ROM extracts, converts and plays ML. The pack covers USA 1.0, 1.1 and 1.2 by Unbound's
     extractor and the owner's USA 1.2 by 9.1.1 (2026-10-05: each mounts it and reaches ML's title and scene select);
     PAL and GameCube releases would be refused until a base from each joins with `--also-base`.
     SoH 9.3.0 (2026-10-06) replaced ZAPD with Torch: its `oot.o2r` has the same 38k files at the same paths, but
     58 differ (sound fonts, boss title cards), so the pack was refused over it. The pack now also covers USA 1.0-1.2
     by 9.3.0 (2026-10-09, in game). A player's 9.3.0
     randomizer saves were moved aside by our app (exact-version check, shared save folder): owner decision 2026-10-09,
     the app has its own folder (`~/Library/Application Support/The Missing Link`) and SoH's is never touched
     (checked through a real `open` launch). Owner decision 2026-10-09: wait for roborich's Unbound 9.3.0, then merge
     it. Done 2026-10-11: Unbound 0.11 (SoH 9.3.0, Torch) is merged into our branch; the pack mounts over 9.2.3 and
     9.3.0 bases and reaches ML's title. The layer tools still compare our fork with Unbound at its last 9.2.3 commit
     (`mspaths.UNBOUND_COMPARE`). Portable builds for every
     Mac, Windows and Linux need Unbound's CI on our branch: the owner adds a submodule
     token to our Unbound fork. Notarizing needs an Apple Developer ID. The earlier route, a builder players run
     (macOS done, `tools/package.py unbound-macos`), stays for testers. Still needed for a stock install: roborich
     taking delta files and the ledger features (held offers), and one pack per supported ROM version.
  5. **End goal, owner 2026-10-11: the builder for stock Unbound.** A player who wants ML with Ship's features gets
     the builder from the MissingShip thread, brings ML's ROM (or the OoT ROM and Kaze's patch, which the builder
     applies), and the builder writes ML for the stock Unbound they already have, with code-mod support from
     [skijer/Shipwright#11](https://github.com/skijer/Shipwright/pull/11). Once that support is upstream, the builder
     is the only thing we maintain. What it takes:
     - **The builder stays in parity with the current SoH.** It extracts ML with our fork's ZAPD (9.2.3's extractor);
       SoH 9.3.0 extracts with Torch, built to write what ZAPD wrote. `tools/extractor_parity.py` compares the two
       over the PAL MQ Debug ROM ML is built on; 2026-10-11 they agree on all 35,612 files but the boss title cards'
       texture type, which ML's own card now follows. Run it on each SoH release; a real difference is fixed in the
       builder, and porting the builder to Torch is the fallback if they ever drift apart in format.
     - **ML's C behaviour becomes a code mod** on #11's API: actors through `RegisterActor`, vanilla actors through
       the actor hooks and `RegisterVB`, items, pause and text through its registries and hooks. A code mod is one
       `.o2r` (manifest, Windows/Linux/macOS binaries, its assets), so the builder can write a single file: our
       prebuilt code (no Nintendo data) plus the assets it makes from the player's ROM. Gaps seen in #11: no
       room-load hook, and on Windows only globals marked `HOST_DATA` are reachable (`sDrawItemTable` is not).
       #11 is on Unbound 0.9 (SoH 9.2.3) and unsigned mods load by default (`UNBOUND_MOD_POLICY open`).
       First actor, 2026-10-11: `ML_TreeBridge` as a code mod on #11's branch (our Unbound fork `ml-codemod`,
       `mod-sdk/mods/missinglink`), the branch's actor code unchanged. ML's Kokiri Forest spawns it by name from the
       layer; it draws ML's model, burns, falls under ML's camera and carries Link. Stock Unbound refused parts of
       ML's layer in the same runs: vanilla-scene entries in `scenes.json` (so the four scenes whose exits name
       custom entrances on vanilla scenes fail to load, Kokiri Forest among them), actor types named after vanilla
       actors and their `assets` key; it does not read the nine other registries the layer carries. Those are the
       upstream offers' work, or the code mod's.
       Stock scene data, 2026-10-11: `build_mod.py --stock` writes the layer without `unbound/scenes.json` and
       rewrites exits to ML's added entrances (`tools/unbound_stock.py`). All 25 ML scenes load on #11's branch. The
       eight exits to entrances ML adds into vanilla scenes (Temple of Time, Stable, Back Alley House,
       `SCENE_HAIRAL_NIWA2`, Forest Temple) land at the scene's own entrance: stock's fixed entrance table has no spare
       entrance there that vanilla's cutscenes do not use. Landing where ML does needs custom entrances on vanilla
       scenes upstream (our branch's U1) or the code mod.
       Stock actor data, 2026-10-11: stock Unbound refuses all ten of the layer's actor files, so `--stock` leaves
       them out and the code mod carries them: ML's flag changes on seven vanilla actors (patched on their ActorDB
       entries, as our branch's overrides do), Bg_Treemouth not spawned (`VB_SPAWN_ACTOR_ENTRY`), and `ml/grotto` and
       `ml/crack_wall` registered under those names (vanilla's code with ML's models and collision; the grotto carries
       ML's destination table, which our branch reads from `unbound/grottos.json`). The same pass found that "load"
       had only meant "reached Scene Init": five scenes (Deku Tree, Forest Temple, Desert Colossus, Kakariko, Bongo
       Bongo's room) crashed on their first frame, because stock does not read ML's scene draw configs and their rooms
       draw through a segment only ML's config sets. The code mod now sets the 25 draw configs at `OnSceneInit`. All
       25 scenes then ran 15 s with no critical log, and the only errors left are ML actors not yet ported. Tested:
       the flags on the seven actors, Bg_Treemouth skipped in the Lost Woods, a Lost Woods grotto taking Link to the
       Deku Tree (ML's own Deku Tree entrance there is spawn 2, which stock does not have, so Link lands at the
       entrance), and the Deku Tree's breakable wall spawning with ML's collision and breaking into debris.
       Kokiri Forest complete, 2026-10-11: `ML_En_Md` (Mido, row 14) and `ML_SongBlock` (row 21) ported as before,
       so every actor ML places in Kokiri Forest runs on #11's branch and the scene loads with no errors. Then, the same day,
       `ML_Prologue` (row 29) and `ML_BgmTrigger` (row 63), which complete the link_home and Bombchu shop slots.
       2026-10-12: Kakariko's (`ML_Bg_Spot02_Objects`, `ML_TalkSpot`, `ML_Villager`) and the fishing pond's
       (`ML_Fishpond`, with the `ML_Elf` it spawns).
       Then the Deku Scrubs: `ML_HintNuts` (row 26; in the Deku Tree, Lost Woods, Gerudo Valley and Gerudo's
       Fortress slots) and `ML_HoleScrub` (row 33), which complete the Deku Tree.
       Then Hollow Root Keep's lifts and blocks (`ML_Lift`, `ML_LiftBlock`, `ML_Elevator`, `ML_ElevatorSpawner`,
       `ML_HakaLift`, `ML_RoomClearSwitch`, `ML_SlideBlock`), which complete it and the Gerudo's Fortress slot.
       Standing on the first lift crashed stock: ML's own collisions have no camera data, which stock's camera reads
       unchecked (our branch's B1). `--stock` now gives them empty camera entries, which covers every ML collision.
       Then the last twelve, 2026-10-12, so all 30 actors ML's rooms place by name run on #11's branch: Bmori1's
       (`ML_Pillar`, `ML_Thief`, `ML_Torch`), `ML_PushBlock`, `ML_DnsShop`, the Epona chase (`ML_EventSpawner`,
       `ML_Malon` and her `ML_Cucco`), the boss (`ML_BossVa`, `ML_BossVaShot`, `ML_Light`), `ML_SkullKid`,
       `ML_BeamHeart`, `ML_Ending` and `ML_FogWall`. The mod cannot hook inside vanilla code, so `ML_Torch`,
       `ML_PushBlock` and `ML_DnsShop` carry vanilla's code with ML's changes built in, and `ML_FogWall` moves the
       player's last-frame position with him after its loop, where our branch skips two wall push-outs.
       Then the Blade Beam, 2026-10-12: ML's En_M_Thunder in that actor's slot (its ActorDB entry, as the player
       spawns it by number), with the beam spawner at `OnPlayerUpdate`. Then the Business Scrubs (En_Dns's slot: ML's
       sale table, the empty bottle, the garden fee), vanilla's Init and Update wrapped.
       Then the push-block colours (after Obj_Oshihiki's Init) and the fairies (En_Elf's update and draw wrapped).
       Then ML's small edits inside the other 25 vanilla actors, 2026-10-12, each on its actor's slot: ML's rules
       around vanilla's Init and Update, ML's copies of the action functions it changed swapped in before vanilla's
       Update, the boulder's cutscene refused through `VB_PLAY_ONEPOINT_CS`, ML's title screen drawn in En_Mag's slot,
       and the ReDeads as vanilla's code with ML's changes built in. Each case was forced and checked on stock except
       the ones that need talking (the Birth Certificate's offer, the Bunny Hood sale, the cursed men's key).
       Then ML's player code, 2026-10-12, on the player's own ActorDB entry (vanilla's init and update wrapped) and
       SoH's hooks: the Magic Hourglass (ML's aim and throw swapped in, the rewind and room counter, the cucco kept
       through a void-out), the triple jump (swapped after vanilla starts the jump), the Soul-Charged sword, the
       child's hands, ML's cucco as a cucco to the player, the Bunny Hood's speed, the void-out without damage, no
       Navi toggle or Saria call, the music kept through holes, no enemy music, the place-name card at y 80 and no
       revival fairy. Out of reach of any hook: the wall push-out skip after a room change, the child-shield rule,
       the Bunny Hood's water factor beyond jumps and rolls, and the pond ride (it needs ML's entrance).
       Then ML's code patches outside actors, 2026-10-12, on SoH's hooks around the code the branch patches: the HUD
       (button and counter shadows at SoH's positions, the Start colour, the green A note, the Soul-Charged sword's B
       icon), ML's title cards (its ribbon in its ten titled slots), text speed 2 by default and two characters a
       step when slowed, B Cancels Choices with both of ML's options in Enhancements > Missing Link, Navi's sounds
       muted, no dispersed sparkles, ML's world-map areas, the title screen's setup and the refusal of vanilla
       scenes ML lacks. Out of reach: the refusal from the debug map select.
       Then Controllable Bombchus letting go of a Bombchu on ML's rail, 2026-10-12: stock's steering lets go on an A
       press, so En_Bom_Chu's slot presses A for it there, the input put back before the next actor updates.
       Then the registries stock does not read, 2026-10-12: the `--stock` layer writes them again as Blobs
       (`missinglink/<name>`, the game's names as numbers from the 9.2.3 headers), and the code mod applies them:
       ML's item slots, capacities, bottles and Bombchu cap on SoH's VBs and tables, the Bombchu Shop's prices, its
       GetItem models, the new save and Skip Intro's destination; its eight event entrances, on vanilla rows into
       scenes ML lacks, and the 21 cutscene destinations, replaced when the cutscene runs its destination; its
       dungeon maps and missing minimaps; its pause menu (ages, positions, cursor rules, ammo counts, arrow colours);
       and its names in SoH's save editor, stats and Time Splits. That reaches the pond ride and the sword's lost
       charge too. Out of reach: the stats' `countsShown` and the countdown icon, the labels on Windows, and a new
       file's preview before its first save. Every case was run on stock, 2026-10-12, through the pond's event chain
       across all eight event entrances. That run found SoH's CreditsFix running some credits cases' destination
       code twice; the code mod now keeps its destination through the second run (Unbound-modapi `2980013a5`).
       Then ML's ten intro cutscenes, 2026-10-12, from `unbound/scenes.json`'s `intro`s: the `--stock` layer ships
       them by entrance as `missinglink/intros`, and the code mod plays them before the game's table runs, refuses
       vanilla's rows and the Requiem, Nocturne and Saria-bridge triggers on those entrances, and keeps SoH's Skip
       Entrance Cutscenes (Unbound-modapi `5d5f05f82`).
       Then the rest of `scenes.json`, 2026-10-12: all 18 of ML's entrances go on unused vanilla rows built from
       ML's definitions, with the 8 exits to them rewritten to those rows (none falls back to a scene's default
       entrance now), and the `--stock` layer ships ML's flags on 164 vanilla entrances and its 9 continue points
       (`missinglink/entrancechanges`) and its scene names, draw configs, save return points and restrictions
       (`missinglink/scenes`). The code mod applies them; the draw configs come from the layer instead of its own
       table. Run on stock: four scenes' exits, the Continue, five scenes' restrictions, a save return and the names
       (Unbound-modapi `f18ca63bb`). The untested cases then ran on stock too: ML's flags on arrival (title cards,
       music, transitions), all nine continue points, all 21 save returns and the grotto's slot 2 (`eba2254de`).
       Then ML's new save from the moment a file is made, 2026-10-12: the mod applies it at `OnSaveFile` after the
       file select first writes a new file and saves again, so the file and its preview data hold ML's start, as on
       the branch (`0cae48a24`). With that, everything ML's layer carries reaches a stock Unbound with the code mod;
       what stays out of reach is listed in the mod's README.
       Then ML's countdown timer icon, 2026-10-13: the Kokiri Sword, set on SoH's `itemMapping` Master Sword entry
       while a countdown runs (macOS and Linux; `92f1e75cb`). The gameplay stats' `countsShown` stays out of reach.
       Then the scene refusal in `Play_Init`, 2026-10-13: it runs at `VB_PLAY_TRANSITION_CS` and picks the layer as
       `Play_Init` does, so the debug map select is refused too; run on stock with the map select, a transition, a
       warp point, a new file's opening and the title screen (`6c5a24eee`), then a file load from the file select (`8042c506a`).
       Then Text Speed's default and the green A note on frames without the HUD, 2026-10-13: set at
       `OnPlayDrawBegin` instead of after `Interface_Draw`, so SoH's "no UI" effect and the title and credits game
       modes keep them (A/B on stock; `fb62eca7b`). Rechecked as out of reach: the child-shield rule (ten
       direct calls in the player code) and the labels on Windows (no C accessor for SoH's name tables).
       The title and credits case then ran on stock: ML shows no message text outside the normal game mode (its
       title credits are pictures; its credits tour stays in the normal mode, its staff texts appearing whole;
       `79b1b584e`).
       Then the HUD shadows on the pause screen's copy frame, 2026-10-13: drawn at `OnPlayDrawEnd` on that frame,
       after the copy and under the buttons, as on the branch (A/B on stock; `ba1cf438b`). Rechecked as out of reach:
       the wall push-out skip for every actor (inside `Actor_UpdateBgCheckInfo`), `countsShown` and the water factor.
       Then B Cancels Choices in the gameplay stats, 2026-10-13: the mod changes the whole frame's input on a choice
       prompt, and the stats (its only other reader) now count the press as B, as on the branch (A/B on stock;
       `e3f188a0c`). What stock still doesn't reach is listed in the mod's README; none of it has a hook. Of ML's player edits, `talkActor` and
       LOD 0 turned out to be no gap; the wall push-out skip, the child-shield rule and the water factor's other
       uses stay out of reach (called or kept inside the host).
       Then ML's side of the hooks offered to skijer, 2026-10-13, on Unbound-modapi `ml-codemod-hooks` (`ml-codemod`
       with skijer/Shipwright#23 to #27 merged; `b84f836af`): the child-shield rule (ML's fist-and-shield hand now
       draws), the wall push-out skip (a room's first frame, the Hourglass rewind, the fog wall's teleport, which no
       longer moves the player's last-frame position), the Bunny Hood's water factor in all its uses, `countsShown`,
       and every label through the mod API's label entries, so on Windows too. The `--stock` layer's labels now carry
       the timestamps of ML's items the stats list and the countdown icon's item number, which the mod can't look up
       there. Each case ran on a host with the five PRs. `ml-codemod` keeps the stock versions until they merge.
       After skijer's review the same evening: the wall push-out hook was withdrawn (no case needs it; `ml-codemod`'s
       fog wall moves the last-frame position, and the playthrough's maze passed on stock), ML's child-shield answer
       is now always false (the no-shield half was a side effect of ML's nop), and the rest went to skijer as one PR,
       skijer/Shipwright#32, with the song cone and the no-camera-data fix. When skijer moved his branch to Unbound
       0.11 (SoH 9.3.0) that night, it was redone on the new base as
       [skijer/Shipwright#33](https://github.com/skijer/Shipwright/pull/33), and the code mod was ported to 9.3.0
       (our Unbound fork `ml-codemod-930`: 9.3.0's function and field names, TimeSplits' types, Blob padding);
       it boots, loads every table and plays the playthrough's first four segments on #33's macOS build. skijer
       merged #33 on 2026-10-14 (eed5004cc); the code mod's Windows build links against his build of it until he
       publishes a 9.3.0 release.
       Then the last note of something unported, 2026-10-13: the Blade Beam's params 0xBEEF, called the sword's glow
       and left inert until ML's `object_link_child` was ported, is an unfinished figure with Link's eyes that ML
       shows for at most 4 frames of a sword draw, away from Link (ML's spawner and actor disassembled; no ML
       cutscene or code holds the sword-draw pose). It stays inert in every port; the notes now say why. With that,
       nothing ML does in play is left that stock, with skijer's offered hooks, doesn't reach.
       Corrected 2026-10-19 (the owner's report): Link's cutscene cue 26 is that pose, and Monktown Valley's and Deku
       Grove's arrival cutscenes hold it while the figure, Saria's soul, speaks from the sword. The code mod draws her
       now (`ml_mthunder.c`), from ML's `object_link_child` DLs the builder extracts (`gMlChildSoulFigure*`).
       Then the code mod for Windows and Linux, 2026-10-13: a workflow on our Unbound fork builds it alone on
       Windows (linked against `soh.lib` from skijer's NEI Unbound modder release) and Linux, for `ml-codemod` and
       `ml-codemod-hooks`; with the macOS build, `merge_mod_packages.py` makes one package for all three. Windows needed
       six game globals declared as imports (`ml_pause.c`, `ml_maps.c`). Its link resolves everything the mod takes
       from the game. The merged package runs on macOS. On Linux it ran on skijer's own release (his modder
       AppImage, the unsigned mod pre-approved by its content hash): each of ML's 25 scenes and the title screen for
       20 s, with no error. Windows is built, not yet run (no Windows test machine). The macOS binary is arm64 only.
       Then the builder delivers it, 2026-10-13: `tools/codemod.py` takes the `ml-codemod` checkout's pushed commit,
       builds the macOS binary, takes Windows and Linux from that commit's green CI run and merges the three into
       `dist/codemod/missinglink.o2r` (`codemod.txt` names the commit and run). `freeze_builder.py` carries it, and
       `build_mod.py --stock` writes it as `mods/missinglink.o2r` beside the layer, after checking every resource its
       manifest requires is in the layer it just made. Run from the checkout, the two files load together on macOS
       with no error. Frozen, the builder writes the same two files (the layer's 6,138 entries identical to the
       checkout's, the code mod byte for byte), and they load the same. That first frozen `--stock` run found the
       freeze missing a header `--stock` reads (`gameplaystats.h`); the freeze now takes them from `unbound_stock`.
       Then Windows, 2026-10-16, on a Windows test machine: skijer's 9.3.0 Windows build, the code mod and mlplay
       play the whole playthrough, every segment with only its known FAILs. Three things were Windows-only.
       (1) In a Windows mod a game function's plain name is the mod's import thunk, so every compare with an
       actionFunc failed and every actionFunc the mod set was one the game's own compares never match (the Bunny
       Hood's speed, the Magic Hourglass, shops, Floormasters): the code mod now takes the game's own address
       (`ML_HOST_FN`, our Unbound fork `d30db5648`, `e461d2699`); mlplay the same for the file select.
       (2) The frozen builder read the repo's UTF-8 sources as cp1252: it runs in Python's UTF-8 mode (32eaa51).
       (3) ZAPD crashed extracting ML's audio: four samples' zero loop and book pointers made it read a
       1.2M-entry codebook past the bank, and the config named two symbol files that don't exist (b2610dd;
       found with ASan on the Mac). The Windows builder's layer is identical to the Mac's (6,138 entries). It must
       run on a desktop session: Unbound's export dies (0xC0000409) in a session with no desktop, such as SSH.
       Released as the pre-release `unbound-930-builder-2` the same night: the three builders, run from OoT USA
       1.0 and Kaze's patch, write the same layer (6,138 entries) and code mod; segments 01, 04 and 11 played on
       the Windows builder's files, 01 and 04 on the macOS builder's.
       Then a window, 2026-10-17 (`pipeline/builder_gui.py`, Tk, offline): the builder opens it when run with no
       arguments or `--gui`, and each package has a double-click launcher (macOS "Build The Missing Link.app",
       Windows ".cmd", Linux ".sh"). It asks for Unbound's folder (or app), the ROM and Kaze's patch, checks them
       (SoH 9.3.0 or later, `oot.o2r` there, the ROM's CRC), runs the builder with `--stock` into Unbound's `mods`
       and shows its progress; on Linux it extracts the AppImage first. Run from the packages on all three, its
       layer and code mod are identical to builder-2's (6,138 entries). On Windows the frozen builder is a console
       program: the window drops the console, so a double-click shows no black box (aac5fa1).
       Released as the pre-release `unbound-930-builder-3` the same day (the same layer and code mod as builder-2).
     - **Unbound's data features we built** (labels, GetItem draws, items, pause, shops, maps) go upstream to
       roborich or move into the code mod. Delta packs only matter for handing out ML without the builder: a
       player who builds from their own ROM gets the whole layer.

## Open work

| ID | Issue | Notes |
|---|---|---|
| [MS-070](https://github.com/thefriendlyhedgehog/MissingShip/issues/42) | Drop-in path: upstream bugfixes and data-driven proposals | Proposals written in [DROPIN_PROPOSALS.md](DROPIN_PROPOSALS.md) for ledger rows 1–101; rows 102–140 still to review. No upstream PRs until the project is done (owner decision, 2026-09-25). |
| [MS-091](https://github.com/thefriendlyhedgehog/MissingShip/issues/48) | Porting-kit knowledge base | Deferred until ML is finished; done in a separate effort. |
| [MS-092](https://github.com/thefriendlyhedgehog/MissingShip/issues/49) | Per-hack workspace layout | Deferred, as above. |
| [MS-093](https://github.com/thefriendlyhedgehog/MissingShip/issues/50) | One-command romhack survey script | Deferred, as above. |


## Known differences from ML, and ML quirks we keep

- **Boot:** SoH's boot logo replaces ML's two boot checks, the Expansion Pak and emulator-accuracy error screens (MS-061). ML's title screen itself is ported (MS-039).
- **Entrance fades:** four entrance day-fade colours differ slightly (MS-027).
- **Audio:** one sample reference in ML's own soundfonts points past the end of its Audiotable (an ML bug), so that sound is silent. In the Bombchu Shop, ML's `Ml_BgmTrigger` silences the area music until switch flag 7 is set. That is ML's design.
- **Closed as authentic ML behaviour:**
  - the Hollow Root Keep Stalfos that walks on air (MS-128);
  - the fast Hollow Root Keep elevator (MS-142);
  - the stable Gold Skulltula that can't be killed with restricted items (MS-146).
- **Settings menu:** SoH's options that do nothing in ML are removed or hidden (MS-145, ledger rows 106–138); the port's own options are on Enhancements → MissingShip. Stock SoH extras that change ML's gameplay stay off by default.

## How we work

- **Checks before pushing.** `python3 tools/check.py` must pass. The `.githooks/pre-push` hook runs it once enabled with `git config core.hooksPath .githooks`.
- **One issue per change.** Put the issue ID in the commit subject, e.g. `MS-014: port En_Nutsball`, and `Refs #N` / `Fixes #N` in the body.
- **The ledger rule.** Any engine change updates `ENGINE_CHANGES.md` in the same commit. An actor change also updates `ML_ACTOR_INVENTORY.md`.
- **Engine work is commits.** Commit it on `missingship` in the engine fork (same `MS-nnn` subject), push, then run `tools/genpatch.py` so `docs/*.patch` match (`PORTING_ACTORS.md` step 6).
- **Tested vs. built.** "Ported" means built and deployed. "Verified" means played in-game.
- **Port ML's actual code.** A port must follow ML's code, recovered by diff or disassembly. Guessed behaviour must be marked as a guess.
- **Order of preference for a fix:**
  1. data;
  2. a generic SoH feature;
  3. an upstreamable bugfix;
  4. ML-specific C, as a last resort.
- **Keep ML as its author intended** (owner decision, 2026-09-25).
  - If a report turns out to match ML's own code, close it as authentic.
  - QoL is limited to what stock SoH already has, plus options the owner asks for.
  - Presentation (how it looks) is the owner's call.
- **One place per setting.** The MissingShip settings page holds only the port's own options; stock SoH settings are never mirrored there.
- **Public defaults.** `profile/shipofharkinian.json` is every new player's starting point: chosen defaults, never a tester's settings.
- **Use ML's area names** in docs, issues, the checklist and tool output, with the slot name after them where it helps, e.g. "Hollow Root Keep (HAKAdan)". Code identifiers keep the slot names. The map lives in `hacks/ml/hack.json` (`areas`); tools print it through `hack.area()`.

| Slot | Area | | Slot | Area |
|---|---|---|---|---|
| `Bmori1` | Monktown Sanctuary (title card) | | `kakariko` | Monktown Valley house |
| `HAKAdan` | Hollow Root Keep (title card) | | `malon_stable` | Cluck Cluck Ranch stable |
| `kakariko3` | Library (title card) | | `turibori` | Fishing Pond |
| `night_shop` | Bombchu Shop (title card) | | `spot00` | title screen |
| `spot01` | Monktown Valley (title card) | | `ydan` | Deep Woods grotto |
| `spot04` | Kokiri Forest (title card) | | `testroom` | Deku Road grotto |
| `spot09` | Deku Road (title card) | | `spot11` | Deku Grove grotto |
| `spot10` | Deep Woods (title card) | | `spot05` | Kokiri Forest side area |
| `spot12` | Deku Grove (title card) | | `tokinoma` | Kokiri Forest interior |
| `spot20` | Cluck Cluck Ranch (title card) | | `hairal_niwa2` | Monktown Valley side area |
| `HAKAdan_bs` | Hollow Root Keep boss room | | `link_home` | Link's house |
| `kokiri_home`, `kokiri_home3`, `kokiri_home4` | Kokiri Forest houses | |  |  |

All 25 ML scenes are named.

## Milestones

| Milestone | Issues | Result |
|---|---|---|
| M0: Team footing | MS-001 … MS-009, MS-014 | Done 2026-09-25: configurable paths, one build script, the engine fork as a submodule, `tools/check.py` and the pre-push hook. |
| M1: Verify | MS-010 … MS-013, MS-017 … MS-019, MS-094, MS-105, MS-106 | Done 2026-09-26: the full playtest checklist verified. |
| M2: Data-only quick wins | MS-015, MS-020 … MS-027, MS-095 | Done 2026-09-26: textures, dungeon maps and markers, credits, skyboxes, sword-turn animations, entrance flags. |
| M3: Actors by scene | MS-030 … MS-040, MS-096 … MS-139 | Done 2026-09-26: every changed or custom actor, scene by scene, including both dungeons and the boss. |
| M4: Link | MS-050 … MS-055 | Done 2026-09-26: ML's model, triple jump, player edits, Magic Hourglass, Soul-Charged sword. |
| M5: Front end and polish | MS-060 … MS-063 | Done 2026-09-26: HUD restyle, title screen, opening; the pause cursor rechecked. |
| M6: Drop-in | MS-016, MS-070 | MS-016 fixed; MS-070 open (proposals written, PRs held). |
| Platforms | MS-080 … MS-083 | Done 2026-09-27: Windows and Linux packages (engine from GitHub Actions, `tools/package.py`), tested on both. A macOS (Apple Silicon) package from the local build followed. |
| Prod: no game data | MS-153 … MS-156 | Done 2026-10-17: players build the mod from their own ROM, and the packages carry no game data (`tools/nodata.py`, MS-156). Still open: before the repo goes public, its history (which still holds the ROMs and archives committed before 2026-09-27) is cleaned, by a fresh repo or a rewrite; the owner decides which at that point. |
| ML on Unbound | — | Open, started 2026-09-27: see Status. |
| Porting kit | MS-090 … MS-093 | MS-090 done (per-hack tool config); the rest deferred until ML is finished. |

---

## Issue history

Every issue, from GitHub. Details, test notes and commits are on each issue.

### M0: Team footing

| ID | Issue | Result |
|---|---|---|
| [MS-001](https://github.com/thefriendlyhedgehog/MissingShip/issues/1) | Make script paths configurable (no hardcoded /Volumes paths) | done 2026-09-25 |
| [MS-002](https://github.com/thefriendlyhedgehog/MissingShip/issues/2) | Merge cycle.sh and ms_cycle.sh | done 2026-09-25 |
| [MS-003](https://github.com/thefriendlyhedgehog/MissingShip/issues/3) | Bring ENGINE_CHANGES.md up to date with this week's actor ports | done 2026-09-25 |
| [MS-004](https://github.com/thefriendlyhedgehog/MissingShip/issues/4) | Correct the stale README | done 2026-09-25 |
| [MS-005](https://github.com/thefriendlyhedgehog/MissingShip/issues/5) | Regenerate the actor inventory from the full scan, add a Verified column | done 2026-09-25 |
| [MS-006](https://github.com/thefriendlyhedgehog/MissingShip/issues/6) | Re-derive ML custom-code hooks across all overlay tables and address ranges | done 2026-09-25 |
| [MS-007](https://github.com/thefriendlyhedgehog/MissingShip/issues/7) | Decide: private Shipwright fork branch vs patch-only workflow | done 2026-09-25 |
| [MS-008](https://github.com/thefriendlyhedgehog/MissingShip/issues/8) | Add a pre-push check script (exits, hashes, patch applies, build) | done 2026-09-25 |
| [MS-009](https://github.com/thefriendlyhedgehog/MissingShip/issues/51) | One ZAPD build and one scene table for the whole pipeline | done 2026-09-25 |
| [MS-014](https://github.com/thefriendlyhedgehog/MissingShip/issues/52) | Make the engine fork a submodule of MissingShip | done 2026-09-25 |

### M1: Verify

| ID | Issue | Result |
|---|---|---|
| [MS-010](https://github.com/thefriendlyhedgehog/MissingShip/issues/9) | Playtest everything built but unverified | done 2026-09-26 |
| [MS-011](https://github.com/thefriendlyhedgehog/MissingShip/issues/10) | Hard freeze pulling a crate in Cluck Cluck Ranch stable (malon_stable) room 3 | not reproduced in playtest (2026-09-25) |
| [MS-012](https://github.com/thefriendlyhedgehog/MissingShip/issues/11) | Deep Woods sometimes does not load the next zone | done 2026-09-26 |
| [MS-013](https://github.com/thefriendlyhedgehog/MissingShip/issues/12) | Locate and play through Malon's race | done 2026-09-25 |
| [MS-017](https://github.com/thefriendlyhedgehog/MissingShip/issues/55) | Saria's Song at the song-block door asks "talk to Saria?" before opening it | done 2026-09-25 |
| [MS-018](https://github.com/thefriendlyhedgehog/MissingShip/issues/56) | B answers "No" / closes yes-no prompts (QoL) | done 2026-09-25 |
| [MS-019](https://github.com/thefriendlyhedgehog/MissingShip/issues/57) | MissingShip settings page; Ocarina on D-Down only; D-pad ocarina playback on | done 2026-09-25 |
| [MS-094](https://github.com/thefriendlyhedgehog/MissingShip/issues/58) | Area title text sits 4 px low on ML's ribbon | done 2026-09-25 |
| [MS-105](https://github.com/thefriendlyhedgehog/MissingShip/issues/69) | Blank green title card entering untitled scenes (Deep Woods grotto) | done 2026-09-26 |
| [MS-106](https://github.com/thefriendlyhedgehog/MissingShip/issues/70) | Rain in Kokiri Forest (Song of Storms rain ML removed) | done 2026-09-26 |

### M2: Data-only quick wins

| ID | Issue | Result |
|---|---|---|
| [MS-015](https://github.com/thefriendlyhedgehog/MissingShip/issues/53) | Retry the skipped IA4 scene textures now that the Metal guard exists | done 2026-09-26 |
| [MS-020](https://github.com/thefriendlyhedgehog/MissingShip/issues/13) | Texture aliases: push blocks, chests, bomb flowers, boss/haka doors, jya lift | done 2026-09-26 |
| [MS-021](https://github.com/thefriendlyhedgehog/MissingShip/issues/14) | Beamos and Saria DL aliases | done 2026-09-26 |
| [MS-022](https://github.com/thefriendlyhedgehog/MissingShip/issues/15) | Dungeon minimaps and pause dungeon maps (map_i_static, map_48x85_static) | done 2026-09-25 |
| [MS-023](https://github.com/thefriendlyhedgehog/MissingShip/issues/16) | Dungeon map chest/boss markers (map_mark_data) | done 2026-09-26 |
| [MS-024](https://github.com/thefriendlyhedgehog/MissingShip/issues/17) | Credits text (staff_message_data_static) | done 2026-09-26 |
| [MS-025](https://github.com/thefriendlyhedgehog/MissingShip/issues/18) | Skyboxes vr_fine1 and vr_cloud2 | done 2026-09-26 |
| [MS-026](https://github.com/thefriendlyhedgehog/MissingShip/issues/19) | gameplay_keep sword-turn (kiru) animations | done 2026-09-26 |
| [MS-027](https://github.com/thefriendlyhedgehog/MissingShip/issues/20) | Entrance music-continuity and transition flags | done 2026-09-26 |
| [MS-095](https://github.com/thefriendlyhedgehog/MissingShip/issues/59) | Entrance collision review and mapping | done 2026-09-25 |

### M3: Actors by scene

| ID | Issue | Result |
|---|---|---|
| [MS-030](https://github.com/thefriendlyhedgehog/MissingShip/issues/21) | Port Navi (En_Elf) behaviour + hook 0x80602000 | done 2026-09-25 |
| [MS-031](https://github.com/thefriendlyhedgehog/MissingShip/issues/22) | Port En_Nutsball (Deku nut projectile) | done 2026-09-26 |
| [MS-032](https://github.com/thefriendlyhedgehog/MissingShip/issues/23) | Port En_Ko (Kokiri text swaps, hook 0x80602200) | done 2026-09-26 |
| [MS-033](https://github.com/thefriendlyhedgehog/MissingShip/issues/24) | Finish partial actors: En_Fz modes, En_Niw_Lady, En_Hy | done 2026-09-26 |
| [MS-034](https://github.com/thefriendlyhedgehog/MissingShip/issues/25) | Monktown Valley (spot01) actors (9) | done 2026-09-26 |
| [MS-035](https://github.com/thefriendlyhedgehog/MissingShip/issues/26) | Bombchu Shop (night_shop): En_Ossan + En_GirlA shop items | done 2026-09-26 |
| [MS-036](https://github.com/thefriendlyhedgehog/MissingShip/issues/27) | Hollow Root Keep (HAKAdan) dungeon actors | done 2026-09-26 |
| [MS-037](https://github.com/thefriendlyhedgehog/MissingShip/issues/28) | Boss fight: Boss_Va + custom actors 0x01/0x17/0x22 + En_Vm + Door_Warp1 | done 2026-09-26 |
| [MS-038](https://github.com/thefriendlyhedgehog/MissingShip/issues/29) | Monktown Sanctuary (Bmori1) dungeon actors | done 2026-09-26 |
| [MS-039](https://github.com/thefriendlyhedgehog/MissingShip/issues/30) | Boss_Ganon2 (kakariko3) and En_Mag (spot00) | done 2026-09-25 |
| [MS-040](https://github.com/thefriendlyhedgehog/MissingShip/issues/31) | Find spawners of the remaining changed unplaced actors | done 2026-09-25 |
| [MS-096](https://github.com/thefriendlyhedgehog/MissingShip/issues/60) | Port Bg_Spot16_Bombstone (spot01) | done 2026-09-26 |
| [MS-097](https://github.com/thefriendlyhedgehog/MissingShip/issues/61) | Port En_Dodojr (spot01) | done 2026-09-26 |
| [MS-098](https://github.com/thefriendlyhedgehog/MissingShip/issues/62) | Port En_Hs (spot01) | done 2026-09-26 |
| [MS-099](https://github.com/thefriendlyhedgehog/MissingShip/issues/63) | Port En_Rd (spot01, night_shop) | done 2026-09-26 |
| [MS-100](https://github.com/thefriendlyhedgehog/MissingShip/issues/64) | Port En_Daiku (spot01) | done 2026-09-26 |
| [MS-101](https://github.com/thefriendlyhedgehog/MissingShip/issues/65) | Port Bg_Spot02_Objects + ML's object 0xA1 (spot01) | done 2026-09-26 |
| [MS-102](https://github.com/thefriendlyhedgehog/MissingShip/issues/66) | Port Obj_Makekinsuta (spot01) | done 2026-09-26 |
| [MS-103](https://github.com/thefriendlyhedgehog/MissingShip/issues/67) | Port Bg_Toki_Hikari as Ml_TalkSpot (spot01) | done 2026-09-26 |
| [MS-104](https://github.com/thefriendlyhedgehog/MissingShip/issues/68) | Port Bg_Po_Event as Ml_BgmTrigger (spot01, hairal_niwa2, night_shop) | done 2026-09-26 |

### M4: Link

| ID | Issue | Result |
|---|---|---|
| [MS-050](https://github.com/thefriendlyhedgehog/MissingShip/issues/32) | Port object_link_child and link_animetion | done 2026-09-26 |
| [MS-051](https://github.com/thefriendlyhedgehog/MissingShip/issues/33) | Port the Link animation override 0x806023E0 | done 2026-09-25 |
| [MS-052](https://github.com/thefriendlyhedgehog/MissingShip/issues/34) | Port the remaining player-overlay edits | done 2026-09-26 |
| [MS-053](https://github.com/thefriendlyhedgehog/MissingShip/issues/35) | Magic Hourglass rewind | done 2026-09-26 |
| [MS-054](https://github.com/thefriendlyhedgehog/MissingShip/issues/36) | Soul-Charged sword model and glow | done 2026-09-26 |
| [MS-055](https://github.com/thefriendlyhedgehog/MissingShip/issues/37) | Locate ML code past 0x80608000 | done 2026-09-25 |

### M5: Front end and polish

| ID | Issue | Result |
|---|---|---|
| [MS-060](https://github.com/thefriendlyhedgehog/MissingShip/issues/38) | HUD restyle (B/C buttons, key counter, colour) | done 2026-09-26 |
| [MS-061](https://github.com/thefriendlyhedgehog/MissingShip/issues/39) | ML title screen: decide scope and port | decided: out of scope; ML's title screen ported in MS-039 |
| [MS-062](https://github.com/thefriendlyhedgehog/MissingShip/issues/40) | File select and opening edits | done 2026-09-25 |
| [MS-063](https://github.com/thefriendlyhedgehog/MissingShip/issues/41) | Re-verify pause quest-status cursor | done 2026-09-26 |

### M6: Drop-in

| ID | Issue | Result |
|---|---|---|
| [MS-016](https://github.com/thefriendlyhedgehog/MissingShip/issues/54) | libultraship GfxDpLoadBlock mis-sizes 4-bit block loads (unsigned -1 shift) | done 2026-09-25 |
| [MS-070](https://github.com/thefriendlyhedgehog/MissingShip/issues/42) | Drop-in path: upstream bugfixes and data-driven proposals | **open** |

### Platforms

| ID | Issue | Result |
|---|---|---|
| [MS-080](https://github.com/thefriendlyhedgehog/MissingShip/issues/43) | Windows build and package | done 2026-09-26 |
| [MS-081](https://github.com/thefriendlyhedgehog/MissingShip/issues/44) | Linux build and package | done 2026-09-27 |
| [MS-082](https://github.com/thefriendlyhedgehog/MissingShip/issues/45) | Cross-platform pipeline scripts | not needed: the packages ship the finished mod (2026-09-27) |
| [MS-083](https://github.com/thefriendlyhedgehog/MissingShip/issues/46) | Check non-Metal backends for the zero-size texture guard | done 2026-09-25 |

### Porting kit

| ID | Issue | Result |
|---|---|---|
| [MS-090](https://github.com/thefriendlyhedgehog/MissingShip/issues/47) | Generalise tools with a per-hack config | done 2026-09-25 |
| [MS-091](https://github.com/thefriendlyhedgehog/MissingShip/issues/48) | Porting-kit knowledge base | **open** |
| [MS-092](https://github.com/thefriendlyhedgehog/MissingShip/issues/49) | Per-hack workspace layout | **open** |
| [MS-093](https://github.com/thefriendlyhedgehog/MissingShip/issues/50) | One-command romhack survey script | **open** |

### Found in porting and playtesting (no milestone)

| ID | Issue | Result |
|---|---|---|
| [MS-107](https://github.com/thefriendlyhedgehog/MissingShip/issues/71) | Port Boss_Tw slot as Ml_SlideBlock (HAKAdan ×26) | done 2026-09-25 |
| [MS-108](https://github.com/thefriendlyhedgehog/MissingShip/issues/72) | Port ML custom actor 0x1A as Ml_RoomClearSwitch (HAKAdan room 20) | done 2026-09-26 |
| [MS-109](https://github.com/thefriendlyhedgehog/MissingShip/issues/73) | Port Bg_Hidan_Dalm slot as Ml_ElevatorSpawner (HAKAdan room 19) | done 2026-09-26 |
| [MS-110](https://github.com/thefriendlyhedgehog/MissingShip/issues/74) | Port En_Kakasi slot as Ml_Elevator (HAKAdan) | done 2026-09-25 |
| [MS-111](https://github.com/thefriendlyhedgehog/MissingShip/issues/75) | Port Bg_Mori_Elevator slot as Ml_HakaLift (HAKAdan room 2) | done 2026-09-25 |
| [MS-112](https://github.com/thefriendlyhedgehog/MissingShip/issues/76) | Port En_Floormas giant variant (HAKAdan room 20) | done 2026-09-26 |
| [MS-113](https://github.com/thefriendlyhedgehog/MissingShip/issues/77) | Port En_Ssh changes (HAKAdan cursed men give small keys) | done 2026-09-25 |
| [MS-114](https://github.com/thefriendlyhedgehog/MissingShip/issues/78) | Port Bg_Mizu_Shutter changes (HAKAdan) | done 2026-09-25 |
| [MS-115](https://github.com/thefriendlyhedgehog/MissingShip/issues/79) | Child Link's Hylian Shield is invisible in his hand | done 2026-09-26 |
| [MS-116](https://github.com/thefriendlyhedgehog/MissingShip/issues/80) | Deep Woods gate next to the two scrubs has no collision | done 2026-09-26 |
| [MS-117](https://github.com/thefriendlyhedgehog/MissingShip/issues/81) | Monkey Town maze grotto hole (room 3) is invisible | done 2026-09-26 |
| [MS-118](https://github.com/thefriendlyhedgehog/MissingShip/issues/82) | Option to stop the magic bean salesman flashing | done 2026-09-26 |
| [MS-119](https://github.com/thefriendlyhedgehog/MissingShip/issues/83) | Bunny Hood appears to do nothing | done 2026-09-25 |
| [MS-120](https://github.com/thefriendlyhedgehog/MissingShip/issues/84) | Grotto on the upper Deku trading area soft-locks on entry | done 2026-09-26 |
| [MS-121](https://github.com/thefriendlyhedgehog/MissingShip/issues/85) | Port ML's Shadow Temple boss (Boss_Va rewrite, slot 0xBA) as Ml_BossVa | done 2026-09-25 |
| [MS-122](https://github.com/thefriendlyhedgehog/MissingShip/issues/86) | Port ML custom actor 0x17 as Ml_BossVaShot | done 2026-09-25 |
| [MS-123](https://github.com/thefriendlyhedgehog/MissingShip/issues/87) | Port ML actor 0x01 (En_Elf copy) as Ml_Elf | done 2026-09-26 |
| [MS-124](https://github.com/thefriendlyhedgehog/MissingShip/issues/88) | Port ML actor 0x22 (En_Light copy) as Ml_Light | done 2026-09-25 |
| [MS-125](https://github.com/thefriendlyhedgehog/MissingShip/issues/89) | Port En_Vm change (Beamos not Z-targetable) | done 2026-09-26 |
| [MS-126](https://github.com/thefriendlyhedgehog/MissingShip/issues/90) | Blue warps lead to the Fishing Pond (Door_Warp1, cutscene destination 56, entrance 0x420) | done 2026-09-26 |
| [MS-127](https://github.com/thefriendlyhedgehog/MissingShip/issues/91) | Controllable Bombchus as an option | already in stock SoH: Enhancements > Remote Bombchu |
| [MS-128](https://github.com/thefriendlyhedgehog/MissingShip/issues/92) | Hollow Root Keep mini-boss skeleton walks on air and hops into the fire-chest area | authentic to ML, kept (2026-09-25) |
| [MS-129](https://github.com/thefriendlyhedgehog/MissingShip/issues/93) | Magic Hourglass get-item model is broken (only half drawn, stays on screen) | done 2026-09-26 |
| [MS-130](https://github.com/thefriendlyhedgehog/MissingShip/issues/94) | Use Missing Link's own names for areas and scenes in docs and tools | done 2026-09-25 |
| [MS-131](https://github.com/thefriendlyhedgehog/MissingShip/issues/95) | Port ML's Monktown Sanctuary (Bmori1) torch (Boss_Dodongo slot 0x27) as Ml_Torch | done 2026-09-26 |
| [MS-132](https://github.com/thefriendlyhedgehog/MissingShip/issues/96) | Port ML's Monktown Sanctuary (Bmori1) switch pillars (En_Du slot 0x98) as Ml_Pillar | done 2026-09-26 |
| [MS-133](https://github.com/thefriendlyhedgehog/MissingShip/issues/97) | Port ML's Monktown Sanctuary (Bmori1) thieves (En_Nb slot 0xC3) as Ml_Thief | done 2026-09-26 |
| [MS-134](https://github.com/thefriendlyhedgehog/MissingShip/issues/98) | Port ML's Monktown Sanctuary (Bmori1) sliding platform (Bg_Jya_1flift) | done 2026-09-26 |
| [MS-135](https://github.com/thefriendlyhedgehog/MissingShip/issues/99) | Monktown Sanctuary (Bmori1) Peahat plays mini-boss music | done 2026-09-26 |
| [MS-136](https://github.com/thefriendlyhedgehog/MissingShip/issues/100) | Port ML's title screen (En_Mag rewrite, object_mag) | done 2026-09-26 |
| [MS-137](https://github.com/thefriendlyhedgehog/MissingShip/issues/101) | Port ML's Library (kakariko3) push block (Boss_Ganon2 slot 0x17A) as Ml_PushBlock | done 2026-09-26 |
| [MS-138](https://github.com/thefriendlyhedgehog/MissingShip/issues/102) | Port ML's ending card (En_Dodongo slot 0x12) as Ml_Ending | done 2026-09-26 |
| [MS-139](https://github.com/thefriendlyhedgehog/MissingShip/issues/103) | Hollow Root Keep (HAKAdan) Bombchu rail (En_Bom_Chu hook 0x80607A00) | done 2026-09-26 |
| [MS-140](https://github.com/thefriendlyhedgehog/MissingShip/issues/104) | Cosmetics editor colours for child Link's items patch ML's display lists at vanilla indices | done 2026-09-26 |
| [MS-141](https://github.com/thefriendlyhedgehog/MissingShip/issues/105) | Keep the Bunny Hood on through void-outs and deaths (QoL) | done 2026-09-25 |
| [MS-142](https://github.com/thefriendlyhedgehog/MissingShip/issues/106) | Hollow Root Keep B1 <> 1F elevator moves very fast | authentic to ML, kept (2026-09-25) |
| [MS-143](https://github.com/thefriendlyhedgehog/MissingShip/issues/107) | QoL adjustable climbing speed | already in stock SoH |
| [MS-144](https://github.com/thefriendlyhedgehog/MissingShip/issues/108) | Crash when the Gerudo Valley horse race starts | done 2026-09-26 |
| [MS-145](https://github.com/thefriendlyhedgehog/MissingShip/issues/109) | Settings cleanup: MissingShip QoL section and recommended stock SoH settings | done 2026-09-26 |
| [MS-146](https://github.com/thefriendlyhedgehog/MissingShip/issues/110) | Cluck Cluck Ranch stable Gold Skulltula can't be killed (items restricted) | authentic to ML, kept (2026-09-25) |
| [MS-147](https://github.com/thefriendlyhedgehog/MissingShip/issues/111) | Deep Woods maze goes black after entering (fog wall loads rooms 2/1 and unloads room 5) | done 2026-09-25 |
| [MS-148](https://github.com/thefriendlyhedgehog/MissingShip/issues/112) | Visiting Cluck Cluck Ranch takes away the Soul-Charged Kokiri Sword (no Blade Beam, no charge glow) | done 2026-09-26 |
| [MS-149](https://github.com/thefriendlyhedgehog/MissingShip/issues/113) | Monktown Valley boulder switch changes the scenery live, not only after re-entering | done 2026-09-26 |
| [MS-150](https://github.com/thefriendlyhedgehog/MissingShip/issues/114) | Magic Hourglass rewind just after a door puts Link in the unloaded old room (black screen) | done 2026-09-26 |
| [MS-151](https://github.com/thefriendlyhedgehog/MissingShip/issues/115) | After the Hollow Root Keep boss, ML's credits tour (pond to the ending card) was cut short | done 2026-09-25 |
| [MS-152](https://github.com/thefriendlyhedgehog/MissingShip/issues/116) | Monktown Maze (Deep Woods) doesn't show up on the pause world map | done 2026-09-26 |
| [MS-153](https://github.com/thefriendlyhedgehog/MissingShip/issues/117) | Find out what players start from: Kaze's ML v2.0 patch, its base ROM, and whether one ROM can cover SoH's base too | done 2026-10-17: OoT USA 1.0 plus Kaze's patch (or a patched ML v2.0 ROM), checked by CRC; the patch is not bundled. Every playthrough since 2026-10-11 runs on files built from USA 1.0. |
| [MS-154](https://github.com/thefriendlyhedgehog/MissingShip/issues/118) | Standalone mod builder for Windows, Linux and macOS | done 2026-10-17: the builder in every release since unbound-930-builder-1; a window and double-click launchers since builder-3. The three OSes write the same layer and code mod. |
| [MS-155](https://github.com/thefriendlyhedgehog/MissingShip/issues/119) | First launch builds the ML mod: ask for the ML ROM, verify it, run the builder | not planned, 2026-10-17: superseded by ML on Unbound. The engine beta keeps its first-launch builder (ledger row 143); on Unbound, players run the builder window. |
| [MS-156](https://github.com/thefriendlyhedgehog/MissingShip/issues/120) | Release packages carry no game data | done 2026-10-17: `tools/nodata.py` fingerprints every decompressed file of OoT USA 1.0 and ML v2.0 (also byte-swapped) and scans each package and every archive in it; `package.py` runs it on the Unbound packages and removes one that fails. builder-3's three packages pass: their only matches are seven constant tables (64-264 bytes) in the code mod, from vanilla and ML actor code. The retired engine packages fail as expected: SoH's own `soh.o2r` (two button textures and a fishing-pole texture share OoT art) and math tables of up to 2.5 KB in the engine binary; the Unbound packages ship neither. |

---

## Appendix: the 2026-09-25 review

The review that set the plan: every ML dma file was compared against PAL MQ Debug. It is kept as a record of what ML
changed and as a worked example for the porting kit. Everything it lists as built or awaiting playtest has since been
verified (MS-010).

**Method:** every ML dma file was hashed against the debug ROM.
- **257** ML files have no identical debug file:
  - 81 actor overlays (some differ only by relocation);
  - 34 objects;
  - about 107 scene and room files, which the pipeline already handles (25 scenes, 72 rooms);
  - about 35 system files.
- ML **deleted** 261 vanilla objects and many vanilla actors (Hookshot, Epona, …). These need no work unless something spawns them.

### A. Actors placed in ML scenes (all built; awaiting playtest)

| Scene | Unported actors |
|---|---|
| `spot01` (Kakariko slot) | **Built (MS-034):** En_Dodojr ×4, Bg_Toki_Hikari, En_Rd ×6, Bg_Po_Event ×2, Bg_Spot02_Objects ×3, Bg_Spot16_Bombstone, En_Daiku ×2, En_Hs, Obj_Makekinsuta |
| `night_shop` (Bombchu Shop) | **Built (MS-035):** En_Ossan and En_GirlA's Bombchu prices; En_Rd and Bg_Po_Event built in MS-034 |
| `hairal_niwa2` | Bg_Po_Event ×2 (built in MS-034) |
| `HAKAdan` (dungeon) | **Built (MS-036):** Boss_Tw ×26 (Ml_SlideBlock), ACTOR_UNSET_1A (Ml_RoomClearSwitch), Bg_Hidan_Dalm (Ml_ElevatorSpawner), Bg_Mori_Elevator (Ml_HakaLift), En_Floormas (giant), En_Kakasi ×2 (Ml_Elevator), En_Ssh ×4, Bg_Mizu_Shutter |
| `HAKAdan_bs` (boss room) | **Built (MS-037):** Boss_Va (Ml_BossVa, ML's own boss on object_ml_bv) with Ml_BossVaShot, Ml_Elf, Ml_Light, En_Vm and Door_Warp1 |
| `Bmori1` (dungeon) | **Built (MS-038):** En_Peehat (mini-boss music), Boss_Dodongo ×2 (Ml_Torch), En_Du ×3 (Ml_Pillar, object_ml_du), En_Nb ×3 (Ml_Thief, object_ml_nb), Bg_Jya_1flift (sliding platform) |
| `spot00` | **Built (MS-039):** En_Mag (ML's title screen on object_ml_mag) |
| `kakariko3` | **Built (MS-039):** Boss_Ganon2 (Ml_PushBlock, an Obj_Oshihiki copy on object_ml_ganon2) |
| Partial | none since MS-033: Ml_EventSpawner is complete (mode 3's trigger, the pond after the Shadow boss, came with MS-037), En_Niw_Lady and En_Hy were already complete, En_Ko was ported in MS-032 |

### B. Changed actors that are not placed but are spawned at runtime

The old inventory only surveyed placed actors, so it missed these. A spawn scan of `code`, the player overlay, the custom code and every overlay ties them to gameplay.

| Actor | Change | Reached from |
|---|---|---|
| **En_Elf (Navi)** | 4018 text words (a recompile) + hook `0x80602000` | **Ported** (MS-030): ML never draws Navi, and hides the other fairies after the Soul-Charged sword. |
| En_Nutsball (Deku nut projectile) | rewritten (0x590 vs 0x550) | Deku Scrubs, already ported |
| En_GirlA (shop items) | 8 data words | **Ported** (MS-035): Bombchu prices 40 / 80 |
| Door_Warp1 (blue warp) | 5 text words | **Ported** (MS-037): blue warps go to the Fishing Pond (ML entrance 0x41C) |
| En_Vm (Beamos) | 1 data word; object DLs changed | **Ported** (MS-037: never Z-targetable; the DLs in MS-021). Boss_Va spawns three |
| En_Bom_Chu | 974 text words (a shift) + hook `0x80607A00` | **Ported** (MS-139): the hook is a HAKAdan-only Bombchu rail |
| Item_Shield | 1 text word | Vanilla behaviour: the word is object_link_child's moved Deku Shield DL, which ships under its vanilla name (MS-050) |
| Bg_Dy_Yoseizo, Bg_Haka_Huta | rewritten | Bg_Dy_Yoseizo: an unplaced scenery prop, unreachable. Bg_Haka_Huta: relocation noise only (MS-040) |
| En_Dodongo (0x12) | rewritten (0x840 vs 0x2570) | **Ported** (MS-138): ML's ending card, in `spot10` room 1's cutscene header |
| En_Ishi (rocks), En_Karebaba, Obj_Ice_Poly | data / 1 word | En_Ishi: vanilla code, dead ML code in its data padding. En_Karebaba (not hostile) and Obj_Ice_Poly (no AT) ported but unreachable (MS-040) |

### C. ML custom actors in slots vanilla leaves empty

All eight are ported: 3, 5, 6 and 0x1F earlier, 0x1A in MS-036, and the boss fight's three in MS-037:

| Slot | Size | Category / object | Ported as |
|---|---|---|---|
| 0x01 | 0x4AE0 | itemaction / gameplay_keep | Ml_Elf (En_Elf copy): Boss_Va's fairies, and the Navi-type fairy at the pond |
| 0x17 | 0x470 | prop / object_bv (0x8C) | Ml_BossVaShot: Boss_Va's projectile |
| 0x1A | 0x2B0 | bg / gameplay_keep | Ml_RoomClearSwitch (HAKAdan) |
| 0x22 | 0xE90 | itemaction / gameplay_keep | Ml_Light (En_Light copy): Boss_Va's death flames |

### D. Objects not yet ported

| Object | What ML changed | Kind of work |
|---|---|---|
| object_link_child (0x15) | 35,836 words; Link's model | **Ported** (MS-050): ML's whole child Link ships under the vanilla path and names; the cosmetics editor's item colours are MS-140 |
| object_tw, object_nb, object_du, object_mag, object_ganon2, object_ka, object_spot02_objects, object_mori_objects | redrawn for their rewritten actors | with each actor |
| gameplay_keep | sword-turn recoveries `link_fighter_turn_kiruL_end` / `kiruR_end` (frames 16 → 13 / 14, ML's frames) done (MS-026; `kiruR` is unchanged); unused shape DLs; drop-bomb texture and grotto hole are done | data (animations) |
| gameplay_dangeon_keep | push-block textures | **data only**: texture aliases |
| object_box | chest textures + the curve skeleton | mostly data |
| object_bombf | bomb-flower leaf textures | data only |
| object_bdoor, object_haka_door | door textures | data only |
| object_jya_obj | `g1f1fiftTopTex` | data only |
| object_vm | Beamos body + eyelid DLs | data (DL aliases) |
| object_sa | Saria's right-hand limb (1 word) | data |

### E. System overlays and data files

| File | Status |
|---|---|
| **Player overlay** (dma 102) | 88 changed words ≈ 20 distinct edits, **ported** (MS-051, MS-052, MS-053; see ENGINE_CHANGES rows 96–98). Originally: 30 `jal 0x806023E0` Link-animation overrides (**ported**, MS-051: the triple jump); fairy-release spawn replaced by a flag (+0x38CC); a second spawn removed (+0x74CC); a scene-gated music/sfx block (+0x7100); hooks into custom code at 0x80606BA0, 0x80606BC8, 0x80600480, 0x80600D90; Magic Hourglass `j 0x80600E00` (+0x17E5C); 4 forced branches; 3 constant changes (+0x6104, +0xA080, +0x14E20 0x78→0x50, +0x167E0). The Blade Beam works through a different route. |
| Pause menu (dma 101) | 46 words; mostly ported (ledger row 24). Recheck the cursor. |
| Boot screen (dma 97, `ovl_title`) | ML's rebuild of the N64-logo state: two error screens (Expansion Pak missing; "You've met with a terrible fate" for an emulator that fails its accuracy check) around the normal logo. **Out of scope** (MS-061): neither check applies to SoH, which shows its own boot logo. ML's actual title screen is En_Mag (MS-039). |
| File select (dma 100) | 1 forced branch (+0xC9D8): the debug ROM's File 1 → Map Select is never taken. **Nothing to port** (MS-062): SoH only does that with Developer Tools' Debug Enabled on. |
| Opening (dma 99) | 3 words of the title setup: child Link, cutscene 0xFFF0, layer 0. **Ported** (MS-062, row 101); title checked in game. |
| map_mark_data (dma 103) | 58 data words: dungeon-map chest / boss markers. **Ported** (MS-023, with the kaleido pause-map markers ML also edited). |
| map_i_static (dma 113), map_48x85_static (dma 112) | redrawn dungeon minimaps and pause dungeon maps. **Ported** (MS-022: 32 textures, the room + 190 index and ML's map tables). |
| staff_message_data_static (dma 110) | ML's credits text. **Ported** (MS-024: 11 of 48 credits rewritten by ML, via inject_messages.py). |
| Skyboxes | `vr_fine1_static` and `vr_cloud2_static` (+ palettes) redrawn. **Ported** (MS-025: all 12 resources, via inject_minimaps.py). |
| link_animetion (dma 146, 2.5 MB) | Link's animation data changed. **Nothing more to port** (MS-050): ML's only changes are the two sword-turn recoveries MS-026 already ships (`gPlayerAnimData_114420` / `_114FB0`) and their two headers. |
| Entrance flags | ML's title-card / music / transition flags. **Ported** (MS-027): 184 SoH entrance slots take ML's flags; 4 day-fade colours are a known deviation. |

### F. ML custom code (dma 507) still unported

- None left: every hook in ENGINE_CHANGES' hook table is ported or recorded (Navi MS-030, Bombchu MS-139, player hooks MS-017 / MS-052 / MS-053, sword MS-054, HUD MS-060).
- There is no ML code past 0x80608000 (MS-006): the old 0x8064xxxx / 0x807Cxxxx targets were data. The custom code jumps into the player overlay, which always loads at RAM 0x804D23B0.
