---
name: screenshot-game-window-only
description: "test visuals come from the game itself (mlplay shot), never a full-screen capture; window captures fail on macOS 27.0.1"
metadata:
  node_type: memory
  type: feedback
  modified: 2026-09-27T20:04:04.594Z
---

A full-screen `screencapture -x` during a test run (2026-09-27) caught the owner's own RuneLite session instead of the game: the owner uses the Mac while tests run, and the test window may not be in front.

**Why:** the owner's screen is theirs; a full capture shows whatever they are doing and proves nothing about the game.
**How to apply:** use mlplay's `shot <name>` (reads the game's own framebuffer, writes shot-<name>.ppm in SHIP_HOME; `sips -s format png` to view), or rely on the log. Since the macOS 27.0.1 update (2026-10-18) `screencapture -l`, CGWindowListCreateImage (blank) and ScreenCaptureKit (-3811) all fail on the game window; relates to [[test-games-muted]] and the no-interaction rules in [[linux-test-box]].
