# ML on SoH: Unbound — what each engine change becomes (step 2)

Every row of [`ENGINE_CHANGES.md`](ENGINE_CHANGES.md), placed against SoH: Unbound 0.8
(`roborich/Shipwright` branch `unbound`, 0382526, 2026-09-27) and its format spec (`unbound-docs/SPEC.md`, format
version 2). Rows 1–101 follow the groups already worked out in [`DROPIN_PROPOSALS.md`](DROPIN_PROPOSALS.md) (F1–F11,
B1–B5, D1–D3); rows 102–144 are placed here for the first time.

Step 1 is done: `tools/unbound_layer.py` turns ML's archive into an Unbound mod layer (25 scenes, 304 messages, ML's
objects, textures and audio), and Unbound loads it and draws ML's scenes. Everything below is what that layer does
*not* carry.

## A. Already covered by Unbound

| What ML needs | Rows | Unbound feature |
|---|---|---|
| ML's scenes and rooms, including the ones in unused vanilla slots | pipeline; 112 | JSON scenes (SPEC §4); a missing alternate setup falls back to setup 0 (§4.2), which is what row 112's bounds check did |
| ML's text | pipeline (`inject_messages.py`) | `text/<lang>/messages.json` merges per id (§5) |
| Animated scene materials for ML's scenes | draw-config row (part of F6) | `materialAnims` (§4.2), where ML's config is a scroll or colour cycle |
| Collision, room and actor limits | none needed so far | raised limits (§9) |

## B. Data features to build in Unbound (new format, spec'd in its style)

Each is general, so any mod could use it; each gets a SPEC section, a how-doc and a verification plan, as Unbound's
own features do.

