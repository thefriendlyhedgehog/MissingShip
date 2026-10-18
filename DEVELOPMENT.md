# Developing MissingShip

How The Missing Link is ported and packaged. Players need none of this: see the [README](README.md).

> **No ROMs or game data in git.** ROMs and everything built from them (`oot.o2r`, the mod archive, the
> pipeline's `.otr` dumps) are Nintendo's and the ML authors' copyrighted data. `.gitignore` keeps them out:
> each developer supplies their own ROMs, and the pipeline rebuilds the rest locally. `tools/nodata.py --git` checks
> a repo's whole history.

## Platforms

- **macOS** (Apple Silicon, Metal) is the development platform: the pipeline and the checks run here.
- **Windows and Linux** builders are frozen by GitHub Actions; the macOS one is built on this Mac. `tools/package.py`
  makes all three release packages.

## Reference port (development only)

ML's changes were first ported into a fork of SoH 9.2.3, included as the `engine/` submodule (with its own
`libultraship` and `ZAPDTR` forks). It is not distributed: it is where each change was worked out and recorded (`docs/ENGINE_CHANGES.md`), and it supplies the ZAPD
the pipeline runs and the CI workflow that freezes the builders. Each is on its branch `missingship`; one recursive
clone of this repo gets them all:

```bash
git clone --recursive <this repo> && cd MissingShip
git config core.hooksPath .githooks                  # the pre-push check
cd engine
git switch missingship                              # submodules check out detached; work on the branch
cmake -S . -B build-ml -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-ml --target ZAPD -j8             # the ZAPD the pipeline uses
```

- A change there is a commit on `missingship`, pushed; then MissingShip commits the new `engine` pointer and
  `python3 tools/genpatch.py` refreshes the review patches in `docs/` (`--check` fails if they're stale).
- Every change gets a row in `docs/ENGINE_CHANGES.md` (the ledger) in the same commit.

## Mod archive

```bash
python3 pipeline/ms_cycle.py --no-launch  # rebuild ROM → ZAPD → inject text/audio/objects/UI
```

- **Inputs (not in git; bring your own):** `pipeline/tml_2_0.z64` (the ML v2.0 ROM), `home/oot.o2r` (made by SoH from an NTSC-U 1.2 ROM), and the `engine/` checkout. Every step uses its ZAPD (`build-ml/ZAPD/ZAPD.out`, from our patched ZAPDTR) and its tables; nothing else is needed.
- **Output:** `mod/MissingLink_Scenes.o2r`, byte-identical between builds of the same inputs. It is game data, so it is never committed or released.
- The pipeline is plain Python 3 plus ZAPD. It is only exercised on macOS; players run it through the builder.
- **The debug ROM is optional for the build.** Four steps ship a texture or credit only if ML changed it against the OoT **PAL MQ Debug** ROM it was built from. With that ROM at `roms/oot-mq-debug-pal.z64` (not in git), they compute those answers and record them in `pipeline/baseline.json` (true/false answers and names only, committed). Without it, they replay the recorded answers (`pipeline/baseline.py`, MS-154), with byte-identical output. Commit `baseline.json` whenever a build changes it.
- **Analysis tools:** `tools/*.py` do need the debug ROM.
- **Knobs:** set these env vars to restore old workarounds; the defaults are correct: `ML_STRIP_ACTORS`, `ML_REMAP_OBJECTS`, `ML_FIX_SETTIMG_FMT`, `ML_SCRUB`, `ML_STRIP_CS`, `ML_SKIP_FMT`, `ML_SKIP_SLOTS`, `ML_NO_START_FIX`.

## Releases

**ML for SoH: Unbound** (the `unbound-930-builder-N` pre-releases):
1. ML's code mod: `python3 tools/codemod.py` writes `dist/codemod/missinglink.o2r` and `codemod.txt` from
   our Unbound fork's `ml-codemod-930` (macOS built here; Windows and Linux from that commit's CI run). When it
   changes, publish the release with those two files first, then set `CODEMOD_RELEASE` in the `engine/` fork's
   `generate-builds.yml` to it: CI's builders carry the code mod from that release.
   `python3 tools/codemod.py --mod childshield` does the same for `childshield.o2r` (young Link holds the Hylian
   Shield, ML's rule alone, for players who want only that; compiled code, the hand built from their own
   `oot.o2r`; `mod-sdk/mods/childshield`). It ships on its own, outside the builder.
