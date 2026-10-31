---
name: mac-tests-use-metal
description: "Mac test runs must use the Metal renderer: Id 3 on SoH 9.3.0/Unbound 0.11, Id 2 on ≤9.2.3 builds"
metadata:
  node_type: memory
  type: feedback
  modified: 2026-09-28T13:50:15.786Z
---

Mac test homes must force the Metal backend before launching. The id depends on the engine: SoH 9.3.0 / Unbound 0.11 libultraship renumbered `WindowBackend` (DX11 1, OpenGL 2, **Metal 3**); SoH ≤9.2.3 builds (our ML fork apps) use Metal 2. Id 2 on a 9.3.0 build silently gives OpenGL (seen 2026-10-11). The shipped `profile/shipofharkinian.json` (Id 2) only goes into the 9.2.3 fork apps; the Unbound app ships no profile and defaults to Metal on Mac. Linux test-home configs set OpenGL (Id 1), so copying them to the Mac silently tests the wrong renderer.

**Why:** 2026-09-28 the owner noticed Mac screenshots titled "Ship of Harkinian (OpenGL)" after Mac homes were rebuilt from Linux configs.

**How to apply:** force the backend in every Mac run script alongside the muting in [[test-games-muted]]; check the window title in screenshots ([[screenshot-game-window-only]]).