| Id | Feature | Rows | Status against Unbound |
|---|---|---|---|
| U1 | **Vanilla entrance overrides**: a mod changes a vanilla entrance's flags (title card, music continuity, transitions) and destination spawn | 1, 28, 50 (F1) | **Done** for rows 1 and 50 (2026-09-27): an `unbound/scenes.json` entry keyed by a vanilla scene adds entrances to it and overrides its vanilla ones, and `layers` gives an entrance cutscene setups (Unbound SPEC §7.1, `unbound-docs/registries.md`). `tools/unbound_ml_world.py` writes the fork's 32 entrances as 18 groups in 34 vanilla scenes (the eight-entry event groups with layer 4) and ML's flags (`ml_entrance_flags.c`) on the 164 vanilla entries and 24 of the fork's own that it changes, per layer where a group's layers differ. **Row 28 done** (2026-09-28, Unbound `6e412f4f8`): `saveReturn` on a scene and `continueAt` on an entrance say where a loaded save and the game-over Continue resume (SPEC §7.1, `unbound-docs/registries.md`). `tools/unbound_ml_world.py` reads vanilla's rules from Unbound's `Sram_OpenSave` and `KaleidoScope_Update` and ML's meaning from the fork's `ml_entrance_map.c`: 21 scenes' save returns (a save in the Deku Tree resumes at the Lost Woods' south exit, as in the fork) and 9 continue points (two of them, Jabu-Jabu's and Bongo Bongo's boss rooms, continue in place). The rest of row 28 is elsewhere: the fork's own entrances are U1's custom entrances, `Door_Ana` is U5. Outside dungeons ML resumes a save where it was written, which on Unbound is SoH's Remember Save Location option (ENGINE_CHANGES "Setting" row), not mod data: owner decision 2026-09-28, players are told to turn it on (README, *ML on SoH: Unbound*) |
| U2 | **Cutscene destinations by entrance name** (`CS_DESTINATION` to any registered entrance, and the black-fill escape) | 2, 84, 109 (F2) | **Done** (2026-09-27): `unbound/cutscenes.json` names each cutscene's destination entrance, setup, transition and time, after vanilla's code for the number runs; a destination ends the black fill (SPEC §7.5, `unbound-docs/cutscene-destinations.md`). The tool writes 21: the pipeline's rewritten codes and ML's ten patched cases, which keep vanilla's side effects as the fork does (Requiem on 22, the age on 65 and 73). The credits tour runs its chain by log |
| U3 | **Entrance intro-cutscene table and conditional triggers** (which cutscene plays on which entrance and flag, and removing vanilla ones) | 3, 110, 114 (F3) | **Done** (2026-09-27): an entrance's `intro` plays ML's cutscene or removes vanilla's, with the conditional triggers keyed to it (SPEC §7.1). The tool writes ML's ten intros (the pond's with `repeats`, since vanilla's loop replays its Epona flag, as in ML) and `false` on the 28 other entrances vanilla's table plays one at and on the Requiem trigger's entrance; Nocturne goes with ML's Monktown Valley intro, and Saria's bridge trigger stays, as in the fork |
| U4 | **New-save template** (inventory, equipment, songs, starting entrance, where Skip Intro lands) | 13, 115 (F5) | **Done** (2026-09-27): `unbound/save.json` sets the new file's items, buttons, owned and worn equipment, upgrades, quest items and magic, and where Skip Intro lands (SPEC §7.6, `unbound-docs/new-save.md`). The tool writes ML's template and 0x5F0 at noon. The fork's "Starting Ocarina Button" option (D-Down by default) is ours, not ML's: on Unbound the Ocarina goes on C-Left, as ML has it (owner decision 2026-09-28), and the option is not carried over |
| U5 | **Grotto table** (`Door_Ana` destinations and return respawn) | 25 (F4) | **Done** (2026-09-28, Unbound `719d4ef7b`): `unbound/grottos.json` says where each of vanilla's 15 grotto slots leads and whether falling in records the return point (SPEC §7.10, `unbound-docs/grottos.md`). `tools/unbound_ml_world.py` writes ML's table where it differs from vanilla's (slots 1-8, slot 2 the fork's own Deku Tree entrance) and no return point on any slot, as the fork nops it; `tools/unbound_layer.py` places ML's 14 grottos as `ml/grotto`, a type extending `Door_Ana` that draws ML's hole (`object_ml_gkeep/gMlGrottoDL`) from gameplay_keep. By log on macOS, ML's grottos reach the Colossus and the Deku Tree. **Found then, not U5's:** entering ML's Desert Colossus (0x1E1) or Deku Tree (0x618) on Unbound crashed the renderer: those scenes kept vanilla's draw config, which binds segment 8 to a texture ML's display lists call. **Fixed** (2026-09-28, Unbound `1d16528b1`): `drawConfig` on a vanilla scene entry (SPEC §7.1), and `tools/unbound_ml_world.py` writes ML's for its 25 scenes (the ENGINE_CHANGES "Scene draw configs" row; ML's config 0 runs the Deku Tree's handler, so it is written as 19). **Also crashing on Unbound, older than this, both fixed 2026-09-28 in section C:** ML's Kakariko, from row 60 (ML's `Bg_Spot02_Objects`, placed there as 0xF0F, which vanilla's Init reads as a type it has no case for), and ML's Kokiri Forest in `EnMd_Init`, from row 14 (vanilla moves Mido to the end of path 0, and ML's Kokiri Forest has no path list) |
| U6 | **GetItem draw table** | 16 (F7) | Gap |
| U7 | **Pause-menu and minimap tables** (slot positions, cursor redirects, colours, map tables, markers, the dungeon-minimap default) | 24, 48, 49, 138 (F8) | **Done** for the maps, rows 48, 49, 138 and row 5's overworld default (2026-09-27): `unbound/maps.json` changes a vanilla dungeon's minimap and pause map in place in vanilla's tables (room textures, compass points, floors, palettes, room switches, chest and boss marks) and `minimap: false` hides a scene's minimap unless the player picked a position, with L silent (Unbound SPEC §7.8, `unbound-docs/maps.md`). `tools/unbound_ml_maps.py` reads the fork's tables against Unbound's: the ten dungeons (ML's Shadow Temple block textures, index 190 + room, in the other nine; its palettes, floors and Shadow Temple compass; no room switches; its MQ-based marks where they differ from either of Unbound's tables) and `minimap: false` on the 20 overworld scenes. The dungeon minimap shows as vanilla's does, so row 138 needs nothing. Custom scenes still have no map (Unbound's §9 limit). **Row 24 done** (2026-09-27, U7b): `unbound/pause.json` sets the age rules, item grid X, ammo counts (slots 8-10, Sticks and Nuts over slots 9 and 10, Bombchus full at 20), equipment columns and vertical cursor (remap, stops on any slot), quest slot positions, right-edge entry slot 24 and cursor redirects, world-map pieces and point rules (points 0 and 1 hidden, six on eventChkInf 0xA3-0xA9) and the info-panel colours (Unbound SPEC §7.9, `unbound-docs/pause.md`); `tools/unbound_ml_pause.py` reads them against Unbound's. Differences from the fork: the fork's page colours (sPageVtxColor*) never showed, since SoH draws pages from its cosmetics defaults, so they are not written and the pages keep the colours the fork shows (owner decision 2026-09-28: the fork's colours, not ML's parchment); the equipment cursor's column follows the point it lands on (ML's code sets column 1 for points it never reaches); ML's down-move in the upgrade column tests an unused save word, and here tests the upgrades as vanilla does; a remap that leads back to the start ends the move instead of looping. The game-over Continue's ML entrance translation in `KaleidoScope_Update` is U1's `continueAt` (row 28, 2026-09-28) |
| U8 | **Shop tables** | 32, 67 (F9) | Gap |
| U9 | **Vanilla actor overrides**: InitVars flags, "don't spawn this actor" | 8, 41, 42, 82, 94, 122 (F10) | **Done** (2026-09-27): an `unbound/actors.json` entry keyed by an actor's name overrides it everywhere (Unbound SPEC §7.4, `unbound-docs/actor-overrides.md`). `tools/unbound_layer.py` writes ML's six (`ML_OVERRIDES`); the log shows each flag change matching the ledger. Rows 8 and 122 become `Bg_Treemouth` `spawn: false`, which is what the fork's kill in Init amounted to |
| U10 | **Game labels**: item and quest-item names, area names for stats and splits | 108, 111, 121, 137 | **Done** for the labels (2026-09-27): `unbound/labels.json` names items, quest items and timestamps in SoH's UI, per language or for all, lists the Gameplay Stats rows a mod fills, replaces Time Splits' list and groups and sets the countdown icon; `name` on a vanilla scene entry renames it wherever SoH's UI names it (Unbound SPEC §7.7 and §7.1, `unbound-docs/labels.md`). `tools/unbound_ml_ui.py` reads the fork's tables against Unbound's: 12 item and 2 quest-item labels, 4 timestamps, 22 count rows, 28 splits, the Kokiri Sword icon and 25 area names. On Unbound the area names also show in SoH's menus and warp lists, not only in Gameplay Stats. Row 108's `Ml_BossVa` boss-defeat and `Ml_Ending` game-complete calls are ML actor code, not labels, and go with those actors |
| — | Push-block colours | 39 (F11) | Low value; fold into U9 if needed |