2. Run **generate-builds** on the `engine/` fork (*Actions → generate-builds → Run workflow*, branch `missingship`,
   or `gh workflow run generate-builds.yml --repo <the engine/ fork> --ref missingship`).
   It freezes the Windows and Linux builders from this repo's `main`; it is manual only, and the repo secret
   `SUBMODULES_TOKEN` (a read-only token for the submodules) lets it check out the submodules and this repo. Then
   `python3 tools/package.py unbound-ci <run id>` writes `dist/MissingLink-Unbound-windows.zip` and
   `dist/MissingLink-Unbound-linux.tar.gz`.
3. On this Mac: `tools/freeze_builder.py` (with PyInstaller), then `python3 tools/package.py unbound-macos` writes
   `dist/MissingLink-Unbound-macos.zip`.
4. `package.py` checks each package for game data with `tools/nodata.py` (MS-156). It needs OoT USA 1.0 at
   `roms/oot-usa10.z64` and ML v2.0 at `pipeline/tml_2_0.z64` (or `MS_OOT10_ROM`, `MS_ML_ROM`), and removes a
   package that fails.
5. Test each package through its launcher. Builder windows can be driven in tests with `MS_GUI_AUTOTEST=<json>`
   (`pipeline/builder_gui.py`). Then attach the three packages and `codemod.txt` to the release. The code mod (`missinglink.o2r`) is not attached on
   its own: every package carries it and the builder writes it into `mods/`.

## Mod builder (MS-154, MS-155)

Players build the mod themselves from their own files, so no download carries game data (milestone *Prod: no game data*).
Run with no arguments (or `--gui`), the builder opens a window (`pipeline/builder_gui.py`, Tk, offline) that asks for
Unbound, the ROM and the patch and writes ML into Unbound's `mods/` folder with `--stock`; each Unbound package has a
double-click launcher for it; on Windows it drops the console window it was started with. ML's archive alone, by
hand:

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

## ML on SoH: Unbound

What players get is the builder's `--stock` route below: ML for a stock Unbound with skijer's code mods. The pack
route and our Unbound branch came first and stay for testing. ML's home is [SoH: Unbound](https://github.com/roborich/Shipwright/tree/unbound); our fork of it has the
branch `missinglink`. Unbound reads vanilla scenes and text only in its own
JSON form, so `tools/unbound_layer.py` converts ML's with Unbound's own converter and keeps what ML changes:

```bash
python3 tools/unbound_layer.py --unbound <Unbound build>/soh/soh-macos --oot-o2r <oot.o2r> --mod mod/MissingLink_Scenes.o2r
```

