# Working notes (handoff state)

Where the port stands, so a new session (local or cloud) can pick up without the chat history.
Project memory notes from the local sessions are copied in `docs/notes/`. The plan and issue list are in `ROADMAP.md`.

## Standing rules
- No multi-agent swarms (conserve tokens). One game instance at a time; relaunching during testing is fine.
- Settings stay separate from stock SoH. Randomizer out of scope; QoL in scope.
- Every engine change goes in `docs/ENGINE_CHANGES.md`; actor status in `docs/ML_ACTOR_INVENTORY.md`.
- Never commit ROMs or anything built from them (see `.gitignore`).

## Key facts
- ML ROM: code is dma 114, vram base 0x8001CE60. Baseline: PAL MQ Debug, code dma 28, actor table 0xF9440, no shift.
- ML custom engine code: dma 507 at 0x80600000 (`tools/mlcustom.py`).
- gSaveContext 0x8015E660; ML PlayState fixed at 0x80212020.
- ML object table in code at +0x10A6C8 (8-byte {vromStart, vromEnd} entries).
- Many "rewritten" actors are vanilla source recompiled with edits: diff with `tools/aligndiff.py <ml_id> [vanilla_id]`
  (second id when ML moved an actor to another slot), then copy SoH's source and apply the diff.
- Engine: the engine fork, cloned at `engine/` (not tracked here; branch `missingship` on tag 9.2.3; its
  submodules are our libultraship and ZAPDTR forks, branch `missingship`). Commit and push engine work there;
  `tools/genpatch.py` refreshes docs/*.patch.
- Before pushing: `python3 tools/check.py` (enable the hook once: `git config core.hooksPath .githooks`).
- Paths: everything resolves through `pipeline/mspaths.py` (env overrides `MS_*`, see DEVELOPMENT.md).
- New actor dirs need `cmake -S . -B build-ml` (in `engine/`) before building. Deploy: copy `engine/build-ml/soh/soh-macos` over
  `MissingShip.app/Contents/MacOS/MissingShip`, then `python3 pipeline/ms_cycle.py` (rebuilds the .o2r and relaunches; `--no-launch` only builds).
- New ML object: add `pipeline/xml/objects/object_ml_<name>.xml`, name its dma index in ML_OBJECTS in
  `pipeline/ms_cycle.py`, declare paths in `soh/assets/ml_assets.h`.

## Where things stand (2026-10-31)
- **The release route is ML for stock SoH: Unbound** (SoH 9.3.0 or later, skijer's mod API 2). Players install one
  file, `missinglink.o2r` (NEI-Store or Releases); its porter asks for the ROM and ML's patch, writes ML's layer
  `MissingLink_Unbound.o2r` into `mods/` and asks for a restart. Current pre-release: `unbound-930-porter-5`
  (missinglink 0.4, childshield 0.2), made with `tools/porterpack.py`. The builder (`unbound-930-builder-N`) is retired.
- **Code mods** are in `codemods/` (missinglink, childshield), synced from `ml-codemod-930`. skijer signs them through
  his signing issue (`/sign <commit>`); signed files go to his store release `mods`.
- **Held:** missinglink 0.5 (ML's files are Original Quest; Rando, MQ, Boss Rush and speedrun files don't open) and
  ML as a mod quest. On a host with mod quests, ML is its own stop on the quest carousel with its title logo, its world
  covers the game's files only while an ML file is played, and `ml_patch.c` puts back what ML wrote into the game's
  tables. The host side is offered as skijer/Shipwright#45 (opened 2026-10-28, no review yet); both ship together
  once it merges.
- **Branches of our Unbound fork:**
  - `ml-codemod-930`: ML's code mod, `mod-sdk/mods/missinglink`, on skijer's mod API, plus the mod quests host
    commits (the same change as #45); `tools/codemod.py` builds it for every OS.
  - `ml-playtest` / `ml-playtest-930`: `mlplay`, the scripted playthrough and test driver (`reset`, `newfile`, `seed`,
    `shot`).
  - `missinglink`: our Unbound branch and the pack route, kept for testing.
- **The engine fork** (SoH 9.2.3, `engine/`) is the development reference: never distributed, no new features.
- **Windows mods:** a game function's plain name is the mod's import thunk, so compares and stores of game function
  addresses go through `__imp_<name>` (`ML_HOST_FN` in `missinglink.h`). Calls by the plain name are fine.
- **No game data in packages:** `tools/nodata.py` (MS-156), which `package.py` runs.

## Open items
What is left before a public release is in `ROADMAP.md` (Status); the long-term issues are MS-070 and MS-091-093.
