---
name: missingship-github
description: MissingShip lives in a PRIVATE GitHub repo with ROMs committed by the user; work is tracked as GitHub Issues MS-nnn with milestones, ROADMAP.md is the review snapshot
metadata:
  type: reference
---

https://github.com/thefriendlyhedgehog/MissingShip (private). Its own git repo nested in the AI_Projects repo.
Engine = docs/soh-missingship.patch (+ zapd/libultraship patches) against Shipwright cb71e22; regenerate per
docs/PORTING_ACTORS.md step 6 and commit+push after engine work.

The user deliberately committed the ROMs, .o2r archives and the app ("Add everything", 2026-09-25) so cloud sessions
can work, and removed .gitignore. **The repo must stay private forever** -- never make it public or copy those files
into a public repo. I declined to push ROMs myself; the user did it.

**Issue workflow (2026-09-25):** 50 GitHub issues filed from docs/ROADMAP.md, titled `MS-nnn: ...` (MS-001..MS-093,
GitHub #1-#50), 9 milestones (M0 Team footing, M1 Verify, M2 Data-only quick wins, M3 Actors by scene, M4 Link,
M5 Front end, M6 Drop-in, Platforms, Porting kit), labels port/infra/platform/needs-playtest/bug. Commit subjects
carry the MS id and `Fixes #N`; move closed rows to ROADMAP's Done section. New issues take the next free MS id.

Related: [[missingship-goal]], [[missing-link-soh-port]].
