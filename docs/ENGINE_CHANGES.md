# MissingShip engine-change ledger

**Goal:** the entire game working on the SoH engine now; long-run, a **drop-in `.o2r` for stock SoH**.

A stock-SoH mod can only carry data. SoH 9.2.3 has no runtime mod-code path (no plugin loader, no
scripting), and HarbourMasters' own code-modding guide describes only fork-and-compile. So every entry
below is a **drop-in blocker** until it is eliminated, turned into data, or upstreamed as a general
feature that data can drive.

Order of preference for any new fix: **data-side** → **generic data-driven SoH feature (upstream)** →
**upstreamable bugfix** → ML-specific C (last resort, isolated, commented `MissingShip:`, logged here).

## Already drop-in compatible (data, shipped in `MissingLink_Scenes.o2r`)

Scenes and rooms (textures, geometry, exit remaps, cutscene destinations) · audio (574, incl. the
MEDIUM_CART → RAM sample fix) · text (269 messages) · title cards · minimaps · objects
(`object_ml_fishpond`, `object_ml_villager`, `object_ml_lift`, five `object_ml_gi_*` pickup models) · ML's 9 entrance
intro-cutscene scripts · **pickup models shipped under vanilla paths** (Boomerang → Magic Hourglass, Gerudo Card →
Deku Pass, Stone of Agony → Birth Certificate; `inject_objects.py` ALIASES) · **168 redrawn pause/HUD textures**
(item icons, item names, area names, HUD; `inject_ui_textures.py`, only the ones that differ from stock) · **blank
title cards** for the 8 scenes ML gives none (else stock SoH shows the vanilla slot's name, e.g. "Sacred Forest Meadow"
over ML's Saria's house) · raw gameplay_keep texture pointers in ML display lists rewritten as resource hashes.

## Engine changes

