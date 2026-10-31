---
name: batch-work-items
description: Group map rows into batches that share one rebuild and one test pass; conserve the owner's resources
metadata:
  type: feedback
---

Group several ledger rows per work item so one rebuild and one test pass cover them all; keep runs, screenshots and
output small.

**Why:** owner asked 2026-09-29 ("map certain changes together so you can test more at one time with less rebuilding";
"conserve my resources").

**How to apply:** pick rows by shared file/scene/kind, port them all, build once, test with one TEMPDEBUG set covering
all rows (log-once per hook), visit each scene once; only rebuild when a fix is needed. See [[ml-behaviour-as-code-types]].
