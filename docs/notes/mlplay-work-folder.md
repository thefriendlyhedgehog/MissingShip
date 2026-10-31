---
name: mlplay-work-folder
description: "mlplay playthrough saves/bases/layer live in AI_Projects/mlplay-work (persistent), not session scratch; chain.sh replays segments"
metadata:
  node_type: memory
  type: project
  modified: 2026-10-16T10:35:48.883Z
---

The session scratch (/private/tmp/claude-501/...) was wiped when the app quit on 2026-10-15/16, taking every
playthrough save (save-k10/k12/k16), the 9.3.0 base and the --stock layer. Since 2026-10-16 they live in
`/Volumes/MacMiniExternal/AI_Projects/mlplay-work` (outside every repo; never commit it):
- `bases/usa10-930.o2r` (skijer930 build run once on the USA 1.0 ROM), `layer/MissingLink_Unbound.o2r`
  (`pipeline/build_mod.py pipeline/tml_2_0.z64 --unbound ../Unbound-skijer930/build-mac/soh/soh-macos --stock
  --oot-o2r <base> --code-mod dist/codemod/missinglink.o2r --out ...`, from MissingShip).
- `chain.sh [first-seg]` replays the segments in order (01..14; 14 ends on The End, its save is the warp's) and keeps each end save as `save-<NN>`; `chain.log` lists
  FAIL/FORCED per segment. `tx/` holds HAKAdan scene data unzipped from the layer; `tools/polys.py` lists collision (`polys.py bin nv npl x0 x1 z0 z1 [y0 y1]`; kind is awk field $4) and `tools/fmap.py` prints a top-floor height grid ('~' = conveyor type 0).

**Why:** a lost save chain costs a full replay of 01..14 (~2 h of game time on the Mac; 14 alone ~13 min).

**How to apply:** run mlplay with `WORK=/Volumes/MacMiniExternal/AI_Projects/mlplay-work` and copy every end save
there. Related: [[test-games-muted]], [[mac-tests-use-metal]].
