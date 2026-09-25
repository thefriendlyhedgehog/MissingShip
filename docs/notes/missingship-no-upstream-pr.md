---
name: missingship-no-upstream-pr
description: MissingShip owner rule — open NO upstream PRs (HarbourMasters/Kenix3 etc.) until the whole project is done; collect candidates on issue MS-070 instead
metadata:
  type: feedback
---

No upstream pull requests (Shipwright, libultraship, ZAPDTR, ...) until everything in MissingShip is done, "at the least".
Said 2026-09-25 after MS-016 (libultraship GfxDpLoadBlock fix) when I offered to open a PR.

**Why:** the owner wants the project finished before engaging upstream; opening a PR also publishes from their account.
**How to apply:** fix upstreamable bugs on our `missingship` branches, log them in ENGINE_CHANGES, and add a comment to
MS-070 (GitHub #42) as a candidate. Don't offer to open PRs again until the project is complete.

Related: [[missingship-github]], [[missingship-goal]].
