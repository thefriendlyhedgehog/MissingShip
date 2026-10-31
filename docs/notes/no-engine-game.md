---
name: no-engine-game
description: owner 2026-10-18 — the engine-based game (our SoH 9.2.3 fork as a playable MissingShip app) is not served; docs and packaging mention only the Unbound route
metadata:
  type: project
---

Owner, 2026-10-18: "remove all mention of our engine based game as we are not serving that". Done in MissingShip
2899c75: README offers only the Unbound builder route; tools/package.py lost the MissingShip-<os> engine packages.
The fork stays as `engine/`, described only as the development reference (ZAPD for the pipeline, the ledger
docs/ENGINE_CHANGES.md, generate-builds which freezes the Unbound builders).

**Why:** players get ML only on stock SoH: Unbound ([[ml-on-unbound]], [[builder-end-goal]]).
**How to apply:** never present the engine fork as something to play or download, in docs, releases or issues.