| # | Change | Why it exists | Drop-in path |
|---|--------|---------------|--------------|
| 1 | `entrance_table.h`: +32 entrances | ML destinations with no vanilla (scene, spawn) equivalent. Until 2026-09-23 the pipeline read SOHCombo's unpatched table and never used them; now 8 exits do | **Upstream:** let mods append entrances from a resource |
| 2 | `z_demo.c`: `CS_DESTINATION` default case + `CS_BLACK_FILL` escape | ML's opening cutscene ends on a destination vanilla doesn't know | **Upstream (generic):** "destination encodes a raw entrance" is data-agnostic |
| 3 | `z_demo.c`: ML's entrance intro-cutscene table | vanilla's 34 intros fired inside ML scenes | **Upstream:** data-driven table (mods override from a resource); the scripts are already data |
| 4 | `z_sram.c`: stub-entrance guard (replay prologue) | a continue can land on ML's geometry-less intro stub | **Try data-side:** give the stub scene a safe default layer; otherwise upstream as a hook |
| 5 | `z_map_exp.c`: minimap hidden by default | user preference | **Remove for drop-in** — it's a user setting |
| 6 | `z_en_hy.c`: Birth Certificate offer | ML rewrote En_Hy's logic | **Needs mod code** (actor logic) |
| 7 | `z_en_niw_lady.c`: certificate hand-in, Heart Piece | ML rewrote Anju's logic | **Needs mod code** |
| 8 | `z_bg_treemouth.c`: inert | ML gutted the Deku Tree mouth | **Needs mod code** — or upstream a "disable actor" data hook |
| 9 | `actor_table.h` + `ActorDB.cpp` + `actorViewer.cpp`: slots 3, 6 | register ML's new actors | **Needs mod code** |
| 10 | `ovl_Ml_Fishpond`, `ovl_Ml_Villager` | ML's new actors | **Needs mod code** |
| 11 | libultraship `gfx_metal.cpp`: zero-dimension texture guard | engine crash any mod could trigger | **Upstream as a bugfix** |
| 12 | `ovl_Ml_Lift`, `ovl_Ml_LiftBlock` + actor table slots 0x12C, 0xE3 | ML rewrote Obj_Lift (switch lift) and Bg_Mori_Hashira4 (quest-item barrier) wholesale; vanilla files untouched, slots repointed | **Needs mod code** |
| 13 | `z_sram.c` `Sram_InitNewSave`: ML's starting loadout (Ocarina of Time, Kokiri Sword, Hylian Shield, Goron's Bracelet, Adult's Wallet, Epona's + Saria's Songs) | SoH builds new files in C, not from ML's new-save templates | **Upstream:** mods provide a new-save template resource |
| 14 | `z_en_md.c` (Mido): Kokiri Forest uses the Mido's-House lines; Init always takes the stand-at-home branch; Update no longer calls `actionFunc`; 0x1046 → 0x1044 once slingshot ammo ≠ 0 | ML patched vanilla En_Md in 3 places (GetText branch, Init `beq $a0,$a0`, Update `jalr` → `j 0x80602200` into its custom code). Vanilla BlockPath slid him around / SetMovedPos teleported him along ML's unrelated path 0 | **Needs mod code** |
| 15 | `z_sram.c` `Sram_InitNewSave`: Ocarina also on D-Down when "Equip Items on Dpad" is on | QoL, as 2Ship does | Harmless to drop; a user can equip it themselves |
| 16 | `z_draw.c` GetItem draw table: red potion → Deku Nut Milk (`DrawOpa0Xlu1`, ML object 0x11), blue potion → red colours, Bullet Bag 40 → Soul-Charged Kokiri Sword | ML changed these draw-table rows (code data diff); the other three changed pickups are pure data aliases | **Try data-side:** `GID_BULLET_BAG_50` shares the bag DL and potions share the pot DL, so a name override is not enough — upstream a data-driven draw table |
| 17 | `ovl_Ml_MThunder` (slot 0x57 repointed) + `MlMThunder_PlayerUpdate` called from `Player_Update` | ML rewrote En_M_Thunder into the **Blade Beam** (params 0xDEAD, vanilla's unused `gUnusedBeamBladeDL`), a sword-glow prop (0xBEEF, inert until ML's `object_link_child` is ported) and a spin-charge sparkle; the beam spawner lives in ML's custom code at 0x80602DE0 | **Needs mod code** |
| 18 | `RemoveSpinAttackDarkness.cpp`: hook made a no-op | it swaps the update of actor 0x57 and would run vanilla logic on ML's struct | Goes away with row 17 |
| 19 | `Oceff_Wipe`/`Oceff_Wipe2`/`Oceff_Wipe3`/`Oceff_Wipe4` draw: stretch the song-effect cone by `aspect / (4/3)` | on widescreen the 4:3-sized cone's rim shows as a circle (reported playing Saria's Song); upstream `develop` is identical to 9.2.3, so unfixed there | **Upstream as a bugfix** — nothing ML-specific |
| 20 | `ovl_Ml_TreeBridge` (slot 0x005, was UNSET) | ML's tree that a Blade Beam burns and topples into a bridge (switch = params & 0xFF, camera = params >> 8); model appended to `object_wood02`, collision inline in ML's overlay → generated C data | **Needs mod code** |
| 21 | `ovl_Ml_SongBlock` (slot 0x1D1 repointed from Obj_Timeblock) | ML rewrote the Time Block into the block sealing Saria's house: spawns a vanilla En_Okarina_Tag (0x1C0C) and sinks when switch 0x0C is set | **Needs mod code** |
| 22 | `z_en_dekunuts.c` Update: room-0 scrubs clamped within 30 units of home | ML hooks the function's return (`j 0x806008E0`); keeps Deep Woods' water-edge scrubs from being knocked into the water, where they never die | **Needs mod code** |
| 23 | `z_bgcheck.c`: NULL checks on `cameraDataList` in `func_80041A4C` / `func_80041B24` / `func_80041C10` | SoH's `SEGMENTED_TO_VIRTUAL` is the identity, so the existing `gSegments[0]` guards never catch a NULL list; standing on ML's tree bridge (inline collision, no camera data) segfaulted in `Camera_Update` | **Upstream as a bugfix** |
| 24 | Pause menu (`z_kaleido_item.c`, `z_kaleido_collect.c`, `z_kaleido_equipment.c`, `z_kaleido_scope_PAL.c`): quest-status layout + ML's cursor redirects, equipment page layout + ML's vertical move (0x80606C90), any-age item/equipment tables, page/world-map colours and points, item grid X, Bombchu cap 20, quest page entry slot 24 | ML's kaleido overlay (dma 101) data + code + hooks 0x80600690 / 0x80606C90 | **Try data-side** for the tables (upstream: data-driven pause layout); the cursor logic needs mod code |
| 25 | `Door_Ana`: ML's grotto destinations, no grotto-return respawn, hole drawn from ML's gameplay_keep; `entrance_table.h` +1 (`ENTR_MISSINGSHIP_DEKU_TREE_2`) | ML replaced the grotto table (7 of its own entrances) | **Upstream:** data-driven grotto table |
| 26 | `ovl_Ml_FogWall` (slot 0x174), `ovl_Ml_HintNuts` (slot 0x192), `ovl_Ml_EventSpawner` (slot 0x121, partial) | ML rewrites: Deep Woods fog walls + looping corridor, talking scrubs (object 0x005), event spawner (En_Fz) | **Needs mod code** |
| 27 | `En_Firefly`, `En_Dekubaba` (no hostile flag, Baba always drops nuts), `En_Dns` (prices, bugs, Deku-air bottle, garden gate) | ML in-place patches | **Needs mod code** |
| 28 | `ml_entrance_map.c` (generated by `pipeline/gen_entrance_map.py`) + `z_sram.c` dungeon return + game-over continue translate ML entrance numbers; `Door_Ana` table corrected (starts at +0x5FC, two vanilla entries first); `entrance_table.h` +1 (`ENTR_MISSINGSHIP_6B_3`, Fishing Pond intro destination) | 370 of ML's 1556 entrance numbers mean a different place than vanilla's; engine code using literal numbers must go through ML's table | **Upstream:** mod-provided entrance remap resource |
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
| — | Scene draw configs set to ML's (`pipeline/gen_scene_config.py` → `scene_table.h`, 17 slots) | ML changed the draw config of most slots it uses (grotto scenes → SDC_GROTTOS, Deep Woods/Deku Road → SDC_DEKU_TREE, ...). Vanilla's left segment 8 unset in the grotto scenes, so their DLs ran a rupee texture as commands (crash entering the Colossus-slot grotto) | engine table: the scene table is code, so a drop-in .o2r cannot change it; needs a SoH hook that lets a scene resource override its draw config |
| — | Pipeline: `remap_exits` no longer stops at entrance 0 | ML entrance 0 is real (Deep Woods spawn 0); 6 scenes had later exits left unmapped (Deku Road's last three, the grotto arena's, ...). Each list is now bounded by the next header data block: without the 0 terminator the scan ran into tokinoma's player entries and rewrote Player's actor id 0, so Player never spawned after the prologue (crash in `func_800304DC`). `tools/checkexits.py` verifies all 104 real exit slots end to end | data (fixed) |
| — | ZAPD `ZAudio.cpp`: zero-size soundfont guard | pipeline only, never shipped | Upstream as a bugfix (optional) |
| — | Settings: `TextSpeed` 5, `SkipText`, `DpadEquips` | QoL requested by the user | Stock SoH options — document them |
| — | Setting: `RememberSaveLocation` = on | ML resumes where you saved (proven by diffing `Sram_OpenSave`) | Already a stock SoH option — document it for drop-in users |

