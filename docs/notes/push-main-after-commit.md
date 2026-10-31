---
name: push-main-after-commit
description: "Push the main MissingShip repo to origin after every commit (replaces the old \"main is not pushed\" rule)"
metadata:
  node_type: memory
  type: feedback
  modified: 2026-09-27T02:25:45.085Z
---

After each commit in the main MissingShip repo, push it with `git push origin main`. The `.githooks/pre-push` check runs first. Engine and libultraship pushes are unchanged (`git push -q origin HEAD:missingship`).

**Why:** The owner said on 2026-09-27: "push main after each commit from now on". Leaving main unpushed had made GitHub 27 commits stale, and the owner reviews the repo there.

**How to apply:** Commit, then push in the same step. The other commit rules still hold: wait for the owner's test, and no game data (see [[no-copyrighted-data-in-git]]). If a push fails the pre-push check, fix the cause; never bypass it with --no-verify.