## C. Behaviour (actor and player logic in C)

The hard part: about 70 rows (DROPIN_PROPOSALS §5: 6, 7, 9, 10, 12, 14, 17, 20–22, 26, 27, 29–31, 33–37, 40, 46,
51–63, 65, 66, 68–81, 83, 85–101 and parts of 24, 48, 94) plus 103, 105, 107, 122, 123, 125, 126 and 127 here.
Unbound's custom actors are data-only today. Its planned route is `base` (a vanilla actor's code under a new type,
with asset swaps and text remaps) and then `script`.

| Kind | Rows (examples) | Route |
|---|---|---|
| Asset or text swap on a vanilla actor | 38 (breakable wall), 123 (Kokiri frown texture), 127 (scrub's sales line), part of the pickup ALIASES | **38 done** with `base` (`ml/crack_wall` in `tools/unbound_layer.py`, 2026-09-27): ML's two Deku Tree walls spawn from object 0xA3 with ML's model and collision, verified by log. 123 and 127 depend on save state (sword, a sale), so `base`'s fixed swaps don't express them |
| Small rule change in a vanilla actor | 105 (fog wall), 125 (title bars), 103 (bean seller flash) (122, the tree mouth, is covered by U9) | `base` with a named hook per actor, or `script` |
| ML's new actors and Link's changes (triple jump, Magic Hourglass, Soul-Charged sword, Arborachnid, ...) | 51–63, 68–81, 107, ... | `script`, or C actors registered by name (SPEC §4.3 lets a mod place "actors a build adds in code") |

**Owner decision (2026-09-28): C actors registered by name in our Unbound branch** (route A), one per ML change,
named `ML_` and the vanilla actor (Unbound `unbound-docs/missinglink-actors.md`, tagged `SOH [MissingLink]`); a
scripting design may follow if any of it is to go upstream. `tools/unbound_layer.py` (`ML_CODE_ACTORS`) renames ML's
placements to them; vanilla scenes keep the vanilla actor. `base` still comes first where it is enough.

| Row | ML actor | Status |
|---|---|---|
| 60 | `ML_Bg_Spot02_Objects` | **Done** (2026-09-28, Unbound `8193a334f`): ML's Init and Draw on vanilla's instance and action functions, with the live switch (MS-149). ML's Kakariko (entrance 0xDB), which crashed on its first frame, runs by log and screenshot on macOS (Metal). Left there: `Actor_Spawn: no actor has id 0x6` twice (ML's `Ml_Villager`, ledger row 10: done below) |
| 14 | `ML_En_Md` | **Done** (2026-09-28, Unbound `d83a41c52`): ML's Init (always stays at home), Update (no action function; 0x1046 → 0x1044 once the slingshot has ammo) and Kokiri Forest text (the Mido's House lines), on vanilla's instance and helpers. ML's Kokiri Forest (entrance 0xEE), which crashed on its first frame, runs on macOS (Metal); a screenshot shows Mido at his placement, targetable. Talking to him not yet checked in game. The `no actor has id 0x5` it left was row 20, below |
| 20 | `ML_TreeBridge` | **Done** (2026-09-28, Unbound `84ff78cae`): ML's own actor (vanilla's unused 0x005), ported from the fork with its collision in Unbound's layout; needs B1. In ML's Kokiri Forest, with a temporary hook setting its switch: the tree falls under ML's camera, and Link stands on the fallen bridge for 15 s (crash without B1). The Blade Beam (row 17) fells it, and so does a charged spin next to it, as in ML |
| 10 | `ML_Villager`, `ML_Fishpond` | **Done** (2026-09-28, Unbound `cac1a4448`, `d6f84ccd7`): ML's own actors 0x006 and 0x003, ported from the fork and placed by name. Villager: in ML's Kakariko both spawn with their lines; with Bmori1's switch 7 set by a temporary hook a screenshot shows one drawn, facing Link and targetable, and without it he is gone. Pond: it draws the pond area's trees and ground (A/B: without it the area is empty fog). Its params-0 companions, ML's fairy (`ML_Elf`, after the blue warp) and event spawner (`ML_EventSpawner` mode 3, after the pond intro), are spawned by those names: the event spawner is now ported (rows 26, 54, below), the fairy is skipped with an error until it is. Talking to the villagers not yet checked in game |
| 26, 54 | `ML_EventSpawner` (row 26's other two, `Ml_FogWall` and `Ml_HintNuts`, not yet) | **Done** (2026-09-28, Unbound `88a81dba4`): ML's rewrite of En_Fz, ported from the fork and placed by name at ML's 24 placements of 0x121; its entrances are vanilla's in the slots ML builds in. On macOS (Metal): young Epona stands at the ranch; on Deku Road by the event entrance the chase starts, its 40-second timer counting down; at the pond by its event entrance she carries Link. ML's Malon, which the chase's mode 4 spawns, is spawned by name and skipped with an error until it is ported. Not seen to its end: the timer running out and the transfer to Talon's house |
| 17 | ML's `En_M_Thunder` (slot 0x57) + the Blade Beam spawner | **Done** (2026-09-28, Unbound `6aeb7e1b9`): the player spawns 0x57 by number, so this is the first ML change to the game's own code: it is on only while ML's layer ships `missinglink/mod.json` (`tools/unbound_layer.py`, `ML_MARKER`). ML's rewrite then takes ActorDB's En_M_Thunder entry, the spawner runs on SoH's `OnPlayerUpdate` hook, and SoH's Remove Spin Attack Darkness steps aside, as the fork has it. The Gerudo Valley event's take-back finds `ml/gerudo_valley_event` by name. By log and screenshots on macOS (Metal), with a temporary hook firing beams at ammo 40: beams fly and draw, one fells the tree bridge, and without ML mounted nothing changes. Not checked in play: firing by spin charge or combo swing, the charge sparkle. The sword-glow actor mode (0xBEEF) removes itself, as in the fork, which never ported its drawing |
| 99 | The Soul-Charged Kokiri Sword (model, glow, B icon) | **Done** (2026-09-28, Unbound `0355d0941`): on while ML is mounted, as row 17: the fork's `MlSoulSword_Update` runs after the Blade Beam spawner on SoH's `OnPlayerUpdate` hook, patching ML's display lists (ML's layer already replaces vanilla's `object_link_child` ones, so Link draws ML's Kokiri Sword), and the B icon is gated in `z_parameter.c`. By screenshots on macOS (Metal) with a temporary hook setting ammo 40: ML's soul-sword B icon and the soul sword's hilt on Link's back; without ML mounted, nothing changes. The glow is on the blade, not seen sheathed |

