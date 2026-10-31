---
name: soh-unbound-and-prelude
description: "Owner-flagged references for the romhack porting kit - roborich's SoH: Unbound fork (mod-added scenes/entrances, raised limits, stable names) and the Prelude browser editor"
metadata:
  node_type: memory
  type: reference
  modified: 2026-09-25T22:50:48.612Z
---

Owner flagged these on 2026-09-25 as relevant to refining the port process for other romhacks (the porting kit, MS-091..093).

- **github.com/roborich/Shipwright**: a fork of HarbourMasters/Shipwright. Its default branch is `wind-waker-style-cel-shading`.
  - The `unbound` branch is **SoH: Unbound** (9.2.3-unbound0.8), on the same SoH 9.2.3 base as our engine.
    - Mods *add* scenes, entrances (declared by name), objects, dialog IDs and songs, instead of replacing vanilla slots.
    - It raises N64 limits: actors, collision, rooms, world size, message length.
    - Unbound mods need `oot-unbound.o2r` and won't load in stock SoH.
  - `stable-names-poc`: scene resource names without ROM offsets (via torch). Our archive uses ZAPD offset names like `spot20_room_0DL_000460`.
  - `wasm`: SoH in the browser.
- **preludeoflight.com**: a browser editor for SoH/2S2H scenes, actors, text and music. It exports `.o2r` files, and the wiki has a reference page for each actor and scene.
  - It can't import romhacks, and it can't add custom scenes unless Unbound is used.
  - `/wiki/oot/version-details` measures which scene resource names differ between ROM versions.
  - `/wiki/oot/guides/unbound` covers Unbound.

**How to apply:**
- For the porting kit, Unbound's mod-declared tables are the model for replacing our engine table patches (the `ENTR_MISSINGSHIP_*` entrance rows, scene slot reuse) with data shipped in the mod.
- The owner decided on 2026-09-27 to move ML onto Unbound: see [[ml-on-unbound]].
- Related: [[missingship-author-intent]].
