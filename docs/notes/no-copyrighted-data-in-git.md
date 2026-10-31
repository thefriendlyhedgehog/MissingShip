---
name: no-copyrighted-data-in-git
description: "The main MissingShip repo will become the public/prod repo, so no ROMs or ROM-derived archives are tracked any more"
metadata:
  node_type: memory
  type: project
  modified: 2026-09-27T01:21:25.985Z
---

On 2026-09-27 the owner said the main MissingShip repo will eventually be "main prod" and asked to remove every copyrighted file Nintendo would object to. Since then `.gitignore` excludes `*.z64/*.n64/*.v64`, `*.o2r/*.otr` (except `MissingShip.app/Contents/Resources/soh.o2r`, SoH's own assets), `pipeline/ml_code.bin` and all of `home/`. They were untracked with `git rm --cached` and still exist locally.

**Why:** ROMs and archives built from them (oot.o2r, mod/MissingLink_Scenes.o2r, pipeline/*.otr) are Nintendo's and the ML authors' data. This replaces the old rule "commit mod/MissingLink_Scenes.o2r and pipeline/ml.otr".

**How to apply:** Never `git add` game data, even with -f. Removing files from HEAD does not clean the history (1.1 GB .git), so before anything goes public the history needs a rewrite or a fresh repo. The owner decides which. Release packages that bundle the mod archive also distribute ML/Nintendo data, so a public release needs players to build the mod from their own ML ROM. Raise both before going public. Related: [[public-defaults-curated]].

**Decided 2026-09-27:** option B. Packages ship a per-OS mod builder (the pipeline, frozen), and on first launch the game asks for the ML ROM and builds the mod. ML v2.0 is distributed as a patch for a ROM. The prod repo question (fresh repo or history rewrite) is "decide later", before going public. Tracked in milestone "Prod: no game data": MS-153 … MS-156 (#117–#120).

**Superseded for Unbound, 2026-09-29:** the owner chose the pack route. ML on Unbound ships as a pack of delta files (`build_mod.py --unbound --pack`, `tools/package.py unbound-pack`): ML's own bytes (covered by [[kaze-permission]]) plus copies the game makes from the player's own oot.o2r, so no Nintendo data and no player-side builder. Packs are `.o2r` and stay out of git like any archive; they go out as release files. The frozen builder remains for testers. See [[ml-on-unbound]], [[held-upstream-offers]] (delta files must reach roborich for stock Unbound).
