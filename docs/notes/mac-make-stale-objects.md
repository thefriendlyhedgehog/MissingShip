---
name: mac-make-stale-objects
description: macOS make 3.81 compares whole seconds; a stash/build/pop A/B leaves stale .o files in later mod builds
metadata:
  node_type: memory
  type: feedback
  modified: 2026-10-13T15:58:46.497Z
---

After an A/B done with `git stash push <files>` → build → `git stash pop` in one command, the popped sources can share
the same second as the objects compiled from the stashed state. macOS's make 3.81 compares mtimes in whole seconds, so
later `build_mods.py` runs keep the OLD objects (seen 2026-10-13: ml_text.c/ml_hud.c ran old code for three test runs).

**Why:** a test then measures code that isn't the committed source, and the cause looks like a logic bug.

**How to apply:** after any stash/checkout-based A/B, `touch` the restored sources (after a `sleep 1`) before the next
build, and compare `stat` times of the `.c.o` files under `build/mod-sdk/CMakeFiles/missinglink.dir` with their sources
if a result looks like the old behaviour. See [[batch-work-items]].
