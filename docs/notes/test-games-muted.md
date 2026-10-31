---
name: test-games-muted
description: "Every test game launch starts muted (integer 0 for CVars.gSettings.Volume.* in the test home shipofharkinian.json)"
metadata:
  node_type: memory
  type: feedback
  modified: 2026-09-27T18:29:36.529Z
---

Start every test game muted: set `CVars.gSettings.Volume.{Master,MainMusic,SubMusic,Fanfare,SFX}` to the
**integer** 0 in the throwaway home's `shipofharkinian.json` before launching, on the Mac and on the Linux box.
They are integer CVars (0-100); a float 0.0 is ignored and Master falls back to 40 (that mistake played sound once).

**Why:** 2026-09-27 the owner heard a test launch at full volume ("hey its full vlume - start it muted").

**How to apply:** part of preparing any test home (scratchpad homes, `~/Downloads/unbound-test/home` on
[[linux-test-box]]); never touch the owner's own settings for this. Close test games with SIGINT, then SIGKILL if still running.

**Incident 2026-10-13:** cm4.sh was passed `ENTR=0x0EE`; its settings writer crashed on `int()` and the script
launched the game anyway, unmuted, for ~30 s. cm4.sh now parses with `int(x, 0)` and refuses to launch when the
settings step fails. Any new launch script must stop if writing the muted settings fails.
