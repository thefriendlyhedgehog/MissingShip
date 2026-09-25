---
name: ml-custom-actors-actordb
description: "ML's real custom actors hide in vanilla's UNSET actor slots (not high ids); which 5 are actually placed, what each needs, and the verified table offsets in both ROMs"
metadata:
  node_type: memory
  type: project
  originSessionId: c90fe9a6-ad2e-4ae3-ba84-7caccc4ca425
  modified: 2026-09-23T18:40:51.969Z
---

**ML's custom actors occupy vanilla's `DEFINE_ACTOR_UNSET` slots** — low ids, not high ones. ML fills
all 8 unset slots (0x001, 0x003, 0x005, 0x006, 0x017, 0x01A, 0x01F, 0x022) with real overlays. SoH
has no implementation for any of them, so they silently fail to spawn. See [[missing-link-soh-port]].

**5 of the 8 are actually placed in rooms** (scan of 25 scenes / 72 rooms; the other 3 must be
spawned by code or cutscene):

| actor | placed | where | cat | object | object status |
|-------|--------|-------|-----|--------|---------------|
| 0x003 | 13x | turibori_r0 (slot 73) | BG | 0x40 | **PORTED + VERIFIED IN-GAME 2026-09-23** (ML replaced, 54064B) |
| 0x006 | 2x (not 4: alt headers double-count) | spot01_r0 Kakariko (slot 82) | NPC | 0x0C | **PORTED + VERIFIED IN-GAME 2026-09-23** as `Ml_Villager` (after the palette fix below) |
| 0x005 | 2x | spot04_r0 Kokiri (slot 85) | BG | 0x7C | ML modified (same 34768B, differs) |
| 0x01A | 1x | HAKAdan_r20 | BG | 0x01 gameplay_keep | ML modified; **no draw fn** |
| 0x01F | 1x | link_home_r0 (slot 52) | BG | 0x04 | **ML-only object**, 153760B, UNSET in vanilla |

**Every one needs its custom object ported too**, not just C code — ML replaced the object in each
slot, which is why ML's display-list offsets (e.g. 0x978/0x2878/0x3068/0x3728 for actor 0x003) do not
match vanilla's `object_jj.xml` (0x44A0–0x5690). ML also modified `gameplay_keep` itself.
Only 124 of 402 object slots are present in ML at all.

**Port design (implemented for 0x003; full recipe in `MissingShip/docs/PORTING_ACTORS.md`):** flip the slot in
`soh/include/tables/actor_table.h` from `DEFINE_ACTOR_UNSET(ACTOR_UNSET_3)` to a real
`DEFINE_ACTOR(...)` and add the C overlay. Actor ids stay byte-identical, so no scene data has to be
rewritten and nothing downstream shifts. Prefer this over `ActorDB::AddEntry`, which appends at
`nextFreeId` and cannot target a chosen id. (ActorDB is still the tool if a *new* id is ever needed;
worked precedent is `En_Partner`/"Ivan" in `ActorDB.cpp:599-616`.)

**Verified table offsets — derived, not guessed (each confirmed by an independent anchor):**

| table | ML (`code` = dma 114) | PAL GC Debug (`code` = dma 28) |
|-------|----------------------|-------------------------------|
| `gActorOverlayTable` | **0xF9440**, stride 0x20 | **0xF9460**, stride 0x20 |
| `gObjectTable` (RomFile, 8B) | **0x10A6C8** | **0x10A6E8** |

ML sits exactly 0x20 below the debug ROM in both tables. Anchors used: every overlay's own
`ActorInit.id` equals its table index (163 matches, 0 at the rival base); object slot 1 is
`gameplay_keep` at 354224B in both, and `object_jj`'s vrom appears exactly once in debug's code, at
slot 0x40. **Do not trust a "longest run" heuristic for the object table** — it anchors on slot 1 and
comes out 8 bytes late, and a run that counts (0,0) entries anchors in a zero field entirely.

**Baseline ROM for all of this: `/Volumes/MacMiniExternal/Games/SHIPofHarkinian-ROMS/PAL GC (Debug).z64`**
(md5 3c10b67a76616ae2c162def7528724cf, header CRC 0x871E1C92 — the CRC the pipeline already spoofs).
ML is built on the debug layout, so this is the correct baseline, *not* the NTSC 1.0 USA ROM in the
same folder. Its filelist is SoH's own `soh/assets/extractor/filelists/dbg.txt` (1532 entries,
matching its 1532 dma entries) — use it to resolve any debug dma index to a real name.