It writes `mod/MissingLink_Unbound.o2r` (10 MB: 25 scenes, 304 messages, ML's objects, textures and audio) for
Unbound's `mods/` folder, beside an ordinary `oot.o2r`. It carries ML's world, not the behaviour our SoH fork adds in C;
that is the rest of the plan in `docs/ROADMAP.md`.

The layer is what our fork changes, found by comparing the fork's tables with Unbound's on the same SoH version: Unbound
at its last SoH 9.2.3 commit (`mspaths.UNBOUND_COMPARE`), not the Unbound players run, which is on 9.3.0 since 0.11.
The layer names everything it changes, so it applies to either. The tools read that tree from `../Unbound-923-src`
(or `MS_UNBOUND_SRC`) and say how to make it when it is missing:

```bash
git -C ../Unbound worktree add --detach ../Unbound-923-src 7714a97bf0
```

ML's files themselves come from our fork's ZAPD, SoH 9.2.3's extractor. SoH 9.3.0 replaced it with Torch, which was
built to write the same files, so on each SoH release we check that it still does: extract the PAL MQ Debug ROM ML is
built on with both games (each writes `oot-mq.o2r` into a scratch `SHIP_HOME`) and compare them with
`python3 tools/extractor_parity.py <ZAPD's> <Torch's>`. For 9.3.0 the only difference the game sees is the boss title
cards' texture type (IA8, not I8), and ML's own card now follows Torch.

For players it is a pack (owner decision, 2026-09-29: the pack route): one file they drop into the `mods/` folder of
their own Unbound, with no ROM of ML and no builder. Unbound reads *delta files* (our branch; Unbound SPEC §1.7,
`unbound-docs/delta-files.md`), so the pack holds ML's own bytes and copies everything it shares with OoT, as the game
loads it, from the player's `oot.o2r` and `oot-unbound.o2r`. It carries none of Nintendo's data, and is the same file
on every OS. We build it with the mod builder:

```bash
python3 pipeline/build_mod.py pipeline/tml_2_0.z64 --unbound <Unbound build>/soh/soh-macos --pack \
    --oot-o2r <USA 1.0 oot.o2r from Unbound's own extractor> \
    --also-base <USA 1.1 oot.o2r> --also-base <USA 1.2 oot.o2r> --also-base <an oot.o2r from SoH 9.1's extractor> \
    --also-base <USA 1.0, 1.1 and 1.2 oot.o2r from SoH 9.3's extractor, one --also-base each> \
    --out MissingLink_Unbound.o2r
python3 tools/package.py unbound-pack MissingLink_Unbound.o2r     # dist/MissingLink-Unbound.zip, with a README
```

Players never run Python or a builder: they get the game ready to play. `python3 tools/package.py unbound-app-macos
MissingLink_Unbound.o2r` takes Unbound's own app (`cd ../Unbound/build-930 && cpack`, or a CI release `.dmg`), puts the
pack in the app's own mods folder (our Unbound branch loads mods from `Contents/Resources/mods`), signs it ad hoc and
writes `dist/MissingLink-Unbound-macos.dmg` (22 MB) with an Applications link and a README. The player drags the app
to Applications, opens it, and gives it their OoT ROM on first launch as SoH always asks; ML is enabled in the Mod
Menu. The app keeps its files, settings and saves in its own folder (`~/Library/Application Support/The Missing
Link`, set in its Info.plist), never Ship of Harkinian's: sharing it let each game move the other's randomizer saves
aside and mixed their save slots and settings. Built on this Mac it runs only on Apple silicon with the newest macOS (Homebrew's libraries); a release needs
Unbound's CI build (universal, older macOS), and notarizing needs an Apple Developer ID. `unbound-pack` above stays for
players who already have our Unbound.

It builds ML's archive, then the layer, then the pack (`tools/deltapack.py`, about a minute in all): 4.98 MB against
a 10.2 MB layer. Make it from an `oot.o2r` that Unbound's own extractor wrote, and give each other
SoH 9.x extractor's `oot.o2r` of the same ROM with `--also-base`: players keep the one their SoH made (Unbound takes
any 9.x), and 9.1 names samples differently and writes some files differently, so a pack made against 9.2 alone is
refused over a 9.1 base. Sources are then files every version has (under any of their names, with any byte a version
writes differently never copied), and where no delta fits every version the pack carries one variant per version.
Limbs, rooms and sound fonts are never copied from, because extractors leave some of their bytes uninitialised. ROM
releases differ the same way, so each supported release joins with `--also-base` too: the pack we ship covers USA 1.0,
1.1 and 1.2 by SoH 9.2.3's extractor (Unbound through 0.10) and by SoH 9.3.0's (stock SoH and Unbound 0.11: Torch,
which writes 58 files differently: sound fonts and boss title cards), and USA 1.2 by SoH 9.1.1's. A player whose `oot.o2r` comes from another release
(PAL, GameCube) gets the game without the mod, and a popup at startup that says why.
Until roborich takes delta files and the features ML needs, players need our Unbound build.

For a stock Unbound with skijer's code mods (NEI Unbound 2.0, SoH 9.3.0 or later; the end goal in `docs/ROADMAP.md`),
add `--stock`
(not with `--pack`, which stock Unbound does not read). Stock Unbound refuses our branch's entries for vanilla
scenes, and a scene whose exits name an entrance ML adds to a vanilla scene fails to load there, so the stock layer
ships no `unbound/scenes.json` and rewrites those exits to vanilla entrances (`tools/unbound_stock.py`): one that
lands where ML's does, else a spare vanilla entrance into the scene that neither ML nor the game's code uses, else
the scene's own entrance. Stock Unbound refuses the layer's actor files too (ML's changes to vanilla actors, and
its grotto and breakable wall as types of vanilla ones), so the stock layer leaves them out: ML's code mod on
skijer's branch (our Unbound fork's `ml-codemod-930`, `mod-sdk/mods/missinglink`) carries them, along with ML's scene
draw configs, without which five of ML's scenes crash on arrival. It also gives ML's own collisions empty camera
entries, which stock Unbound needs before Link can stand on them. The builder lists each rewrite and what it left
to the code mod, and counts what stock Unbound does not read (scene names, restrictions, save return points, ML's
entrance flags, intro cutscenes, continue points). On 2026-10-11 all eight of ML's such exits fall back to the
scene's own entrance (the spare Temple of Time entrances are vanilla cutscene destinations), and all 25 of ML's
scenes run on skijer's branch with the code mod, which since 2026-10-12 holds every actor ML's rooms place by
name, ML's edits inside vanilla actors and ML's player code (a few player edits no hook reaches are listed in the
mod's README). The code mod calls the host by SoH 9.3.0's names (skijer/Shipwright#33), so it is built for 9.3.0
hosts only and the builder refuses `--stock` for an Unbound built on an older SoH (its `soh.o2r`'s `portVersion`).

Without `--pack` the builder writes the whole layer, which holds the game's data too: for testing only. On macOS it
reads `oot.o2r` from and writes `mods/MissingLink_Unbound.o2r` to Unbound's folder
(`~/Library/Application Support/com.shipofharkinian.soh`, or `SHIP_HOME`; `--oot-o2r` and `--out` choose others).
The frozen builder (`tools/freeze_builder.py`, `tools/package.py unbound-macos`), which let players build the layer
themselves, stays for testers. It refuses an Unbound install without its `assets/` folder, where Unbound would wait on
a popup. Test runs set `SHIP_HOME` to a scratch folder: the default is the Mac user's real SoH folder.

Playing it on Unbound, turn on **Enhancements → Quality of Life → Saving → Remember Save Location**. ML resumes a save
where it was made; without the option, a save made outside a dungeon loads at Link's House, which in ML is the
prologue's stub. A save made in a dungeon resumes where ML sends it either way.

## Checks

```bash
python3 tools/check.py            # a few seconds
```

It runs every automated check and prints PASS / FAIL / SKIP. A check whose inputs are missing, such as the ROMs or the `engine/` checkout, is skipped. The checks cover:
- the scripts compile;
- the `engine/` fork has no uncommitted or unpushed work;
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

## Paths

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
| `MS_SHIPWRIGHT` | `engine/` (the reference port submodule) |
| `MS_ZAPD`, `MS_ZAPD_CWD` | `$MS_SHIPWRIGHT/build-ml/ZAPD/ZAPD.out` (built in `engine/`, patched ZAPDTR), `$MS_SHIPWRIGHT/soh` |
| `MS_OOT10_ROM` | `roms/oot-usa10.z64` (OoT USA 1.0, for `tools/nodata.py`) |
| `MS_UNBOUND_SRC` | `../Unbound-923-src` (Unbound at its last SoH 9.2.3 commit, for the layer tools) |
| `MS_CODEMOD_SRC`, `MS_CODEMOD` | `../Unbound-skijer930` (ML's code mod checkout), `dist/codemod/missinglink.o2r` |

ZAPD's config (`pipeline/Config_ML.xml`) is a template; each run writes the filled-in copy to `pipeline/out/`.

## The hack the tools analyse

The ROM tools in `tools/` read the hack and its baseline from `hacks/<name>/hack.json` (MS-090): each ROM's
dmadata and `code` / player dma indices, `code`'s VRAM, the actor table, `Actor_Spawn`, and the hack's custom-code
files. `hacks/ml` is the default; `MS_HACK` picks another. `python3 tools/hack.py` shows what it resolved. The
tools assume the hack's `code` has zero shift against the baseline, as ML's does against PAL MQ Debug.

`hacks/selftest` compares PAL MQ Debug with itself: every tool runs on it with no other change and reports no
differences, e.g. `MS_HACK=hacks/selftest python3 tools/codediff.py`.

## Layout

| Path | What |
|---|---|
| `mod/MissingLink_Scenes.o2r` | ML's archive, the pipeline's output (local only, not in git) |
| `home/` | development SHIP_HOME (`oot.o2r` for the pipeline; local only, not in git) |
| `pipeline/` | ROM → `.o2r` pipeline: ZAPD XMLs (`xml/scenes`, `xml/objects`), injectors, maps; the mod builder (`build_mod.py`) and its window (`builder_gui.py`) |
| `tools/` | hack-vs-baseline analysis: `modcheck`, `aligndiff`, `dumpactor`, `codediff`, `pldiff`, `mlcustom`, …; project checks: `check`, `genpatch`, `survey_actors`, …; audio: `sfxtrace` (which sound effects play a font slot); release packaging: `package`, `freeze_builder`, `codemod` (ML's code mod, or `childshield`, for every OS), `nodata` (no game data in a package), `deltapack` (a mod as deltas against the player's own base) |
| `hacks/` | per-hack configs for `tools/` (`ml`, `selftest`) |
| `dist/` | release packages from `tools/package.py` (not committed) |
| `docs/ROADMAP.md` | status, history and the remaining long-term work |
| `docs/ENGINE_CHANGES.md` | every change ML makes to the game, why, and how it is ported (the ledger) |
| `docs/UNBOUND_MAP.md` | every ported change placed against SoH: Unbound, and the order of work there |
| `docs/ML_ACTOR_INVENTORY.md` | per-actor port status |
| `docs/PORTING_ACTORS.md` | how to port an actor (diff → C → object → build) |
| `docs/DROPIN_PROPOSALS.md` | what each ported change would need to move upstream or into data |
| `docs/WORKING_NOTES.md`, `docs/notes/` | handoff notes and key facts (addresses, gotchas) |
| `docs/*.patch` | generated from the reference port by `tools/genpatch.py`: its changes against SoH 9.2.3 |

## Goals

1. **All of The Missing Link playable on SoH.** Done; now being checked against ML on N64.
2. **ML for the stock SoH: Unbound players already have**, built by our builder from their own ROM, with ML's code as a code mod. Pre-released; what is left is in the roadmap.
3. **Drop-in for stock SoH, long term.** A stock SoH build can load only data, so every ported change is logged with its route to data or to a generic upstream feature (`docs/ENGINE_CHANGES.md`, `docs/DROPIN_PROPOSALS.md`). No upstream PRs until the project is done.
4. **A porting kit for other romhacks.** The method and tools here (dma/object/actor diffs against a vanilla baseline, the ZAPD pipeline, the C porting workflow) are meant to grow into a reusable way to bring OoT romhacks to the SoH engine.
