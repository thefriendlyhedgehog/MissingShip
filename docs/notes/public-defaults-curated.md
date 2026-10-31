---
name: public-defaults-curated
description: "Shipped default settings (profile/shipofharkinian.json, package profiles) must be curated public defaults, never the owner's testing config"
metadata:
  node_type: memory
  type: feedback
  modified: 2026-09-26T21:33:42.058Z
---

Default options in anything built for public use (profile/shipofharkinian.json, the Windows/Linux package profile from tools/package.py) must be deliberately chosen defaults, not whatever the owner had set while testing (debug mode, warp points, window position, renderer, skips, text speed, etc.).

**Why:** On 2026-09-26, while packaging Windows/Linux builds, the owner said "when we are building out for pub usage we will want to ensure the default options are good and not what I was testing". The shipped Mac profile had carried test leftovers (a boot warp point, second-monitor window position).

**How to apply:** Never copy home/shipofharkinian.json into a profile. Before a public build, review every key in the shipped profile against stock SoH defaults and ML's intended experience, and confirm non-stock choices with the owner. Related: [[never-duplicate-settings]], [[owner-overrides-ml-look]].
