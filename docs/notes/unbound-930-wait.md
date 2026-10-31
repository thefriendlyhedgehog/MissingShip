---
name: unbound-930-wait
description: Unbound 0.11 (SoH 9.3.0) merged into missinglink 2026-10-11 after the owner's wait-for-roborich decision; layer tools compare against a pinned 9.2.3 tree
metadata:
  type: project
---

On 2026-10-09 the owner decided to wait for roborich to move Unbound to SoH 9.3.0 rather than merge 9.3.0 ourselves.
He did (Unbound 0.11, `upstream/unbound` ace55888e). We merged it on 2026-10-11:
- Unbound `missinglink` 4440c5fa2;
- libultraship `ms/missinglink` 8918b411.

Torch replaced ZAPDTR and OTRExporter in Unbound. ML's pack mounts over both 9.2.3 and 9.3.0 bases, and the DMG
clean-install works.

Our SoH fork stays on 9.2.3. The `tools/unbound_ml_*` layer tools therefore compare it with Unbound at its last 9.2.3
commit, not with the Unbound players run:
- the commit is `mspaths.UNBOUND_COMPARE` = 7714a97bf;
- the tree is a worktree at `../Unbound-923-src`, or wherever `MS_UNBOUND_SRC` points;
- `build_mod`, `unbound_layer` and `freeze_builder` check it.

**Why:** comparing 9.2.3 with 9.3.0 picks up SoH's own changes. The pack build failed on z_draw.c because of this.

**How to apply:**
- Keep the worktree; recreate it with `git -C ../Unbound worktree add --detach ../Unbound-923-src 7714a97bf0`.
- Move the pin only when the fork moves to a newer SoH.
- In 9.3.0's libultraship, Metal is backend Id 3 ([[mac-tests-use-metal]]).
- See [[check-unbound-upstream]] and [[pack-rom-releases]].
