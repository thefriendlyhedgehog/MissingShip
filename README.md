# MissingShip — beta

**The Missing Link** (Kaze Emanuar, v2.0) running on the **Ship of Harkinian** engine (SoH 9.2.3), on Windows,
Linux and macOS.

It is a **mod plus a patched engine**, not an emulator or a recompilation:
- ML's data (scenes, rooms, collision, textures, text, audio, objects, UI art) is repacked into an SoH `.o2r` archive by `pipeline/`.
- ML's code (its rewritten actors, its edits to vanilla engine code, and its custom code region at 0x80600000) is **ported to C** in a patched SoH. We diff ML's machine code against the OoT debug ROM, instruction by instruction, and port what changed.
- No ML machine code is executed.

> **No ROMs or game data in git.** ROMs and everything built from them (`oot.o2r`, the mod archive, the
> pipeline's `.otr` dumps) are Nintendo's and the ML authors' copyrighted data. `.gitignore` keeps them out:
> each developer supplies their own ROMs, and the pipeline rebuilds the rest locally. The repo is private
> for now; its history still holds the files committed before 2026-09-27 and must be cleaned before it goes public.

## Status

**Beta.** The whole of The Missing Link is ported and has been played start to finish (MS-010, closed 2026-09-26):
- all 25 scenes and 72 rooms;
- every changed or custom actor;
- Link's model, animations and player-code edits;
- both dungeons and the boss;
- the title screen, credits tour and ending.

The owner is now comparing it against a reference playthrough of ML on N64, and any difference becomes a new issue.
The open issues are long-term only: the drop-in path (#42) and the porting kit (#48–#50).

- **Plan and history:** [`docs/ROADMAP.md`](docs/ROADMAP.md).
- **Every engine change and why:** [`docs/ENGINE_CHANGES.md`](docs/ENGINE_CHANGES.md).

---

## Playing

### What you need

- **Your own Ocarina of Time USA 1.0 ROM** (NTSC 1.0, not Master Quest). None is included.
- **Kaze's The Missing Link v2.0 patch**, from [his release](https://www.romhacking.net/hacks/5334/): the `.zip` as
  downloaded is fine. (A Missing Link v2.0 ROM you already patched works instead of the ROM and patch.)
- A controller, or the keyboard (SoH's default layout).

### Windows

1. Download `MissingShip-windows.zip` from the [Releases](https://github.com/thefriendlyhedgehog/MissingShip/releases) page and unzip it anywhere.
2. Run `MissingShip\MissingShip.exe`.
3. On first launch, pick your OoT ROM. The game turns it into `oot.o2r` in the same folder (a minute or two, once only).
4. It then asks for Kaze's patch and builds The Missing Link from it (about a minute, once only), then boots to ML's title screen. No terminal, nothing to copy by hand.

Have The Missing Link v2.0 ROM already patched? Pick it at the first prompt: the game keeps it for the mod and then
asks for your OoT ROM, which it still needs for the base game's assets (ML's ROM leaves out about a thousand of the
files the engine uses).

### Linux

1. Download `MissingShip-linux.tar.gz` from [Releases](https://github.com/thefriendlyhedgehog/MissingShip/releases) and extract it.
2. Run `./MissingShip.sh`, not the AppImage directly: the script keeps settings and saves in that folder.
3. Pick your ROM and then Kaze's patch on first launch, as on Windows.

**Steam Controller:** outside Steam it acts as a mouse and keyboard. Add `MissingShip.sh` to Steam
(*Games → Add a Non-Steam Game*) and launch it from there.

### macOS (Apple Silicon)

1. Download `MissingShip-macos.zip` from [Releases](https://github.com/thefriendlyhedgehog/MissingShip/releases), unzip it and move `MissingShip.app` to Applications.
2. Open it once. macOS blocks apps from unidentified developers the first time: go to *System Settings → Privacy & Security* and click **Open Anyway** next to MissingShip.
3. Pick your ROM and then Kaze's patch on first launch, as on Windows.

Needs an M1 or later Mac with macOS 11 or later. Settings, saves, logs and `oot.o2r` live in
`~/Library/Application Support/MissingShip` (*Settings → Open App Files Folder* in the menu opens it).

### Where your files are

On Windows and Linux, settings (`shipofharkinian.json`), saves (`Save/`), logs and `oot.o2r` all live next to the
game, so the folder is portable. On macOS they are in `~/Library/Application Support/MissingShip`. Either way
MissingShip never touches a normal Ship of Harkinian install. `mods/MissingLink_Scenes.o2r` is The Missing Link
itself, built on first launch. `MissingLink/` keeps your patched ROM, so when an update changes the mod the game
rebuilds it without asking again. Delete `mods/MissingLink_Scenes.o2r` to make the game build it again.

### Settings

- **Esc** opens the menu.
- The menu is SoH's own, trimmed to what applies to ML. Options that do nothing in ML (randomizer, network, and settings for vanilla items, areas and enemies ML removed) are hidden.
- The port's own options are under **Enhancements → MissingShip**:
  - **Starting Ocarina Button** (default D-Down): where a new game equips the Ocarina.
  - **B Cancels Choices** (default on).
  - **No Bean Seller Flashing** (default off).
- **Defaults** keep ML as its author made it:
  - text at ML's 2× speed;
  - cutscenes play;
  - no overworld minimap, as in ML.

### Known differences from ML on N64

- SoH's boot logo replaces ML's two boot checks (the Expansion Pak and emulator-accuracy error screens), which don't apply here. ML's title screen itself is ported.
- Four entrance day-fade colours differ slightly (MS-027).

---

## Developing

### Platforms

- **macOS** (Apple Silicon, Metal) is the development platform: the pipeline, the checks and `MissingShip.app` are built and tested here.
- **Windows and Linux** engine builds come from GitHub Actions on the engine fork; the macOS package is built on this Mac. `tools/package.py` makes all three release packages.

### Running (macOS)

```bash
./MissingShip.sh
```

- `home/` is a self-contained `SHIP_HOME` and never touches a normal SoH install. It needs `home/oot.o2r`, which SoH generates from an OoT ROM.
- The launcher kills any running instance, copies `mod/MissingLink_Scenes.o2r` into `home/mods/`, seeds `home/shipofharkinian.json` from `profile/` if it is missing, and starts `MissingShip.app`.

### Engine

The engine is a **private fork**, included in this repo as the `engine/` submodule. One recursive clone gets everything at matching versions:

| Repo | Branch | Contents |
|---|---|---|
| [MissingShip-Shipwright](https://github.com/thefriendlyhedgehog/MissingShip-Shipwright) (`engine/`) | `missingship` | SoH 9.2.3 (tag `9.2.3`) + every engine change |
| [MissingShip-libultraship](https://github.com/thefriendlyhedgehog/MissingShip-libultraship) (`engine/libultraship`) | `missingship` | renderer fixes (zero-size texture guard, 4-bit block loads, …) |
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

- Copy `build-ml/soh/soh-macos` to `MissingShip.app/Contents/MacOS/MissingShip`. The binary is not in git: every developer builds it, and players get it from releases.
- A new actor directory needs the `cmake -S . -B build-ml` reconfigure.
- **Engine changes are commits on `missingship`** in `engine/`, pushed. Then MissingShip commits the new `engine` pointer, and `python3 tools/genpatch.py` refreshes the patches in `docs/` (see `docs/PORTING_ACTORS.md` step 6). The patches are generated review copies, not the source of truth; `tools/genpatch.py --check` fails if they're stale.
- Every engine change gets a row in `docs/ENGINE_CHANGES.md` (the ledger) in the same commit.
- Taking a newer SoH release means merging its tag into `missingship` (`git fetch upstream --tags && git merge <tag>`), then rebuilding and replaying the tests.

### Mod archive

```bash
python3 pipeline/ms_cycle.py              # rebuild ROM → ZAPD → inject text/audio/objects/UI → install → relaunch
python3 pipeline/ms_cycle.py --no-launch  # build and install only
```

- **Inputs (not in git; bring your own):** `pipeline/tml_2_0.z64` (the ML v2.0 ROM), `home/oot.o2r` (made by SoH from an NTSC-U 1.2 ROM), and the engine checkout. Every step uses the engine's own ZAPD (`build-ml/ZAPD/ZAPD.out`, from our patched ZAPDTR) and its tables; nothing else is needed.
- **Output:** `mod/MissingLink_Scenes.o2r`, byte-identical between builds of the same inputs. It is game data, so it is never committed; releases carry it.
- The pipeline is plain Python 3 plus ZAPD. It is only exercised on macOS; players don't need it, since the packages ship the finished archive.
- **The debug ROM is optional for the build.** Four steps ship a texture or credit only if ML changed it against the OoT **PAL MQ Debug** ROM it was built from. With that ROM at `roms/oot-mq-debug-pal.z64` (not in git), they compute those answers and record them in `pipeline/baseline.json` (true/false answers and names only, committed). Without it, they replay the recorded answers (`pipeline/baseline.py`, MS-154), with byte-identical output. Commit `baseline.json` whenever a build changes it.
- **Analysis tools:** `tools/*.py` do need the debug ROM.
- **Knobs:** set these env vars to restore old workarounds; the defaults are correct: `ML_STRIP_ACTORS`, `ML_REMAP_OBJECTS`, `ML_FIX_SETTIMG_FMT`, `ML_SCRUB`, `ML_STRIP_CS`, `ML_SKIP_FMT`, `ML_SKIP_SLOTS`, `ML_NO_START_FIX`.

### Releases

1. Run **generate-builds** on the engine fork (*Actions → generate-builds → Run workflow*, branch `missingship`, or
   `gh workflow run generate-builds.yml --repo thefriendlyhedgehog/MissingShip-Shipwright --ref missingship`).
   It builds `soh.o2r`, the Windows exe and the Linux AppImage, and freezes the mod builder for each from this
   repo's `main`. It is manual only; the repo secret `SUBMODULES_TOKEN` (a read-only token for the private repos)
   lets it check out the submodules and this repo.
2. `python3 tools/package.py <run id>` downloads the run and writes `dist/MissingShip-windows.zip` and
   `dist/MissingShip-linux.tar.gz`: the engine, `soh.o2r`, the mod builder in `builder/`, a settings profile from
   `profile/` (minus the Mac-only window block) and a README. No game data: no ROM, `oot.o2r` or mod.
3. On this Mac, build the engine at the same commit, freeze the builder (`tools/freeze_builder.py`, with PyInstaller)
   and run `python3 tools/package.py macos`. It writes `dist/MissingShip-macos.zip`: the app with its Homebrew
   libraries copied into `Contents/Frameworks` (SDL3 included, which sdl2-compat loads at run time), the builder and
   the default settings in `Contents/Resources`, ad-hoc signed. On first launch the engine copies the settings into
   `~/Library/Application Support/MissingShip` (ledger row 142). Apple Silicon only.
4. Attach the three files to a GitHub release on this repo.

`profile/shipofharkinian.json` is what every new player starts with, so it holds chosen defaults, never a
tester's own settings.

### Mod builder (MS-154, MS-155)

Players build the mod themselves from their own files, so no download carries game data (milestone *Prod: no game data*).
The game runs the builder itself on first launch (`soh/Extractor/MissingShipBuilder.cpp` in the engine, ledger row
143): after SoH makes `oot.o2r`, it asks for Kaze's patch (adding the OoT ROM just extracted), runs the bundled
builder with a progress window, and shows the builder's message if it fails. The builder keeps the ML ROM in
`MissingLink/tml_2_0.z64` (`--save-rom`), and the game records which builder (`version.txt`) and `oot.o2r` made the
mod in `MissingLink/built.txt`; when either changes, it rebuilds from the kept ROM without asking. A build with no
bundled builder (the development app) skips all of this. By hand:

```bash
python3 pipeline/build_mod.py <files...> --oot-o2r <oot.o2r> --out <MissingLink_Scenes.o2r>
```

- `<files>` are whatever the player has, in any order, told apart by their contents. It accepts:
  - **The Missing Link v2.0 ROM**; or
  - **an OoT USA 1.0 ROM plus Kaze's patch**: either of his `.bps` files, or the `.zip` from his page as downloaded.

  ROMs can be `.z64`, `.n64` or `.v64`, loose or zipped. Every ROM and result is checked by CRC, and a wrong ROM, a missing patch or an older ML patch each get a plain message.
- It then applies the patch in whichever byte order it was made for, and runs the same pipeline in a temporary folder. It uses `pipeline/baseline.json` in place of the debug ROM. Its output is byte-identical to `ms_cycle.py`'s for the same `oot.o2r`, in about 7 s.
- Players get Kaze's patch from [his release](https://www.romhacking.net/hacks/5334/). It is made for OoT **USA 1.0**, and SoH accepts that ROM for `oot.o2r` too, so one ROM covers both.
- `tools/freeze_builder.py` freezes it with PyInstaller into `dist/builder/MissingShipBuilder/`: no Python needed, with ZAPD and the SoH files the pipeline reads (tables and object XMLs, 2.8 MB) bundled. Run it on each OS. On macOS it also bundles SDL3, which Homebrew's sdl2-compat (linked by ZAPD) loads at start-up.

### ML on SoH: Unbound (in progress)

The next home for ML is [SoH: Unbound](https://github.com/roborich/Shipwright/tree/unbound); our copy is the private
`thefriendlyhedgehog/MissingShip-Unbound` (branch `missinglink`). Unbound reads vanilla scenes and text only in its own
JSON form, so `tools/unbound_layer.py` converts ML's with Unbound's own converter and keeps what ML changes:

```bash
python3 tools/unbound_layer.py --unbound <Unbound build>/soh/soh-macos --oot-o2r <oot.o2r> --mod mod/MissingLink_Scenes.o2r
```

It writes `mod/MissingLink_Unbound.o2r` (10 MB: 25 scenes, 304 messages, ML's objects, textures and audio) for
Unbound's `mods/` folder, beside an ordinary `oot.o2r`. It carries ML's world, not the behaviour our SoH fork adds in C;
that is the rest of the plan in `docs/ROADMAP.md`.

For players it is a pack (owner decision, 2026-09-29: the pack route): one file they drop into the `mods/` folder of
their own Unbound, with no ROM of ML and no builder. Unbound reads *delta files* (our branch; Unbound SPEC §1.7,
`unbound-docs/delta-files.md`), so the pack holds ML's own bytes and copies everything it shares with OoT, as the game
loads it, from the player's `oot.o2r` and `oot-unbound.o2r`. It carries none of Nintendo's data, and is the same file
on every OS. We build it with the mod builder:

```bash
python3 pipeline/build_mod.py pipeline/tml_2_0.z64 --unbound <Unbound build>/soh/soh-macos --pack \
    --oot-o2r <oot.o2r from Unbound's own extractor> --out MissingLink_Unbound.o2r
python3 tools/package.py unbound-pack MissingLink_Unbound.o2r     # dist/MissingLink-Unbound.zip, with a README
```

It builds ML's archive, then the layer, then the pack (`tools/deltapack.py`, about 25 s in all): 4.96 MB, 3.3 MB
zipped, against a 10.2 MB layer. Make it from an `oot.o2r` that Unbound's own extractor wrote, since that is what a
player has; limbs, rooms and sound fonts are never copied from, because extractors write them differently. A player
whose `oot.o2r` comes from another ROM version gets the game without the mod and a log line naming what is missing.
Until roborich takes delta files and the features ML needs, players need our Unbound build.

Without `--pack` the builder writes the whole layer, which holds the game's data too: for testing only. On macOS it
reads `oot.o2r` from and writes `mods/MissingLink_Unbound.o2r` to Unbound's folder
(`~/Library/Application Support/com.shipofharkinian.soh`, or `SHIP_HOME`; `--oot-o2r` and `--out` choose others).
The frozen builder (`tools/freeze_builder.py`, `tools/package.py unbound-macos`), which let players build the layer
themselves, stays for testers. It refuses an Unbound install without its `assets/` folder, where Unbound would wait on
a popup. Test runs set `SHIP_HOME` to a scratch folder: the default is the Mac user's real SoH folder.

Playing it on Unbound, turn on **Enhancements → Quality of Life → Saving → Remember Save Location**. ML resumes a save
where it was made; without the option, a save made outside a dungeon loads at Link's House, which in ML is the
prologue's stub. A save made in a dungeon resumes where ML sends it either way.

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

### Layout

| Path | What |
|---|---|
| `MissingShip.app` | macOS app bundle for development: Info.plist, SoH's assets and `soh.o2r`; the engine binary is built locally, not in git |
| `mod/MissingLink_Scenes.o2r` | the ML archive the pipeline produces (local only, not in git) |
| `home/` | self-contained SHIP_HOME for development (settings, saves, `oot.o2r`; local only, not in git) |
| `profile/` | the default settings every new player starts with |
| `pipeline/` | ROM → `.o2r` pipeline: ZAPD XMLs (`xml/scenes`, `xml/objects`), injectors, maps |
| `tools/` | hack-vs-baseline analysis: `modcheck`, `aligndiff`, `dumpactor`, `codediff`, `pldiff`, `mlcustom`, …; project checks: `check`, `genpatch`, `survey_actors`, …; audio: `sfxtrace` (which sound effects play a font slot); release packaging: `package`, `deltapack` (a mod as deltas against the player's own base) |
| `hacks/` | per-hack configs for `tools/` (`ml`, `selftest`) |
| `dist/` | release packages from `tools/package.py` (not committed) |
| `docs/ROADMAP.md` | status, history and the remaining long-term work |
| `docs/ENGINE_CHANGES.md` | every engine change, why it exists, and its drop-in path |
| `docs/UNBOUND_MAP.md` | every engine change placed against SoH: Unbound, and the order of work there |
| `docs/ML_ACTOR_INVENTORY.md` | per-actor port status |
| `docs/PORTING_ACTORS.md` | how to port an actor (diff → C → object → build) |
| `docs/DROPIN_PROPOSALS.md` | what each engine change would need to move upstream or into data |
| `docs/WORKING_NOTES.md`, `docs/notes/` | handoff notes and key facts (addresses, gotchas) |
| `docs/*.patch` | generated from the engine fork by `tools/genpatch.py`: changes against SoH 9.2.3 (soh, ZAPDTR, libultraship) |

### Why an .app bundle

- macOS attributes per-app audio by bundle identity (`com.missingship.MissingShip`), so a bare executable is invisible to per-app volume tools.
- `getMainBundlePath()` returns the bundle's `Resources`, which is why `assets/` and `soh.o2r` live there.
- `SHIP_HOME` is set both by the launcher and by `LSEnvironment` in `Info.plist`.

## Goals

1. **All of The Missing Link playable on SoH.** Done; now being checked against ML on N64.
2. **Drop-in for stock SoH, long term.** A stock SoH build can load only data, so every engine change is logged with its route to data or to a generic upstream feature (`docs/ENGINE_CHANGES.md`, `docs/DROPIN_PROPOSALS.md`). No upstream PRs until the project is done.
3. **A porting kit for other romhacks.** The method and tools here (dma/object/actor diffs against a vanilla baseline, the ZAPD pipeline, the C porting workflow) are meant to grow into a reusable way to bring OoT romhacks to the SoH engine.

## Credits

MissingShip is ported with Kaze Emanuar's permission.

### The Missing Link (v2.0)

| Who | Role |
|---|---|
| Kaze Emanuar | Producer. Hacking: game planning, direction and code; level design |
| Zel | Hacking: 3D model design, code, dialogue and cutscenes; level design, maps and writing |
| CDi-Fails | Level design, 3D models and maps, cutscenes, writing |
| Zerovolt0v, A6stringthing, Sauraen, Badub, DezZival | Music |
| Rankaisija, Ultra553 | Boss animations |
| OstrealavaO2 | Cutscenes |
| Aurora-Xatu, Ty Anderson | Concept art |
| Zerovolt0v, Sauraen, E-Gor, GTM, Collins, Ricar, Fig | Testing |
| Nokaubure, z64me, the OoT decomp team | Special thanks |

Credits from The Missing Link v2.0's release.

### Engine and tools

- Ship of Harkinian: HarbourMasters.
- The OoT decompilation: zeldaret.
