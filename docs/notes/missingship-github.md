---
name: missingship-github
description: MissingShip is pushed to a PRIVATE GitHub repo (thefriendlyhedgehog/MissingShip); engine lives there only as patches, ROMs/o2r are gitignored
metadata:
  type: reference
---

https://github.com/thefriendlyhedgehog/MissingShip (private, created 2026-09-25 so the user can continue in cloud sessions).
Its own git repo nested in the AI_Projects repo. Engine = docs/soh-missingship.patch (+ zapd/libultraship patches) against
Shipwright cb71e22; regenerate per docs/PORTING_ACTORS.md step 6 and commit+push after engine work.

**Why:** ROMs and anything built from them (*.z64, *.o2r, *.otr, ml_code.bin, MissingShip.app, home/) must never be committed.
**How to apply:** keep the repo private; before pushing, check `git ls-files` for ROM-derived binaries.

Related: [[missingship-goal]], [[missing-link-soh-port]].
