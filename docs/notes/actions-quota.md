---
name: actions-quota
description: owner 2026-10-19 - GitHub Actions on the private repos is out of quota until Nov 1 and must be used sparingly after; build on the test boxes instead
metadata:
  type: feedback
---

On 2026-10-19 GitHub refused to start generate-builds (billing / spending limit). The owner won't raise it: "the billing issue will have to wait until November 1 - we need to be mindful and not kill the quota next month".

**Why:** private-repo Actions minutes are paid and capped. A test run that's started and then cancelled still spends minutes.
**How to apply:** don't start workflows on the private forks (generate-builds, build-missinglink) before 2026-11-01. After that, run them only for a release, never to "check" something. Freeze the Windows and Linux builders on the test boxes ([[windows-test-box]], [[linux-test-box]]) and build mods locally. skijer's sign-mod workflow runs on his repo, so it doesn't count. Related: [[mod-signing]], [[builder-end-goal]].

A push to the Unbound fork's ml-codemod-930 (or ml-codemod*) starts build-missinglink on its own (push trigger). On
2026-10-19 (34ed557e3) it was refused in 3 s for billing, so it cost nothing, but once billing is back each push spends
minutes: before 2026-11-01 batch code mod commits into one push.