Overlay diffing recipe that works: the last word of an overlay file is the distance back to its
`OverlayRelocationSection` {textSize, dataSize, rodataSize, bssSize, nRelocations, relocs[]}; mask
each relocated word by type (2=R_MIPS_32 whole word, 4=R_MIPS_26 low 26 bits, 5/6=HI16/LO16 low 16)
before comparing, since ML's overlays are relocated 0x80 lower than the debug ROM's.

Disassembly: `pip install capstone` works; use `Cs(CS_ARCH_MIPS, CS_MODE_MIPS32|CS_MODE_BIG_ENDIAN)`.
Do not name the script `dis.py` — it shadows the stdlib module capstone imports.

**Registering an actor takes three edits, not one.** Besides `actor_table.h`, you must add a
description in `ActorDB.cpp` (`actorDescriptions` is an `unordered_map`; a missing key yields
`nullptr` and `AddEntry` builds a `std::string` from it -- UB, not a blank name), and remove the
now-stale `ACTOR_UNSET_n` from the exclusion list in
`soh/soh/Enhancements/debugger/actorViewer.cpp` or the build fails on an undeclared identifier.
Removing it is also what you want: the Actor Viewer can then spawn the actor anywhere, which beats
walking to its scene to see it. CMake globs sources at configure time, so a new actor directory
needs `cmake -S . -B build-ml` before the build picks it up.

**Verifying a ported actor in-game (worked for 0x003):** enable `gDeveloperTools.DebugEnabled` and
`gOpenWindows.ActorViewer` in `MissingShip/home/shipofharkinian.json` **with the game stopped** (SoH
rewrites the file on exit), set the viewer's ID, tick **Actor Name Tags**, and spawn. The tag shows the
ActorDB name (`Ml_Fishpond`), which proves registration independently of whether anything draws.
Ported actors render in ANY scene: ZAPD rewrites segment-6 references into absolute resource paths,
so the original object bank does not need to be loaded. Viewer spawns are not saved.

**computer-use background clicks do not reach SoH's ImGui.** `app_click` reports ok but ImGui
ignores input unless the window has focus (a params `+` click left the value unchanged). Either the
user clicks, or take full-screen control -- which steals focus from their other apps, so ask first.
Screenshots via `app_screenshot` work fine in the background.

**0x006 (`Ml_Villager`) facts:** 10-limb FLEX skeleton @0x5760 (dListCount 7), 30-frame idle @0x56A4,
object at ML dma 356. Self-kills unless `gSaveContext.sceneFlags[SCENE_FOREST_TEMPLE].swch` bit 7 is
set (ML builds Bmori1 in that slot). Text id rides in placement rot.z (0x605F, 0x6063 -- both already
shipped by our message port). Tint via env colour from params. One CI8 texture (0xC00) had to be
declared by hand -- see PORTING_ACTORS.md on `texDecl == nullptr`.

**Open lead for the scene port:** `build_rom_inline.py` sizes textures from `G_SETTILESIZE`, so it has
the same blind spot as ZAPD -- a render tile with no SETTILESIZE of its own inherits the previous
size and goes unrecorded. That may explain some entries in `pipeline/unresolved.json` and any
remaining odd scene textures. Not yet checked.

**Villager "giant tan pyramid" bug (fixed):** ZAPD turned a 256-entry `G_LOADTLUT` at 0x2180 into a
512-byte palette resource that swallowed the root limb's vertex array at 0x21B8; ZAPD binds an address
to the FIRST declaration containing it, so 30 torso vertices were read from palette bytes. Fix: declare
the palette at its true size (28 entries -- exactly the room before the vertices, and exactly max index
27 + 1). Diagnosed by elimination, each step proven: ML's flex draw code is byte-identical to vanilla
(only +0x20 data shifts), DL/G_MTX encoding matches a working vanilla flex object, skeleton header and
animation are byte-identical to ML's raw data, all 102 DL hashes resolve -- then found the one VTX hash
resolving to an OTEX. **Check to run on every new object: every G_VTX_OTR_HASH must resolve to an OARR.**
Also: `inject_objects.py` was append-only and silently kept stale resources after an XML fix; it now
rewrites the archive with the regenerated objects' old entries dropped.

## ML modifies VANILLA actors' logic (birth certificate case, 2026-09-23)
ML changes behaviour inside vanilla actor overlays, which SoH cannot see (it runs its own C). Found the
technique that works: an instruction-aligned diff is useless because ML's overlays were rebuilt with a
different compiler (delay slots filled differently), so diff SEMANTICS instead -- call-target counts
(cancel the uniform 0x20 code shift), constants present on one side only, and data-section halfwords.
- **En_Hy (0x16E):** text-state DONE_FADING case changed from 0x709E/0x709F (lost-dog chime) to
  0x709D/0x709F offering `GI_STONE_OF_AGONY` (120/10) unless held or `eventChkInf` flag 0x08 set.
  Ported as a patch to `z_en_hy.c` -- VERIFIED IN-GAME 2026-09-23 (user received it; shows in pause menu).
