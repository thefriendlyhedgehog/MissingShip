# Porting a Missing Link custom actor

ML's custom actors are not at high actor ids. They occupy the eight slots vanilla OoT leaves as
`DEFINE_ACTOR_UNSET`: **0x001, 0x003, 0x005, 0x006, 0x017, 0x01A, 0x01F, 0x022**. Five are actually
placed in rooms; the other three must be spawned by code or cutscene.

| actor | placed | where | category | object slot | object status |
|-------|--------|-------|----------|-------------|---------------|
| 0x003 | 13x | turibori_r0 | BG | 0x40 | replaced by ML — **ported** |
| 0x006 | 2x | spot01_r0 (Kakariko) | NPC | 0x0C | replaced by ML — **ported** (`Ml_Villager`) |
| 0x005 | 2x | spot04_r0 (Kokiri) | BG | 0x7C | modified by ML |
| 0x01A | 1x | HAKAdan_r20 | BG | gameplay_keep | modified; no draw fn |
| 0x01F | 1x | link_home_r0 | BG | 0x04 | ML-only object, 153760B |

## Why each port is two halves

ML replaced the *contents* of the object slot each of these actors uses, so the display lists the
actor draws are at offsets that do not exist in vanilla's object. Actor 0x003 draws from segment-6
offsets 0x978 / 0x2878 / 0x3068 / 0x3728, while vanilla `object_jj.xml` has its display lists at
0x44A0–0x5690. Porting the C without porting the object gives you a crash or garbage geometry.

## Recipe

### 1. Read the overlay

```bash
cd <scratchpad>
python3 mldis.py <dma> "actor 0xNNN"      # capstone MIPS disassembly of .text
```

Find the overlay via ML's `gActorOverlayTable` at **code offset 0xF9440, stride 0x20**
(`vromStart, vromEnd, vramStart, vramEnd, loadedRamAddr, initInfo, name, allocType`). The
`ActorInit` at `initInfo - vramStart` gives category, objectId, instanceSize and the four function
pointers. Its `id` field must equal the table index — that is the check that the base is right.

The overlay's last word is the distance back to its `OverlayRelocationSection`
(`textSize, dataSize, rodataSize, bssSize, nRelocations, relocs[]`), which is how to find where
.text ends and .data/.rodata begin.

**Name every engine call from evidence, not memory.** The debug build embeds `osSyncPrintf` strings
in most functions, so follow each `jal` target's `lui`/`addiu` pairs to the strings it references
(`CollisionCheck_setOC()`, `Si2_draw_SV` = `SkelAnime_DrawFlexOpa`, ...). Leaf functions carry no
strings; read their bodies instead (`Actor_Kill` nulls update/draw at +0x130/+0x134). SoH still
uses some decomp placeholder names, e.g. the talk offer is `func_8002F2F4`.

**Check for self-kill gates before testing.** An actor that calls `Actor_Kill` in Init unless a save
flag is set vanishes the instant you spawn it, which looks identical to a broken port. Set the flag in
**Developer Tools → Save Editor → Flags → Saved Scene Flags** (32 unlabeled boxes, 8 per row; box *i*
is bit *i*).

Useful landmarks when reading the disassembly:
- `play + 0x1C24` is `&play->actorCtx`, so a call taking it plus 9 more args is `Actor_Spawn`.
- `0x8015E660` is `gSaveContext`; offset 0 is `entranceIndex`.
- `Actor + 0xF4/0xF8/0xFC` are `uncullZone{Forward,Scale,Downward}`.
- A float second argument travels in `a1` as raw bits when the first argument is a pointer (o32).

### 2. Extract the object

Add `pipeline/xml/objects/<name>.xml` declaring **only the display lists the actor draws** — ZAPD
follows them and pulls in the vertices and textures they reference, so a 54KB object never has to be
described by hand:

```xml
<Root>
    <File Name="object_ml_fishpond" Segment="6">
        <DList Name="gMlFishpond0DL" Offset="0x978"/>
    </File>
</Root>
```

The `File Name` must match the name given to that object's **dma index** in `filelists/dbg.txt`.
ML reorganised dmadata, so these indices carry object data under unrelated vanilla names (object
slot 0x40 lives at dma 12, whose vanilla name is `icon_item_nes_static`). The cycle script rewrites
that filelist every run, so add the index to `ML_OBJECTS` in `ms_cycle.sh` — otherwise the name is
silently lost on the next rebuild and ZAPD fails. Find the dma index through ML's `gObjectTable` at
code offset **0x10A6C8** (8-byte `RomFile` entries).

