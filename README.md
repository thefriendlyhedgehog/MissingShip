# MissingShip — alpha

**The Missing Link** (Kaze Emanuar, v2.0) running on the **Ship of Harkinian** engine (SoH 9.2.3).

It is a **mod plus a patched engine**, not a recompilation:
- ML's data (scenes, rooms, collision, textures, text, audio, objects, UI art) is repacked into an SoH `.o2r` archive by `pipeline/`.
- ML's code (its rewritten actors, its edits to vanilla engine code, and its custom code region at 0x80600000) is **ported to C** in a patched SoH. We diff ML's machine code against the OoT debug ROM, instruction by instruction, and port what changed.
- No ML machine code is executed.

> **This repository is private and must stay private.** It contains ROM files and archives built from
> them (Nintendo's and the ML authors' copyrighted data). Never make it public, fork it publicly, or
> copy these files into a public repo.

**Plan and status:** [`docs/ROADMAP.md`](docs/ROADMAP.md) has the full review of what is left, the milestones and the issue list. The same issues are tracked on GitHub Issues.

## Platforms

- **macOS** (Apple Silicon, Metal): the only platform built and tested so far.
- **Windows and Linux**: planned, the same as upstream SoH. Nothing in the port is Mac-specific by design; the work is build/packaging plus the pipeline scripts (see the *Platforms* milestone).

## Status

### Works (built and in the game)

- **World:** 25 ML scenes / 72 rooms load and render with correct collision; all 1281 scene textures resolve. Each ML scene sits at its real scene ID with ML's draw config.
- **Navigation:** ML's entrance table is remapped (all 104 exit slots checked by `tools/checkexits.py`), plus 34 extra entrances and ML's grotto table. ML entrance numbers used in code go through `MlEntrance_ToSoh`.
- **Text and presentation:** ML's text (269 messages replaced), area title cards (with ML's coloured ribbon), and ML's 9 entrance intro cutscenes plus its illustrated prologue.
- **Audio:** ML's soundtrack (574 sequences, fonts and samples).
- **Inventory:** ML's item layout (slot table, capacities, repurposed items: Magic Hourglass, Deku Nut Milk, Birth Certificate, Deku Pass, Soul-Charged Kokiri Sword), ML's item restrictions, starting loadout, the pause-menu layout and redrawn icons. Saves from before the port are migrated.
- **Engine behaviour:** the Blade Beam, and ML's edits to vanilla engine code:
  - Navi silenced;
  - wall-skip after teleports;
  - saves resume where you saved;
  - text at 2× speed;
  - world-map regions;
  - the overworld minimap hidden (ML hides it too).
- **Actors:** about 30 ML actors ported or patched. The list is in [`docs/ML_ACTOR_INVENTORY.md`](docs/ML_ACTOR_INVENTORY.md).

### Built but not yet playtested

The batches from 2026-09-24/25 are built but not playtested (ROADMAP issue MS-010). They include:
- the Deku Road Skull Kid and the Blade Beam heart;
- the grotto scrub shop and the hole-hopping scrub;
- young Malon's race and ML's cucco;
- title cards, save resume and the item page.

### Not yet ported (summary; details in the ROADMAP)

- **Navi's behaviour** (heavily rewritten by ML), the Deku nut projectile, and Kokiri text swaps.
- **Two dungeons' actors** (`HAKAdan`, `Bmori1`) and the **Boss_Va fight** (with 3 ML-only actors).
- The `spot01` area and the night shop (about 12 actors).
- **Link:** ML's model, animations, about 20 player-code edits, the Magic Hourglass rewind and the Soul-Charged sword visuals.
- **Data-only pieces:**
  - dungeon maps and their chest/boss markers;
  - credits text;
  - two skyboxes;
  - redrawn chest, push-block, door and bomb-flower textures;
  - entrance music/transition flags.
- **Front end:** ML's title screen, file-select and opening tweaks, and the HUD restyle. For now, SoH's boot goes straight to file select.

### Known bugs

- A hard freeze pulling a crate in the ranch stable (MS-011).
- Deep Woods sometimes not loading the next zone; probably fixed, needs a playtest (MS-012).

## Running (macOS)

```bash
./MissingShip.sh
```

