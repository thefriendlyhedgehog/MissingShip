---
name: dont-ask-replays
description: Never ask the owner to replay a stretch they already played; rebuild the route from their recordings
metadata:
  node_type: memory
  type: feedback
  modified: 2026-10-18T14:52:06.379Z
---

When the owner has played a stretch for an mlplay recording, don't ask them to play it again for a cleaner
recording, even if it was split by a save or full of detours. Rebuild the route from the logs instead
(`mlplay-work/spots/route.py <log> [from] [to]` simplifies rec lines into waypoints and button presses per room),
plus the scene data in the o2r (`scenes/<name>/rooms/N.json` actor lists, `spots/colx.py` with SCENE=<name>).

**Why:** 2026-10-18 the owner answered a replay request for segment 04 with "why again!". Their time is the
scarce resource; extra test runs cost only Mac time.

**How to apply:** ask the owner only for stretches no recording covers, with an exact spot. See [[keep-play-tips]].
