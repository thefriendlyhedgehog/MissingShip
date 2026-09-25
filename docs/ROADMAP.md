# MissingShip roadmap

The team plan: what is still left to port, in what order, and the issues to work through.
Last full review: **2026-09-25**, with every ML file compared against PAL MQ Debug.

- **Detail lives elsewhere:**
  - `ML_ACTOR_INVENTORY.md`: per-actor status.
  - `ENGINE_CHANGES.md`: every engine change and its drop-in path.
  - `WORKING_NOTES.md`: handoff notes and key addresses.
  - `PORTING_ACTORS.md`: how to port an actor.
- **Updating this file:**
  - Update this file when an issue closes or a new gap is found.
  - Issues are tracked on **GitHub Issues** (title prefix `MS-nnn`, one milestone each). The tables below are the review snapshot; new issues get the next free ID in both places.
  - Close an issue on GitHub from the fixing commit (`Fixes #N`) and move its row to *Done*.
  - Never reuse an ID.

## How we work

- **One issue per change.** Put the issue ID in the commit subject, e.g. `MS-014: port En_Nutsball`.
- **The ledger rule.** Any engine change updates `ENGINE_CHANGES.md` in the same commit. An actor change also updates `ML_ACTOR_INVENTORY.md`.
- **Regenerate the patch.** After engine work, regenerate `docs/soh-missingship.patch` (`PORTING_ACTORS.md` step 6) and check that it still applies to SoH `cb71e22`.
- **Tested vs. built.** "Ported" means built and deployed. "Verified" means played in-game. Record which one applies in the inventory.
- **Port ML's actual code.** A port must follow ML's code, recovered by diff or disassembly. Guessed behaviour must be marked as a guess.
- **Order of preference for a fix:**
  1. data;
  2. a generic SoH feature;
  3. an upstreamable bugfix;
  4. ML-specific C, as a last resort.

---

## Review: what is still unported (2026-09-25)

**Method:** every ML dma file was hashed against the debug ROM.
- **257** ML files have no identical debug file:
  - 81 actor overlays (some differ only by relocation);
  - 34 objects;
  - about 107 scene and room files, which the pipeline already handles (25 scenes, 72 rooms);
  - about 35 system files.
- ML **deleted** 261 vanilla objects and many vanilla actors (Hookshot, Epona, …). These need no work unless something spawns them.

### A. Actors placed in ML scenes (26 todo + 4 partial)