- `home/` is a self-contained `SHIP_HOME` and never touches a normal SoH install. It needs `home/oot.o2r`, which SoH generates from an OoT ROM.
- The launcher kills any running instance, copies `mod/MissingLink_Scenes.o2r` into `home/mods/` and starts `MissingShip.app`.
- Start a **new file** from the file select.

## Building

### Engine

The engine is a **private fork**, included in this repo as the `engine/` submodule (MS-014). One recursive clone gets everything at matching versions:

| Repo | Branch | Contents |
|---|---|---|
| [MissingShip-Shipwright](https://github.com/thefriendlyhedgehog/MissingShip-Shipwright) (`engine/`) | `missingship` | SoH 9.2.3 (tag `9.2.3`) + every engine change |
| [MissingShip-libultraship](https://github.com/thefriendlyhedgehog/MissingShip-libultraship) (`engine/libultraship`) | `missingship` | Metal zero-size texture guard |
| [MissingShip-ZAPDTR](https://github.com/thefriendlyhedgehog/MissingShip-ZAPDTR) (`engine/ZAPDTR`) | `missingship` | ZAudio zero-size soundfont guard |

These are private repos with SoH's full history rather than GitHub forks, because GitHub forks of public repos are always public and the engine carries ML-derived data.

```bash
git clone --recursive https://github.com/thefriendlyhedgehog/MissingShip.git && cd MissingShip
git config core.hooksPath .githooks                  # the pre-push check
cd engine
git switch missingship                              # submodules check out detached; work on the branch
git remote add upstream https://github.com/HarbourMasters/Shipwright.git   # for SoH updates
cmake -S . -B build-ml -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-ml --target soh ZAPD -j8         # the engine, and the ZAPD the pipeline uses
```

- Copy `build-ml/soh/soh-macos` over `MissingShip.app/Contents/MacOS/MissingShip`.
- A new actor directory needs the `cmake -S . -B build-ml` reconfigure.
- **Engine changes are commits on `missingship`** in `engine/`, pushed. Then MissingShip commits the new `engine` pointer, and `python3 tools/genpatch.py` refreshes the patches in `docs/` (see `docs/PORTING_ACTORS.md` step 6). The patches are generated review copies, not the source of truth; `tools/genpatch.py --check` fails if they're stale.
- Taking a newer SoH release means merging its tag into `missingship` (`git fetch upstream --tags && git merge <tag>`), then rebuilding and replaying the tests.

### Mod archive

```bash
python3 pipeline/ms_cycle.py              # rebuild ROM → ZAPD → inject text/audio/objects/UI → install → relaunch
python3 pipeline/ms_cycle.py --no-launch  # build and install only
```

- **Inputs:** `pipeline/tml_2_0.z64` (the ML v2.0 ROM), `home/oot.o2r`, and the engine checkout. Every step uses the engine's own ZAPD (`build-ml/ZAPD/ZAPD.out`, or `ZAPD.exe` on Windows, from our patched ZAPDTR, built with the engine) and its tables; nothing else is needed.
- **Any OS (MS-082):** the build is plain Python 3 plus ZAPD, so it runs on macOS, Windows and Linux. Only the relaunch is macOS-only until the Windows / Linux packages (MS-080 / MS-081); `ms_cycle.sh` just calls `ms_cycle.py`.
- **Analysis tools:** `tools/*.py` also need the OoT **PAL MQ Debug** ROM at `roms/oot-mq-debug-pal.z64`.
- **Knobs:** set these env vars to restore old workarounds; the defaults are correct: `ML_STRIP_ACTORS`, `ML_REMAP_OBJECTS`, `ML_FIX_SETTIMG_FMT`, `ML_SCRUB`, `ML_STRIP_CS`, `ML_SKIP_FMT`, `ML_SKIP_SLOTS`, `ML_NO_START_FIX`.

### Checks

```bash
python3 tools/check.py            # a few seconds; add --build to build the engine too
```

It runs every automated check and prints PASS / FAIL / SKIP. A check whose inputs are missing, such as the ROMs or the engine checkout, is skipped. The checks cover:
- the scripts compile;
- the engine fork has no uncommitted or unpushed work;
- `docs/*.patch` match the fork;
- the change log names every patched file;
- every ML exit remaps correctly;
- no ML entrance that ported code compares collides with another route into the same SoH entrance, unless `pipeline/entrance_collisions.json` documents it (`checkentrances`);
- every resource reference in the mod resolves, except the pipeline's deliberate skips;
- the actor inventory matches the ROMs.

The committed git hook runs it before every push. Enable it once per clone:

```bash
git config core.hooksPath .githooks
```

### Paths

Every script finds its inputs through `pipeline/mspaths.py`. Defaults resolve from the repo's own location, so a fresh clone works anywhere. Check what it resolved with:

```bash
python3 pipeline/mspaths.py
```

Override any path with an env var:

| Variable | Default |
|---|---|
| `MS_ML_ROM` | `pipeline/tml_2_0.z64` |
| `MS_DEBUG_ROM` | `roms/oot-mq-debug-pal.z64` |
| `MS_OOT_O2R` | `home/oot.o2r` |
| `MS_SHIPWRIGHT` | `engine/` (the engine fork submodule) |
| `MS_ZAPD`, `MS_ZAPD_CWD` | `$MS_SHIPWRIGHT/build-ml/ZAPD/ZAPD.out` (built with the engine, patched ZAPDTR), `$MS_SHIPWRIGHT/soh` |

ZAPD's config (`pipeline/Config_ML.xml`) is a template; each run writes the filled-in copy to `pipeline/out/`.

### The hack the tools analyse

The ROM tools in `tools/` read the hack and its baseline from `hacks/<name>/hack.json` (MS-090): each ROM's
dmadata and `code` / player dma indices, `code`'s VRAM, the actor table, `Actor_Spawn`, and the hack's custom-code
files. `hacks/ml` is the default; `MS_HACK` picks another. `python3 tools/hack.py` shows what it resolved. The
tools assume the hack's `code` has zero shift against the baseline, as ML's does against PAL MQ Debug.

`hacks/selftest` compares PAL MQ Debug with itself: every tool runs on it with no other change and reports no
differences, e.g. `MS_HACK=hacks/selftest python3 tools/codediff.py`.

## Layout

| Path | What |
|---|---|
| `MissingShip.app` | macOS app bundle: the patched SoH build |
| `mod/MissingLink_Scenes.o2r` | the ML archive the pipeline produces |
| `home/` | self-contained SHIP_HOME (profile, saves, `oot.o2r`) |
| `profile/` | seed settings copied into `home/` on first run |
| `pipeline/` | ROM → `.o2r` pipeline: ZAPD XMLs (`xml/scenes`, `xml/objects`), injectors, maps |
| `tools/` | hack-vs-baseline analysis: `modcheck`, `aligndiff`, `dumpactor`, `codediff`, `pldiff`, `mlcustom`, …; project checks: `check`, `genpatch`, `survey_actors`, … |
| `hacks/` | per-hack configs for `tools/` (`ml`, `selftest`) |
| `docs/ROADMAP.md` | review, gameplan, issues |
| `docs/ENGINE_CHANGES.md` | every engine change, why it exists, and its drop-in path |
| `docs/ML_ACTOR_INVENTORY.md` | per-actor port status |
| `docs/PORTING_ACTORS.md` | how to port an actor (diff → C → object → build) |
| `docs/WORKING_NOTES.md`, `docs/notes/` | handoff notes and key facts (addresses, gotchas) |
| `docs/*.patch` | generated from the engine fork by `tools/genpatch.py`: changes against SoH 9.2.3 (soh, ZAPDTR, libultraship) |

## Goals

1. **All of The Missing Link playable on SoH.**
2. **Drop-in for stock SoH, long term.** A stock SoH build can load only data, so every engine change is logged with its route to data or to a generic upstream feature (`docs/ENGINE_CHANGES.md`).
3. **A porting kit for other romhacks.** The method and tools here (dma/object/actor diffs against a vanilla baseline, the ZAPD pipeline, the C porting workflow) are meant to grow into a reusable way to bring OoT romhacks to the SoH engine, until there's a ready-made porting option or the hacks are all done.

## Why an .app bundle

- macOS attributes per-app audio by bundle identity (`com.missingship.MissingShip`), so a bare executable is invisible to per-app volume tools.
- `getMainBundlePath()` returns the bundle's `Resources`, which is why `assets/` and `soh.o2r` live there.
- `SHIP_HOME` is set both by the launcher and by `LSEnvironment` in `Info.plist`.

## Credits

- The Missing Link: Kaze Emanuar and team.
- Ship of Harkinian: HarbourMasters.
- The OoT decompilation: zeldaret.
