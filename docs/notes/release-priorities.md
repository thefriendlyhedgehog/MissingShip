---
name: release-priorities
description: Owner's calls on the pre-release list - no more scripted playthroughs (2026-10-18, testers take over), macOS code signing is a small concern
metadata:
  type: project
---

Owner 2026-10-17, on the left-before-release list:
- Playthrough reruns go on the Mac and the Windows box only; don't rerun the playthrough on Linux.
- 2026-10-18: no more scripted playthroughs at all. The owner played ML through in full themselves; public testers
  cover the rest. Drop the segment 05-07 handover and the 01-03 grotto placements; don't replay segments to verify
  a release.
- Code mod signing only matters on macOS and is of very small concern for now; don't spend time on it unless asked.
- No GitHub Support purge of pre-rewrite commits ([[repo-rewrite]]), provided nothing public carries Nintendo code
  or data ([[no-copyrighted-data-in-git]]).

**Why:** the owner wants effort on what players hit, not on hardening paths.
**How to apply:** when picking the next item, leave these three out unless the owner raises them. Related:
[[keep-going-no-asking]], [[linux-test-box]].
