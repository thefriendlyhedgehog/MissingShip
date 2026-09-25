---
name: missing-link-soh-port
description: "WORKING pipeline that gets The Missing Link's scene data booting inside Ship of Harkinian as a mod o2r — exact ROM layout facts, ZAPD invocation, and the gotchas that crash it"
metadata:
  type: project
---

**Goal (set 2026-09-18):** a booting version of The Missing Link on the SoH engine, mod preferred
over standalone recomp. **Status: ACHIEVED at the "SoH boots and loads/renders ML scene data"
level.** SoH 9.2.3 runs stably with ML scenes overriding vanilla scene slots. Geometry renders but
with wrong textures — ML *objects* are not ported yet (next step).

**Why a mod and not a full extraction:** ML's ROM has only **508 dmadata files** vs vanilla's 1510,
so a full SoH extraction would fail on ~1000 missing assets. Overriding individual scene slots via a
mod o2r degrades gracefully and is the tractable path.

**Missing Link ROM facts (all verified this session, do not re-derive):**
- Build the ROM: apply `tml_z64_2_0_big_endian.bps` to OoT **NTSC 1.0 USA** (md5
  `5bd1fe107bf8106b2ab6650abecd54d6`). Result: 16,777,216 bytes, md5 `b87b4ff48d645782ff1e6f568c7d3b61`,
  internal name `THE MISSING LINK`, header CRC at 0x10 = **0x8CDD7051**.
- **dmadata is at 0x12F70, NOT 0x7430** — that is the OoT *debug ROM* offset, i.e. Kaze built on a
  debug-ROM layout, stripped to 508 files and recompressed. The first 28 file entries still match the
  debug filelist (`dbg.txt`) names exactly; beyond that the layout diverges completely.
- `code` = dma index **114**, vrom 0xA94000-0xBCEF30, decompresses to 1,290,032 bytes.
- `gActorOverlayTable` @ code offset **0xF8B50**, stride **0x1C** (release-build `ActorOverlay`, no
  `name` field), ~37 slots, covering dma 313-348.
- Object table @ code offset **0x102EDC**, 8-byte RomFile entries, dma 126-143.
- **Content: 25 scenes and 72 rooms**, found by classifying every file's header commands (scene =
  has cmd 0x04 room list; room = has cmd 0x0A/0x08). Scene dma indices: 400, 407, 412, 439, 441, 443,
  445, 447, 449, 452, 455, 457, 462, 464, 466, 468, 472, 475, 477, 481, 489, 493, 499, 502, 505.
  Rooms follow each scene (e.g. scene 400 -> rooms 401-406; scene 412 -> 25 rooms 414-438).

**The working pipeline (scripts live in /tmp/mlwork, recreate from this recipe if gone):**
1. `bps_apply.py` — from-scratch BPS decoder; verifies all three embedded CRCs.
2. `yaz0.py` — Yaz0 decompressor; `get_file(rom, vs,ve,rs,re)` handles compressed/raw dma entries.
3. `build_rom.py` — the key trick. Produces a ZAPD-consumable ROM copy that:
   - **rewrites header CRC at 0x10 to `0x871E1C92` (OOT_PAL_GC_DBG1)** so ZAPD's `ZRom` picks
     filelist `dbg.txt` + dmadata offset 0x12F70 — which is *already* ML's real offset. This avoids
     patching/rebuilding ZAPD at all.
   - injects modified scene/room files **appended uncompressed at the end of the ROM**, rewriting
     their dmadata entry to `physStart=newOffset, physEnd=0` (physEnd 0 = uncompressed).
   - **removes scene/room header command 0x18 (alternate headers)** — see crash note below.
4. A generated `filelists/dbg.txt` with 508 lines where the ML scene/room indices are named as the
   *vanilla* scene they should override (e.g. `spot00_scene`, `spot00_room_0`); everything else gets
   a throwaway `mlfile_NNN` name.