| Scene | Unported actors |
|---|---|
| `spot01` (Kakariko slot) | En_Dodojr ×4, Bg_Toki_Hikari, En_Rd ×6, Bg_Po_Event ×2, Bg_Spot02_Objects ×3, Bg_Spot16_Bombstone, En_Daiku ×2, En_Hs, Obj_Makekinsuta |
| `night_shop` | En_Ossan (shopkeeper; also spawns En_GirlA, see B), En_Rd, Bg_Po_Event |
| `hairal_niwa2` | Bg_Po_Event ×2 |
| `HAKAdan` (dungeon) | **Boss_Tw ×26** (ML reuses the slot for something small: 0x490 bytes vs 0x11AE0), ACTOR_UNSET_1A (ML custom), Bg_Hidan_Dalm, Bg_Mori_Elevator, En_Floormas, En_Kakasi ×2, En_Ssh ×4, Bg_Mizu_Shutter |
| `HAKAdan_bs` (boss room) | **Boss_Va**: the whole boss fight, see C |
| `Bmori1` (dungeon) | En_Peehat, Boss_Dodongo ×2, En_Du ×3, En_Nb ×3, Bg_Jya_1flift |
| `spot00` | En_Mag (title logo) |
| `kakariko3` | Boss_Ganon2 |
| Partial | En_Fz / Ml_EventSpawner (only modes 2, 5, 6, 7), En_Niw_Lady (certificate hand-in only), En_Hy (certificate offer only), En_Ko (shares Mido's hook 0x80602200; not done) |

### B. Changed actors that are not placed but are spawned at runtime

The old inventory only surveyed placed actors, so it missed these. A spawn scan of `code`, the player overlay, the custom code and every overlay ties them to gameplay.

| Actor | Change | Reached from |
|---|---|---|
| **En_Elf (Navi)** | 4018 text words + hook `0x80602000` | Spawned by `code`: always present. Only her sounds are ported. |
| En_Nutsball (Deku nut projectile) | rewritten (0x590 vs 0x550) | Deku Scrubs, already ported |
| En_GirlA (shop items) | 8 data words (prices / items?) | En_Ossan |
| Door_Warp1 (blue warp) | 5 text words | Boss_Va |
| En_Vm (Beamos) | 1 data word; object DLs changed | Boss_Va (spawned as child) |
| En_Bom_Chu | 974 text words + hook `0x80607A00` | Player (Bombchus exist; cap 20 is ported) |
| Item_Shield | 1 text word | Player (dropped or burnt shield) |
| Bg_Dy_Yoseizo, En_Dodongo (0x12), Bg_Haka_Huta | rewritten | spawner not found yet |
| En_Ishi (rocks), En_Karebaba, Obj_Ice_Poly, En_GirlA | data / 1 word | spawner not found yet |

### C. ML custom actors in slots vanilla leaves empty

The 3, 5, 6 and 0x1F slots are ported. Still to do:

| Slot | Size | Category / object | Notes |
|---|---|---|---|
| 0x01 | 0x4AE0 | itemaction / gameplay_keep | spawned by Boss_Va (can also spawn Navi) |
| 0x17 | 0x470 | prop / object_bv (0x8C) | spawned by Boss_Va |
| 0x1A | 0x2B0 | bg / gameplay_keep | placed in HAKAdan |
| 0x22 | 0xE90 | itemaction / gameplay_keep | spawned by Boss_Va |

**Boss fight bundle:**
- Boss_Va (0x2C00 bytes, a full rewrite);
- object_bv (213 KB, heavily changed);
- custom actors 0x01, 0x17 and 0x22;
- En_Vm and Door_Warp1.

### D. Objects not yet ported

| Object | What ML changed | Kind of work |
|---|---|---|
| object_link_child (0x15) | 35,836 words; Link's model | big: the "Link object port" |
| object_bv (0x8C) | boss model | with Boss_Va |
| object_tw, object_nb, object_du, object_mag, object_ganon2, object_ka, object_spot02_objects, object_mori_objects | redrawn for their rewritten actors | with each actor |
| gameplay_keep | sword-turn animations `link_fighter_turn_kiruL_end` / `kiruR` / `kiruR_end` + unused shape DLs; drop-bomb texture and grotto hole are done | data (animations) |
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
| **Player overlay** (dma 102) | 88 changed words ≈ 20 distinct edits, **mostly unported**: 30 `jal 0x806023E0` Link-animation overrides; fairy-release spawn replaced by a flag (+0x38CC); a second spawn removed (+0x74CC); a scene-gated music/sfx block (+0x7100); hooks into custom code at 0x80606BA0, 0x80606BC8, 0x80600480, 0x80600D90; Magic Hourglass `j 0x80600E00` (+0x17E5C); 4 forced branches; 3 constant changes (+0x6104, +0xA080, +0x14E20 0x78→0x50, +0x167E0). The Blade Beam works through a different route. |
| Pause menu (dma 101) | 46 words; mostly ported (ledger row 24). Recheck the cursor. |
| Title screen (dma 97) | ML's own title: 0x1170 text + 76 KB of data (images). Not ported; SoH boots to file select. |
| File select (dma 100) | 1 forced branch (+0xC9D8). Not ported. |
| Opening (dma 99) | 3 words: new-game / attract-mode entrance and cutscene index. Not ported. |
| map_mark_data (dma 103) | 58 data words: dungeon-map chest / boss markers. **Not ported.** |
| map_i_static (dma 113), map_48x85_static (dma 112) | redrawn dungeon minimaps and pause dungeon maps. **Not ported** (the minimap index is room + 190). |
| staff_message_data_static (dma 110) | ML's credits text. Not ported. |
| Skyboxes | `vr_fine1_static` and `vr_cloud2_static` (+ palettes) redrawn. Not ported. |
| link_animetion (dma 146, 2.5 MB) | Link's animation data changed. Not ported (goes with the Link port). |
| Entrance flags | 39 SoH entrances used by ML have different title-card / music / transition flags. Title cards are fixed; music and transitions are not. |

### F. ML custom code (dma 507) still unported

- Magic Hourglass (0x80600E00, 0x80601000, 0x80601680, actor 0x18A).
- Soul-Charged sword model and glow (patches object_link_child).
- Link animation swap 0x806023E0.
- HUD restyle: 0x80604E30 (B/C buttons, alpha cap 125), 0x806053E0 (key counter), colour at 0x80089180.
- Navi 0x80602000.
- Bombchu 0x80607A00.
- Player hooks 0x80606BA0, 0x80606BC8, 0x80600480, 0x80600D90.
- The code past 0x80608000 (0x8064xxxx, 0x807Cxxxx) has not been located yet.

---

## Gameplan

Milestones, in order. Each milestone lists its issue IDs.

1. **M0: Team footing** (MS-001 … MS-008)
   - Anyone can check out, build and run.
   - The docs tell the truth.
   - No hardcoded paths.
2. **M1: Playtest what is built** (MS-010)
   - Turn the untested batches into "verified" or into bugs, before building more on top of them.
3. **M2: Data-only quick wins** (MS-020 … MS-027)
   - Textures, maps, credits and skyboxes, all through the pipeline with no engine code.
   - Also moves the drop-in goal forward.
4. **M3: Main path, scene by scene** (MS-030 … MS-049)
   - Finish every actor in a scene before moving on, so each scene can be fully verified.
   - Order: the areas already started (`spot04`, `spot09`, `spot10`, `spot12`, `ydan`), then `spot01` + `night_shop` + `hairal_niwa2`, then `HAKAdan` + its boss, then `Bmori1`, then `kakariko3` / `spot00`.
5. **M4: Link** (MS-050 … MS-055)
   - object_link_child, link_animetion, the kiru animations, the player overlay edits, the Soul-Charged sword visuals and the Magic Hourglass.
   - These are tangled together, so they are done as one project.
6. **M5: Front end and polish** (MS-060 … MS-063)
   - Title screen, file select, opening, HUD restyle, entrance music / transition flags.
7. **M6: Drop-in path** (MS-070)
   - Move every ledger row that can be moved to data or to a generic upstream feature.
8. **Platforms** (MS-080 … MS-083)
   - Windows and Linux builds, like upstream SoH.
   - Long-term; macOS is the test platform until then.
9. **Porting kit** (MS-090 … MS-093)
   - Turn this repo's method and tools into a reusable way to port other OoT romhacks to the SoH engine, until there is a ready-made porting option or the hacks are all done.

---

## Issues

Type: **bug** = something wrong in what we ship; **port** = ML content not yet in; **infra** = tooling / docs / process; **platform** = Windows / Linux support.

### M0: Team footing

| ID | Type | Issue | Done when |
|---|---|---|---|
| [MS-001](https://github.com/thefriendlyhedgehog/MissingShip/issues/1) | infra | 28 scripts in `tools/` and `pipeline/` hardcode `/Volumes/MacMiniExternal/...` (ROM paths, SoH checkout, ZAPD binary, output paths). | One config (env vars or a `paths.py` / `paths.sh` resolved from the repo root), with the defaults documented; the tools run from a fresh clone. |
| [MS-002](https://github.com/thefriendlyhedgehog/MissingShip/issues/2) | infra | `cycle.sh` and `ms_cycle.sh` are near-duplicates. Every ML_OBJECTS edit has to be made twice, and they will drift. | One script (or a shared `ml_objects` table both read). |
| [MS-003](https://github.com/thefriendlyhedgehog/MissingShip/issues/3) | infra | The ledger is behind the code. There are no `ENGINE_CHANGES.md` rows for Ml_BeamHeart, Ml_SkullKid, Ml_DnsShop, Ml_HoleScrub, En_Weather_Tag, En_Wonder_Talk, Obj_Oshihiki, En_Cow, En_St / En_Sw, Obj_Bombiwa, Bg_Hidan_Kowarerukabe. Ml_Malon and Ml_Cucco were filed under "ML `code` patches" instead of the numbered actor table. Ml_DnsShop is missing from the inventory. | Every repointed slot and every edited vanilla actor has a numbered row. |
| [MS-004](https://github.com/thefriendlyhedgehog/MissingShip/issues/4) | infra | `README.md` is stale. It says audio is vanilla (it's ported), that "36 custom actor overlays and 18 custom objects" are unported (wrong numbers), and that cutscenes don't work (entrance intros are ported; overall status unverified). Its "engine change" section describes only the Metal fix. | The README matches this review and points here. |
| [MS-005](https://github.com/thefriendlyhedgehog/MissingShip/issues/5) | infra | `ML_ACTOR_INVENTORY.md` only surveys *placed* actors. | Regenerated from the full scan: B and C added, and a *Verified* column added. |
| [MS-006](https://github.com/thefriendlyhedgehog/MissingShip/issues/6) | infra | `modcheck.py` only reports hooks below 0x80608000, and several of the "17 hooked overlays" in the ledger (0x09E, 0x0F4, 0x11B, 0x12E, 0x141, 0x173, 0x18A, 0x18B, 0x198, 0x199) are not actor slots with hooks in the current scan. | The hook list is re-derived across all overlay tables (actor, effect, gamestate) and all target ranges, and the ledger is corrected. |
| [MS-007](https://github.com/thefriendlyhedgehog/MissingShip/issues/7) | infra | Engine changes live only as a 350 KB patch; code review and merges are painful. | Decide: a private Shipwright fork with a `missingship` branch (patch generated from it) vs. the patch-only workflow. |
| [MS-008](https://github.com/thefriendlyhedgehog/MissingShip/issues/8) | infra | No automated checks. | A script that runs `checkexits.py`, `checkhashes.py`, the patch-applies check and a build; run it before every push (or in CI where ROMs are available). |

### M1: Verify

| ID | Type | Issue | Done when |
|---|---|---|---|
| [MS-010](https://github.com/thefriendlyhedgehog/MissingShip/issues/9) | bug? | Playtest everything built but not yet verified: title cards, Navi muted, text speed, save resume location, pause item page + old-save migration, bean seller (Bunny Hood), Deku Road Skull Kid + Blade Beam heart, grotto scrub shop, hole-hopping scrub (ydan room 2), readable notes (Kokiri house, kakariko3), Deep Woods wall-skip, Malon's race + ML cucco. | Each item marked verified in the inventory or filed as its own bug. |
| [MS-011](https://github.com/thefriendlyhedgehog/MissingShip/issues/10) | bug | Hard freeze pulling a crate in `malon_stable` room 3 (Bg_Spot15_Rrbox ×4, params 0). Suspected out-of-bounds read of `D_808B45C4` when switch 0 is set. Possibly a place the player isn't meant to reach yet. | Reproduced, root-caused, fixed or proven unreachable. |
| [MS-012](https://github.com/thefriendlyhedgehog/MissingShip/issues/11) | bug? | "Deep Woods sometimes doesn't load the next zone." Probably fixed by the wall-skip port (ML's 0x80602C80). | Confirmed fixed in play, or reopened with a repro. |
| [MS-013](https://github.com/thefriendlyhedgehog/MissingShip/issues/12) | port | Where is Malon's race? The finish-line Malon (params 0xFFFF) and ML entrances 0x138 / 0x13C aren't placed on the scene map yet. | The race is located and played start to finish. |

### M2: Data-only quick wins (pipeline, no engine code)

| ID | Type | Issue | Done when |
|---|---|---|---|
| [MS-020](https://github.com/thefriendlyhedgehog/MissingShip/issues/13) | port | Texture aliases (like `inject_objects.py` ALIASES): push blocks (gameplay_dangeon_keep), chests (object_box, plus the curve skeleton), bomb-flower leaves, boss door, haka door, `g1f1fiftTopTex`. | ML textures shown under the vanilla paths. |
| [MS-021](https://github.com/thefriendlyhedgehog/MissingShip/issues/14) | port | Beamos (object_vm) body and eyelid DLs, Saria right-hand limb (object_sa). | DL aliases shipped. |
| [MS-022](https://github.com/thefriendlyhedgehog/MissingShip/issues/15) | port | Dungeon minimaps and pause dungeon maps (map_i_static dma 113, map_48x85_static dma 112), plus ML's minimap index (room + 190, 0x80080D20). | ML's maps show in ydan / HAKAdan / Bmori1. |
| [MS-023](https://github.com/thefriendlyhedgehog/MissingShip/issues/16) | port | map_mark_data (dma 103, 58 data words): dungeon-map chest / boss markers. | Markers match ML. |
| [MS-024](https://github.com/thefriendlyhedgehog/MissingShip/issues/17) | port | Credits text (staff_message_data_static, dma 110). | Injected as a resource. |
| [MS-025](https://github.com/thefriendlyhedgehog/MissingShip/issues/18) | port | Skyboxes `vr_fine1` and `vr_cloud2` (+ palettes; ML dma 128/129, 138/139). | Injected. |
| [MS-026](https://github.com/thefriendlyhedgehog/MissingShip/issues/19) | port | gameplay_keep sword-turn animations (`kiruL_end`, `kiruR`, `kiruR_end`). | Injected under the vanilla names (check how they interact with Link's animations, MS-051). |
| [MS-027](https://github.com/thefriendlyhedgehog/MissingShip/issues/20) | port | Entrance flags: 39 entrances with ML's music-continuity / transition-style flags. | A data override, or a small engine table plus a ledger row. |

### M3: Actors, by scene

| ID | Type | Issue | Done when |
|---|---|---|---|
| [MS-030](https://github.com/thefriendlyhedgehog/MissingShip/issues/21) | port | **En_Elf (Navi)**: 4018 words + custom hook 0x80602000. She's in every scene, so this comes first. | Behaviour diffed and ported. |
| [MS-031](https://github.com/thefriendlyhedgehog/MissingShip/issues/22) | port | En_Nutsball (rewritten), fired by the Deku Scrubs we already ship. | Ported, and the scrubs verified with it. |
| [MS-032](https://github.com/thefriendlyhedgehog/MissingShip/issues/23) | port | En_Ko: Kokiri text swaps + texture patch (hook 0x80602200, shared with Mido). | Ported. |
| [MS-033](https://github.com/thefriendlyhedgehog/MissingShip/issues/24) | port | Finish the partial actors: En_Fz / Ml_EventSpawner (remaining modes), En_Niw_Lady, En_Hy. | Full behaviour. |
| [MS-034](https://github.com/thefriendlyhedgehog/MissingShip/issues/25) | port | `spot01`: En_Dodojr, Bg_Toki_Hikari, En_Rd, Bg_Po_Event, Bg_Spot02_Objects (+ object_spot02_objects), Bg_Spot16_Bombstone, En_Daiku, En_Hs, Obj_Makekinsuta. | One issue per actor when started. |
| [MS-035](https://github.com/thefriendlyhedgehog/MissingShip/issues/26) | port | `night_shop`: En_Ossan + En_GirlA shop items (8 data words: prices?). | The shop works as in ML. |
| [MS-036](https://github.com/thefriendlyhedgehog/MissingShip/issues/27) | port | `HAKAdan`: Boss_Tw slot ×26 (small ML rewrite + object_tw), ACTOR_UNSET_1A (custom), Bg_Hidan_Dalm, Bg_Mori_Elevator (+ object_mori_objects), En_Floormas, En_Kakasi (+ object_ka), En_Ssh, Bg_Mizu_Shutter. | The dungeon can be completed. |
| [MS-037](https://github.com/thefriendlyhedgehog/MissingShip/issues/28) | port | **Boss fight** (`HAKAdan_bs`): Boss_Va + object_bv + custom actors 0x01, 0x17, 0x22 + En_Vm + Door_Warp1. | The boss can be beaten and the warp works. |
| [MS-038](https://github.com/thefriendlyhedgehog/MissingShip/issues/29) | port | `Bmori1`: En_Peehat, Boss_Dodongo (+ rewritten En_Dodongo 0x12?), En_Du (+ object_du), En_Nb (+ object_nb), Bg_Jya_1flift. | The dungeon can be completed. |
| [MS-039](https://github.com/thefriendlyhedgehog/MissingShip/issues/30) | port | `kakariko3`: Boss_Ganon2 (+ object_ganon2). `spot00`: En_Mag (+ object_mag). | Ported. |
| [MS-040](https://github.com/thefriendlyhedgehog/MissingShip/issues/31) | port | Find the spawners of Bg_Dy_Yoseizo, En_Dodongo, Bg_Haka_Huta, En_Ishi, En_Karebaba, Obj_Ice_Poly, Item_Shield, En_Bom_Chu (hook 0x80607A00). | Each one ported, or recorded as unreachable. |

### M4: Link

| ID | Type | Issue | Done when |
|---|---|---|---|
| [MS-050](https://github.com/thefriendlyhedgehog/MissingShip/issues/32) | port | object_link_child (35,836 words changed) and link_animetion (dma 146). | ML's Link model and animations in game. |
| [MS-051](https://github.com/thefriendlyhedgehog/MissingShip/issues/33) | port | Link animation override 0x806023E0 (30 call sites in the player overlay, 5 in `code`). | Ported. |
| [MS-052](https://github.com/thefriendlyhedgehog/MissingShip/issues/34) | port | The other player-overlay edits (see E): fairy-release flag, removed spawn, scene-gated audio block, custom hooks 0x80606BA0 / 0x80606BC8 / 0x80600480 / 0x80600D90, forced branches, constants. | Each one identified and ported. |
| [MS-053](https://github.com/thefriendlyhedgehog/MissingShip/issues/35) | port | Magic Hourglass: ring-buffer rewind (0x80600E00 / 0x80601000 / 0x80601680) + actor 0x18A. Right now the item throws a vanilla boomerang. | The item works as in ML. |
| [MS-054](https://github.com/thefriendlyhedgehog/MissingShip/issues/36) | port | Soul-Charged sword model and glow (actor 0x57 params 0xBEEF). Needs MS-050. | Visible on Link. |
| [MS-055](https://github.com/thefriendlyhedgehog/MissingShip/issues/37) | infra | Locate ML code past 0x80608000 (0x8064xxxx, 0x807Cxxxx). | Mapped, and its features added to this list. |

### M5: Front end and polish

| ID | Type | Issue | Done when |
|---|---|---|---|
| [MS-060](https://github.com/thefriendlyhedgehog/MissingShip/issues/38) | port | HUD restyle: B/C button backs (0x80604E30, alpha cap 125), key counter (0x806053E0), colour 0x80089180. | Matches ML. |
| [MS-061](https://github.com/thefriendlyhedgehog/MissingShip/issues/39) | port | Title screen (dma 97, ML's own). Decide whether it's in scope next to SoH's boot flow. | Decided; ported or recorded as out of scope. |
| [MS-062](https://github.com/thefriendlyhedgehog/MissingShip/issues/40) | port | File select (1 edit) and opening (3 words: new-game entrance / cutscene index). | Ported. |
| [MS-063](https://github.com/thefriendlyhedgehog/MissingShip/issues/41) | port | Recheck the pause-menu quest-status cursor (a misalignment was reported before row 24). | Verified. |

### M6: Drop-in

| ID | Type | Issue | Done when |
|---|---|---|---|
| [MS-070](https://github.com/thefriendlyhedgehog/MissingShip/issues/42) | infra | Go through `ENGINE_CHANGES.md` and turn each row that can move into a data-side or upstream proposal: Metal texture guard, bgcheck NULL guard, Oceff widescreen fix (bugfixes); entrance table, cutscene / grotto / new-save tables, draw configs (data-driven features). | A proposal list, and the bugfix PRs upstream. |

### Platforms (long-term; macOS is the test platform for now)

| ID | Type | Issue | Done when |
|---|---|---|---|
| [MS-080](https://github.com/thefriendlyhedgehog/MissingShip/issues/43) | platform | Windows build of the patched engine (MSVC / SoH's Windows CMake presets) and a portable package (exe + `soh.o2r` + mod `.o2r`). | Boots to ML's start area on Windows. |
| [MS-081](https://github.com/thefriendlyhedgehog/MissingShip/issues/44) | platform | Linux build (SoH's Linux CMake / AppImage route) and package. | Boots to ML's start area on Linux. |
| [MS-082](https://github.com/thefriendlyhedgehog/MissingShip/issues/45) | platform | Cross-platform pipeline: shell scripts (`cycle.sh`, `ms_cycle.sh`, `MissingShip.sh`) into Python or add per-OS equivalents; ZAPD binary per platform; no macOS-only commands (`sed -i ''`, `pkill`, `open`). Depends on MS-001. | The mod archive builds on all three OSes. |
| [MS-083](https://github.com/thefriendlyhedgehog/MissingShip/issues/46) | platform | Check the engine patches for Mac-only paths: the libultraship fix is in `gfx_metal.cpp` only — confirm whether the DX11/OpenGL backends need the same zero-size texture guard. | Each backend reviewed; guards added where needed. |

### Porting kit (long-term)

| ID | Type | Issue | Done when |
|---|---|---|---|
| [MS-090](https://github.com/thefriendlyhedgehog/MissingShip/issues/47) | infra | Generalise the tools: baseline ROM, hack ROM, code dma index, VRAM base and table offsets come from a per-hack config instead of ML constants. | `tools/` runs against a second hack with only a new config. |
| [MS-091](https://github.com/thefriendlyhedgehog/MissingShip/issues/48) | infra | Knowledge base: document the method (dma/object/actor hash survey → per-instruction diff → "recompiled vanilla with edits" ports → custom-code-region hunting → ZAPD object XMLs) and the reusable facts about OoT romhack layouts. | A `docs/porting-kit/` guide a new contributor can follow for a new hack. |
| [MS-092](https://github.com/thefriendlyhedgehog/MissingShip/issues/49) | infra | Per-hack workspace layout (`hacks/<name>/` with its config, XMLs, maps, ledger, inventory) so several hacks can share the pipeline and tools. | ML moved into the layout without breakage. |
| [MS-093](https://github.com/thefriendlyhedgehog/MissingShip/issues/50) | infra | Survey script: one command that produces the ROADMAP-style review (changed actors placed/spawned, custom slots, objects, system files, custom code hooks) for any hack. | The ML review regenerates from it. |

### Done

- MS-004 — README corrected (this commit) — rewritten with the current status, platforms and goals.

(Move closed issues here: `MS-nnn — commit — one line`.)