`inject_objects.py` then runs ZAPD over `xml/objects` and merges the result into `ml.otr`.

**Check the log for `texDecl == nullptr`.** It means ZAPD found a texture it could not size, and it
ships a fallback that renders as garbage. The cause seen so far: a display list that issues a render
`G_SETTILE` for a new texture but **no `G_SETTILESIZE` of its own**, silently inheriting the previous
texture's dimensions on that tile (the villager's CI8 at `0xC00`). Declare it by hand, with its
palette, and confirm the size two ways before trusting it: the `G_LOADBLOCK` byte count, and the
texture ending exactly where the next one begins.

```xml
<Texture Name="gMlVillagerTLUT_001BA0" OutName="gMlVillagerTLUT_001BA0" Format="rgba16" Width="256" Height="1" Offset="0x1BA0"/>
<Texture Name="gMlVillagerTex_000C00" OutName="gMlVillagerTex_000C00" Format="ci8" Width="32" Height="64" Offset="0xC00" TlutOffset="0x1BA0"/>
```

Then render any palettized texture with its TLUT and look at it: a wrong palette pairing still
decodes cleanly, it just decodes to noise.

### 3. Write the actor

Create `soh/src/overlays/actors/ovl_<Name>/z_<name>.{c,h}` in `engine/`, following any vanilla
actor for style. Reference ML assets through `soh/assets/ml_assets.h`, which declares them the way
`soh_assets.h` does:

```c
#define dgMlFishpond0DL "__OTR__objects/object_ml_fishpond/gMlFishpond0DL"
static const ALIGN_ASSET(2) char gMlFishpond0DL[] = dgMlFishpond0DL;
```

### 4. Register it — three edits, all required

1. `soh/include/tables/actor_table.h` — flip the slot **in place**:
   `DEFINE_ACTOR_UNSET(ACTOR_UNSET_3)` → `DEFINE_ACTOR(Ml_Fishpond, ACTOR_ML_FISHPOND, ALLOCTYPE_NORMAL)`.
   Editing in place keeps every actor id byte-identical, so no scene data is rewritten and nothing
   downstream shifts. Prefer this to `ActorDB::AddEntry`, which appends at `nextFreeId` and cannot
   target a chosen id.
2. `soh/soh/ActorDB.cpp` — add a description. `actorDescriptions` is an
   `unordered_map<u16, const char*>`; a missing key yields `nullptr`, and `AddEntry` builds a
   `std::string` from it. That is undefined behaviour, not a blank name.
3. `soh/soh/Enhancements/debugger/actorViewer.cpp` — remove the now-stale `ACTOR_UNSET_n` from its
   exclusion list, or the build fails on an undeclared identifier. Removing it is also what you
   want: the Actor Viewer can then spawn the actor anywhere, which is the easiest way to see it
   without walking to its scene.

### 5. Build

CMake globs sources at **configure** time, so a new actor directory needs a reconfigure:

```bash
cmake -S . -B build-ml && cmake --build build-ml --target soh -j8
```

Then copy `build-ml/soh/soh-macos` over `MissingShip.app/Contents/MacOS/MissingShip`.

### 6. Commit, regenerate the patches, update the docs

The engine is the private fork `MissingShip-Shipwright`, checked out as the `engine/` submodule on branch `missingship`
(see the README). Commit the actor there with the issue ID in the subject, push, then pin the new commit in MissingShip:

```bash
cd engine
git add soh/src/overlays/actors/ovl_<Name>/ soh/include/tables/actor_table.h soh/assets/ml_assets.h   # what you touched
git commit -m "MS-0nn: port <Name>" && git push
cd .. && python3 tools/genpatch.py && git add engine docs/*.patch     # commit with the docs changes below
```

Only committed work reaches the patches, and `tools/check.py` fails if `engine/` is not at the pinned commit. A change inside `libultraship` or `ZAPDTR` is committed and pushed in that
submodule first (branch `missingship`), then the new submodule pointer is committed in the engine.

Then:
- Add the change to `docs/ENGINE_CHANGES.md` and run `python3 tools/checkledger.py`, which fails if any patched file is missing from the ledger.
- Set the actor's **Status** in `docs/ML_ACTOR_INVENTORY.md` to `ported (...)`.
- Set its **Verified** cell to `yes (date)` only once it has been seen working in the game.
- To refresh the inventory's machine columns (changes, placements, spawners), run `python3 tools/survey_actors.py --write`; hand-edited cells survive.
- Finally run `python3 tools/check.py --build`. The pre-push hook runs it without `--build` anyway.