5. Minimal per-scene XMLs (ZAPD parses scenes structurally, so this is all that's needed):
   `<Root><File Name="X_scene" Segment="2"><Scene Name="X_scene" Offset="0x0"/></File>
    <File Name="X_room_0" Segment="3"><Room Name="X_room_0" Offset="0x0"/></File></Root>`
6. A custom `Config_ML.xml` — **must contain at least one `<ExternalFile>` entry** or ZAPD silently
   extracts nothing (HandleExtract's whole ExtractDirectory body sits inside
   `for (auto& extFile : cfg.externalFiles)`).
7. Invocation (HISTORICAL: since MS-009, 2026-09-25, the pipeline runs the engine fork's own ZAPD from `Shipwright/soh/` via `pipeline/mspaths.py`; SOHCombo is no longer used. Old form, from `SOHCombo/soh/`):
   `../build-macos-sdl2/ZAPD/ZAPD.out ed -se OTR --otrfile <out.otr> --portVer 9.1.1
    -i <xmldir>/scenes -b <patched rom> -fl <filelistdir> -o <tmp> -osf <tmp> -gsf 0 -rconf Config_ML.xml`
   ("Unsupported argument: --otrfile/--portVer" is a harmless warning — ZAPD prints it, then
   delegates to the exporter's own arg parser.)
8. Install as **`~/Library/Application Support/com.shipofharkinian.soh/mods/<name>.o2r`**.

**Gotchas that cost real time — do not repeat:**
- **Extension matters:** a `.otr` file is loaded as **MPQ**; ZAPD's output is a **ZIP/O2R**. Naming
  the mod `.otr` fails with "Failed to load mpq file". Use **`.o2r`**.
- **`--portVer 9.1.1`** makes portVersion match SoH 9.2.3's `oot.o2r` (`01 00 09 00 01 00 01`).
- OoT non-dungeon scenes export to **`scenes/shared/<name>_scene/...`** (only dungeons with MQ
  variants use `scenes/nonmq/`). This matches stock `oot.o2r`. The `nonmq`/`mq` branch in
  `DisplayListExporter.cpp` is gated on `game == ZGame::MM_RETAIL`, so it never applies to OoT here.
- **CRASH: `Scene_CommandAlternateHeaderList`** (EXC_BAD_ACCESS). ML scenes use header command
  **0x18 (alternate headers)**; SoH dereferences garbage for them. Fix = strip 0x18 from scene and
  room headers by shifting the command list up (trailing bytes after the 0x14 terminator are never
  parsed, so total length can stay the same). After stripping, SoH loads the scene and stays up.
- **Do NOT zero the actor(0x01)/object(0x0B)/transition-actor(0x0E) counts** to "make it safe" —
  that makes ZAPD itself die with `std::out_of_range: vector`. Leave the lists intact.
- **Disable other mods when testing** (Kevin's instruction 2026-09-18): the Holidays in Hyrule
  Models/Textures/Sequences otrs were moved to `mods/_disabled/`. They alter Link's model and world
  textures and confound any visual check.
- Get crash causes from `~/Library/Logs/DiagnosticReports/soh-*.ips` (JSON-lines: first line is
  metadata, rest is the body; read `threads[triggered].frames` + `usedImages` for symbols). Far
  faster than guessing.

**Verified result:** with only `MissingLink_Scenes.o2r` active, SoH 9.2.3 launches, logs
`Adding Archive .../MissingLink_Scenes.o2r`, then `Scene Init - sceneNum: 0x51` (Hyrule Field =
overridden `spot00`), and runs stably at ~19% CPU rendering ML geometry. Overriding `spot00` is
notable because the **title-screen demo loads Hyrule Field**, so ML data is exercised immediately on
boot without needing any menu navigation.

**Next steps (in priority order):**
1. **Port ML objects** — rooms reference ML object IDs via header cmd 0x0B; SoH resolves those
   against *vanilla's* object table, so models/textures come out wrong (world renders as a grey
   cloudy mass). Map ML room object lists -> ML object table (code 0x102EDC) -> export those files
   into the matching vanilla object slots using `ObjectList_OoTMqDbg.txt` for ID->name.
2. Port ML actor IDs similarly, or neutralize them, so scene actors behave.
3. Expand beyond the 5 currently-mapped scenes to all 25.

See [[soh-romhack-port-project.md]] for the Ghidra reverse-engineering of ML's `code` segment (the
cutscene dispatcher rewrite, File Select replacement, TMLDBG finding) — that work is what supplies
the code-side porting knowledge this asset pipeline does not cover.

**Session 2 results (2026-09-18, same day) — isolation, textures, and the one remaining blocker.**

**Isolated profile (Kevin's explicit instruction — keep ML testing separate from his real installs):**
- `SHIP_HOME` env var is honored by libultraship (`libultraship/src/ship/Context.cpp:584`), so ML
  testing runs against its own profile and never touches the normal SoH install.
- Launcher: **`/Volumes/MacMiniExternal/AI_Projects/SOH/run_ml.sh`** — sets
  `SHIP_HOME=/Volumes/MacMiniExternal/AI_Projects/SOH/ml_profile` and execs the soh binary
  **directly** (must bypass `open -a`, because the .app's Info.plist sets its own SHIP_HOME).
- `ml_profile/` holds its own `oot.o2r`, `mods/MissingLink_Scenes.o2r`, Save/, logs/ and a
  **stock auto-generated `shipofharkinian.json`** — no randomizer, no debug items, no alt assets.
- Kevin's real install was fully restored afterwards (config, `file1.sav`, Holidays mods, and the
  ML mod removed). Backups were kept in /tmp/mlwork/*.backup.
- **SoH scans `mods/` RECURSIVELY** — moving mods into a `mods/_disabled/` subfolder does NOT
  disable them (this is why gingerbread hearts persisted). Move them out of the tree entirely.
  Mods are also tracked in `shipofharkinian.json` at `CVars/gSettings/EnabledMods` plus
  `CVars/gSettings/AltAssets`.

**Texture pipeline (solved):** ML room display lists reference their textures from **segment 0x02
(the scene file)**, not the room. ZAPD cannot auto-discover those, which is exactly why vanilla scene
XMLs carry dozens of explicit `<Texture ... AddedByScript="true"/>` entries. Recovered the metadata by
walking the room DLs in F3DEX2 and reading the render tile:
- `G_SETTIMG (0xFD)` -> texture address; `G_LOADTLUT (0xF0)` -> the preceding SETTIMG was a palette;
  `G_SETTILE (0xF5)` **with tile==0** -> the real fmt/siz (do NOT take fmt/siz from SETTIMG, it is the
  DRAM load format and yields nonsense like "ci16"); `G_SETTILESIZE (0xF2)` tile 0 -> width/height via
  `((lrs-uls)>>2)+1`.
- Emit `TlutOffset` **only** for ci4/ci8 or ZAPD aborts with
  `'TlutOffset' declared in non color-indexed texture`.
- Result: 22 textures for the outdoor scene, 57 across the 5 mapped scenes. Decoding them to PNG
  confirms real ML art — grass, water, tree bark, stone, wood planks, a banner (a forest/village map).

**THE REMAINING BLOCKER: SoH does not draw ML room geometry.** Everything below was verified, so do
not re-check it:
- Room geometry is genuinely present: outdoor room = **1378 vertices**, interior room = 1032.
- Spawn points are **inside** the geometry bbox (outdoor spawn (-748,145,-410) vs bbox
  x[-1855,4442] y[-955,1752] z[-2843,1587]), so Link is not simply out of bounds.
- Vertices are in segment 0x03 (room) and resolve fine; textures in segment 0x02 (declared).
- The exported `MORO` Room resource **does** correctly reference its DLs by name
  (`scenes/shared/spot00_scene/spot00_room_0DL_0003E8` etc.), structurally matching vanilla.
- Mesh header is type 0 with 22 entries; the DL offsets in it match the exported DL resources.
- Ruled out: the translucent pass (zeroing all 22 xlu DL pointers changed nothing); the 2 stray
  `seg 0x09` DL calls (they are nested, not in the top-level opaque chain).
- What you actually see: an **outdoor** ML scene renders as the cloudy skybox with Link standing on
  ML collision; an **interior** ML scene renders pure black. i.e. skybox + actors draw, room DLs do not.
- **Do NOT try dropping the lighting/skybox commands (0x0F / 0x11) to "simplify" — SoH crashes
  without them.**
- Next avenue: this needs white-box debugging, not more black-box guessing — build SoH/ComboShip from
  source and instrument the room draw path (`z_room.c` / `OTRExporter_Room` resource loading) to see
  whether the DL resources are found and executed at runtime.

**Working artifacts:** the whole pipeline is archived at
`/Volumes/MacMiniExternal/AI_Projects/SOH/ml_pipeline/` (bps_apply.py, yaz0.py, build_rom.py,
gen_textures.py, dlscan.py, dec_tex.py, xml/, filelists/, Config_ML.xml, ml.otr).

**Session 2 addendum — further ruled out (do not retry these):**
- `Set_XXXXXX` resources in an exported room are **alternate headers**, NOT the mesh. Vanilla rooms
  have ~11 because they carry day/night alt headers; ML rooms legitimately export 0 once 0x18 is
  stripped. Chasing "missing Set_ resources" is a dead end.
- **Room alt-headers (0x18) must ALSO be stripped, not just the scene's.** Keeping them in rooms
  crashes SoH on load (verified).
- ZAPD's `SetMesh.cpp` maps meshHeaderType **0 -> RoomShapeCullable**, which is wrong (0 is
  RoomShapeNormal, 8-byte entries; 2 is Cullable, 16-byte entries). **Vanilla OoT rooms are type 2,
  ML rooms are type 0** — verified: vanilla spot00_room_0 = type 2/36 entries, ML room 474 = type
  0/22 entries. `build_rom.py` now rewrites ML's type-0 mesh into a valid type-2 structure (appended
  at end of the room file, cull sphere centre 0 radius 0x7FFF so nothing is culled, 0x0A command
  repointed, and the scene's room list updated for the room's new length). **This conversion did NOT
  fix rendering on its own**, but it is correct and should be kept.
- Control experiment that proves the toolchain is fine: running this exact ZAPD/pipeline against the
  **vanilla** OoT ROM produces a normal, complete room export (11 Set_, geometry intact). So the
  pipeline is sound; the gap is specific to how SoH consumes ML's room data.

**Current state:** `ml_profile` + `MissingLink_Scenes.o2r` boots and runs stably; ML scene, collision
and textures load; **room geometry still does not draw** (outdoor ML scene shows the skybox, indoor
shows black, Link stands on ML collision). This is the single remaining blocker and now needs
white-box debugging of SoH's room draw path rather than more data-side guessing.

**SESSION 3 BREAKTHROUGH (2026-09-18) — ML room geometry now RENDERS in SoH.** Three real bugs
found and fixed. The technique that cracked it: **run VANILLA OoT data through the exact same
pipeline as a control.** The vanilla control also crashed, which proved the bug was in my
transformations rather than in ML's data — do this first next time something looks data-specific.

**Bug 1 (the big one): never remove a scene/room header command by SHIFTING the command list.**
Shifting commands up to delete cmd 0x18 corrupts the file — the vanilla control crashed in
`Cutscene_ProcessCommands` with poison `0xabababab`. Injection alone (no strip) was fine, which
isolated it. **Fix: overwrite the 0x18 command IN PLACE with `0x16` (SetEchoSettings, echo=0)** —
same length, pointer-free, harmless at runtime, and supported by the resource loader.
- `0x09` (Scene_CommandUndefined9) is a runtime no-op BUT the OTR **SceneFactory rejects it**
  ("Failed to load scene command of type 9"), so 0x16 is the right choice, not 0x09.
- Root cause of the original alt-header crash: `Scene_CommandAlternateHeaderList`
  (`soh/soh/z_scene_otr.cpp:343`) indexes `cmdHeaders->headers[gSaveContext.sceneLayer - 1]` with
  **no bounds check**. NULL entries are handled gracefully; a too-short vector is not. ML scenes
  have fewer alt headers than vanilla, so it reads out of bounds.
- Pointing 0x18 at a hand-built list of NULL pointers did NOT work — ZAPD derives the count from
  `GetDeclarationSizeFromNeighbor(offset)/4`, so a hand-placed list at EOF isn't sized as expected.

**Bug 2: object lists crash object-bank init.** ML rooms list ML object IDs (header cmd 0x0B);
SoH resolves them against VANILLA's object table, giving invalid entries and a poison-pointer crash
in `func_80096FE8`. **Fix: rewrite every object-list entry to object ID 1 (gameplay_keep)**, which is
always resident. Keep the count — zeroing counts makes ZAPD die with `std::out_of_range`.

**Bug 3: mesh header type.** Vanilla OoT rooms are mesh type **2**; ML rooms are type **0**, and
ZAPD's `SetMesh.cpp` maps type 0 to `RoomShapeCullable` (wrong — that's the type-2 layout). The
pipeline now rewrites ML's type-0 mesh into a valid type-2 structure (appended at end of file,
cull sphere centre 0 / radius 0x7FFF so nothing culls, 0x0A repointed, scene room-list resynced).

**Result:** SoH boots, loads the ML scene, and **draws ML's terrain** — cliffs, ledges, a canyon —
with Link standing on it and a working HUD. Verified by screenshot.

**Still open — textures render psychedelic/rainbow.** That is the classic signature of CI textures
whose TLUT never resolves. All 67 texture references in the test room point at **segment 0x02 (the
scene file)**; they are declared in the scene XML with `TlutOffset`, which is necessary but evidently
not sufficient, because the room's DL also issues a raw `LOADTLUT` against the seg-2 palette address.
**Do NOT "fix" this by declaring each palette as its own `<Texture Format="rgba16" .../>` resource at
the palette offset — that REGRESSES geometry back to not drawing at all** (verified). Next idea worth
trying: vanilla's `ExternalTlut=`/`ExternalTlutOffset=` attributes (see `spot04.xml`) rather than a
standalone palette declaration.

**Kevin's in-game observations (useful, consistent with the design):** on load Link is **floating**
(vanilla entrance-table spawn coords don't match ML's much smaller map), and **voiding out lands him
in a proper romhack area** (the scene's own void-out/respawn point is correct). He also reports
wrong loading-zone destinations and no start menu — all expected, because **SoH is still running
vanilla OoT's entrance table, scene table and game logic underneath; only the scene assets are
swapped.** Fixing those means porting ML's entrance/scene tables, not more asset work.

**SESSION 3 (cont.) — texture investigation: what is PROVEN good, so do not re-check it.**
Geometry renders; textures come out psychedelic/rainbow. Verified exhaustively at the data level:
- Exported texture payloads are **byte-identical to the ROM** (checked tex + palette).
- Exported texture headers are **correct** — note the resource header is **little-endian**:
  `fmt,width,height,datasize` at 0x40. Confirmed e.g. CI8 32x32/1024B (fmt=4) and IA8 64x64/4096B
  (fmt=8). TextureType enum: 1=RGBA32 2=RGBA16 3=CI4 4=CI8 5=I4 6=I8 7=IA4 8=IA8 9=IA16.
- DL export is correct: `G_SETTIMG` -> `G_SETTIMG_OTR_HASH` (0x20), tile setup (0xF2/0xF3/0xF5)
  preserved, counts match the ROM. DL resource commands start at **0x40**, words **little-endian**.
- Textures ARE found at runtime: `interpreter.cpp:4287` logs `G_SETTIMG_OTR_HASH: Texture is null`
  for failures and **no such errors appear** in the log.
- ZAPD **auto-discovers** scene textures — explicit `<Texture>` declarations are unnecessary; the
  known-good vanilla control used a minimal XML. Declaring them (with or without TLUTs, vanilla
  dimensions ci4=4x4 / ci8=16x16) changes nothing visually.
- It is NOT a palette problem: a DL with no `LOADTLUT` at all still renders psychedelic.
- It is NOT lighting: Link and the HUD render with correct colours in the same frame.

**Real find (fix applied, correct but not sufficient):** libultraship's
`gfx_set_timg_otr_hash_handler_custom` takes the texture format from the **command word**
(`fmt=C0(21,3)`, `size=C0(19,2)`), NOT from the texture resource. ML's `SETTIMG` carries the *load*
format (16b block-load) while the true format is on render tile 0 — vanilla's happen to agree, ML's
do not. `build_rom.py` now rewrites every SETTIMG's fmt/siz bits to match its tile-0 descriptor
(25 patched in the test room). Keep this; it did not by itself fix the colours.

**Next thread (no build required):** SoH ships a live display-list debugger. Enable the window cvar
**`CVars.gWindows.SohGfxDebugger`** (registered in `soh/soh/SohGui/SohGui.cpp:125`, menu entry in
`SohMenuDevTools.cpp:305`) and step the failing DL to see the actual tile/TMEM state. Remaining
suspects: the `G_SETCOMBINE` (0xFC) mode, and multi-tile/TEXEL1 sampling where a second tile is
never loaded.

**Verification harness (important — this made testing repeatable):** SoH's WarpPoints feature boots
straight into a chosen scene/position, bypassing the attract demo. In `ml_profile/shipofharkinian.json`:
set `CVars.gSettings.BootSequence = 4` (BOOTSEQUENCE_WARPPOINT) and add
`"WarpPoints": {"MissingLink": {"entranceId":205, "roomNum":0, "pos":{"x":-748,"y":200,"z":-410},
"rotY":0, "bootToPoint":true}}`. Without this, Link spawns floating in the void (vanilla entrance
coords vs ML's much smaller map) and every screenshot just shows skybox — which caused several false
"regression" conclusions. **Always verify via the warp point, never from a spawn-position screenshot.**

**Debug tooling enabled in ml_profile (2026-09-18).** SoH's Dev Tools menu has: Stats, Console,
Save Editor, Hook Debugger, Collision Viewer, Actor Viewer, DList Viewer, Value Viewer, Message
Viewer, **Gfx Debugger**, plus Popout Menu / Debug Mode / Resource logging toggles and a Log Level
selector. Set these directly in `ml_profile/shipofharkinian.json` (clicking the ImGui sidebar did not
switch panels reliably from a background screenshot):
- `CVars.gDeveloperTools.ResourceLogging = 1` and `CVars.gDeveloperTools.LogLevel = 1` (Debug)
- `CVars.gOpenWindows.SohGfxDebugger = 1`   (Gfx Debugger window)
- other windows follow the same pattern, e.g. `CVars.gOpenWindows.DisplayListViewer`,
  `ActorViewer`, `CollisionViewer`, `SohConsole`, `SohStats` (names from `soh/soh/SohGui/SohGui.cpp`).

**Resource logging is extremely useful** — it dumps every parsed scene/room command as XML, which
independently CONFIRMED all of this session's fixes are landing:
`<SetMesh MeshHeaderType="2" PolyNum="22">` with 22 `<Polygon ... MeshOpa="scenes/shared/
spot00_scene/spot00_room_0DL_000680" .../>` entries and `Unknown="32767"` (the never-cull radius),
`<SetObjectList>` all `Id="1"` (the gameplay_keep remap), and the alt-header slot showing as a
harmless `<SetEchoSettings Echo="0"/>`. So scene/room structure reaching the engine is correct.

**Gfx Debugger works** and is the right tool for the remaining texture bug: it pauses the frame and
shows the live display-list tree (gsSPSegment/G_DL/gsDPNoOpTag...), a Disp Stack, and a **Tiles panel
listing "Loaded Textures" with dimensions and type** (e.g. `0: 128x128; type=RGBA32`). Stepping to a
room DL and reading the Tiles panel will show exactly what format the renderer has bound versus what
the texture really is — that is the open question. Driving it through background screenshots is slow;
it is far faster to step it interactively.

**Two more real fixes (2026-09-18, via the Gfx Debugger + ZAPD stderr):**
1. **`texDecl == nullptr! PTR = 0x00003880` (and ~75 more) during ZAPD export.** Those PTRs are the
   scene-segment texture offsets. ZAPD auto-discovers textures only while parsing the **scene's own**
   DLs; textures referenced from the **room's** DLs into segment 2 need **explicit `<Texture>`
   declarations** — which is exactly why vanilla scene XMLs carry dozens of `AddedByScript` entries.
   Restoring the generated declarations (formats from the tile-0 scan, ci4 palettes as 4x4 / ci8 as
   16x16) took the error count **75 -> 0**. So the declarations ARE required; an earlier conclusion
   that they were neutral was wrong (it was masked by the floating-spawn confound).
2. **ML actor IDs spawn arbitrary vanilla actors.** The Gfx Debugger showed a garbage 128x128 CI8
   noise tile belonging to `G_MARKER: objects/gameplay_keep/gKokiriDustMoteMaterialDL` — a vanilla
   dust-mote effect drawing with an unset segment 8. Caused by remapping ML objects to gameplay_keep.
   Fixed by neutralising the room's actor list: overwrite header cmd **0x01 in place with 0x16**
   (same no-op trick as 0x18).

**How to read the Gfx Debugger (learned the hard way):** each display list starts with a
`G_MARKER: <resource path>` line — that is how you tell ML room terrain
(`scenes/shared/spot00_scene/spot00_room_0DL_...`) from an actor/effect DL. The exporter's
**unresolved-texture fallback looks like `gsDPSetTextureImage(G_IM_FMT_I, G_IM_SIZ_16b, 1,
0x08000001)`** — format I/16b with a `0x08......`-style bogus address. Seeing that means the texture
reference did not resolve at export time. The Tiles panel shows the resulting garbage with a live
thumbnail (pure static = unresolved).

**Segment note:** OoT sets segments 8/9 from the **scene draw config** in `z_scene_table.c`
(`gSPSegment(POLY_OPA_DISP++, 0x08, ...)`), not from room draw (`z_room.c` only sets segment 3).
Anything ML expects in segments 8/9 comes from Kaze's own draw-config code and will never be set by
vanilla SoH.

**Still open:** textures remain psychedelic even with 0 texDecl errors and actors removed. Next data
point needed: open the Gfx Debugger, find a DL whose `G_MARKER` is `spot00_room_0DL_*`, and read its
`gsDPSetTextureImage` line — if it still shows the `G_IM_FMT_I/16b/0x08000001` fallback the reference
is still unresolved; if it shows a real format the problem is downstream in tile setup.

**SESSION 4 (2026-09-18) — textures now RESOLVE and the game runs clean. Two root causes found.**

**Which binary actually runs (this mattered a lot, do not forget):** `run_ml.sh` execs
**`/Volumes/MacMiniExternal/AI_Projects/SOH/soh.app/Contents/MacOS/soh`** — that is **stock SoH
9.2.3**, NOT a source build. `SOHCombo/build-macos-sdl2/` contains only **ZAPD.out** (+ soh.o2r);
there is no built soh binary there. So any reasoning about libultraship source behaviour must be
checked against *stock 9.2.3*, which lacks several guards the SOHCombo tree has.

**ROOT CAUSE of "textures never resolve": ZAPD computes hashes for seg-2 scene textures referenced
from room DLs, but never EXPORTS them as resources.** Declaring them in the scene XML silenced the
`texDecl == nullptr` errors but produced 0/67 resolved at runtime.
**Fix = INLINE the textures into the room file.** `build_rom_inline.py` (now the canonical
`build_rom.py`) copies each seg-2 texture/palette payload into the room file, repoints the
`G_SETTIMG`/`G_LOADTLUT` operand to **segment 3** (room), records a manifest
(`/tmp/mlwork/inlined.json`), and scrubs any leftover non-seg3 pointers to a fallback texture.
`gen_xml_inline.py` then emits room-local `<Texture>` declarations from that manifest.
**Result: 173 textures inlined, `texDecl` errors 0, and ZERO `G_SETTIMG_OTR_HASH: Texture is null`
in a full run — first time ever.**

**CRASH this exposed, and its two fixes.** Once textures actually resolved, the app aborted with
`-[MTLTextureDescriptorInternal validateWithDevice:]:1421: failed assertion 'MTLTextureDescriptor
has height of zero'`. The `.ips` stack named it exactly: `ImportTextureI4` -> `UploadTexture` ->
`AGXTexture initWithDevice:desc:`.
- `libultraship/src/fast/interpreter.cpp:940` computes `height = sizeBytes / widthBytes`, so any
  texture whose loaded `sizeBytes` is smaller than one line yields **height 0**. **Stock SoH 9.2.3
  hands that straight to Metal.** (SOHCombo's `gfx_metal.cpp` `UploadTexture` *does* early-return on
  `width==0||height==0` — that guard is why this never showed up in the Combo tree.) The bug was
  latent all along; pre-inlining the texture simply never resolved, so the importer bailed first.
- **MISTAKE TO NEVER REPEAT: do not "correct" the `G_SETTIMG` width field.** LOADBLOCK-style loads
  encode **width=1** there deliberately; the engine derives line size/height from it. Rewriting it to
  the real width drives `height` to 0 and causes exactly this abort.
- **Working fix:** exclude **i4 (fmt=4, siz=0)** textures from inlining — only 2 of them, and they
  are the ones whose LOADBLOCK/SETTILE pairing yields `sizeBytes < lineBytes`. Wired as
  `SKIP_FMT` / env var **`ML_SKIP_FMT="4:0"`** in `build_rom_inline.py`.

**Test harness: `/tmp/mlwork/cycle.sh`** — one command does rebuild ROM -> regen XML from manifest ->
ZAPD repack -> install into `ml_profile/mods/` -> relaunch -> report `RESULT: OK|FAIL` with pid, CPU
and assertion count. Run it as `ML_SKIP_FMT="4:0" /tmp/mlwork/cycle.sh`. Iterating formats through
this loop is ~1 minute per try; use it instead of hand-running the steps.

**Verified state:** `RESULT: OK pid=... cpu=6.6`, 0 assertions, 0 `Texture is null`, 0 `[error]`
lines other than the harmless `gamecontrollerdb.txt` one. Inlined format census: rgba16 34, ci4 11,
ci8 41, ia8 7, ia16 3, i8 11, i4 2 (the i4 pair excluded).

**Next:** confirm visually (via the WarpPoint harness) that the inlined textures render correctly
rather than merely resolving; then re-enable i4 properly by fixing the SETTILE/LOADBLOCK pairing
instead of skipping it; then the entrance/scene-table port for spawn, loading zones and start menu.

**SESSION 4 RESULT: ML TEXTURES NOW RENDER CORRECTLY. The crash was MY scrub pass, not the data.**

Screenshot-verified: ML geometry with real ML art — wooden walkways and railings, stone walls, dirt
paths, grass — Link standing on a wooden platform, normal HUD, stable, `assert=0`, `texnull=0`,
and no `[error]` lines except the harmless `gamecontrollerdb.txt` one.

**THE ACTUAL ROOT CAUSE of `MTLTextureDescriptor has height of zero` — do not re-introduce this.**
`inline_scene_textures` had a final "scrub" pass that repointed every *leftover* non-segment-3
`G_SETTIMG` operand at a fallback inlined texture. That is what aborted, every time:
- The DL still programs its **own** tile width via SETTILE/SETTILESIZE. Stock SoH 9.2.3 computes
  `height = sizeBytes / widthBytes` with **no zero guard**, so a fallback texture narrower than the
  tile the DL sets collapses height to 0 and Metal aborts.
- Changing the fallback from the smallest to the **largest** inlined texture did NOT help.
- **Leaving unresolved pointers alone is safe** — the importer bails early on a null address. So the
  scrub is now **off by default** in `build_rom_inline.py` (opt back in only via `ML_SCRUB=1`, and
  don't).

**How it was found — the method worth reusing:** format-based guessing (`ML_SKIP_FMT`) did not
converge, and an ordinal bisect (`ML_SKIP_RANGE`) reported CRASH on *both* halves. Two "both halves
fail" results mean the variable being bisected is not the cause — that is the signal to suspect a
pass that runs unconditionally. Disabling the scrub confirmed it in one cycle.

**Harness bug that produced false OKs (fixed):** `cycle.sh` sampled liveness at a fixed 30s, but the
attract demo loads spot00 (`sceneNum: 0x51`) **~2-3 minutes** after launch, and that is when the
texture crash fires. It now polls for either the assertion, process exit, or scene-0x51-then-survive-
25s and prints `RESULT: OK|CRASH|EXITED|TIMEOUT`. **Never judge an ML build before scene 0x51 loads.**

**Baked-in defaults (plain `python3 build_rom.py` now produces the good build):** scrub off,
`ML_SKIP_FMT` defaults to `4:0` (skip i4). ~60 textures inlined in spot00_room_0.

**Remaining cosmetic issue:** some ground/grass surfaces still show coloured speckle — those are the
references that are now deliberately left unresolved. Recovering them means exporting the seg-2
textures as real resources (or inlining the few remaining formats correctly), not scrubbing.

**Next:** re-test whether i4 can be re-enabled now that the scrub is gone (only 2 textures);
then the entrance/scene-table port, which is what actually fixes the floating spawn, the wrong
loading zones and the missing start menu.

**SESSION 4 (cont.) — SPECKLED TEXTURES FIXED. Two more causes, both found by instrumenting
rather than guessing.** Final state: **119 textures + 52 palettes inlined, 0 texDecl errors,
0 `Texture is null`, 0 assertions, `RESULT: OK`**, and screenshot-verified clean water, stone,
wood and dirt surfaces with no speckle.

**The instrumentation that found both (keep it, it lives in `build_rom_inline.py`):** a `diag()` pass
that walks every room DL after inlining and records each texture reference the inliner did NOT
create, writing `/tmp/mlwork/unresolved.json` (segment-2 refs still not inlined) and
`/tmp/mlwork/roomlocal.json` (segment-3 room-local refs not in the manifest), each with its
fmt/siz taken from the render SETTILE and w/h from the render SETTILESIZE. This turned "some
surfaces look speckled" into an exact list of 14 references in about one minute. **Do this first
next time instead of bisecting.**

**Cause 1 (12 of the 14 — this WAS the speckle): the render tile is not always tile 0.**
`gsDPLoadMultiBlock` loads through tile **7** (`G_TX_LOADTILE`) and renders from tile **1**. Both
`fix_settimg_fmt` and `inline_scene_textures` keyed on `tile == 0`, so every multi-block texture was
skipped entirely — those were the noisy ground/water surfaces. **Fix: key on `tile != 7`** (i.e.
"the first SETTILE/SETTILESIZE that is not the load tile") in both functions. rgba16 inlined went
34 -> 46 and unresolved refs went 14 -> 2. Verified `RESULT: OK` and visibly clean afterwards.

**Cause 2 (the last `texDecl == nullptr! PTR = 0x030014B8`): a segment-3 pointer, not segment 2.**
`tent_room_0` has two genuinely **room-local** ML textures (`0x14B8` ia4 64x64, `0x1CB8` ia16 8x16)
that ZAPD failed to auto-discover. They are not an inlining problem at all — they just need explicit
`<Texture>` declarations, which `gen_xml_inline.py` now emits from `roomlocal.json`. texDecl errors
2 -> 0. **Note the segment in a texDecl PTR before assuming it is a scene texture** — `0x03......`
means room-local.

**The two remaining unresolved refs are i4 and must stay skipped (verified by experiment):**
`spot00_room_0 @ 0x8880` and `labo_room_0 @ 0x4D50`. Re-enabling i4 inlining (`ML_SKIP_FMT=""`)
reproduces the Metal height-zero abort immediately — their LOADBLOCK/SETTILE pairing genuinely
yields `sizeBytes < lineBytes`. That is 2 surfaces out of 121 textures; leaving them unresolved is
safe (the importer bails on a null address).

**Canonical build is now env-var-free:** `cd /tmp/mlwork && ./cycle.sh` rebuilds, repacks, installs
and reports `RESULT: OK|CRASH|EXITED|TIMEOUT`. Defaults baked in: scrub OFF, `ML_SKIP_FMT=4:0`,
render tile = first non-7 tile. Archived to `/Volumes/MacMiniExternal/AI_Projects/SOH/ml_pipeline/`
(build_rom.py, build_rom_inline.py, gen_xml_inline.py, cycle.sh, xml/, filelists/, inlined.json,
unresolved.json, roomlocal.json, ml.otr).

**SESSION 5 (2026-09-18) — SCENE TABLE AND ENTRANCE TABLE PORTED, both as pure data. 25 scenes.**

**ML's own tables, located in `ml_code.bin` (do not re-derive):**
- **Scene table @ code offset `0x10cbb0`**, 110 slots x **0x14 (20) bytes** — `SceneTableEntry` is
  `RomFile sceneFile(8) + RomFile titleFile(8) + u8 unk_10, config, unk_12, unk_13`. 25 slots used.
  Find it by searching `code` for a known scene's `(vromStart, vromEnd)` dmadata pair; empty slots are
  all-zero, which is why a naive "every entry must match dmadata" scan finds nothing.
- **Entrance table @ code offset `0x10b360`**, immediately BEFORE the scene table, 1556 x 4 bytes
  (`u8 scene, u8 spawn, u16 flags`), preceded by zero padding. Entries come in groups of 4 identical
  rows (the scene-layer group), which is the visual signature to look for.
- Extracted to `ml_scenetable.json` / `ml_entrancetable.json`; the full scene+room map (room lists
  resolved from each scene's header cmd 0x04 back to dma indices) is `ml_full_map.json`.

**THE KEY INSIGHT — the scene table ports by RENAMING, with no engine change.** ML's scene-table
**slot index IS the vanilla scene ID**, and SoH indexes its own compiled-in table by that same ID.
So naming each ML scene file after the vanilla scene occupying the same slot makes SoH load ML's
scene wherever the game asks for that ID. **All five of the earlier hand-guessed names were wrong:**
`dma 472` is **spot04 (Kokiri Forest)**, not spot00; ML's real Hyrule Field is `dma 466`; 439=HAKAdan_bs,
462=tokinoma, 464=turibori. Correct map now lives in `ml_map.py` (25 scenes, 72 rooms, all resolved).

**The entrance table ports as data too, via the scene EXIT LIST (header cmd 0x13).** SoH uses
vanilla's `gEntranceTable`, so ML exit ids resolve to the wrong destinations — this is exactly the
"loading zones go to the wrong places" symptom. `ml_entrance.py` rewrites each exit-list entry from
ML id `e` to a vanilla entrance `v` where `vanilla[v] == ml[e]` as `(scene, spawn)`. **52 of 60
references remap**; 8 have no vanilla equivalent and are left alone. Concrete example: ML's ydan exit
308 means (91,1)=Lost Woods spawn 1, but vanilla 308 goes to (14,1) — remapped to vanilla 425.
Take the LOWEST matching vanilla index: the entrance system expects the first entry of a 4-row
scene-layer group and applies the layer offset itself.

**CORRECTION to an earlier note in this file — the nonmq/mq branch is NOT MM-only for OoT.**
`DisplayListExporter.cpp:1053` selects `scenes/nonmq` (vs `scenes/shared`) when the XML path contains
**`dungeons/`** AND the filename matches its MQ-variant regex (`ydan|ddan|bdan|Bmori1|HIDAN|MIZUsin|
jyasinzou|HAKAdan|HAKAdanCH|ice_doukutu|men|ganontika`). `OTRPlay_SpawnScene` looks scenes 0-9 (plus
Gerudo Training and Inside Ganon's Castle) up under nonmq/, so exporting those to `shared` makes the
scene load as nullptr and then **SIGSEGV in `StringHelper::Sprintf` via `OTRPlay_SpawnScene`** with a
garbage `scene->sceneFile.fileName`. Fix: put ydan/Bmori1/HAKAdan XMLs in `xml/scenes/dungeons/`.
`ML_SKIP_SLOTS` in `ml_map.py` can exclude slots while debugging.

**Also: `Config_ML.xml`'s `<ExternalFile XmlPath=...>` must point at a file that EXISTS.** After
reorganising the XML folders it still referenced the deleted `scenes/indoors/link_home.xml`, and ZAPD
aborted with `uncaught exception std::runtime_error: invalid XML file`.

**Verified:** `RESULT: OK`, 25 scenes, 72 rooms, 1825 textures inlined, 0 texDecl errors,
0 `Texture is null`, 0 assertions.

**START MENU — read this before promising anything.** `BootSequence = 4` in `ml_profile` is OUR
warp-point harness and is itself why "there is no start menu"; setting it to **0** restores the normal
title/file-select boot. But that shows **SoH's own (vanilla) file select**: ML's start menu is custom
MIPS in ML's `code` segment, and SoH compiles its own `z_file_choose.c`, so ML's menu **logic cannot
be ported as data at all** — it needs an engine source build, and this machine has **no SoH binary
built from source** (`SOHCombo/build-macos-sdl2/` contains only ZAPD.out; `run_ml.sh` runs stock
9.2.3). ML's menu *artwork* could in principle be swapped as assets, but the relevant files sit past
dma index 28 where ML's filelist diverges from the debug ROM, so they must be identified by content
first. Do not claim the start menu is ported.

**SESSION 5 (cont.) — boot path, the cutscene wall, and audio.**

**BOOT: use `BootSequence = 2` (BOOTSEQUENCE_FILESELECT).** Enum in
`soh/soh/Enhancements/enhancementTypes.h`: 0 DEFAULT, 1 AUTHENTIC, **2 FILESELECT**, 3
DEBUGWARPSCREEN, 4 WARPPOINT. FILESELECT boots straight to the file select, skipping the attract
demo. Verified `RESULT: OK` with all 25 scenes.

**THE ATTRACT DEMO HANGS — and this is an engine-side wall, not an asset bug.** With
`BootSequence = 0` the title demo loads ML's spot00 and **freezes**: measured **0.00 mean pixel
delta over 8s** (PIL diff of `screencapture -l <windowid>` frames), input ignored, log stops
growing, process alive spinning at ~6% CPU. Cause: the demo runs a scene cutscene, and ML's
cutscene data is written for ML's own rewritten cutscene dispatcher (see
[[soh-romhack-port-project]]), which vanilla `Cutscene_ProcessCommands` loops on forever.
- **Neutralising cmd 0x17 does NOT help** — it trades the hang for `EXC_BAD_ACCESS` at
  `0xabababababababab` in `Cutscene_ProcessCommands` (via `Play_Update -> func_800645A0 ->
  func_80068ECC`), i.e. the demo still insists on running a cutscene and now finds nothing.
  `strip_cutscenes()` is kept in `build_rom_inline.py` but is **opt-in via `ML_STRIP_CS` and OFF by
  default**. Route around the demo with BootSequence=2 instead.
- Expect in-game cutscenes to hit the same wall. A real fix means giving cmd 0x17 a minimal VALID
  vanilla cutscene (header `totalEntries, endFrame` then `CS_END` 0xFFFFFFFF) rather than removing
  it — untried, and ZAPD's ZCutscene parser may object.

**Technique worth reusing: prove "frozen" numerically, don't eyeball it.** `screencapture -x -l
<windowId>` + PIL `ImageChops.difference` mean-abs-delta distinguishes a static camera (small but
non-zero) from a true hang (exactly 0.00). Kevin spotted the freeze before I did because I was
reading changing MD5s as motion — MD5 differs on trivial noise, so use the pixel delta.

**AUDIO — music randomizer is NOT involved.** `ml_profile` has no randomizer CVars at all.
Two real facts instead:
1. **ML's scene sound-settings command (0x15) carries ML's own sequence IDs**, and ML's
   `Audiobank`/`Audioseq`/`Audiotable` (dma 3/4/5) are NOT ported, so those IDs resolve against
   VANILLA's sequence table. They are semantically sensible, so it sounds coherent but is not ML's
   soundtrack: 0x1f->NA_BGM_LINK_HOUSE (ML house interiors), 0x55->NA_BGM_SHOP (night_shop),
   0x60->NA_BGM_POTION_SHOP, 0x2c->NA_BGM_FOREST_TEMPLE (ydan), 0x3f->NA_BGM_SPIRIT_TEMPLE (Bmori1),
   0x2a->NA_BGM_FIRE_TEMPLE (HAKAdan). Sequence names: `soh/include/sequence.h` (`#define`s, not an
   enum — parse with a #define regex).
2. **`ml_profile` was auto-generated with `Window.AudioBackend = coreaudio`, but Kevin's real SoH
   install uses `sdl`** — SoH ships sdl specifically to dodge the libultraship CoreAudio sample-rate
   bug on macOS ([[comboship-macos-port]]). Corrected to `sdl` 2026-09-18. **Any new SHIP_HOME
   profile will default to coreaudio — set it to sdl immediately.**

**SESSION 5 (cont.) — the cutscene wall hits GAMEPLAY too, and the working stopgap.**

Starting a new file from the file select hangs on a **black screen with music still playing**
(audio runs on its own thread, so "music continues" is the signature of the GRAPHICS/game thread
being stuck, not a dead process). State at the hang: `Scene Init - sceneNum: 0x34,
entranceIndex: 0xbb` = SCENE_LINKS_HOUSE / ENTR_LINKS_HOUSE_CHILD_SPAWN, i.e. ML's wake-up intro
cutscene (`scenes/shared/link_home_scene/link_home_sceneCutsceneData_0000D8`, 796 bytes).
13 cutscene resources export across the 25 scenes, so this will recur wherever one plays.

**WORKING STOPGAP — SoH's own cutscene-skip toggles.** Set in `ml_profile/shipofharkinian.json`
under `CVars.gEnhancements.TimeSavers.SkipCutscene.*` (prefix `gEnhancements` comes from
`CMake/soh-cvars.cmake`): **Intro, Story, Entrances, OnePoint, LearnSong, BossIntro = 1**.
Verified: boots to the file select and stays healthy (~22% CPU).
- `SkipIntro.cpp` works by setting `gSaveContext.cutsceneIndex = 0` when
  `entranceIndex == ENTR_LINKS_HOUSE_CHILD_SPAWN` **and** `cutsceneIndex == 0xFFF1`, so ML's script
  is never walked. **It only fires on the NEW-GAME spawn — an existing save stuck mid-intro will
  still hang; start a fresh file.**
- Full list of available skips: BossIntro, Entrances, GlitchAiding, Intro, LearnSong, OnePoint,
  QuickBossDeaths, Story.
- These are stock time-saver enhancements, NOT randomizer or cheats (profile still has zero
  randomizer CVars) -- but they ARE a deviation from "fully stock", which Kevin asked for, so say so.

**The proper fix, still untried:** point each scene's cmd 0x17 at a minimal VALID vanilla cutscene
(header `totalEntries, endFrame`, then `CS_END` = 0xFFFFFFFF) instead of ML's script. That gives the
dispatcher something well-formed that ends immediately, keeping scene transitions intact without
relying on skip toggles. Risk: ZAPD's ZCutscene parser may reject a stub. Deleting cmd 0x17 outright
is already known NOT to work (null-deref crash).

**SESSION 5 (cont.) — the new-game start, and BUILDING SoH FROM SOURCE for the texture guard.**

**New-game start is FIXED declaratively (`ml_map.py`).** SoH hardcodes the new-game spawn to
`ENTR_LINKS_HOUSE_CHILD_SPAWN` (0xbb -> scene 52 spawn 0) and ML's entrance table agrees -- but
**ML's scene 52 is a 128-byte STAGING STUB** (the only stub of the 25). Its intro cutscene runs 4000
frames then `CS_DESTINATION`s to **entrance 119 = scene 54 (malon_stable)**, the real start.
- Parse `CS_DESTINATION` by scanning the script for command id **0x03E8**; layout is
  `0x03E8, 1, (dest<<16)|startFrame, (endFrame<<16)` = 16 bytes. Other ML scenes with destinations:
  tokinoma->spot09, turibori->HAKAdan, spot12->kokiri_home4, Bmori1->spot10.
- **FIX: map dma 457 (malon_stable) to slot 52 and drop the stub (dma 455).** malon_stable has
  **no cutscene at all** and a real 27KB room, so the start is clean. Cost: vanilla's Stable occupies
  slot 54; the only ML exits targeting scene 54 were spawns 1-3, which already had no vanilla
  entrance equivalent. Override lives in `ml_map.py` (`ML_NO_START_FIX=1` disables it).
- **`SkipCutscene.Intro` must STAY ON even with a real start scene.** The new-game path sets
  `gSaveContext.cutsceneIndex = 0xFFF1` to play the spawn scene's cutscene; malon_stable has none, so
  the dispatcher reads poison -> `EXC_BAD_ACCESS 0xabababababababab` in `Cutscene_ProcessCommands`.
  All other skips are off.

**A hand-built minimal cutscene DOES NOT fix the hang — tried and failed.** Rewriting cmd 0x17's
script in place as `CS_BEGIN_CUTSCENE(1,2); CS_DESTINATION(dest,1,2); CS_END` exports fine through
ZAPD (link_home cutscene 796B -> 100B) but the game still freezes (0.00 pixel delta, never leaves
scene 0x34). `stub_cutscenes()` is kept in `build_rom_inline.py` but it is NOT a solution. Do not
retry this without new information.

**THE TEXTURE SKIP LIST IS REACTIVE AND NOW COSTS VISIBLE QUALITY.** Loading malon_stable exposed a
**ci4** texture with the same degenerate tile setup as the i4 pair -> Metal height-zero abort in
`ImportTextureCi4`. Adding `2:0` to `ML_SKIP_FMT` stops the crash but leaves **197 textures
unresolved (188 ci4 + 9 i4, ~15% of 1281)** -- screenshot-confirmed as broken/garbage WALL textures
in the starting interior, and texDecl errors 0 -> 7. Each new scene visited can expose another
format. This is a workaround, not a fix.

**THE REAL FIX: stock SoH 9.2.3 has NO zero-dimension guard; SOHCombo's libultraship does.**
Verified by reading the cloned 9.2.3 source directly, not inferred.
- **SOHCombo CANNOT produce a stock SoH binary**: its `soh` is `add_library(... SHARED)` and the only
  executable target is `ComboShip` (combo/CMakeLists.txt). Its build dirs contain ZAPD/OTRExporter only.
- So: **cloned `HarbourMasters/Shipwright` tag `9.2.3`** (matches the stock binary, so the mod's
  `portVer 9.1.1` and the existing `oot.o2r` stay valid) to
  **`/Volumes/MacMiniExternal/AI_Projects/Shipwright`** (150MB, `--depth 1 --recurse-submodules`).
- Guard added to `libultraship/src/fast/backends/gfx_metal.cpp` in
  `GfxRenderingAPIMetal::UploadTexture`: early-return when `width == 0 || height == 0`.
- Build: `cmake -H. -Bbuild-ml -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0`
  (configure OK in 36s with Homebrew cmake 4.4.3 / ninja / GLEW / nlohmann_json / libpng), then
  `cmake --build build-ml -j10`.
- **Once it runs: drop BOTH format skips (`ML_SKIP_FMT=""`) and re-inline all 197.** Only genuinely
  degenerate textures get skipped at draw time instead of aborting the process.

**SESSION 5 RESULT — SOURCE-BUILT SoH 9.2.3 WITH THE GUARD WORKS. Broken walls fixed.**
Screenshot-verified: ML's starting stable interior renders with correct wooden plank walls, beams,
bed, hay, logs and floor (previously the walls were repeating ci4 garbage). Stable at ~11% CPU,
**0 assertions, 0 `Texture is null`, 0 texDecl errors, 1281/1281 textures inlined, 0 unresolved.**

**Build recipe (works, ~5 min on a 10-core Mac mini):**
```
cd /Volumes/MacMiniExternal/AI_Projects
git clone --depth 1 --branch 9.2.3 --recurse-submodules --shallow-submodules \
  https://github.com/HarbourMasters/Shipwright.git
# add the guard (see below), then:
cd Shipwright
cmake -H. -Bbuild-ml -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build-ml -j10          # 1610/1610, 0 errors
cp /Volumes/MacMiniExternal/AI_Projects/SOH/soh.app/Contents/Resources/soh.o2r build-ml/soh/
```
- **Binary: `Shipwright/build-ml/soh/soh-macos`** (60MB arm64, NOT an .app bundle).
- **The guard** goes in `libultraship/src/fast/backends/gfx_metal.cpp`,
  `GfxRenderingAPIMetal::UploadTexture`: early-return on `width == 0 || height == 0`. Stock 9.2.3
  genuinely lacks it (verified in the cloned source); SOHCombo's libultraship already has it.
- Engine assets: copy `soh.o2r` from the stock 9.2.3 bundle -- same version, so it just works.
  Game assets stay in `ml_profile/oot.o2r`. Tag 9.2.3 was chosen precisely so the mod's
  `portVer 9.1.1` and the existing `oot.o2r` remain valid.
- Launcher: **`/Volumes/MacMiniExternal/AI_Projects/SOH/run_ml_src.sh`** (same `SHIP_HOME=ml_profile`,
  must `cd` to the binary's dir first). `run_ml.sh` + the stock app are untouched for A/B comparison.
- Only linker noise: "building for macOS-11.0, but linking with dylib ... built for newer version
  26.0" for opus/opusfile/libogg. Harmless.

**WITH THE GUARD, DROP BOTH TEXTURE SKIPS:** run the pipeline with `ML_SKIP_FMT=""`. The whole
reactive skip-list problem is gone -- a degenerate texture is now a skipped draw, not an abort.
`cycle.sh` is parameterised: `ML_LAUNCHER=./run_ml_src.sh ML_PGREP="soh-macos"` (plus
`ML_SKIP_SLOTS="" ML_SKIP_FMT=""`).

**The source build is also the unlock for everything else previously blocked on "no SoH built from
source"** -- notably the cutscene dispatcher (ML cutscenes still hang/crash vanilla's
`Cutscene_ProcessCommands`; currently worked around with `SkipCutscene.Intro` + the slot-52 start
remap) and any real start-menu work.

**PROJECT NAME: "MissingShip"** (Kevin named it 2026-09-18). Packaged alpha lives at
**`/Volumes/MacMiniExternal/AI_Projects/MissingShip/`** — self-contained: `bin/soh-macos` +
`bin/soh.o2r`, `mod/MissingLink_Scenes.o2r`, `home/` (its own SHIP_HOME with oot.o2r),
`profile/` seed settings, `pipeline/` (full ROM->o2r toolchain), `docs/` (the engine diff),
`MissingShip.sh` launcher, and a README stating what works and what does not.

**BREAKTHROUGH — ML REUSES VANILLA ACTORS AND OBJECTS. Two of my own hacks were wrong.**
The tables at code `0xF8B50` (37 entries, 36 overlay files) and `0x102EDC` (19 entries, 18 files)
are only ML's **additions**, NOT the whole tables. The rooms actually reference **94 distinct actor
IDs and 80 distinct object IDs in VANILLA's id space** (max actor 337, max object 399; SoH ships
474 actors / 404 objects). Spot-checked and coherent: object 5=OBJECT_CROW, 124=OBJECT_BV,
241=OBJECT_MASTERKOKIRIHEAD, 273=OBJECT_NWC; actor 273=ACTOR_EN_BLKOBJ, 94=ACTOR_EN_BB,
85=ACTOR_EN_DEKUNUTS, 337=ACTOR_EN_ZL3.
- So **`strip_actors` and `fix_objects` were over-defensive and made the world empty geometry.**
  The object-bank crash they were added for was really the command-SHIFTING corruption, fixed
  separately much later; I never revisited them. **Both are now OFF by default** (opt back in with
  `ML_STRIP_ACTORS=1` / `ML_REMAP_OBJECTS=1`). Verified `RESULT: OK`, 0 errors, with them restored.
- Lesson: when a defensive workaround is added for a crash whose real cause is later found
  elsewhere, go back and remove the workaround.

**ROOT CAUSE of the black/garbage walls was ALSO mine: `fix_settimg_fmt`.** Two different things
read the format and I conflated them:
- **SETTILE** fmt/siz drive DECODING (`ImportTexture` reads `texture_tile[tile].fmt/.siz`).
- **SETTIMG** siz drives the LOADBLOCK BYTE MATH (`word_size_shift`: 4b -> `size>>1`,
  16b -> `size<<1`).
ML correctly used a **16b block load feeding a 4b render tile**. Forcing SETTIMG to match the render
tile shrank `size_bytes` 4x, so `height = sizeBytes / lineBytes` collapsed to 0 -> first the Metal
abort, then (with the guard) skipped draws = black walls. Offline the same tiles compute
512B / 32B lines = height 16, which proved the DL data was always fine.
**`fix_settimg_fmt` is now OFF by default** (`ML_FIX_SETTIMG_FMT=1` to restore). With it off:
**0 `CI4 ZERO-DIM` events**, 0 assertions, 0 unresolved textures, walls render correctly.
It never fixed the psychedelic colours it was added for — decoding never read SETTIMG.

**Instrumenting the source build is how this was found.** Added an `SPDLOG_ERROR` in
`ImportTextureCi4` dumping sizeBytes / tileLine / resultLine / fullImgLine / loadedLine / hscale /
tmem / palette / resource path whenever a dimension is zero. Incremental rebuild is ~1 min.
Keep doing this instead of black-box bisection.

**MissingShip PACKAGING — ship it as an .app bundle, not a bare binary (2026-09-18).**
`MissingShip.app` with `CFBundleIdentifier = com.missingship.MissingShip`, ad-hoc signed.
- **Why it matters:** macOS attributes per-app audio by BUNDLE IDENTITY. A bare Mach-O is invisible
  to per-app volume tools (Kevin uses **QuietDown** — see [[quietdown-project]]) and also never
  appears in the accessibility app list, which is why `app_screenshot`/`list_apps` could not find it
  and I had to enumerate windows via Quartz `CGWindowListCopyWindowInfo`.
- **Layout is dictated by `getMainBundlePath()` = `[[NSBundle mainBundle] resourcePath]`**
  (`libultraship/src/ship/utils/AppleFolderManager.mm`): for a bare binary that is its own
  directory; for an .app it is `Contents/Resources`. So `assets/` + `soh.o2r` go in
  `Contents/Resources`, binary + `gamecontrollerdb.txt` in `Contents/MacOS`. Mirrors stock SoH.
- `SHIP_HOME` set both by the launcher AND by `LSEnvironment` in Info.plist (so double-click works).
- **`pkill -f` IS DANGEROUS in a launcher**: it matches any shell whose command line merely contains
  the pattern — including the tool call writing the script — and killed the parent (exit 144).
  Use **`pkill -x MissingShip`** (exact process name).
- Kevin's instruction: **only ever run ONE instance at a time.**

**VERIFICATION LESSON (cost several false reports this session):** process liveness is NOT evidence
of correctness. Three false readings in a row: a 30s `RESULT: OK` on a run that crashed at 60s; an
"ALIVE" report on a process showing an "Extractor assets not found" dialog; and `pid=none` on a
perfectly healthy game because `pgrep -f "MissingShip/bin/soh-macos"` cannot match a process whose
command line is `./soh-macos` (the launcher `cd`s first). Also `timeout`'s exit code **124** means
the process survived the whole window -- that is SUCCESS, not failure. **Always confirm with a
window screenshot** (`screencapture -x -l <wid>`), and get the wid from Quartz when the app has no
bundle. `setsid` does NOT exist on macOS.

**MILESTONE: NPCs AND DIALOGUE WORK.** Screenshot-verified: an NPC spawns in ML's starting area and
a conversation renders with a working text box. That confirms the restored actor/object lists
function end to end, not just at load time.

**THE COMPLETE TEXTURE FORMAT RULE (2026-09-18) — this resolves the whole psychedelic/black/streaky
saga. Three different things, never conflate them:**
1. **`SETTIMG` siz -> LOADBLOCK byte math ONLY.** `word_size_shift`: 4b `size>>1`, 16b `size<<1`.
   **NEVER rewrite it** (`fix_settimg_fmt` is off; it collapsed height to 0 -> black walls).
2. **render `SETTILE` fmt/siz -> DECODING, declared format and bpp.** `ImportTexture` reads
   `texture_tile[tile].fmt/.siz`. The render tile is **any tile != 7** (7 = `G_TX_LOADTILE`);
   `gsDPLoadMultiBlock` renders from tile 1, so never hardcode tile 0.
3. **render `SETTILESIZE` -> width/height.**

**Lon Lon Ranch streaking (fixed):** the inliner was taking fmt/siz from **SETTIMG**, which carries
the *load* format — `settimg_siz=2` (16b) on every tile in that room — so all 16 CI8, 7 CI4 and 3 IA8
textures were declared **rgba16/ia16**: wrong bpp, wrong inlined byte count, wrong decode = vertical
streaks. Fixed by tracking the render SETTILE. spot20_room_0 went **34 -> 59 inlined textures**, and
the exported resource headers now read CI8/CI4/IA8/RGBA16 matching the DL exactly.
**Note the two passes were entangled:** `fix_settimg_fmt` had been masking the inliner reading the
wrong source, which is why removing it surfaced this.

**Offline verification technique that found it (use this instead of guessing):** walk the room DLs
pairing `G_LOADBLOCK` texel count with the render `G_SETTILE`, compute
`size_bytes` (via the SETTIMG siz shift), `lineBytes = line*8`, `height = size/line`,
`width = line/bpp`, then diff that census against `inlined.json`. Any format or count mismatch is
the bug. Confirm afterwards by reading the exported resource headers straight out of the .o2r
(little-endian `fmt,width,height,datasize` at **0x40**; TextureType 1=RGBA32 2=RGBA16 3=CI4 4=CI8
5=I4 6=I8 7=IA4 8=IA8 9=IA16).

**CHAMBER OF THE SAGES TELEPORT (fixed) — exits landing in unrelated vanilla scenes.**
Symptom: walking into a house at Lon Lon Ranch warped the player to the Chamber of the Sages.
Log gave it exactly: `Scene Init - sceneNum: 0x44, entranceIndex: 0x6c`. ML entrance 108 =
(scene 54, spawn 1) = malon_stable's interior; vanilla entrance 108 = (68, 0) = kenjyanoma.
Two of my own changes combined:
1. **The start-fix moves malon_stable off slot 54 onto 52**, so scene 54 ships nothing and any ML
   exit aimed at 54 falls through to vanilla's occupant.
2. **`remap_exits` ran its "is this scene populated?" check BEFORE translating the scene id**, so a
   raw scene-54 target looked unshipped, `break`-ed out of that exit list entirely, and left the ML
   id in place. Silent: the reference was never even counted.
**Fixes in `ml_entrance.py`:** `SCENE_TRANSLATE = {54: 52}` applied **before** the populated check,
plus a same-scene fallback (`min` of any vanilla entrance reaching that scene) when no vanilla
entrance matches the exact spawn -- wrong spawn in the right building beats an unrelated scene.
Vanilla only defines spawns 0 and 1 for slot 52, so ML spawns 2/3 land at spawn 0.
**Result: 60/60 exit references remap, 0 left raw (was 52/8).** The 8 "no vanilla equivalent" exits
recorded earlier were exactly this bug waiting to fire.
**Lesson: a validity check that `break`s makes bugs invisible — the count silently dropped from 60
to 55 references visited, which is how the gap was spotted. Always compare totals, not just successes.**

**MINIMAP PORTED (2026-09-18) — and a MAJOR correction to the ML code VRAM base.**

**CORRECTION: ML `code`'s VRAM base is `0x8001ce60`, NOT `0x8000b2e0`.** The earlier figure does not
resolve pointers in the data region. Derived without guessing, by an offset signature: vanilla's
`MapData` has `owMinimapPosX` at field 11 and `owMinimapWidth` at field 16, and those two arrays sit
`0x184` apart, so scan for two u32 exactly 20 bytes apart differing by exactly `0x184`. That gives
`MapData @ code 0x108998`, base `0x8001ce60`, **28/28 pointers resolving** — and every array then
decodes to exact vanilla values, which is independent confirmation. **Use this technique whenever a
base is in doubt: an internal offset relationship is base-independent.**

**THERE WERE NO MINIMAP TABLES TO PORT.** ML's `owMinimapPosX`, `owMinimapPosY`, `owMinimapWidth`,
`owMinimapHeight`, `owEntranceIconPosX/Y` are **byte-identical to vanilla**. ML never customised the
minimap geometry -- only the artwork. Diffing before porting saved writing an engine patch that
would have changed nothing.

**ML's `map_grand_static` is dma 111 (58192 B), NOT dma 25.** dma 25 carries that name from the
debug-ROM filelist but ML replaced the slot with something else (16240 B, decodes as noise at every
format). Found by content search: the real blob is ~58184 B (exactly vanilla's payload total) and is
>45% zero bytes. **Do not trust the first-28 debug filelist names for ML's CONTENT — only names.**
All 24 areas decode cleanly as **IA4**, dimensions from owMinimapWidth/Height, sliced with
owMinimapTexOffset/Size (read the offsets as **u16** -- as s16 they go negative past 32767).

**Port method (same as the title cards):** slice the blob per area, reuse VANILLA's resource header
verbatim (identical format/dims), write to `textures/map_grand_static/<Name>MinimapTex` in the mod
archive. 24 textures injected covering 20 areas plus 4 state variants (drained Lake Hylia, broken
Gerudo bridge, explored Graveyard, and the `gGerudosFortessMinimapTex` typo slot).
`pipeline/inject_minimaps.py`, wired into `ms_cycle.sh`.

**CORRECTION (same day): ML DOES NOT SHIP CUSTOM OVERWORLD MINIMAPS.** Byte-diffing every area of
ML's map_grand_static (dma 111) against vanilla's resources: **19 of 20 are IDENTICAL to vanilla**;
only **area 12 (Gerudo's Fortress slot)** is ML-custom. So the minimap not matching the area is
**ML's own behaviour, not a port defect** — on real hardware ML shows the same vanilla minimaps over
its redrawn world. `inject_minimaps.py` is therefore a near-no-op (it legitimately fixes area 12 and
is proven to work — the in-game minimap does come from the injected resource), but it cannot make
the minimap match: there is no ML artwork to port.
**I claimed the decoded maps were "clearly custom, not vanilla shapes" BEFORE diffing them — wrong,
that was eyeballing against memory of OoT.** Always byte-diff against the vanilla resource before
asserting that romhack data is custom; it is cheap and it is the whole answer.
If a matching minimap is ever wanted it has to be DRAWN (IA4, dims from owMinimapWidth/Height) and
injected through the existing path.

**MINIMAP HIDDEN BY DEFAULT (Kevin's call, 2026-09-18).** Since ML ships no custom overworld
minimaps, showing vanilla's map over ML's world is worse than showing none. Set
**`CVars.gCosmetics.HUD.Minimap.PosType = 4`** (HIDDEN, from the `PosType` enum in
`soh/soh/Enhancements/cosmetics/cosmeticsTypes.h`: 0 ORIGINAL_LOCATION, 1 ANCHOR_LEFT,
2 ANCHOR_RIGHT, 3 ANCHOR_NONE, **4 HIDDEN**, 5 ANCHOR_TO_LIFE_METER). No engine change needed --
`z_map_exp.c:807` skips the minimap draw and `z_map_mark.c:186` suppresses its markers, so the whole
element goes away with no orphaned position dot. Applied to BOTH `home/shipofharkinian.json` (live)
and `profile/shipofharkinian.json` (the seed copied on first run).

**CORRECTION — SoH DISCARDS UNREGISTERED CVar SECTIONS FROM shipofharkinian.json.** Writing
`CVars.gCosmetics.HUD.Minimap.PosType` into the profile did NOT work: SoH rewrites the config at
startup and dropped the whole `gCosmetics` section (the live file keeps only gDeveloperTools,
gEnhancements, gGeneral, gOpenWindows, gSettings). Cosmetic CVars appear to register only once the
Cosmetics editor initialises. **Always re-read the config after launch to confirm a setting stuck** —
`gSettings`/`gEnhancements` edits DO persist (BootSequence, SkipCutscene etc. all held), but
`gCosmetics` does not.
**Working fix = change the default in the source** (we build the engine anyway):
`soh/src/code/z_map_exp.c` has 34 `HUD.Minimap.PosType` call sites but only **TWO are draw gates** —
the dungeon minimap (~line 807, `!= HIDDEN`) and the overworld minimap (~line 897, `!= 4`). Change
the CVarGetInteger default at those two from `0` to `HIDDEN`; leave the other ~32 (pure positioning
maths, only reached once drawing is already decided). The Cosmetics editor still overrides it, so
the minimap can be switched back on. Diff saved at `MissingShip/docs/soh-missingship.patch`.

**MAJOR REFRAME (2026-09-18): THE OPENING CUTSCENE WAS PROBABLY NEVER HUNG — and ML's cutscene
scripts are NOT malformed.** Parsing ML's link_home script with vanilla's exact rules
(`Cutscene_ProcessCommands`, z_demo.c:1697) walks cleanly to the end: **20 commands, all standard
vanilla opcodes, no unknown types** — `CAM_EYE`(8 pts), `CAM_AT`(8 pts), `SCENE_TRANS_FX` x9,
`TEXTBOX` x6 groups, then `TERMINATOR/DESTINATION -> entrance 119`.
It is a **text-driven prologue staged in a deliberately EMPTY room** (ML's link_home is a 128-byte
stub with no geometry), so:
- the black screen is BY DESIGN, not a rendering failure;
- my "0.00 pixel delta => hung" measurement was a black room with a textbox waiting for input;
- **SoH's default A button is the keyboard `X` key**
  (`libultraship/.../ControllerDefaultMappings.cpp:49  { BTN_A, { LUS_KB_X } }`). I had sent
  Return, which does not advance an OoT textbox — so "input ignored" was my own test error.
**Therefore the start-fix and SkipCutscene.Intro were BOTH unnecessary workarounds.** Reverted:
`ML_NO_START_FIX=1` restores malon_stable to slot 54 and ML's stub to 52; all SkipCutscene toggles
are 0; `SCENE_TRANSLATE` is now conditional on the start-fix so exits still resolve (60/60 remapped).
**Cutscene command strides for offline parsing** (from z_demo.c): most cmds `4 + n*48`;
`CS_CMD_09`/`SETTIME` `4 + n*12`; `TEXTBOX` `4 + n*12`; `SCENE_TRANS_FX` 12; `TERMINATOR` 12;
camera cmds (0x1/0x2/0x5/0x6/0x7/0x8) = 8 + 16 per point until `continueFlag == -1`; there IS a
`default:` case that skips `4 + n*48`, so unknown opcodes only desync if their stride differs.

**THE LEFTOVER-WORKAROUND PATTERN BIT AGAIN (2026-09-18).** After proving ML's cutscene scripts are
fine, restarting still gave a black screen **with a HUD** (HUD visible = gameplay state, NOT a
cutscene playing). Cause: **`stub_cutscenes` was still running UNCONDITIONALLY** in
`build_rom_inline.py`, replacing all 13 scenes' scripts with my 100-byte stub and destroying the
opening prologue. I had gated `strip_cutscenes` behind `ML_STRIP_CS` but never gated `stub_cutscenes`.
Now behind **`ML_STUB_CS`** (default off). Verify with the resource size: ML's real link_home
cutscene is **796 B**; my stub was **100 B**.
**This is the THIRD time a workaround outlived the bug it was added for** (`fix_settimg_fmt`,
`strip_actors`/`fix_objects`, now `stub_cutscenes`). **Rule: when a root cause is found, immediately
grep the pipeline for every transformation added while chasing it and gate or delete them.**
**Diagnostic shortcut learned:** black screen **with HUD** = normal gameplay in an empty/■ room;
black screen **without HUD** = a cutscene is actually running. That one bit distinguishes the two
failure modes instantly.
Save-state check that confirmed the trigger was fine: `home/Save/fileN.sav` is JSON —
`entranceIndex: 187` (0xbb) and `cutsceneIndex: 65521` (0xFFF1) is exactly the new-game intro state.

**CONFIG-EDIT HAZARD: SoH OVERWRITES shipofharkinian.json ON EXIT.** Editing the profile while the
game is RUNNING is silently undone — the running instance saves its in-memory CVars over your edit
when it quits. This is how `SkipCutscene.Intro` stayed `1` after I had "disabled" it (and
`SkipIntro.cpp` then zeroed `gSaveContext.cutsceneIndex`, killing the opening cutscene and leaving
the empty stub room + HUD).
**Correct procedure: `pkill -x MissingShip` FIRST, wait, edit the json, launch, then RE-READ the
file to confirm the value survived.** Combine with the earlier note that `gCosmetics` is dropped
entirely — only `gSettings`/`gEnhancements`/`gGeneral`/`gDeveloperTools`/`gOpenWindows` persist.

**Cutscene debugging: instrument `func_80068ECC` (z_demo.c ~2069).** A log of
`frames/state/segment/totalEntries/endFrame/cutsceneIndex` there is decisive, because the whole
block is gated on `gSaveContext.cutsceneIndex >= 0xFFF0`. **Zero log lines = cutsceneIndex was
already cleared**, which points at a CVar/skip, not at the cutscene data. Verified good along the
way: trigger fires (`Cutscene_HandleConditionalTriggers ... cutsceneIndex: 0xfff1`), resource loads
by name, ZAPD's parse matches an offline parse (20 entries / endFrame 4000), and the exported
resource is byte-compatible with a stock 9.2.3 cutscene (`[count, totalEntries, endFrame, cmds...]`
at 0x40) — so SOHCombo's ZAPD exporting for SoH 9.2.3 is fine for cutscenes.

**MILESTONE (2026-09-18): ML'S OPENING CUTSCENE AND REAL STARTING POSITION NOW WORK END TO END,
with ML's real data and ZERO workarounds.** Verified:
`Scene Init - sceneNum: 0x36, entranceIndex: 0x2f9` — the prologue runs ~75s in ML's staging room,
then transitions to malon_stable (scene 54) via vanilla entrance 761. 0 assertions, 0 texture nulls.
All of `ML_NO_START_FIX=1` (real slot layout), SkipCutscene all-off, `ML_STUB_CS` off.

**TWO REAL BUGS, both needing an engine patch (`docs/soh-missingship.patch`):**

**1. `cmd->base` in CS_CMD_TERMINATOR is a DESTINATION ID in vanilla, but ML uses it as a DIRECT
ENTRANCE INDEX.** `Cutscene_Command_Terminator`'s `switch (cmd->base)` covers only **1..67 and has
NO `default:`**. ML's destinations: **119** (the opening -> malon_stable) and **106** fell straight
through = player stranded; **24** and **55** were worse, colliding with unrelated vanilla cases and
warping somewhere wrong. Fix is split:
- pipeline `fix_cutscene_destinations()` (ml_entrance.py) rewrites each base to
  **`0x8000 | vanillaEntrance`** (bit 15 can never collide with a real destination ID);
- engine adds a `default:` that recognises the sentinel and uses the low 15 bits as the entrance.
**When inserting that `default:`, note `Cutscene_Command_Terminator` contains TWO `switch (cmd->base)`
statements** — the CreditsFix *delay* switch comes first. I patched the wrong one and it silently did
nothing. Anchor on `gSaveContext.cutsceneIndex = 0;` (precedes only the destination switch) and
assert `ENTR_CUTSCENE_MAP_0` appears in the first case before editing.

**2. `TRANS_MODE_CS_BLACK_FILL` (17) swallows the queued transition.** The new-game spawn leaves
`play->transitionMode` at 17 for the whole prologue (this is what legitimately holds the screen
black). Play_Update's dispatch (`z_play.c:738`) only starts a transition when
`transitionMode == TRANS_MODE_OFF`, so the trigger is skipped; then the CS_BLACK_FILL handler
(`z_play.c:~1144`) clears `transitionTrigger` outright as soon as `cutsceneTransitionControl` is
non-zero — which `Cutscene_Command_Terminator` sets to 1 immediately BEFORE the destination switch.
Self-defeating. **Fix: in the destination handler also set `play->transitionMode = TRANS_MODE_OFF;`
and `gSaveContext.cutsceneTransitionControl = 0;`.** Vanilla presumably never fires a destination
while still in black-fill, so this path was untested upstream.

**The diagnosis technique that worked (do this, not code-reading):** a **state-change tracer** —
log `mode/trigger/nextEntrance` only when one of them changes. A capped per-frame logger is useless
here: my first version burned all 20 lines on the initial spawn transition before the prologue even
started. The change-tracer produced exactly 4 lines and the bug was visible in 2 of them.

**ML MESSAGE TEXT PORTED (2026-09-18) — `pipeline/inject_messages.py`.**
SoH keeps ALL dialogue in ONE resource: `text/nes_message_data_static/ntsc_nes_message_data_static`
(type `TXTO`). Layout: 0x40 header, then `u32 count`, then per message
`{u16 id, u8 textBoxType, u8 textBoxPos, u32 len, len bytes}`.
**ML keeps vanilla's message SYSTEM but rewrote the text:**
- message table at **ml_code 0x12e4c8**, 2115 entries of `{u16 textId, u8 typePos, u8 pad, u32 seg}`,
  ascending by id; `typePos` packs type in the high nibble, position in the low nibble;
- the text blob is **dma 96** (229664 B, ~89% printable) — **NOT dma 21**, whose debug-filelist name
  `nes_message_data_static` is misleading, exactly like dma 25 was for the minimap blob. Found it by
  decompressed-size + printable-ratio scan. **Third time a first-28 filelist name lied about content.**
- **Trim each message at its 0x02 (MSG_END) terminator** before comparing/exporting: slicing to the
  next entry's offset includes padding and falsely reports ~1648 differences. Correctly trimmed,
  **265 messages genuinely differ** and get replaced; the rest keep vanilla's.
ML's real prologue narration recovered (ids 0x1065-0x106c): "According to the legend passed down...",
"There was once a boy, who lived his life peacefully, deep within the forest.", ... ending
"...But what led him on that journey, and where did Navi go?" — i.e. the Navi-search premise.

**CODE REVIEW PASS 1 (2026-09-18) — 8 findings, all fixed. `./ms_cycle.sh` with NO env vars now
produces the correct build** (25 scenes, 72 rooms, 60 exits remapped, 4 cs destinations fixed,
0 destructive passes). Previously that exact documented command silently produced a **21-scene**
archive with a broken opening, because two workaround DEFAULTS were never updated:
1. `ml_map.py` `ML_SKIP_SLOTS` defaulted to `'0,3,7'` — obsolete once MQ dungeons were routed to
   `xml/scenes/dungeons/` (→ scenes/nonmq). **Now defaults to empty.**
2. `ml_map.py` start-fix applied unless `ML_NO_START_FIX` — obsolete once the opening cutscene
   worked. **Inverted to opt-in `ML_START_FIX`**, and `ml_entrance.SCENE_TRANSLATE` now follows it.
**Lesson (4th occurrence): a workaround's DEFAULT must be flipped the moment its bug is really
fixed, not just made overridable.**
3. **`inject_messages.py` truncated 28 of 2046 messages**: `find(b'\x02')` treats a control-code
   ARGUMENT equal to 0x02 as MSG_END. Fixed with a control-code walk using
   `CTRL_ARGS={0x05:1,0x06:1,0x07:2,0x0C:1,0x0E:1,0x12:2,0x13:1,0x14:1,0x15:3,0x1E:1}`.
   Replaced count 265 → **269**, resource +400 bytes of recovered text.
4/5. Removed leftover engine instrumentation (`ML-DEST` log; the CI4 ZERO-DIM SPDLOG_ERROR, which
   also dereferenced `GetInitData()` unchecked inside a per-frame path).
6. The 32 appended entrances had `displayTitleCard=true` → popped title cards on interior arrivals.
   Now `false`. (DEFINE_ENTRANCE args: enum, scene, spawn, continueBgm, displayTitleCard, in, out.)
7. `z_map_exp.c:897` compared against literal `4` while :807 used `HIDDEN`. Both use the enum now.
8. Python string-surgery had left switch closers at column 0 in vendored upstream source. Restored.

**SETTINGS REVERTED TO DEFAULTS:** `gDeveloperTools.ResourceLogging=0`, `LogLevel=2` (spdlog info;
release default per OTRGlobals.cpp). These had been on since the texture debugging and were dumping
every scene command as XML — run log went **725 → ~21 lines**.

**TextureFilter: REVERTED to 0 (FILTER_THREE_POINT, SoH default).** Setting it to 2 (FILTER_NONE /
point sampling) removed the foliage halos but made every surface blocky at modern resolution —
a bad trade. **OPEN ITEM: foliage alpha-edge halos.** Ruled out port-side causes: declared formats
match the DL render tiles exactly (spot12: 85 rgba16 / 20 ci8 / 1 ci4 both sides), transparent
texels are NOT white (0% in all but one texture), and there is no multitexturing (all draws single
tile). So it is three-point filtering interpolating ML's 1-bit-alpha edges. Likely correct fix:
**alpha-edge dilation in the texture pipeline** (bleed opaque RGB outward into transparent texels)
rather than a global filter change.

**ML AUDIO PORTED (2026-09-18) — via SoH's OWN extractor, NOT Shipify. `pipeline/inject_audio.py`.**
Verified running: 574 resources injected (110 sequences, 38 soundfonts, 425 samples), 0 assertions,
0 errors.

**Do NOT follow Shipify's README for SoH 9.2.3.** Shipify (github.com/Zelllll/shipify, Java, ~7
stars, last push 2025-01) targets the LEGACY flow: `patch_rom` + `patch_files.txt` + hand-edited
`Audio.xml` offsets inside a Shipwright source tree, then regenerate the whole .otr. SoH 9.2.3
stores audio as individual **v2 resources**: `audio/sequences/*` (magic `OSEQ`), `audio/fonts/*`
(`OSFT`), `audio/samples/*_META` (`OSMP`), plus one `audio/audio` manifest (`OAUD`). Shipify is
still useful as a *reference* for which inputs matter, and for custom objects (Z64Object.java).

**The maintainable path = ZAPD's own `ZAudio` + `OTRExporter/AudioExporter`**, driven by an
`Audio.xml` exactly like vanilla's:
```
<File Name="code" OutName="audio" RangeStart="0x0" RangeEnd="0x12CBB0">
  <Audio Name="audio" Offset="0x00" SoundFontTableOffset=... SequenceTableOffset=...
         SampleBankTableOffset=... SequenceFontTableOffset=...>
```
Copy `soh/assets/xml/GC_MQ_D/audio/Audio.xml` VERBATIM — **ML kept vanilla's audio tables at the
SAME code offsets**: SoundFontTable `0x138270` (38), SequenceTable `0x1386A0` (110),
SampleBankTable `0x138D90` (7), SequenceFontTable `0x1384E0`. Verified the extents match ML's files
exactly (SoundFontTable max end == 87680 == ML Audiobank size; SequenceTable 170240 <= Audioseq
170256; all 7 sample banks inside the 4990608-byte Audiotable).
**Prereqs:** filelist must name **dma 114 = `code`** (ZAudio reads `<File Name="code">`), and
dma 3/4/5 keep Audiobank/Audioseq/Audiotable. Use the **Shipwright 9.2.3 ZAPD**
(`build-ml/ZAPD/ZAPD.out`), not SOHCombo's, so the v2 format matches the engine.

**REAL ZAPD BUG FOUND AND FIXED (upstreamable) — `docs/zapd-missingship.patch`.**
`ZAudio::ParseRawData`'s SEQUENCE loop skips `size == 0` entries but the SOUNDFONT loop did not.
ML zeroes out unused soundfonts while leaving their instrument/drum/sfx counts intact, so ZAudio
walked those counts from `ptr == 0` and read far past the audiobank -> `EXC_BAD_ACCESS` in
`ParseSoundFont`. 22 of ML's 38 fonts trigger it. Fix: `if (sft.size == 0) continue;` mirroring the
sequence guard. Not ML-specific — any hack that zeroes unused fonts hits this.
Note `ZAPDTR` is a SUBMODULE: capture its diff with `git -C ZAPDTR diff`, not from the parent repo.

**Prelude of Light (preludeoflight.com)** — browser-based OoT editor that reads/writes `.o2r` and
shows scenes/actors/collision/exits. Kevin confirmed it loads our mod archive. **Use it as a
VALIDATOR, not a source of truth** — our pipeline regenerates deterministically from the ML ROM, so
hand-edits there would be overwritten by the next `ms_cycle.sh`. Ideal for answering "our bug or
ML's data?" visually instead of via engine instrumentation.

**CORRECTION (2026-09-18): ML HAS NO CUSTOM OBJECTS. The "18 custom objects" claim was WRONG.**
The table at code **0x102EDC is the SKYBOX file table, not the object table.** dma 126-143 are
**9 skybox pairs** (texture + palette), which is why the sizes alternate 49152 / 256 — a skybox
texture is 5 concatenated sub-textures (4 x 128x64 CI8 @8192 + 1 x 128x128 CI8 @16384 = **49152**),
and the 256-byte files are its 16x8 RGBA16 TLUT.
**Every one is byte-identical to vanilla**: 7 of 9 TLUTs match vanilla by name (gSunriseSkyboxTLUT,
gSunsetSkyboxTLUT, gNightSkyboxTLUT, gSunrise/Day/NightOvercastSkyboxTLUT, gHoly0SkyboxTLUT), and
all 6 diffable textures are identical to vanilla when reconstructed as
`gXxxSkybox1Tex..5Tex` concatenated. **So there is nothing to port — no objects, no skyboxes.**
This is consistent with ML reusing vanilla actors and objects wholesale (94 actor IDs / 80 object
IDs, all inside vanilla's range).
**METHOD NOTE — how the first diff attempt was still wrong:** comparing ML's 49152 B file against
vanilla *resources* found "0 textures of this size" and I nearly called it custom. Vanilla stores
each skybox as 5 SEPARATE resources; you must concatenate them before diffing. **A size mismatch
means the comparison is wrong, not that the data is custom** — same trap as the minimaps.

**STILL TRUE AND NOW VERIFIED: the 36 actor overlays (dma 313-348) ARE real MIPS code.**
Checked by opcode distribution (ratio 0.63-0.77) and prologue signatures — `27 bd ff e0`
= `addiu $sp,$sp,-0x20`, `af bf 00 1c` = `sw $ra,0x1c($sp)`. Not portable as data; this remains the
one genuine machine-code barrier in the project.

**FOLIAGE HALO — STILL UNSOLVED. Four causes RULED OUT with evidence (do not re-test these):**
1. **Wrong declared format** — spot12's inlined formats match the DL render tiles exactly
   (85 rgba16 / 20 ci8 / 1 ci4 on both sides).
2. **Multitexturing** — every triangle draws with a single tile (625 on tile 0, 147 on tile 1).
3. **White RGB under transparent texels** (the classic alpha-bleed cause) — measured: RGBA16
   foliage textures hold leftover BROWN/GREEN art under alpha=0 (0% white in all but one texture),
   and all 11 ci8 TLUTs have transparent entries of **pure black** (RGB sum 0/93). Neither can
   produce a BRIGHT fringe.
4. **Soft alpha blending** — the foliage uses **alpha-TEST** render modes
   (`CVG_X_ALPHA|ALPHA_CVG_SEL`, i.e. G_RM_AA_ZB_TEX_EDGE style) on 88 of 89 draws, not FORCE_BL.
**`pipeline/dilate.py` (alpha-edge dilation for RGBA16) was written but deliberately NOT wired in** —
it barely changes these textures because their transparent regions already hold similar foliage
tones, and it addresses a cause now disproven. Keep it for when a real bleed case turns up.
**Setting `gSettings.TextureFilter = 2` (FILTER_NONE) does remove the halos but makes every surface
blocky — reverted to 0 (THREE_POINT). Not an acceptable fix.**
**NEXT STEP: get a frame with the halo actually visible (or inspect the leaf geometry in Prelude of
Light) before theorising further.** Remaining suspects: vertex colours/lighting on the leaf quads,
the combiner (only 4 distinct combiners in spot12), or overlapping coplanar leaf quads.

---

## UPDATE 2026-09-23 — pipeline moved off /tmp (it was destroyed)

macOS reaped `/tmp/mlwork`, taking every working file with it. **The pipeline now lives at
`/Volumes/MacMiniExternal/AI_Projects/MissingShip/pipeline/`** and all scripts hold that absolute
path. The source ROMs survived only because a Sep-18 snapshot sat in `SOH/ml_pipeline/`
(`tml_2_0.z64`, `ml_code.bin`) — those are now copied into the pipeline dir. Never work out of
`/tmp` again for this project.

Verified after the move: a full rebuild reproduces the shipped archive **byte-for-byte across all
4035 entries** (same names, same per-entry sha256; only zip timestamps differ).

**Two traps the move exposed, both now fixed in `ms_cycle.sh`/`cycle.sh`:**

1. **The filelist rewrite silently breaks audio.** The cycle rewrites `filelists/dbg.txt` in place
   every run, renaming scene/room entries to `mlfile_N`. dma 114 is ML's `code` file, and
   `Audio.xml` looks it up **by name**, so the rewrite left it as `mlfile_114` and ZAPD's ZAudio
   **segfaulted (exit 139)**. The scripts now force `lines[114]='code'` after the rewrite. Symptom
   to recognise: `audio extraction FAILED (exit -11)` and an archive of 3461 instead of 4035.
   Note ZAPD also prints `Unsupported argument: --otrfile` on every run — that is harmless noise,
   printed before the exporter gets its turn at the arg. Do not chase it.
2. **`scene_textures.json` must be REGENERATED, not copied.** It is a build input consumed by
   `gen_xml_inline.py` but produced by `gen_textures.py`, and it never lived outside `/tmp`. The
   stale Sep-18 copy in `SOH/ml_pipeline/` is 3KB covering the old 21-scene map; the correct
   regenerated file is 21KB. Copying it would have silently shipped a downgraded build.

**`SOH/ml_pipeline/` is a stale Sep-18 snapshot** — its `build_rom_inline.py`, `ml_map.py` and
`ml_entrance.py` all differ from the current ones and it has no `inject_audio.py`. Treat it as the
ROM source only; `MissingShip/pipeline/` is canonical.

## Continue / save-load behaviour (fixed 2026-09-23)

**Symptom:** loading an existing save -> HUD over a black screen. Log: `entranceIndex: 0xbb` ->
`sceneNum: 0x34` (link_home, ML's 128-byte intro staging stub) with `cutsceneIndex: 0`. Only NEW files
had ever been tested; the old start-fix workaround masked this.

**Cause, proven by diffing `Sram_OpenSave` (ML @0x800A8484 vs debug ROM @0x800A84A4, found via its
`SCENE_DATA_ID` debug string):** the two are identical except ML nops out the `savedSceneId ==
SCENE_LINKS_HOUSE` check and BOTH default-case stores (`+0x148-0x150`, `+0x17C`, `+0x184`). So ML never
overwrites the saved entrance for overworld saves: **continue resumes where you saved.** Dungeon
cases are untouched.

**Fix:** SoH already has exactly this as `gEnhancements.RememberSaveLocation` (it `break`s before the
same stores). Enabled in BOTH `MissingShip/home/` and `MissingShip/profile/shipofharkinian.json` --
it is ML's native behaviour, not a debug toggle, so it belongs in the stock profile.
Plus a guard in `z_sram.c` (in `docs/soh-missingship.patch`): if the continue entrance is still
`ENTR_LINKS_HOUSE_CHILD_SPAWN` (a save written inside the stub, or SoH's grotto/fairy-fountain
fallback, which ML lacks), set `cutsceneIndex = 0xFFF1` to replay the prologue, which routes the player
to the stable from ML's own data. Deliberately NOT a hardcoded redirect to the remapped stable
entrance (0x2F9) -- that would couple the engine to the pipeline's remap table.
`cutsceneIndex` is restored from the save inside `Sram_OpenSave`/`Save_LoadFile` and file select
never resets it, so the guard sticks.

**Gotcha:** SoH writes the save right after a continue loads, so a bad continue immediately
persists its bad entrance -- file 2 got stored at 0xBB this way.

## Audio crash: ML samples authored as MEDIUM_CART (fixed 2026-09-23)
**Symptom:** SIGSEGV on the audio thread (`AudioSynth_ProcessNote` -> `_platform_memmove`, garbage
address) a few seconds into Deep Woods (ML `spot10`, slot 0x5B, seq 0x2C -> font 12).
**Cause:** 16 of ML's 425 samples (its new ones, plus a re-authored "Goron Drum") carry
`medium = 2` (MEDIUM_CART, streamed from ROM by DMA). Vanilla's 449 are all MEDIUM_RAM. SoH keeps every
sample in RAM, but `ProcessNote` still routes non-RAM media through `AudioLoad_DmaSampleData`, which
treats the RAM pointer as a cartridge address. **Fix (data, engine untouched):** `inject_audio.py`
rewrites byte 0x41 of every `audio/samples/*_META` (resource header 0x40 + codec) to 0.
Ruled out first, with evidence: all font->sample refs resolve (38 fonts); all samples ADPCM with sane
loops and 16 states (note SoH's loader writes `loopStateCount` entries into a fixed `loop.state[16]`
with no bounds check -- a latent overflow if a hack ever ships more).
ML replaced font 12 (Forest_Temple) wholesale: 1128B vs vanilla 380B.
**Also:** `inject_audio.py` was append-only like `inject_objects.py`; both now REPLACE their resource
class, so incremental re-runs can't keep stale resources. A crash like this after any audio change:
check sample media first.

## CORRECTION (2026-09-23): the game starts in tokinoma -> Kokiri Forest, NOT the Lon Lon stable
Everything above that says the prologue "CS_DESTINATIONs to entrance 119 = scene 54 (malon_stable)" is
WRONG. A terminator's `base` is a destination CODE. Cutscene_Command_Terminator has TWO
`switch (cmd->base)` blocks; the earlier session read the first (an 11-case delay helper, 55..78) and
concluded "no case 119". The real one (second switch, 0..119 + our default) has case 119 ->
`nextEntranceIndex = 0x5F0`, and ML's copy is byte-identical (same jump table at code 0x11E08C vs debug
0x11E0AC, same body at 0x80066F44). 0x5F0 resolves through ML's OWN entrance table -> scene 0x43
`tokinoma` spawn 0, whose intro (code 106 -> ML 0x574) sends you to spot04 Kokiri Forest. The user
suspected this from playing. `ml_entrance.fix_cutscene_destinations` now resolves code -> entrance ->
ML table -> shipped entrance, parsing SoH's switch (handles `Entrance_OverrideNextIndex(ENTR_X)`), and
covers the ENTRANCE_CS intro scripts too. Known loss: destination cases also set state (case 119: noon,
white fade); the bit-15 escape path only transitions.
**Also fixed:** ml_entrance.py read SOHCombo's tables, hiding the 32 appended entrances, so exits fell
back to spawn 0 ("clipped camera / void on arrival"). It now reads Shipwright's (the shipped engine).
**VERIFIED IN-GAME 2026-09-23 (user: "this seems better to start"):** new file -> prologue (76 s, text
over black by design: 6 textboxes, 11 fades, fixed camera, no geometry/actors) -> terminator 0x8053 ->
tokinoma + its intro (flag 0xA1) -> terminator 0x80EE -> Kokiri Forest (0x55) + ML's spot04 intro
(flag 0xA3). One earlier run restarted the prologue at ~35 s instead (first textbox re-fired); NOT
reproduced -- suspect SoH's debug cutscene-skip (START with DebugEnabled on). Temporary LUSLOG traces
tagged `MissingShip-TRACE` are in z_demo.c/z_play.c for this; STRIP them once the opening is stable.

## ML's starting loadout (ported 2026-09-23)
ML's new-save templates (not code -- Sram_InitNewSave is identical to MQ-debug) start Link with: Ocarina
of Time (slot 7, on C-Left), Kokiri Sword (on B), Hylian Shield, Kokiri Tunic/Boots (equipment 0x1121),
Goron's Bracelet + Adult's Wallet (upgrades 0x1040), Epona's + Saria's Songs (questItems 0x6000), magic 0.
Applied in z_sram.c after Save_InitFile(false). Template location: code 0x10D6F0 (inventory), playerData
at -0x58, equips at -0x0C. Existing saves need a NEW FILE to get it.
Also: the chest in ML spot05 gives GI 0x60 (bullet bag 40) -- UNCHANGED in ML's GetItem table. In ML that
upgrade IS the "Soul-Charged Kokiri Sword" (text 0x0007 rewritten, pickup model object 0x17B replaced).
ML's GetItem table changed only 2 entries, both model-only: GI 0x39 -> object 0x146 (certificate model),
GI 0x10 -> object 0x11. The Blade Beam ability is claimed by ML's text; not yet located in player code.