## The hard wall: actor logic

ML rewrote a large set of vanilla actors (see `placed_actor_status.json`) and added its own in vanilla's
unused actor slots. Every row marked **Needs mod code** above is one of these (26 of 43 so far). None can be pure data in
stock SoH. Realistic long-run options, to decide with the user:

- **Upstream a mod-code mechanism** into SoH (plugin API or scripting) — the only route to a true
  drop-in with ML's full behaviour; a large architectural change that's HarbourMasters' call.
- **Upstream narrow data-driven hooks** that cover the common cases (e.g. "this NPC offers item X after
  text Y", "disable actor Z in this scene") — covers some of ML, not all.
- **Split distribution:** a drop-in `.o2r` for the data plus a MissingShip build for full behaviour.

Until then, actor ports live in the fork, isolated and logged here, so each can move to whichever
mechanism lands.

## ML's custom code region (0x80600000)

ML ships engine-level code outside every overlay: **dma file 507** (vrom 0x3922600, 32 KB) is loaded at
**0x80600000**, and there are more targets past 0x80608000 (0x8064xxxx, 0x807Cxxxx) still to locate. It is reached by
`j`/`jal` hooks from 17 actor overlays (0x018, 0x060, 0x09E, 0x0DA, 0x0F4, 0x11B, 0x12E, 0x141, 0x163,
0x16D, 0x173, 0x174, 0x18A, 0x18B, 0x18E, 0x198, 0x199) and about 22 sites in `code`. List them with the scan in
`tools/` (search overlays for j/jal into 0x806xxxxx–0x807xxxxx). Every hook is an engine change to port, and
this region is the likely home of ML's player abilities (Blade Beam).

### ML custom-code features: status

| Feature | ML code | Status |
|---|---|---|
| Blade Beam (spin ≥ 8 frames, or first swing of a combo at full health; Soul-Charged sword = slingshot ammo 40) | 0x80602DE0, player `code+0x72658` hook | **ported** (row 17) |
| Soul-Charged sword model + glow on Link | same routine: DMA 4 KB from ROM 0x794000, patches `object_link_child` DLs, pulses vertex colours; actor 0x57 params 0xBEEF | not ported |
| Magic Hourglass (boomerang slot): player position ring buffer, rewind, actor 0x18A (ML copy of Oceff_Wipe) | 0x80600E00 (player +0x17E5C), 0x80601000, 0x80601680; player +0x38CC throw → flag | not ported — the item currently throws a vanilla boomerang |
| Kokiri/Mido text swaps after the sword (ammo[6] ≠ 0) + a Kokiri texture patch | 0x80602200 (En_Ko, En_Md) | Mido ported (row 14); En_Ko pending |
| Crystal switches vs. beam: invisible explosion (ML's gutted En_Bom clone in slot 0x0E) | beam update | ported as params 0xB0B0 of row 17 |
| Fairy-release spawn removed (player +0x74CC) | player overlay | not ported |
| Deku scrub leash (room 0) | 0x806008E0 | ported (row 22) |
| Pause menu: quest-status layout (quest item positions moved/hidden, cursor-navigation table cleared to 0x09), cursor/prompt colours, world-map point data, hooks at 0x80600690 ×4, 0x80606C90 ×2, 0x80607F20 | ML dma 101 (kaleido): 46 code words, 288 data halfwords | **not ported — next** (quest-status cursor reported misaligned) |

### Known data gap: entrance flags

ML's entrance table carries its own title-card / keep-music / transition flags. The pipeline remaps ML exits to
SoH entrance indices but SoH takes those flags from its own table: 39 SoH entrances used by ML differ (2 are used
by ML with conflicting flags: 0x000, 0x0EE). Title cards are covered by the blank-card data fix; music continuity
and transition styles are not yet — **upstream idea:** let a mod override entrance flags from a resource.

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
Link's animation (skips gameplay_keep 0x04003148, substitutes others at entrance 0x420) -- part of the sword visuals.

Still open: the scene draw-config handlers (0x80099550-0x800A06F4), which ML rewrote wholesale; the Link animation
override 0x806023E0.

## ML `code` patches (full pass, 0x8001CE60-0x80114000 word diff vs MQ debug; `tools/codediff.py`)

| Patch | ML | SoH |
|---|---|---|
| Navi's sounds muted (fairy dash/vanish/fly, Hey/Listen/Watch out/call, NA_HELLO_2) + Skull Kid laugh | 0x800F738C → 0x80602D00 (IDs & 0xF1FF) | code_800F7260.c `MlAudio_IsMuted` |
| Dispersed sparkles off (`EffectSsKiraKira_SpawnDispersed` returns at once) | 0x80028BB0 `jr $ra` | z_effect_soft_sprite_old_init.c |
| Wall push-out skipped for 2 frames after a room/scene change, and for the next 2 calls after the Deep Woods corridor teleport | 0x8002E5CC → 0x80602C80; counters 0x80609FF0 (0x80600E80) / 0x80602038 | new `code/ml_state.c`, z_actor.c, Ml_FogWall, z_player.c |
| Area title cards: ML ribbon (custom-code tex +0x3BE0, 160x8 IA16) tinted per scene + white title at (88,68); boss slot 0x18 vanilla | 0x800319A8 → 0x806037F0 | z_actor.c `MlTitleCard_Draw`, `ml_titlecard_banner.inc` |
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
glyphs differ), Link object DL/animation-ROM pointers (with the Link object port). Open: ML's HUD restyle (B/C button
backs 0x80604E30 with alpha cap 125, key counter 0x806053E0, colour at 0x80089180) and dungeon minimap texture index
(room + 190, 0x80080D20).
