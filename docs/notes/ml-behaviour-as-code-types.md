---
name: ml-behaviour-as-code-types
description: Owner decision 2026-09-28 - ML's actor/behaviour changes go into our Unbound branch as C code types registered by name (route A), not scripting yet
metadata:
  type: project
---

On 2026-09-28 the owner chose route A for UNBOUND_MAP section C ("behaviour beyond `base`"): ML's changed actor code is ported into our Unbound branch (`missinglink`) as C code types registered by name, one documented type per ML change, placed by name from the layer like `ml/grotto`. Scripting (route B) may come later if anything is to go upstream. First: row 60 (`Bg_Spot02_Objects`, the Kakariko crash), then row 14 (Mido, the Kokiri Forest crash).

**Why:** both crashes (Kakariko, Kokiri Forest) were ML code changes that `base` (asset swaps only) cannot express; A makes ML playable soonest.

**How to apply:** keep roborich's style ([[ml-on-unbound]]): `// SOH [Unbound]` tags, SPEC section, how-doc, verification. Record each row as done in docs/UNBOUND_MAP.md.