## D. SoH-fork only: not needed on Unbound, or the owner decides

| What | Rows | Note |
|---|---|---|
| Menu, editor and preset curation (removed options, hidden cosmetics, retired CVars, Randomizer/Network tabs) | 106, 116–118, 120, 130–136 | Stock SoH menus on Unbound unless a generic "mod profile" feature is wanted; our rule of never mirroring stock settings still holds |
| Packaging, app name, first-launch builder | 140–144 | Revisit when ML on Unbound is playable: the builder would add `tools/unbound_layer.py` |
| Debug aids | 104 (trace log), 113 (console cutscene index) | Dev-only; 113 could be offered to Unbound |
| Removed rows | 102 | Nothing to carry |

## E. Bugfixes to offer Unbound (and SoH)

| Id | Rows | Fix |
|---|---|---|
| B1 | 23 | NULL-check `cameraDataList` in `z_bgcheck.c`. **Carried on our Unbound branch** (2026-09-28, `84ff78cae`, with row 20, whose bridge needs it; crash without, none with) |
| B2 | 19 | Song-cone width for widescreen (`Oceff_Wipe`-`Wipe4`) |
| B3 | 44 | libultraship `GfxDpLoadBlock` `word_size_shift` sign |
| B4 | 64 | Skip the place-name card when the scene has none |
| B6 | 124 | libultraship: texture-edge (`CVG_X_ALPHA`) modes stay alpha-tested when the blender reads memory |
| B7 | 126 | Controllable Bombchus: release control on rail walkways (general form: on a "no bombchu" surface) |
| B8 | — | ZAPD: two bytes per skeleton limb and one per soundfont drum entry are written uninitialized, so `oot.o2r` differs between platforms (found 2026-09-27) |

Pipeline rows (128 grass floors, 129 sky seams, 139 split samples) are data changes already inside the mod archive
and need nothing from Unbound.

## Order of work (step 3)

1. `base`: **built** on our Unbound branch (SPEC §7.3, `unbound-docs/based-actors.md`), tested on macOS and Linux; applied to row 38. U9 (vanilla actor overrides) built next and applied to rows 8, 41, 42, 82, 94 and 122.
2. U1–U4 (entrances, cutscene destinations, intro cutscenes, new save): **built** (Unbound `551f68d7c`, fixture `unbound-docs/examples/story-wiring`) and applied by `tools/unbound_ml_world.py`; ML's opening, Skip Intro and credits tour run their chains on macOS by log. The layer also drops the junk after ML's pond exit list (0x2E00, no entrance in ML either), which Unbound rightly refused. Row 28 (where a loaded save and the game-over Continue resume) followed on 2026-09-28 as `saveReturn` and `continueAt`.
3. U7 and U10 (pause menu, minimap, labels), U5, U6, U8.
4. Behaviour beyond `base`: the owner's decision above.