- **ML reskins vanilla items as quest items:** Stone of Agony = "Birth Certificate" (get-item text
  0x0068 rewritten). Pause-menu icon/name still show Stone of Agony unless ML's icon/name textures are
  ported (not done).
- **En_Niw_Lady (0x13C, Anju) is the Cucco Lady.** ML deleted vanilla's pen-scanning cucco count in
  func_80ABA244 and patched in place: holding the Stone of Agony (and flag 0x08 unset) sets
  cuccosInPen = 7, reusing vanilla's whole reward path (text 0x503B -> func_80ABA654). In
  func_80ABA654 it offers GI_HEART_PIECE (not GI_BOTTLE) and sets eventChkInf flag 0x08. Once 0x08 is
  set, ML overwrites sMissingCuccoTextIds[0] with 0x503D ("Cuccos are the best!"). PORTED to
  z_en_niw_lady.c 2026-09-23 (built; in-game test pending). Two deliberate deviations: the 0x503D line
  is selected per-frame instead of mutating the static table (SoH statics live all session and would
  leak into other saves), and getItemId is stored as GI_HEART_PIECE (ML leaves GI_BOTTLE, which
  func_80ABAC00 would re-offer on a missed frame). The certificate is never taken from inventory --
  ML doesn't either.

## Vanilla behaviour misfiring in ML scenes (2026-09-23)
- **Entrance intro cutscenes:** ML replaced vanilla's 34-entry `sEntranceCutsceneTable` (debug code
  0x1013B4) with its own 11 (ML code 0x101394). SoH's vanilla table fired Gerudo Valley's flyover in ML's
  Deku Road (0x117) and Gerudo Fortress's in ML's Deku Grove (0x129) -- camera zoomed out and clipping.
  Fixed: z_demo.c now holds ML's 9 valid entries (0x420 = stale all-zero pointer, 0x503 = unported
  scene), entrances remapped like exits, scripts exported via `ENTRANCE_CS` in gen_xml_inline.py.
  8 of the 9 were already exported under ZAPD auto-names because scene headers point at them.
- **Bg_Treemouth (0x03E):** ML empties init/destroy/draw (2336 -> 560B). Vanilla's version ran the
  Kokiri meeting (flag 0x0C, text 0x107D) inside ML's Deep Woods and bounced the player out. Fixed by
  Actor_Kill in Init. ML's cosmetic particle spawner (calls 0x800297A4) not ported.
- **Obj_Lift (0x12C) -- the "blocks sunk in the ground":** ML replaced BOTH code (624 vs 2256B) and object
  0x11D (2080 vs 6144B). Not yet ported.

**Full survey of placed vanilla actors vs ML: `MissingShip/docs/placed_actor_status.json`.** Read it
carefully: "object CHANGED 354224B vs 354224B" is gameplay_keep, which ML edited somewhere -- it does NOT
mean that actor's model changed. Strong signals are large code-size deltas or objects of a different
size (Boss_Tw 26x in HAKAdan: -71KB code, object 10KB vs 241KB -- a total repurpose). Small +-16B deltas are
usually compiler noise. Same-size code can still hide a semantic change (En_Hy was -48B), so run
`tools/semdiff.py` before trusting "SAME-SIZE".
**Tools preserved in `MissingShip/tools/`** (mlrom, ovldiff, semdiff, fndiff, fnid, mldis) -- scratchpad is
ephemeral.

## THE RIGHT BASELINE IS `PAL MQ (Debug).z64`, not `PAL GC (Debug).z64` (found 2026-09-23)
ML's `code` is exactly MQ-debug's size (1,290,032) and layout -- ZERO shift. Every `-0x20` I "cancelled"
earlier was the gap between the non-MQ and MQ debug builds. Consequences:
- **ML's gSaveContext = 0x8015E660** (MQ-debug's). Verified via Sram_OpenSave's savedSceneNum load
  (ML 0x8015E6C6; non-MQ debug 0x8015E6E6 -> its base is 0x8015E680). All ML flag decodes done with
  0x8015E660 (villager, En_Hy, Anju, lift barrier) are therefore CORRECT.
- Diff ML against MQ-debug (same folder) for clean word-for-word comparisons: 5.4% of code words differ,
  mostly ML's rebuilt tables. Sram_InitNewSave: 0 differing words vs MQ.
- The tools in MissingShip/tools/ still point at the non-MQ debug ROM; switch them when next touched.
