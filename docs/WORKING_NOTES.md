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
- Engine: private fork `MissingShip-Shipwright`, the `engine/` submodule (branch `missingship` on tag 9.2.3; submodules MissingShip-libultraship /
  MissingShip-ZAPDTR, branch `missingship`). Commit + push engine work there, then commit the `engine` pointer here; `tools/genpatch.py` refreshes docs/*.patch.
- Before pushing: `python3 tools/check.py` (enable the hook once: `git config core.hooksPath .githooks`).
- Paths: everything resolves through `pipeline/mspaths.py` / `mspaths.sh` (env overrides `MS_*`, see README).
- New actor dirs need `cmake -S . -B build-ml` (in `engine/`) before building. Deploy: copy `engine/build-ml/soh/soh-macos` over
  `MissingShip.app/Contents/MacOS/MissingShip`, then `pipeline/ms_cycle.sh` (rebuilds the .o2r and relaunches).
- New ML object: add `pipeline/xml/objects/object_ml_<name>.xml`, name its dma index in ML_OBJECTS in
  `ms_cycle.sh`, declare paths in `soh/assets/ml_assets.h`.

## Last batch (2026-09-25, built + deployed, NOT yet tested in game)
- **Ml_Malon** (slot 0xE7, ML dma 236; object 0xE0 = dma 55 -> object_ml_ma1). Race state = switch 0x11 of scene 0x5A,
  read from the save at Init. Arriving by ML entrance 0x13C: set entrance to 0x58, she speaks first (0x204D won /
  0x2050 lost). params 0xFFFF = finish line (within 80, not below her, timer > 0 -> switch 0x11, timerState 10,
  warp 0x13C). Otherwise host: text 0x204A, choice 0 -> 0x204C + warp 0x138 (the race), else 0x204B. After a win:
  text 0x204F and a cucco (actor 0x0D params 4) at (-778, -485, 308) rot y 0x1680; also spawned when 0x204E shows.
  All ML entrance numbers go through `MlEntrance_ToSoh`. Where ML places the race is not yet identified.
- **Ml_Cucco** (slot 0x0D, vanilla En_Poh's): ML's copy of En_Niw with object 0x09 (dma 354, a recoloured
  object_niw -> object_ml_cucco). Implemented inside ovl_En_Niw (`Ml_Cucco_InitVars`, `NIW_IS_ML`): health 0x77 at
  Init, all CHICKEN_CRY_M and CRY_A calls removed (CRY_N and DEKU_WAKEUP kept).
- Earlier untested batch: title cards, Navi muted, text speed 2, save resume location, item page layout, bean
  seller, Deku Road Skull Kid + Blade Beam heart, grotto scrub shop, hole-hopping scrub (ydan room 2), readable
  notes, Deep Woods wall-skip.

## Open items
- Actor todo list: `docs/ML_ACTOR_INVENTORY.md` (rows marked todo).
- HUD restyle (ML 0x80604E30 / 0x806053E0, colour 0x80089180); dungeon minimap textures (room + 190).
- Magic Hourglass (0x80600E00, not ported). Link object/animation port.
- Crate freeze in malon_stable room 3 (Bg_Spot15_Rrbox, possible OOB read of D_808B45C4 with switch 0 set) -- low priority.
