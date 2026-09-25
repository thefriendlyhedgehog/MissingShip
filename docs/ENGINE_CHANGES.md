# MissingShip engine-change ledger

**Goal:** the entire game working on the SoH engine now; long-run, a **drop-in `.o2r` for stock SoH**.

A stock-SoH mod can only carry data. SoH 9.2.3 has no runtime mod-code path (no plugin loader, no
scripting), and HarbourMasters' own code-modding guide describes only fork-and-compile. So every entry
below is a **drop-in blocker** until it is eliminated, turned into data, or upstreamed as a general
feature that data can drive.

Order of preference for any new fix: **data-side** → **generic data-driven SoH feature (upstream)** →
**upstreamable bugfix** → ML-specific C (last resort, isolated, commented `MissingShip:`, logged here).

The code for every row lives in the private engine fork `MissingShip-Shipwright` (branch `missingship`, on SoH tag
9.2.3); `docs/soh-missingship.patch` is generated from it by `tools/genpatch.py`.

What each row needs to leave the fork (bugfix, data-driven feature, data-side retry, or mod code) is in
[`DROPIN_PROPOSALS.md`](DROPIN_PROPOSALS.md) (MS-070).

## Already drop-in compatible (data, shipped in `MissingLink_Scenes.o2r`)

Scenes and rooms (textures, geometry, exit remaps, cutscene destinations) · audio (574, incl. the
MEDIUM_CART → RAM sample fix) · text (269 messages) · credits (11 of 48, MS-024) · title cards · minimaps and dungeon maps · skyboxes (day, overcast sunset; MS-025) · objects
(`object_ml_fishpond`, `object_ml_villager`, `object_ml_lift`, five `object_ml_gi_*` pickup models) · ML's 9 entrance
intro-cutscene scripts · **pickup models shipped under vanilla paths** (Boomerang → Magic Hourglass, Gerudo Card →
Deku Pass, Stone of Agony → Birth Certificate; `inject_objects.py` ALIASES) · **168 redrawn pause/HUD textures**
(item icons, item names, area names, HUD; `inject_ui_textures.py`, only the ones that differ from stock) · **blank
title cards** for the 8 scenes ML gives none (else stock SoH shows the vanilla slot's name, e.g. "Sacred Forest Meadow"
over ML's Saria's house) · raw gameplay_keep texture pointers in ML display lists rewritten as resource hashes. · **textures ML redrew inside otherwise-vanilla objects** (MS-020, `inject_obj_textures.py`: the Triforce push blocks, chest, bomb-flower leaves, boss door, haka door) and the chest collision's surface material (wood → stone), shipped under vanilla paths. They are compared against the MQ debug ROM, so debug-vs-retail differences never ship. · **ML's one-word object edits** reproduced by patching stock resources (MS-021, `inject_obj_patches.py`, each checked against the ML ROM): Beamos body and top-eyelid display lists end immediately (ML draws neither); Saria's right-hand limb uses the hand-holding-ocarina display list. Link's sword-turn recoveries (MS-026): `gPlayerAnim_link_fighter_turn_kiruL_end` / `_kiruR_end` run 13 / 14 frames instead of 16, and play ML's re-animated frames (`misc/link_animetion/gPlayerAnimData_114420` / `_114FB0`).

Every resource in the archive is marked **custom** in its header (MS-140, `ms_cycle.py` `mark_custom`), as other SoH mods mark theirs. SoH's by-index display-list patches (cosmetics colours, age-dependent equipment, Temple of Time medallions, ...) skip custom resources. On ours they would overwrite whatever ML command sits at vanilla's index.

## Engine changes

