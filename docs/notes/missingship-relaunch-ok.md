---
name: missingship-relaunch-ok
description: "During MissingShip testing, the user has pre-approved killing the running game to relaunch a new build -- no need to ask first"
metadata:
  node_type: memory
  type: feedback
  originSessionId: c90fe9a6-ad2e-4ae3-ba84-7caccc4ca425
  modified: 2026-09-23T23:51:28.103Z
---

When a new MissingShip build is staged during active testing, **relaunch it without asking**: kill the
old instance (`pkill -x MissingShip`) and start `MissingShip.sh`. The user said (2026-09-23): "start it
up and dont hesitate to kill the old one if we are in the middle of testing."

**Why:** the user is iterating on bug reports and wants the fix in front of them immediately; asking
first just stalls the loop. Earlier they had asked me to check their screen before killing, so this is
an explicit upgrade of that permission for testing sessions.

**How to apply:**
- Still take a screenshot first if the user has just asked me to look at something on screen, so
  what they wanted me to see isn't destroyed.
- Keep the one-instance rule: kill, wait until `pgrep -x MissingShip` is empty, then launch.
- Mention the relaunch in the reply, and remind them about anything unsaved being lost if the change
  was made mid-play (Remember Save Location puts them back at their last save).
- Outside active testing, default back to asking. See [[missing-link-soh-port]], [[missingship-goal]].
