---
name: check-unbound-upstream
description: "Owner rule 2026-09-28 - Unbound advances fast; fetch roborich's unbound (and libultraship) before each Unbound work item and bring new commits into missinglink"
metadata:
  node_type: memory
  type: feedback
  modified: 2026-09-29T18:03:54.828Z
---

Before starting each piece of Unbound work, `git fetch upstream` in the Unbound clone and compare with `missinglink`'s base (`git log HEAD..upstream/unbound`). If there are new commits, merge them into `missinglink` (never push upstream), rebuild, re-run the affected tests, and tell the owner what changed, especially SPEC changes that affect our layer or overlap features we built (roborich may build the same thing).

libultraship: since 2026-09-29 Unbound's submodule points at our private thefriendlyhedgehog/our libultraship fork branch `missinglink` (roborich's `unbound` + fixes B3 and B6). In the submodule, remote `origin` is roborich/libultraship and `ms` is ours: `git -C libultraship fetch -q origin`, `git log HEAD..origin/unbound`; new commits are merged into `missinglink`, pushed to `ms`, and the submodule pointer committed in Unbound. The owner chose to also offer the fixes to roborich upstream (2026-09-29). ZAPDTR was a submodule the same way (our our ZAPDTR fork `missinglink`, fix B8) until Unbound 0.11 (SoH 9.3.0, merged 2026-10-11) replaced it with Torch (HarbourMasters/Torch `main`, not forked); fix B8 lives on in the engine fork's ZAPD, which the pipeline uses. `git submodule sync` must not be left pointing `origin` at ours.

skijer: since 2026-10-13 our offers go to skijer/Shipwright `unbound-modapi` (the mod API `ml-codemod` is built on; remote `skijer` in Unbound-modapi). Fetch it too before code-mod work and merge new commits into `ml-codemod`, above all merged offers ([[held-upstream-offers]]).

**Why:** the owner said on 2026-09-28 that Unbound "is rapidly advancing" and we must keep checking it for updates.

**How to apply:** fetching the public upstream on the Mac is fine; the Linux box still syncs by rsync only ([[linux-test-box]]). Unbound's CI builds only `unbound`, tags and PRs; running it on `missinglink` would need the token to cover our libultraship fork, which only the owner sets. Related: [[ml-on-unbound]].