| # | Change | Why it exists | Drop-in path |
|---|--------|---------------|--------------|
| 1 | `entrance_table.h`: +32 entrances | ML destinations with no vanilla (scene, spawn) equivalent. Until 2026-09-23 the pipeline read SOHCombo's unpatched table and never used them; now 8 exits do | **Upstream:** let mods append entrances from a resource |
| 2 | `z_demo.c`: `CS_DESTINATION` default case + `CS_BLACK_FILL` escape | ML's opening cutscene ends on a destination vanilla doesn't know | **Upstream (generic):** "destination encodes a raw entrance" is data-agnostic |
| 3 | `z_demo.c`: ML's entrance intro-cutscene table | vanilla's 34 intros fired inside ML scenes | **Upstream:** data-driven table (mods override from a resource); the scripts are already data |
| 4 | `z_sram.c`: stub-entrance guard (replay prologue) | a continue can land on ML's geometry-less intro stub | **Try data-side:** give the stub scene a safe default layer; otherwise upstream as a hook |
| 5 | `z_map_exp.c`: minimap hidden by default | user preference | **Remove for drop-in** — it's a user setting |
| 6 | `z_en_hy.c`: Birth Certificate offer | ML's only edit to En_Hy: `tools/fnpair.py` (MS-033) shows every other function unchanged, so the port is complete | **Needs mod code** (actor logic) |
| 7 | `z_en_niw_lady.c`: certificate hand-in, Heart Piece | ML's only edits to Anju (one function; the longer .data is alignment padding), so the port is complete (MS-033) | **Needs mod code** |
| 8 | `z_bg_treemouth.c`: inert | ML gutted the Deku Tree mouth | **Needs mod code** — or upstream a "disable actor" data hook |
| 9 | `actor_table.h` + `ActorDB.cpp` + `actorViewer.cpp`: slots 3, 6 | register ML's new actors | **Needs mod code** |
| 10 | `ovl_Ml_Fishpond`, `ovl_Ml_Villager` | ML's new actors | **Needs mod code** |
| 11 | libultraship `gfx_metal.cpp`, `gfx_opengl.cpp`, `gfx_direct3d11.cpp`: zero-dimension texture guard (OpenGL and DX11 added in MS-083, the same code as upstream) | engine crash any mod could trigger | **Already upstream** (Kenix3/libultraship#1040, adaad35d); drop on rebase |
| 12 | `ovl_Ml_Lift`, `ovl_Ml_LiftBlock` + actor table slots 0x12C, 0xE3 | ML rewrote Obj_Lift (switch lift) and Bg_Mori_Hashira4 (quest-item barrier) wholesale; vanilla files untouched, slots repointed | **Needs mod code** |
| 13 | `z_sram.c` `Sram_InitNewSave`: ML's starting loadout (Ocarina of Time, Kokiri Sword, Hylian Shield, Goron's Bracelet, Adult's Wallet, Epona's + Saria's Songs) | SoH builds new files in C, not from ML's new-save templates | **Upstream:** mods provide a new-save template resource |
| 14 | `z_en_md.c` (Mido): Kokiri Forest uses the Mido's-House lines; Init always takes the stand-at-home branch; Update no longer calls `actionFunc`; 0x1046 → 0x1044 once slingshot ammo ≠ 0 | ML patched vanilla En_Md in 3 places (GetText branch, Init `beq $a0,$a0`, Update `jalr` → `j 0x80602200` into its custom code). Vanilla BlockPath slid him around / SetMovedPos teleported him along ML's unrelated path 0 | **Needs mod code** |
| 15 | `z_sram.c` `Sram_InitNewSave`: starting Ocarina button is a setting (MS-019): D-Down by default (needs "Equip Items on Dpad", else C-Left), or C-Left (ML's template), both, or none | QoL: keep the C buttons free, as 2Ship does | Harmless to drop; a user can equip it themselves |
| 16 | `z_draw.c` GetItem draw table: red potion → Deku Nut Milk (`DrawOpa0Xlu1`, ML object 0x11), blue potion → red colours, Bullet Bag 40 → Soul-Charged Kokiri Sword | ML changed these draw-table rows (code data diff); the other three changed pickups are pure data aliases | **Try data-side:** `GID_BULLET_BAG_50` shares the bag DL and potions share the pot DL, so a name override is not enough — upstream a data-driven draw table |
| 17 | `ovl_Ml_MThunder` (slot 0x57 repointed) + `MlMThunder_PlayerUpdate` called from `Player_Update` (ML's sword take-back on its entrance 0x400: MS-148 dropped it as unreachable, but ML's patched cutscene destination 61 does reach it, on Deku Road's cutscene layer after the ranch; since MS-151 it fires only on `ENTR_MISSINGSHIP_GERUDO_VALLEY_EVENT`, never on ordinary ranch visits) | ML rewrote En_M_Thunder into the **Blade Beam** (params 0xDEAD, vanilla's unused `gUnusedBeamBladeDL`), a sword-glow prop (0xBEEF, inert until ML's `object_link_child` is ported) and a spin-charge sparkle; the beam spawner lives in ML's custom code at 0x80602DE0 | **Needs mod code** |
| 18 | `RemoveSpinAttackDarkness.cpp`: hook made a no-op | it swaps the update of actor 0x57 and would run vanilla logic on ML's struct | Goes away with row 17 |
| 19 | `Oceff_Wipe`/`Oceff_Wipe2`/`Oceff_Wipe3`/`Oceff_Wipe4` draw: stretch the song-effect cone by `aspect / (4/3)` | on widescreen the 4:3-sized cone's rim shows as a circle (reported playing Saria's Song); upstream `develop` is identical to 9.2.3, so unfixed there | **Upstream as a bugfix** — nothing ML-specific |
| 20 | `ovl_Ml_TreeBridge` (slot 0x005, was UNSET) | ML's tree that a Blade Beam burns and topples into a bridge (switch = params & 0xFF, camera = params >> 8); model appended to `object_wood02`, collision inline in ML's overlay → generated C data | **Needs mod code** |
| 21 | `ovl_Ml_SongBlock` (slot 0x1D1 repointed from Obj_Timeblock) | ML rewrote the Time Block into the block sealing Saria's house: spawns a vanilla En_Okarina_Tag (0x1C0C) and sinks when switch 0x0C is set | **Needs mod code** |
| 22 | `z_en_dekunuts.c` Update: room-0 scrubs clamped within 30 units of home | ML hooks the function's return (`j 0x806008E0`); keeps Deep Woods' water-edge scrubs from being knocked into the water, where they never die | **Needs mod code** |
| 23 | `z_bgcheck.c`: NULL checks on `cameraDataList` in `func_80041A4C` / `func_80041B24` / `func_80041C10` | SoH's `SEGMENTED_TO_VIRTUAL` is the identity, so the existing `gSegments[0]` guards never catch a NULL list; standing on ML's tree bridge (inline collision, no camera data) segfaulted in `Camera_Update` | **Upstream as a bugfix** |
| 24 | Pause menu (`z_kaleido_item.c`, `z_kaleido_collect.c`, `z_kaleido_equipment.c`, `z_kaleido_scope_PAL.c`): quest-status layout + ML's cursor redirects, equipment page layout + ML's vertical move (0x80606C90), any-age item/equipment tables, page/world-map colours and points, item grid X, Bombchu cap 20, quest page entry slot 24 | ML's kaleido overlay (dma 101) data + code + hooks 0x80600690 / 0x80606C90 | **Try data-side** for the tables (upstream: data-driven pause layout); the cursor logic needs mod code |
| 25 | `Door_Ana`: ML's grotto destinations, no grotto-return respawn, hole drawn from ML's gameplay_keep; `entrance_table.h` +1 (`ENTR_MISSINGSHIP_DEKU_TREE_2`) | ML replaced the grotto table (7 of its own entrances) | **Upstream:** data-driven grotto table |
| 26 | `ovl_Ml_FogWall` (slot 0x174), `ovl_Ml_HintNuts` (slot 0x192), `ovl_Ml_EventSpawner` (slot 0x121; completed in row 54) | ML rewrites: Deep Woods fog walls + looping corridor, talking scrubs (object 0x005), event spawner (En_Fz) | **Needs mod code** |
| 27 | `En_Firefly`, `En_Dekubaba` (no hostile flag, Baba always drops nuts), `En_Dns` (prices, bugs, Deku-air bottle, garden gate) | ML in-place patches | **Needs mod code** |
| 28 | `ml_entrance_map.c` (generated by `pipeline/gen_entrance_map.py`) + `z_sram.c` dungeon return + game-over continue translate ML entrance numbers; `Door_Ana` table corrected (starts at +0x5FC, two vanilla entries first); `entrance_table.h` +1 (`ENTR_MISSINGSHIP_6B_3`, ML entrances 0x0D8-0x0DB) | 370 of ML's 1556 entrance numbers mean a different place than vanilla's; engine code using literal numbers must go through ML's table | **Upstream:** mod-provided entrance remap resource |
| 29 | `ovl_Ml_Prologue` (slot 0x01F) | ML's illustrated prologue: 5 CI4 pictures from ML-only object 0x04, drawn as 24 strips | **Needs mod code** |
| 30 | `ovl_Ml_BeamHeart` (slot 0x1C repointed from En_Reeba) | ML dma 166: invisible spawner; a Blade Beam within 45 units spawns a Piece of Heart (En_Item00 0x106, collectible flag 1) that rises to y 176; gone once the flag is set | **Needs mod code** |
| 31 | `ovl_Ml_SkullKid` (slot 0x115 repointed from En_Skj) | ML dma 250, Deku Road: params 1 = the Skull Kid with two bomb-flower targets (sets switch 7 when both are down); params 2 = invisible speaker, text 0x1029 / 0x102A, then a Piece of Heart | **Needs mod code** |
| 32 | `Ml_DnsShop_InitVars` in `ovl_En_Dns` (slot 0x119 repointed from Demo_Shd) | ML dma 253 is a copy of En_Dns: the grotto scrub shop. Own sale table (Deku Shield 50, Green Potion, nut upgrade 40); the Piece of Heart row becomes a Recovery Heart once switch 9 is set; En_Dns's garden/bottle special cases skipped (`DNS_IS_ML_SHOP`) | **Needs mod code** (the table alone could be data if SoH exposed shop tables) |
| 33 | `ovl_Ml_HoleScrub` (slot 0x18C repointed from En_Weiyer) | ML dma 298, recompiled En_Hintnuts: the hole-hopping scrub puzzle (hops between three En_Dekunuts flowers, caught when push blocks cover two; switch 15), sinks other enemies to y -960 | **Needs mod code** |
| 34 | `ovl_Ml_Malon` (slot 0xE7 repointed from En_Ma1) + `object_ml_ma1` (ML object 0xE0, dma 55) | ML dma 236: young Malon rewritten with a new model. Race host (text 0x204A, choice warps to ML entrance 0x138), finish line (params 0xFFFF: switch 0x11 of scene 0x5A, timer stop, warp 0x13C), post-race greeting, releases ML's cucco after a win. Entrance numbers via `MlEntrance_ToSoh` | **Needs mod code**; the object is data |
| 35 | `Ml_Cucco_InitVars` in `ovl_En_Niw` (slot 0x0D repointed from En_Poh) + `object_ml_cucco` (ML object 0x09, dma 354) | ML dma 154 is a copy of En_Niw on a recoloured cucco object: health 0x77 at Init, all CHICKEN_CRY_M / CRY_A calls removed (`NIW_IS_ML`) | **Needs mod code**; the object is data |
| 36 | `z_en_weather_tag.c`: all five story-flag `Actor_Kill`s removed | ML's En_Weather_Tag never removes itself, so its weather always runs | **Needs mod code** |
| 37 | `z_en_wonder_talk.c`: Init keeps only the switch (params & 0x3F) and never kills; text = params + 0x300 with no follow-up; flags OR 0x31 around the action | ML dma 276: readable notes (Kokiri house, kakariko3) | **Needs mod code** |
| 38 | `z_bg_hidan_kowarerukabe.c`: object OBJECT_RU1, `gMlCrackWallDL` / `gMlCrackWallCol` for all three types; `object_ml_crackwall` (ML object 0xA3, dma 33) | ML's breakable wall uses one model and collision of its own | **Try data-side:** ship ML's DL/collision under the vanilla `gFireTemple*` paths (like the pickup ALIASES) and keep the vanilla object |
| 39 | `z_obj_oshihiki.c`: Deku Tree row (240,196,111), Shadow Temple row (154,240,111) | ML recolours the push-block rows its scenes use (overlay data +0x40, +0x88) | **Needs mod code** (static table) — or upstream a data-driven colour table |
| 40 | `z_en_cow.c`: limb 5 no longer hidden | ML nops the store that hid it | **Needs mod code** |
| 41 | `z_en_st.c`, `z_en_sw.c`: `ACTOR_FLAG_HOSTILE` dropped | ML's Skulltulas / Skullwalltulas don't count as enemies (flags 0x35→0x31, 0x15→0x11) | **Needs mod code** |
| 42 | `z_obj_bombiwa.c`: `ACTOR_FLAG_UPDATE_CULLING_DISABLED` | ML's InitVars flags 0 → 0x10 | **Needs mod code** |
| 43 | `soh/assets/ml_assets.h` | resource paths of every ML object the ported actors draw (all shipped in `MissingLink_Scenes.o2r`) | Goes away with the actors that use it |
| 44 | libultraship `interpreter.cpp` `GfxDpLoadBlock`: `word_size_shift` made `int32_t` (MS-016) | as `uint32_t`, the -1 for 4-bit loads became 0xFFFFFFFF and the size came out 0 bytes (a zero-height texture). ML doesn't hit it; any mod that block-loads with SETTIMG siz=4b would | **Upstream as a bugfix**: Kenix3/libultraship `main` still has it |
| 45 | `SohMenuEnhancements.cpp`: **Enhancements > MissingShip** page; `code/ml_settings.h` (option values) (MS-019) | every MissingShip-specific option is a normal SoH menu option; also shows Equip Items on Dpad and Dpad Ocarina Playback (stock CVars) | Goes with the options it hosts; the page itself is fork-only UI |
| 46 | `z_player.c`: no conversation with text 0xE0 ("talk to Saria?"): `ML_SKIP_TALK` early-out in `Player_StartTalking` with each caller's ML fall-through, and no `Player_StartTalking` in the post-song Navi branch (MS-017) | ML hook player +0x20F38 → 0x80600D90 (returns to ra + 8) and player +0x1C2B0 (`jal` replaced). Without it, Saria's Song at the song-block door asked "talk to Saria?" before the door opened | **Needs mod code** (player logic) |
| 47 | `z_message_PAL.c` `Message_ShouldAdvance(Silent)`: "B Cancels Choices" (MS-018): on a choice prompt B selects the no/cancel option (the last one, or `sMlChoiceBException`: 14 audited exceptions, some ignore B) and holding B never confirms; option on the MissingShip page, default on | QoL requested in playtest: B used to confirm the highlighted option (usually Yes) | **Upstream as a SoH enhancement** after the project (the exception table is per game text) |
| 48 | `z_map_exp.c` `Map_InitRoomData`: dungeon minimap index = room + 190 (the Shadow Temple block) for every dungeon, falling back to vanilla's index past the table; `z_map_data.c`: ML's values for the tables it changed (room palettes, max palette count, palette rooms, Shadow Temple compass offsets / compass info / floor Y / floor IDs, and every room-switch slot 255) (MS-022) | ML code 0x80080D20 and gMapData tables at code +0x106CC0..+0x108934; textures via inject_minimaps.py | **Needs mod code** for the index; the tables are data that upstream keeps in C (upstream candidate: data-driven map tables) |
| 49 | `z_map_mark.c` / `z_lmap_mark.c`: always load the MQ marker tables; `z_map_mark_data.c`: ML's 21 Shadow Temple minimap marker entries; `z_lmap_mark_data.c`: ML's pause-map markers for Shadow Temple floors 1-3 and Bottom of the Well floors 0-2 (MS-023) | ML's map_mark_data (dma 103, 58 words at +0x5514..+0x5F78) and kaleido marker table (dma 101 +0x17AA0, entries 27-32) are MQ debug's with edits; SoH picked the Vanilla tables because we run non-MQ | **Needs mod code** (C tables); upstream candidate: data-driven marker tables |
| 50 | `ml_entrance_flags.c/.h` (generated by `pipeline/gen_entrance_flags.py`), called from `Play_Init` (`z_play.c`): ML's music-continuity / title-card / transition flags on the 184 SoH entrance slots that differ, of the 275 that ML's reachable entrances land on (MS-027) | ML's exits are remapped onto SoH entrances, so SoH applied its own flags (e.g. music kept playing into Sacred Forest Meadow, special fades instead of ML's plain ones) | **Upstream idea:** let a mod override entrance flags from a resource; until then mod code |
| 51 | `z_en_elf.c`: `EnElf_Draw` returns at once for Navi (params 0) after parking her at y -524288; `EnElf_Update` parks every other fairy at y -2048 once slingshot ammo (the Soul-Charged sword marker) is non-zero (MS-030) | ML's En_Elf is a recompile of vanilla (its 4018 changed words are delay-slot and address shifts) with exactly these two edits: inline at +0x3C1C / +0x42C8, and hook 0x80602000 at +0x3AD8. ML has no visible Navi | **Needs mod code** (actor logic) |
| 52 | `z_en_nutsball.c`: a Deku nut reflects only off the Hylian Shield, at any age (vanilla: the Deku Shield as a child); and it shatters, like hitting a wall, whenever Link's current animation is gameplay_keep's `gPlayerAnim_link_swimer_swim_15step_up` (compared by path, because SoH's animation symbols are strings) (MS-031) | ML's En_Nutsball is a recompile of vanilla's with exactly these two edits (+0x208 shield test, +0x300 the animation check on player +0x1BC == 0x040032E8); its data matches apart from one shifted pointer |
| 53 | `z_en_ko.c` (Kokiri): `EnKo_CanSpawn` treats scenes 0x2E–0x55 as Kokiri Forest; `func_80A98DB4` fades by distance in every scene; `EnKo_MlAfterSword`, called at the end of Update, swaps 11 text IDs to the after-the-sword lines once slingshot ammo ≠ 0 and registers `gMlKw1MouthFrownTex` as a blended replacement for `gKw1MouthTex` (the Kokiri frown), unregistering it on a save without the sword (MS-032) | ML's En_Ko is a recompile of vanilla with two branch edits (+0x1BB4 `beql`→`b`, +0x2028 `beq`→`b`; both only matter outside Kokiri Forest, where ML places no Kokiri) and its Update epilogue replaced by `j 0x80602200`. That hook scans 24 pairs at 0x806021A0 (only the first 11 are a table; the rest is its own code, and the one that could match, 0x1040→0x0003, is never read), then, if the actor's object holds 0x102 at +0x178C (object_kw1's mouth row 12), copies 0xE0 bytes from 0x80602B00 over it. The pipeline ships that patched texture (`inject_obj_patches.py`, checked against the hook) because SoH can't write object memory |
| 54 | `ovl_Ml_EventSpawner` completed: young Epona on `object_horse_link_child` (now the actor's object, as in ML). Mode 0/8+ stands and blinks; mode 1 (event entrances only) runs the horse chase: manual cutscene with camera setting 4, horse-race music, then `Interface_SetTimer(40)` while she gallops path 0 with ML's per-stretch gait table; once she stops, the timer reaching 0 sends Link to ML entrance 0x13C. Mode 3 walks forward with Link on her back. Mode 4 now spawns Ml_Malon (MS-033) (MS-144: eye texture set at Init, since the first draw comes before the first Update) | ML dma 259, fully disassembled. Deliberate differences, each commented in the source: (1) ML's collider init is laid out for `Collider_SetCylinderType1` but passed to `Collider_SetCylinder`, which makes it OC-less with a JntSph shape and a negative element count, so it never touches anything; SoH keeps its size for bgcheck and never registers it; (2) ML's dust-ring count is an uninitialised stack halfword; a random 0-8 stands in; (3) ML's once-per-overlay-load chase flag is reset when the last instance is destroyed. Mode 3 is reached after the Shadow boss: blue warp -> pond intro -> ML entrance 0x420, where Ml_Fishpond spawns it (row 84) |
| 55 | `z_bg_spot16_bombstone.c`: both `OnePointCutscene_Init(play, 4180, ...)` camera cuts removed (bomb near the boulder, boulder hit) (MS-096) | ML's recompile drops exactly those two calls (`tools/fnpair.py`) |
| 56 | `z_en_dodojr.c` (Baby Dodongo): in func_809F6510 each dust puff's height is the actor's own `world.pos.y`, not `dustPos.y` (MS-097) | The only change besides the stripped divide-by-zero traps (compiler) |
| 57 | `z_en_hs.c`: killed at the end of Update until switch flag 7 of scene 3 (ML's Bmori1 event, as Ml_Villager) (MS-098) | ML's only edit |
| 58 | `z_en_rd.c` (ReDead/Gibdo): killed in Init once switch flag 7 of scene 3 is set; the first scream keeps its 40-frame freeze but loses the lock-on call and rumble; the facing scream loses freeze, rumble and lock-on; no item drop on death; the body never sinks or goes away (fade reset to 0xFF each loop, shrink never stored, `Actor_Kill` unreachable) (MS-099) | ML's recompile, per function (`tools/fnpair.py`) |
| 59 | `z_en_daiku.c` (carpenters): freed carpenters are never removed; the Thieves' Hideout jailed setup is gone (all tent-idle); UpdateTalking/UpdateText test Kakariko (0x52) instead of the Carpenters' Tent (0x39); killed at the end of Update until switch flag 7 of scene 3, otherwise params 0 → 4 (ML's music call after it is nop'd) (MS-100) | ML's recompile. EscapeSuccess's EnGe3 spawn is nop'd in ML but can't be reached without the jailed setup, so vanilla's code stays |
| 60 | `z_bg_spot02_objects.c` + `object_ml_spot02` (ML object 0xA1, dma 32): Init keeps params whole and always runs the type 0-2 setup, with ML's collision for type 2+; a non-zero high byte skips the collision; switch flag `params & 0x1F` sinks (high byte 0, flag clear) or removes (high byte ≠ 0, flag set) it; Draw draws ML's model for high byte 0 only (MS-101) (MS-149, owner request: while the switch is clear the object watches it, and when it is set it rises back 200 units or is removed live; ML applies the change only on the next load) | ML hand-patched Init and replaced Draw. The scrolling-texture branch for other high bytes needs `health & 0x7F` ≠ 0, which Init never sets, so it is left out |
| 61 | `z_obj_makekinsuta.c/.h`: bugs on the soil spawn a Piece of Heart (En_Item00 0x0C06) that rises 40 units and settles back, not a Gold Skulltula; two fields added (MS-102) | ML rewrite (0x1A0 bytes). SoH stops moving the heart once it has been collected (ML writes to stale memory) |
| 62 | `ovl_Ml_TalkSpot` (slot 0x06A, was Bg_Toki_Hikari): invisible talk spot, text 0x6026, radius 150 (MS-103) | ML rewrite, dma 203 |
| 63 | `ovl_Ml_BgmTrigger` (slot 0x093, was Bg_Po_Event): params 0 silence until the Bmori1 event, 1 Kakariko (Adult) after it, 2 Inside the Deku Tree, 3 Link's falling scream once (MS-104) | ML rewrite, dma 213; reads `gAudioContext.seqPlayers[0].seqId` like ML |
| 64 | `z_actor.c` TitleCard_InitPlaceName: the place-name card shows only for the scene slots ML gives a title (0x03, 0x07, 0x2B, 0x32, 0x52, 0x55, 0x5A, 0x5B, 0x5D, 0x63) (MS-105) | ML, like vanilla, skips the card when the scene's titleFile is empty. SoH dropped that check, so ML's entrance title flags (MS-027) drew a blank ribbon, e.g. in Deep Woods grottos |
| 65 | `z_oceff_storm.c`: the Song of Storms no longer spawns En_Okarina_Effect, so it doesn't make it rain (MS-106) | ML deletes En_Okarina_Effect (its actor-table entry is all zeros) |
| 66 | `z_en_ossan.c`: Init wrapped: after ML's Bmori1 event the shop music starts, before it the shopkeeper is removed; the bombchu shop's Dodongo's Cavern kill is gone; Link stays drawn when shopping starts; the cursor doesn't pulse (blue fixed at 255, green left at its initial value) (MS-035) | ML hand-patched Init, State_Idle and the cursor colour update; the other 94 functions match |
| 67 | `z_en_girla.c`: Bombchus cost 40 (10) and 80 (20) Rupees instead of 100 and 180, all eight bombchu entries (MS-035) | ML's only change to En_GirlA (data only) |
| 68 | `ovl_Ml_SlideBlock` (slot 0x0DC, was Boss_Tw) + `object_ml_slideblock` (ML object 0xD3, dma 51): switch-driven sliding block, 8 coloured models and 2 collisions chosen by the params high byte (MS-107) | ML hand-written rewrite, dma 234; placed 26 times in HAKAdan |
| 69 | `ovl_Ml_RoomClearSwitch` (slot 0x01A, unset in vanilla, id `ACTOR_ML_ROOMCLEARSWITCH`): holds switch 0x25 while the room has enemies, then clears it (MS-108). `ActorDB.cpp` gets its description and `actorViewer.cpp` drops it from the unset list | ML custom actor, dma 164; HAKAdan room 20. Without a description ActorDB crashes at startup (NULL name string) |
| 70 | `ovl_Ml_ElevatorSpawner` (slot 0x040, was Bg_Hidan_Dalm): spawns the tall Ml_Elevator once (MS-109) | ML rewrite, dma 184 |
| 71 | `ovl_Ml_Elevator` (slot 0x17B, was En_Kakasi) + `object_ml_elevator` (ML object 0x154, dma 380): two-height elevator, recalled when Link is below, camera setting 9 on the tall one (MS-110) | ML rewrite, dma 290. Its Hourglass-rewind recall reads 0x80609FF4, stubbed as `gMlRewindStep` in `code/ml_state.c` until MS-053 |
| 72 | `ovl_Ml_HakaLift` (slot 0x087, was Bg_Mori_Elevator) + `object_ml_hakalift` (ML object 0x72, dma 22): 675-unit lift (MS-111) | ML rewrite, dma 206 |
| 73 | `z_en_floormas.c`: giant variant (params 0x20: scale 0.016, collider ×3, mini-boss music) that splits into three full-size Floormasters (MS-112) | ML recompile with an Init branch, a Die branch and a SetupSplit copy; the rest matches |
| 74 | `z_en_ssh.c`: each cursed man says text 0x22 + params, then gives a small key once (chest flag params) (MS-113) | ML recompile; face-reaction and token texts removed |
| 75 | `z_bg_mizu_shutter.c`: no camera cutscene for steep shutters, 6-frame wait instead of 30 (MS-114) | ML recompile, one function |
| 76 | `code/ml_state.c`: `gMlRewindStep` (ML 0x80609FF4, non-zero while the Magic Hourglass rewinds); set by the Magic Hourglass since MS-053 (row 98) | Read by ML's En_Kakasi and Boss_Va |
| 77 | `z_player_lib.c`: a child's right hand holding the Hylian Shield draws ML's `gMlChildRightFistAndHylianShieldDL` (`object_ml_link_child`, ML object 0x15 dma 6, three DLs) (MS-115) | ML points every child entry of the right-hand shield table (0x80125CE8) at its own hands; only the Hylian one is ported until MS-050 |
| 78 | `ovl_Ml_BossVa` (slot 0x0BA, was Boss_Va) + `object_ml_bv` (ML object 0x8C, dma 26): ML's Shadow Temple boss, two phases, 51-sphere collider, 4 fairies, 3 riding Beamos, death flames, blue warp (MS-121). Scene cutscenes 0x168/0xACF0/0xB050 exported via `ml_entrance.ACTOR_CS` | ML hand-written rewrite, dma 225, fully disassembled. Deliberate differences, commented in the source: (1) ML calls `Math_Atan2S` with doubles and so always gets atan2(0, 0); SoH passes 0; (2) ML writes through fairy/Beamos/flame pointers after those actors may be gone; SoH checks each is still in an actor list |
| 79 | `ovl_Ml_BossVaShot` (slot 0x017, unset in vanilla, `ACTOR_ML_BOSSVA_SHOT`): the boss's homing projectile (MS-122). ActorDB/actorViewer updated | ML custom actor, dma 109 |
| 80 | `z_en_elf.c`: `Ml_Elf_InitVars` (slot 0x001, unset in vanilla, `ACTOR_ML_ELF`): En_Elf without the Kokiri fairy's steering; ML's own En_Elf hooks (MS-030's two blocks) now apply to `ACTOR_EN_ELF` only (MS-123) | ML copy of vanilla En_Elf, dma 147, one call nopped |
| 81 | `z_en_light.c`: `Ml_Light_InitVars` (slot 0x022, unset in vanilla, `ACTOR_ML_LIGHT`): no light info set (MS-124) | ML copy of vanilla En_Light, dma 172, two calls removed |
| 82 | `z_en_vm.c`: Beamos never Z-targetable (MS-125) | ML's only change: InitVars flags 0x11 -> 0x10 |
| 83 | `z_door_warp1.c`: first Forest/Water/Spirit/Shadow blue warp and the child's repeat Shadow warp go to ML entrance 0x41C (Fishing Pond) (MS-126) | ML recompile, five entrance literals (0x6B x4, 0x568) |
| 84 | `z_demo.c` destination case 56 -> `MlEntrance_ToSoh(0x420)` with cutscene 0xFFF0; `entrance_table.h` +8 (`ENTR_MISSINGSHIP_FISHING_POND_EVENT`, 0x636, an 8-entry pond group so base + layer 4 is still the pond); `gen_entrance_map.py` maps ML 0x420-0x427 there; `ml_entrance.ML_PATCHED_CODES` leaves code 56 to the engine; `Ml_Fishpond` compares mapped entrances and spawns its Ml_Elf (MS-126) | ML patched case 56 in code (0x80066050: Kakariko gate 0xFFF4 -> 0x420 0xFFF0), used only by the pond intro. The pipeline had sent it to scene 0x6B spawn 3 via vanilla's case, and both pond entrances shared SoH 0x45F |
| 85 | `z_obj_syokudai.c`: `Ml_Torch_InitVars` (slot 0x027, was Boss_Dodongo; `actor_table.h`): ML's torch copy, Z-targetable, its own lit count; the switch never relights it, any lit torch sets the switch, a partly lit group burns 50 x count - 640 frames, going out clears the switch (MS-131) | ML copy of vanilla Obj_Syokudai, dma 175, four edits, each commented in the source |
| 86 | `ovl_Ml_Pillar` (slot 0x098, was En_Du) + `object_ml_du` (ML object 0x9D, dma 30): Bmori1's switch-raised pillars, one with a short manual cutscene (MS-132) | ML hand-written rewrite, dma 217, fully disassembled |
| 87 | `ovl_Ml_Thief` (slot 0x0C3, was En_Nb) + `object_ml_nb` (ML object 0xB3, dma 38): Bmori1's three thieves; params 1 spawns and later removes two ReDeads; switch 7 plays scene cutscene 0x350 (`ml_entrance.ACTOR_CS`) and they run off (MS-133) (MS-144: eye texture set at Init, since the first draw comes before the first Update) | ML hand-written rewrite, dma 228, fully disassembled. Deliberate difference: ML kills its ReDead pointers blind; SoH checks each is still in the enemy list |
| 88 | `z_bg_jya_1flift.c` Update: ML's z-sliding platform (+-197 at 4/frame by switch, y fixed at 233) that carries Link (MS-134) | ML recompile, dma 299; the carry is ML custom code 0x80606C00 inlined |
| 89 | `z_en_peehat.c` Init: mini-boss music (MS-135) | ML's only change: one call added |
| 90 | `z_en_mag.c`: ML's title screen (`EnMag_DrawInnerMl`) on `object_ml_mag` (ML object 0x14A, dma 378: its own layout): ML's logo, credits bar and red-pulsing PRESS START image instead of subtitle/copyright/font text; letterbox + hidden HUD while the title cutscene runs; copyright fade step 10 (MS-136) | ML recompile, dma 286, fully disassembled; update otherwise identical to vanilla non-MQ |
| 91 | `z_obj_oshihiki.c`: `Ml_PushBlock_InitVars` (slot 0x17A, was Boss_Ganon2; `actor_table.h`) on `object_ml_ganon2` (ML object 0x153, dma 379): own collision + one DL; switch records a push to the wall (block starts 80 along -z when set); stuck on blue switches; carries note En_Wonder_Talk params 7 along z (MS-137) | ML copy of vanilla Obj_Oshihiki, dma 289, with hand-written patches (hardcoded play 0x80212020). Deliberate differences: ML's note search has no NULL check (SoH stops at the list end); ML's stray -80 writes to prevPos.z, a feet position and yawSin are left out |
| 92 | `ovl_Ml_Ending` (slot 0x012, was En_Dodongo) + `ml_ovl_ending` (the three IA8 text images from ML dma 159's own data): ML's ending card in `spot10` room 1's cutscene header, Saria fading out and "The End" / title fading in (MS-138) | ML hand-written rewrite, dma 159, fully disassembled |
| 93 | `z_en_bom_chu.c` Update: HAKAdan Bombchu rail (`EnBomChu_MlRail`) (MS-139) | ML's only change: `jal 0x80607A00` at the end of Update; that custom routine inlined |
| 94 | `z_en_karebaba.c` FLAGS without `ACTOR_FLAG_HOSTILE`; `z_obj_ice_poly.c` Idle no longer sets the ice's AT (MS-040) | ML's only changes: InitVars flags 5 -> 1; one `CollisionCheck_SetAT` nopped. Neither actor is placed or spawned in ML, so both are unreachable; ported for completeness |
| 95 | `z_player_lib.c`: the child entries of `sPlayerRightHandHookshotDLs` / `sPlayerLeftHandHammerDLs` are the closed right hand and the left fist; `ml_assets.h`'s child Hylian Shield hand moves to `object_link_child` (MS-050) | ML's `code` tables at 0x108D14-0x1092E4 point these entries at those hands. The model itself is data: `object_link_child` (dma 6) ships under the vanilla path and names, one LOD (the Far names are copies, `inject_obj_patches.py` LC_ALIASES) |
| 96 | `z_skelanime.c` LinkAnimation_Change calls `MlState_LinkAnimation`; `z_player.c` Update calls `MlState_JumpUpdate` (`ml_state.c`): ML's triple jump. A running jump plays run_jump, then kiruL_end, then kiruR_end (ML's replaced data) while each follows within 16 frames of landing. At the Fishing Pond cutscene entrance (ML 0x420) every Link animation is the horse idle (MS-051) | ML routes all 34 LinkAnimation_Change calls through 0x806023E0; the step is kept by the tail of 0x80602DE0 (hooked into Player_DrawImpl), ticked here at the end of Player_Update instead, which is the same point in the frame for Link |
| 97 | `z_player.c`: ML's other player-overlay edits (MS-052): no void-out damage (+0x6104); secret-hole falls keep the music, silent in scene 0, ML's stray write to respawn[DOWN].tempCollectFlags kept (+0x7100); no revival fairy (+0x74CC); Bunny Hood speed (+0xBD68 → 0x80606BA0, +0x17938 → 0x80606BC8); no Navi toggle on A (+0xA080); place-name card y 80 (+0x14E20); sequence mode always STILL (+0x167E0); LOD 0 always (+0x18404); talkActor NULL check in Player_Action_Talk (+0x19368) and no talkActor assignment in Player_StartTalking's TALK-flag branch (+0x20F90); room counter reset before a void-out (+0x1D728 → 0x80600480); exit falls keep the music (+0x1D764) | Word diff of player dma 102 vs debug 34. Not ported: Navi's spawn stays (SoH dereferences naviActor; she's hidden by row 51), the get-item object size assert (+0x8CC8, SoH loads by name), the debug noclip (+0x1DB54, an opt-in SoH cheat), the two GetItem object ids (+0x21470 nut milk, +0x21568 certificate: drawn by name already). The boomerang edits and 0x80600480's cucco check are the Hourglass (MS-053) |
| 98 | `code/ml_state.c` `MlState_PlayerFrame` / `MlState_PlayerFrameEnd` / `MlState_VoidOut`, called around Player_UpdateCommon and before the void-out: the room counter (now also filled at frame 17), ML's Magic Hourglass (the boomerang throw sets `gMlHourglassTrigger` with no aim hold and no En_Boom; with the counter at 169 Link rewinds through his 164-entry history with the Song of Time wipe) and its cucco keeper (after a void-out with a cucco around, ML's cucco respawns beside Link in no room) (MS-053) | ML 0x80600E00, 0x80601680, 0x80601000, 0x80600480 and player +0x378C / +0x38CC. Filled history entries read facing 0 (ML writes it as a word); kept. ML's cucco is found by id 0x19 in ML (its copy keeps En_Niw's id); SoH also checks Ml_Cucco's own id |
| 99 | `code/ml_state.c` `MlSoulSword_Update` (from Player_Update) + `object_link_child`'s appended soul sword (`gMlChildSoulSheathDL` 0x2D660, `gMlChildSoulSwordHiltDL` 0x2D9E8, `gMlChildSoulSwordBladeDL` 0x2DE80); `z_parameter.c` B icon: with slingshot ammo 40 (ML's Soul-Charged Kokiri Sword marker) the sheath, hilt and blade DLs are patched to call the soul sword, the blade's 8 prim colours pulse (0,255,150) ↔ (0,175,70) while powered (spin charge, odd combo swing at full health, normal2fighter_free) and are white otherwise, and a Kokiri Sword on B shows the Bullet Bag 40 icon ML redrew as the soul sword (MS-054) | ML 0x80602DE0 (hooked into Player_DrawImpl) writes object_link_child's segment every frame and DMAs the icon from icon_item_static into the B slot; SoH patches the shared DL resources instead (undone without the marker). ML reloads the icon only over the Kokiri Sword's (a pixel check), so SoH swaps only that one |
| 100 | `z_parameter.c`: ML's HUD look: black drop shadows under the B and C button backs (+2 px, alpha ≤ 125), the Start button (+1 px, pause states 6–7) and the rupee / small-key counts (+1 px; keys in the Shadow Temple slot only); Start button colour (223, 73, 73), SoH's colour-scheme default dropped (MS-060) | ML 0x80604E30 over Interface_DrawItemButtons' Graph_OpenDisps, 0x806053E0 over Interface_Draw's Gfx_SetupDL_39Overlay (both draw their shadows, then vanilla draws on top), and the prim colour at code 0x80089180 (120,120,120 in ML's GameCube debug base). Shadows follow SoH's HUD positioning options |
| 101 | `z_opening.c`: the title runs with child Link, cutsceneIndex 0xFFF0 and scene layer 0 (MS-062) | ML's ovl_opening (dma 99), 3 words vs vanilla's 0 / 0xFFF3 / 7. ML's spot00 has no alternate headers, so the layer only matters for Link's age. ML's file select (dma 100) only forces off the debug File 1 → Map Select, which SoH already keeps behind Debug Enabled |
| 102 | `code/ml_state.c` `MlState_RememberMask` / `MlState_RestoreMask`, called from `Play_TriggerVoidOut` and `Play_LoadToLastEntrance` (`z_play.c`) and `Player_Init`; option **Keep Bunny Hood** on the MissingShip page (default on) (MS-141, #105) | QoL requested by the owner, not ML: the Bunny Hood worn at a void-out, respawn or game-over continue is still on afterwards (vanilla takes masks off on every scene load; SoH's Persistent Masks keeps every mask through everything) | Fork-only option; harmless to drop |
| 103 | `ovl_En_Ms` `EnMs_Update`: ML's 53-frame flash skips the colour filter when **No Bean Seller Flashing** is on (MissingShip page, "Characters", default off) (MS-118, #82) | QoL requested by the owner, opt-in: stops the bean seller (ML's Bunny Hood trader) from flashing; off keeps ML's flash | Fork-only option; harmless to drop |
| 104 | `z_kankyo.c` `func_800766C4`: logs `MissingShip-TRACE rain target ...` whenever the rain targets (`unk_EE[0]`, `unk_F2[0]`) change, with scene, `gWeatherMode` and cutscene state (MS-106, #70) | Diagnostic: stray rain in Kokiri Forest has no static source; the log line names it the next time it happens | Remove once MS-106 is solved |
| 105 | `ovl_Ml_FogWall`: Init sets `fogOnly` when the wall's room is on neither side of its transition entry; Update then only works out the side for Draw and skips the room-loading action (MS-147, #111) | ML places its 9 Deep Woods room-5 fog walls with params 0, i.e. transition entry 0 (rooms 2/1). Crossing the ground-level one loads room 2, swaps rooms every frame and can unload room 5, leaving a black void | Bugfix of an ML side effect; keep |
| 106 | Menu: removed "Navi Timer Resets on Scene Change", the Navi timer display, the Minigames sidebar and "Bunny Hood Effect"; `MissingShip_ClearRetiredOptions` (bootcommands.c) clears their CVars before every `ShipInit::InitAll` (boot, presets) (MS-145, #109) | Owner's settings review: ML has no Navi, its minigames aren't vanilla's, and its Bunny Hood boost is the author's design. The conditional timer stays (ML's race uses it). |
| 107 | `ml_state.c` MlState_PlayerFrame: the Magic Hourglass history refill (frame 17 after the room key changes) waits while `roomCtx.prevRoom` is still loaded (MS-150, #114) | Deviation from ML (owner-approved soft-lock fix): a door changes `curRoom` while Link is still in the doorway, so ML's refill could record a spot in the old room; rewinding there after it unloaded left Link behind a shut door in an undrawn room. |
| 108 | Gameplay Stats (`gameplaystats.cpp`): `ResolveSceneID` returns ML's area names (`sMlAreas`, from hack.json); labels: Boomerang → Magic Hourglass, Bullet Bag 40 → Soul-Charged Sword, Barinade → Arborachnid, Ganon → Game Completed. `Ml_BossVa` calls `GameInteractor_ExecuteOnBossDefeat` when its blue warp appears; `Ml_Ending` Init sets the Ganon timestamp and `gameComplete` (Deep Woods only) (MS-145, #109) | Owner's settings review: stats showed vanilla names, and ML's boss and ending never reached SoH's hooks. |
| 109 | Cutscene destinations 3, 4, 22, 51, 55, 61, 65, 73 and 105 in `z_demo.c` Cutscene_Command_Terminator go to new 8-entry entrance groups `ENTR_MISSINGSHIP_*_EVENT` (entrance_table.h 0x63E-0x675: ranch, Deku Road, Kokiri Forest, Deep Woods grotto, Deku Grove, Monktown Valley, Deep Woods) with cutsceneIndex 0xFFF0 and ML's black fades; the pipeline leaves those codes alone, rewrites the others to ML's row at entrance + layer, and covers every scene header's cutscene (MS-151, #115) | ML patched these cases (code 0x80065428-0x80066B48) to its own entrances + layer 4: the credits tour after the Hollow Root Keep boss, pond -> ranch -> Deku Road (Requiem of Spirit) -> Deku Grove -> Monktown Valley -> Kokiri Forest -> Deep Woods grotto -> Deep Woods' ending card. The layer needs a group at least five entries long; vanilla's are four. Not reproduced: case 4's dropped break (unused), and ML's change of a 0xDB case (42 or 47) to 0xDC (unused). |
| 110 | `sEntranceCutsceneTable` gains ML's Monktown Valley intro (ML entrance 0x03C → `ENTR_KAKARIKO_VILLAGE_FRONT_GATE`, flag 0xA9, `gMlSpot01IntroCs` at spot01 +0x290) (MS-152, #116) | The row sits after the table's first 0xFFFF entry, so the port missed it; its flag also shows world-map point 9 (row 24's point table). |
| 111 | Time Splits (`TimeSplits.cpp`): `splitObjectList` and `popupList` replaced with ML's items (ML names: Magic Hourglass, Deku Nut Milk, Soul-Charged Kokiri Sword with its icon, Birth Certificate, Deku Pass), Arborachnid, and entrance splits for ML's dungeons; vanilla's escape/death splits dropped. Gameplay Stats labels the Stone of Agony Birth Certificate and the Gerudo Card Deku Pass (MS-145, #109) | Owner's settings review: Time Splits needed a complete redo for ML. Items are those ML hands out in play (chests, the playtest log and save). |
| 112 | `Scene_CommandAlternateHeaderList` (`z_scene_otr.cpp`) bounds-checks the alternate-header list; an index past its end takes the null-entry path (main header). The pipeline now keeps ML's lists instead of overwriting cmd 0x18 (`build_rom_inline.py`, `ML_STRIP_ALTHEADERS=1` restores the old behaviour) (MS-151, #115) | ML's lists are four entries long (five in the boss room) and hold only setup 4, the credits layer: each tour stop's cutscene and its room actors. The unchecked index crashed on a short list, so the lists were stripped, which sent every stop after the pond to its main header with no cutscene. |
| 113 | Console `entrance <hex> [cutscene hex]` takes an optional cutsceneIndex (`debugconsole.cpp`) | Test aid: reaches cutscene layers (e.g. `entrance 63E fff0`, the ranch credits stop) without playing up to them. |
| 114 | `Cutscene_HandleConditionalTriggers` (`z_demo.c`) drops vanilla's Requiem of Spirit (SoH 0x1E1) and Nocturne of Shadow (SoH 0x0DB) triggers, and `timesaver_hook_handlers.cpp` their Skip Songs stand-ins (MS-120, #84) | ML never arrives by its own 0x1E1 or 0x0DB, so neither fires in ML. The port maps entrances by scene and spawn: ML 0x120, the Deku Grove upper scrub grotto, lands on SoH 0x1E1, where the Requiem trigger set the Requiem flag and moved Link to spawn 0 under cutscene layer 0xFFF0, which that scene lacks (Link frozen). SoH 0x0DB is Monktown Valley's gate. `tools/checkentrances.py` now checks vanilla code's entrance literals too. |
| 115 | Skip Intro (`SkipIntro.cpp`) goes where ML's prologue ends, ML entrance 0x5F0 at noon (cutscene destination 119), not to the start entrance with the cutscene cleared | In ML the start entrance is the prologue's staging room, a scene with no geometry: skipping the prologue there left a new game in a black void. |
| — | Scene draw configs set to ML's (`pipeline/gen_scene_config.py` → `scene_table.h`, 17 slots) | ML changed the draw config of most slots it uses (grotto scenes → SDC_GROTTOS, Deep Woods/Deku Road → SDC_DEKU_TREE, ...). Vanilla's left segment 8 unset in the grotto scenes, so their DLs ran a rupee texture as commands (crash entering the Colossus-slot grotto) | engine table: the scene table is code, so a drop-in .o2r cannot change it; needs a SoH hook that lets a scene resource override its draw config |
| — | Pipeline: `remap_exits` no longer stops at entrance 0 | ML entrance 0 is real (Deep Woods spawn 0); 6 scenes had later exits left unmapped (Deku Road's last three, the grotto arena's, ...). Each list is now bounded by the next header data block: without the 0 terminator the scan ran into tokinoma's player entries and rewrote Player's actor id 0, so Player never spawned after the prologue (crash in `func_800304DC`). `tools/checkexits.py` verifies all 104 real exit slots end to end | data (fixed) |
| — | ZAPD `ZAudio.cpp`: zero-size soundfont guard | pipeline only, never shipped | Upstream as a bugfix (optional) |
| — | Settings (seed profile `profile/shipofharkinian.json`): `TextSpeed` 5, `SkipText`, `DpadEquips`, `CustomOcarina.Dpad` (D-pad plays the C notes while the Ocarina is out, MS-019) | QoL requested by the user | Stock SoH options — document them |
| — | Setting: `RememberSaveLocation` = on | ML resumes where you saved (proven by diffing `Sram_OpenSave`) | Already a stock SoH option — document it for drop-in users |

## The hard wall: actor logic

ML rewrote a large set of vanilla actors (see `placed_actor_status.json`) and added its own in vanilla's
unused actor slots. Every row marked **Needs mod code** above is one of these (27 of 47 so far). None can be pure data in
stock SoH. Realistic long-run options, to decide with the user:

- **Upstream a mod-code mechanism** into SoH (plugin API or scripting) — the only route to a true
  drop-in with ML's full behaviour; a large architectural change that's HarbourMasters' call.
- **Upstream narrow data-driven hooks** that cover the common cases (e.g. "this NPC offers item X after
  text Y", "disable actor Z in this scene") — covers some of ML, not all.
- **Split distribution:** a drop-in `.o2r` for the data plus a MissingShip build for full behaviour.

Until then, actor ports live in the fork, isolated and logged here, so each can move to whichever
mechanism lands.

## ML's custom code region (0x80600000)

- **Where it lives:** ML ships engine-level code outside every overlay: **dma file 507** (vrom 0x3922600, 32 KB), loaded at **0x80600000** by boot's last loader entry. There is **no** ML code past 0x80608000.
- **The old claim was wrong.** It said "targets at 0x8064xxxx / 0x807Cxxxx" and "17 hooked actor overlays". Those came from scanning data as code, with a mis-based actor table. The ten extra actors it listed (0x09E, 0x0F4, 0x11B, 0x12E, 0x141, 0x173, 0x18A, 0x18B, 0x198, 0x199) are byte-for-byte vanilla. That includes 0x18A (Oceff_Wipe), which is not an ML copy.
- **Fixed RAM addresses:** ML's heap layout is fixed. The **player overlay always loads at RAM 0x804D23B0**, and the custom code's only references outside `code` / boot are trampolines into it:
  - `jal 0x804E913C`: player +0x16D8C, the function the Hourglass hook displaced;
  - `j 0x804EA218`: player +0x17E68, the return after that hook;
  - `j 0x804F32F0`: player +0x20F40, the return after the +0x20F38 hook. The routine at 0x80600DAC re-runs the two overwritten prologue instructions first.
- **Other fixed addresses** the custom code relies on: `gSaveContext` 0x8015E660, PlayState 0x80212020, the player instance near 0x802245B0.

**Generated by `python3 tools/hookscan.py --md`** (MS-006). Every relocatable overlay in the ROM, plus `code` (only words that differ from MQ Debug) and the custom code itself. Relocated jumps are local and ignored.

| File | Hooks (offset: jump → target) |
|---|---|
| code | +0x1176c `jal 0x80602c80`, +0x14b48 `jal 0x806037f0`, +0x64c40 `jal 0x80602da0`, +0x6be84 `jal 0x80604e30`, +0x6dc88 `jal 0x806053e0`, +0x72658 `jal 0x80602de0`, +0x7c584 `jal 0x80607d00`, +0x86d10 `jal 0x806023e0`, +0x87264 `jal 0x806023e0`, +0x872c8 `jal 0x806023e0`, +0x87324 `jal 0x806023e0`, +0x87384 `jal 0x806023e0`, +0xda52c `j 0x80602d00` |
| actor 0x018 En_Elf | +0x3ad8 `jal 0x80602000` |
| actor 0x060 En_Dekunuts | +0x1424 `j 0x806008e0` |
| actor 0x0da En_Bom_Chu | +0x114c `jal 0x80607a00` |
| actor 0x163 En_Ko | +0x2aa0 `j 0x80602200` |
| actor 0x16d En_Md | +0x1c88 `j 0x80602200` |
| actor 0x174 Ml_FogWall | +0xe3c `jal 0x80602080` |
| actor 0x18e Bg_Jya_1flift | +0x4c0 `jal 0x80606c00` |
| ovl_kaleido_scope | +0x118 `jal 0x80600690`, +0x18c `jal 0x80600690`, +0x1dc `jal 0x80600690`, +0x228 `jal 0x80600690`, +0x5038 `jal 0x80606c90`, +0x510c `jal 0x80606c90`, +0x16128 `j 0x80607f20` |
| ovl_player_actor | +0x950 `jal 0x806023e0`, +0x9c0 `jal 0x806023e0`, +0xa04 `jal 0x806023e0`, +0xa44 `jal 0x806023e0`, +0xa88 `jal 0x806023e0`, +0x205c `jal 0x806023e0`, +0x2618 `jal 0x806023e0`, +0x2a30 `jal 0x806023e0`, +0x55a0 `jal 0x806023e0`, +0x9e84 `jal 0x806023e0`, +0xa224 `jal 0x806023e0`, +0xaa58 `jal 0x806023e0`, +0xabe0 `jal 0x806023e0`, +0xbd9c `jal 0x80606ba0`, +0xd4bc `jal 0x806023e0`, +0xe99c `jal 0x806023e0`, +0x11314 `jal 0x806023e0`, +0x114e0 `jal 0x806023e0`, +0x1196c `jal 0x806023e0`, +0x119bc `jal 0x806023e0`, +0x1213c `jal 0x806023e0`, +0x144e0 `jal 0x806023e0`, +0x14644 `jal 0x806023e0`, +0x17938 `jal 0x80606bc8`, +0x17e5c `j 0x80600e00`, +0x1a1e4 `jal 0x806023e0`, +0x1a2c4 `jal 0x806023e0`, +0x1d728 `jal 0x80600480`, +0x1ed6c `jal 0x806023e0`, +0x1edd8 `jal 0x806023e0`, +0x1f454 `jal 0x806023e0`, +0x1f9c0 `jal 0x806023e0`, +0x1fa68 `jal 0x806023e0`, +0x20f38 `j 0x80600d90` |

Never loaded (no table entry points at them), listed for completeness:

| File | Hooks |
|---|---|
| dma 119 | +0x118 `jal 0x80600690`, +0x18c `jal 0x80600690`, +0x1dc `jal 0x80600690`, +0x228 `jal 0x80600690`, +0x5038 `jal 0x80606c90`, +0x510c `jal 0x80606c90`, +0x16128 `j 0x80607f20` |
| dma 120 | +0x3ac4 `jal 0x80600000`, +0xbd9c `jal 0x80606ba0`, +0x17938 `jal 0x80606bc8`, +0x17c84 `jal 0x80604d10`, +0x17e5c `j 0x80600e00`, +0x1c964 `jal 0x80604cc0` |

Custom code jumping outside `code`, boot and itself:

- +0x800 `j 0x806484c4` → unknown
- +0x838 `j 0x806484c4` → unknown
- +0xdb4 `j 0x804f32f0` → player +0x20f40
- +0xfe8 `jal 0x804e913c` → player +0x16d8c
- +0xff8 `j 0x804ea218` → player +0x17e68
- +0x1838 `j 0x806484c4` → unknown
- +0x1890 `j 0x8034383c` → unknown
- +0x1898 `j 0x80344c50` → unknown
- +0x18a0 `j 0x8064686c` → unknown
- +0x18a8 `j 0x80388084` → unknown
- +0x2b28 `j 0x802cde58` → unknown
- +0x6df4 `jal 0x80586c14` → unknown

Data pointers into the custom region: none

The "unknown" jumps inside the custom code (+0x800, +0x838, +0x1838, +0x1890–0x18A8, +0x2B28, +0x6DF4) were checked by hand. They are data (display-list words, byte tables) decoded as instructions.

### Hook targets: what they do and their status

Descriptions of unported targets are a first reading of the code; confirm them when porting.

| Target | Hooked from | What it does | Status |
|---|---|---|---|
| 0x80602DE0 | code +0x72658 | Blade Beam spawner (spin ≥ 8 frames, or first swing of a combo at full health; Soul-Charged sword = slingshot ammo 40); also patches `object_link_child` DLs and pulses the sword glow | beam **ported** (row 17); sword model, glow and B icon **ported** (row 99) |
| 0x80602C80 | code +0x1176C | wall push-out skip after room change / Deep Woods teleport | **ported** (`ml_state.c`) |
| 0x80602080 | Ml_FogWall +0xE3C | the fog wall's teleport / position logic (writes the wall-skip counter) | **ported** with Ml_FogWall (row 26) |
| 0x806037F0 | code +0x14B48 | area title card ribbon | **ported** |
| 0x80602DA0 | code +0x64C40 | overworld minimap never drawn | **ported** (hidden by default) |
| 0x80607D00 | code +0x7C584 | world-map area remap | **ported** |
| 0x80602D00 | code +0xDA52C | Navi / Skull Kid sound mute | **ported** |
| 0x80604E30, 0x806053E0 | code +0x6BE84, +0x6DC88 | HUD drop shadows: black copies of the B / C / Start button backs (2 px, alpha capped at 125; Start only in pause states 6–7) and of the rupee and key counts (1 px; keys only in scene 7), drawn before the vanilla ones | **ported** (row 100) |
| 0x806023E0 | code ×5, player ×29 | Link animation override: the triple jump, and the horse idle at Fishing Pond cutscene entrance 0x420 | ported (MS-051, row 96) |
| 0x80600690, 0x80606C90, 0x80607F20 | pause menu | quest-status layout and cursor, vertical move on the equipment page, world-map points | **ported** (row 24; world-map points in the item-restriction table) |
| 0x80600E00 (+ 0x80601000, 0x80601680) | player +0x17E5C | wraps Player_UpdateCommon (+0x16D8C): room counter (0x80601680: full history at frame 17), Magic Hourglass history and rewind (Oceff_Wipe params 1 + NA_SE_IT_SHIELD_REFLECT_MG; entries 69..161, 4 a frame, update skipped meanwhile), then the cucco keeper (0x80601000) | **ported** (row 98) |
| 0x80600D90 | player +0x20F38 | `Player_StartTalking` returns to ra + 8 (skipping each caller's branch) when the actor's text is 0xE0, the "talk to Saria?" prompt; else re-runs the prologue and continues | **ported** (row 46, MS-017), with player +0x1C2B0 |
| 0x80600480 | player +0x1D728 | clears the room-frame counter at 0x80609FF0 (already ported as `gMlRoomFrames` in `ml_state.c`), then walks the PROP list for actor 0x19 (En_Niw, the cucco) and, if one is there, sets the Magic Hourglass halfwords 0x80600AA8 = 0x77 and 0x80600AAA = 1 | **ported** (rows 97, 98) |
| 0x80606BA0 | player +0xBD9C | Bunny Hood: R_RUN_SPEED_LIMIT (REG 45) 550, or 1100 with the hood on, and the target speed's float bits + 0x400000 | **ported** (row 97) |
| 0x80606BC8 | player +0x17938 | the out-of-water speed factor: 1.0, or 1.25 when Link's currentMask (0x8022470F, ML's fixed player + 0x15F) is the Bunny Hood | **ported** (row 97) |
| 0x806008E0 | En_Dekunuts +0x1424 | room-0 scrub leash | **ported** (row 22) |
| 0x80602200 | En_Md +0x1C88, En_Ko +0x2AA0 | Kokiri/Mido text swaps after the sword (ammo[6] ≠ 0), and the Kokiri frown (object_kw1 mouth rows 12–18) | **ported**: Mido row 14, En_Ko row 53 |
| 0x80602000 | En_Elf +0x3AD8 | end of `EnElf_Update` (every fairy but Navi): once ammo[6] ≠ 0, world.pos.y = -2048.0 | **ported** (row 51, MS-030) |
| 0x80607A00 | En_Bom_Chu +0x114C | Bombchu behaviour gated on scene 7 and Z-position bounds | not ported (MS-040) |
| 0x80606C00 | Bg_Jya_1flift +0x4C0 | player-proximity check (X/Z within 48 units of the player instance) | not ported (MS-038) |
| 0x80600000, 0x80604CC0, 0x80604D10 | only the never-loaded player copy (dma 120) | unused by the shipped game | nothing to port |
| — | player +0x38CC | the boomerang throw's En_Boom spawn replaced by a flag write (0x80609FFF = 1): the Magic Hourglass trigger, with +0x378C (no aim hold) | **ported** (row 98) |
| — | beam update | crystal switches vs. beam: invisible explosion (ML's gutted En_Bom clone in slot 0x0E) | ported as params 0xB0B0 of row 17 |

### Entrance collisions (MS-095)

`tools/checkentrances.py`, run by `check.py`, finds ML entrance groups that land on one SoH entrance while ported code
tells them apart (every `== MlEntrance_ToSoh(0x...)`). It checks each against every route in: scene exits, cutscene
destinations, ML's grotto table and code-set entrances. The Fishing Pond pair (0x41C / 0x420) was mapped apart in
row 84. The one remaining overlap, 0x058 / 0x157 (identical ranch entries), is harmless and documented in
`pipeline/entrance_collisions.json`.

### Entrance flags (ported, row 50)

ML's entrance table carries its own title-card / keep-music / transition flags, but the pipeline remaps ML's exits
onto SoH entrance indices, so SoH applied its own. `gen_entrance_flags.py` lines each reachable ML entrance group
(exits, cutscene destinations, ML's grotto table, `MlEntrance_ToSoh` literals) up with the SoH group it lands on,
layer by layer, and the engine writes ML's flags onto the 184 slots that differ. ML's leftover vanilla entries
(unreachable in ML) are ignored. **Known deviation:** 4 SoH entrances (Kokiri Forest 0x0EE / 0x0F0, Gerudo's Fortress
0x129 / 0x12B) are reached by two ML groups that disagree only on the day-layer fade colour (black vs white); they
take the fields both agree on and keep SoH's colour. Giving the minority group its own appended entrance would
make those exact. Rerun the generator after changing entrances, as with `gen_entrance_map.py`.

## ML's inventory layout (ported)

ML's item system is vanilla `code` reading ML's data: a word diff of `code` (tools/codediff.py, zero shift vs MQ debug)
shows Item_Give and the Inventory_* functions untouched apart from the edits below, so SoH (which matches the decomp)
behaves like ML once the data matches. Ported:

| Change | ML source | SoH location |
|---|---|---|
| `gItemSlots`: Sticks→9, Nuts→10, Bombs→8, Boomerang (Hourglass)→15, bottles→13, child trade→16 | code +0x10A604 | z_inventory.c |
| Strength upgrade mask cleared (menu/Item_Give see no bracelet) but `Player_GetStrength` keeps the real mask 0x1C0 | code +0x10A358, 0x8008F040 | z_inventory.c `gUpgradeMasks[2]`, z_player_lib.c |
| Bomb bag / nut capacity 20 at level 0 | code +0x10A3A4, +0x10A3D4 | z_inventory.c `gUpgradeCapacities` |
| `Inventory_HasEmptyBottle` checks 18, 13, 14, 21 | 0x80086ADC.. | z_parameter.c |
| Bombchu cap 20 in Item_Give (2) and the HUD "full" colour | 0x80085B70.., 0x8008A000 | z_parameter.c |
| Pause ammo digits only for slots 8-10 | kaleido +0x74B0/+0x74D8 | z_kaleido_item.c |
| Saves from before the port migrated to ML's slots on load | — (SoH-only) | z_sram.c `MlInventory_MigrateVanillaLayout` |
| En_Ms: Bunny Hood for 20 Deku Nuts (takes them all), 0x405E/0x405F, periodic flash | ML dma 229 | ovl_En_Ms |

Drop-in path: all of these are engine data/code; a drop-in .o2r needs SoH to expose the slot table (or a mod hook).
Literal `SLOT_*` constants stay vanilla on purpose -- ML's code keeps them too (e.g. the two `items[SLOT_NUT]` checks
in the button-restriction code, Inventory_ConsumeFairy's bottle loop).

Item restrictions and related hooks (ported):

| Change | ML source | SoH location |
|---|---|---|
| Restriction table: vanilla's shop/minigame/indoor rows reset to the overworld default; new rows for slot 0x56 and 0x5C (Hookshot, Farore's, Din/Nayru, "all") and 0x6B (also the B button) | code +0x108A10 (`sRestrictionFlags`) | z_parameter.c |
| World-map points: 0 and 1 hidden, 2/4/5/8/9/10 from eventChkInf[10] bits 0x08/0x20/0x80/0x40/0x200/0x100, every pause update | kaleido +0x16128 → 0x80607F20, table 0x80607EE0 | end of KaleidoScope_Update |
| Overworld minimap never drawn; L no longer toggles it there | 0x80081AA0 → 0x80602DA0, 0x80082184 | z_map_exp.c: already an opt-in cosmetic (hidden by default); L toggle now only when it is shown |

`0x806023E0` (hooked into five LinkAnimation calls at 0x800A3B70..0x800A41E4) is not a restriction routine: it swaps
Link's animation: a running jump (gameplay_keep 0x04003148) becomes the next step of ML's triple jump, and at
entrance 0x420 every Link animation becomes the horse idle. Ported as row 96.

Still open: the scene draw-config handlers (0x80099550-0x800A06F4), which ML rewrote wholesale.

## ML `code` patches (full pass, 0x8001CE60-0x80114000 word diff vs MQ debug; `tools/codediff.py`)

| Patch | ML | SoH |
|---|---|---|
| Navi's sounds muted (fairy dash/vanish/fly, Hey/Listen/Watch out/call, NA_HELLO_2) + Skull Kid laugh | 0x800F738C → 0x80602D00 (IDs & 0xF1FF) | code_800F7260.c `MlAudio_IsMuted` |
| Dispersed sparkles off (`EffectSsKiraKira_SpawnDispersed` returns at once) | 0x80028BB0 `jr $ra` | z_effect_soft_sprite_old_init.c |
| Wall push-out skipped for 2 frames after a room/scene change, and for the next 2 calls after the Deep Woods corridor teleport | 0x8002E5CC → 0x80602C80; counters 0x80609FF0 (0x80600E80) / 0x80602038 | new `code/ml_state.c`, z_actor.c, Ml_FogWall, z_player.c |
| Area title cards: ML ribbon (custom-code tex +0x3BE0, 160x8 IA16) tinted per scene + white title at (88,68) (drawn at y 64 because inject_titles.py pads ML's 144x16 image 4 rows down into the 144x24 slot; MS-094); boss slot 0x18 vanilla | 0x800319A8 → 0x806037F0 | z_actor.c `MlTitleCard_Draw`, `ml_titlecard_banner.inc` |
| World-map area remap 1→4, 4→9, 10→6, 9→1, 6→10 | 0x800993E4 → 0x80607D00 | z_scene.c misc settings |
| Save resumes at its saved entrance outside dungeons (vanilla default case nop'd) | 0x800A85CC.. | z_sram.c `Sram_OpenSave` |
| `Player_IsChildWithHylianShield` true for a child with no shield | 0x8008E9E8 | z_player_lib.c |
| Cutscene title card y 120 → 80 | 0x80064B44 | z_demo.c |
| Ocarina A-note green (80,255,150) | 0x80106A18 | z_message_PAL.c |
| Text reveals 2 chars per step (SoH Text Speed default 2) | 0x80109908 | z_message_PAL.c, SohMenuEnhancements.cpp |
| Scene draw config 0 = Deku Tree handler (nop'd default falls through) | 0x80099550 | z_scene_table.c |

Not ported / not applicable: debug camera toggle and D-Right cutscene skip (SoH gates both behind debug), credits
trigger at Gerudo Valley setup 6 (already `#if 0` in SoH), heap/arena/audio sizes, arena memsets, controller count,
the pause prerender filter state, font file offsets (ordered-font glyphs at 0 instead of 0x380D4 -- only matters if ML's
glyphs differ), Link object DL/animation-ROM pointers (SoH draws by resource name, and MS-050 ships ML's object under the vanilla names). ML's HUD restyle is row 100.
(The dungeon minimap index, room + 190 at 0x80080D20, is row 48.)
