---
name: ml-hood-roll-gaps
description: "ML's long gaps (Hollow Root Keep rooms 5/14/16/17/24) are crossed by rolling off the edge with the Bunny Hood, not running jumps; not a port gap (2026-10-15)"
metadata:
  node_type: memory
  type: project
  modified: 2026-10-15T21:01:55.422Z
---

Hollow Root Keep's gaps that a running jump misses by 1-2 frames (room 17's pillars, room 5's column vines, room 16's
y 875 walkway to the map chest) are crossed by a **roll off the edge with the Bunny Hood on**. ML doubles
R_RUN_SPEED_LIMIT with the hood (1100). A roll's speed (~13.5) is clamped to that in the air, so it leaves the edge at
~10.9 against ~9.1 for a run. Verified in play on skijer's 9.3.0 + the code mod on 2026-10-15; the code mod already
matches ML, so nothing needed fixing. A void-out takes the hood off.

Also room 14 (lower block -159 to upper -75, 2026-10-17: owner confirmed "hooded roll long jump"; it ends in a ledge
grab, and the take-off frame decides it, so the script retries with `again above <y>`) and room 24's south ring gap
to the third cursed man (8/8). Switch flags 0x20-0x37 last through the scene's doors (only 0x38+ clear on a room
change), so room 13's crystal sets 0x20 for room 14.

**Why:** a session spent hours hunting a "missing" player edit; the owner's D4 run simply rolled.

**How to apply:** when an ML jump looks impossible, try a hooded roll before suspecting the port. mlplay has
`rollto <x> <z> <dist> [frames]` (Unbound-modapi ml-playtest 185f8fec7). For per-frame physics, a temporary mlplay
trace of pos/linearVelocity/velocity.y/stateFlags1 settles it fast. Related: [[ml-on-unbound]].
