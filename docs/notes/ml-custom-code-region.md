---
name: ml-custom-code-region
description: "The Missing Link keeps engine-level code in dma file 507 loaded at 0x80600000, reached by j/jal hooks patched into vanilla overlays and code -- check for these before calling an ML actor 'vanilla'"
metadata:
  node_type: memory
  type: project
  originSessionId: c90fe9a6-ad2e-4ae3-ba84-7caccc4ca425
  modified: 2026-09-24T20:30:00.000Z
---

ML's "modified vanilla actors" are often vanilla overlays with a few patched instructions, one of which
jumps into ML's **custom code region**: dma file **507** (vrom 0x3922600, 32 KB) loaded at **0x80600000**.
Targets beyond 0x80608000 (0x8064xxxx, 0x807Cxxxx) exist too and are not yet located. Found 2026-09-24 via
Mido (En_Md 0x16D: `jalr $t9` actionFunc call replaced by `j 0x80602200`).

**Why:** a per-instruction diff is the only way to see these; semantic diffs call them "same". Hooks exist
in 17 actor overlays and ~22 sites in `code`, so this region is likely where Blade Beam and other player
abilities live.

**How to apply:**
- Diff against **PAL MQ (Debug).z64** with code dma 28, actor table base 0xF9440 and no shift. The tools in
  `MissingShip/tools` were fixed for this on 2026-09-24; before that, semdiff used the GC table base 0xF9460
  plus a +0x20 shift and compared the wrong actors.
- Scan an overlay for `j`/`jal` targets in 0x80600000–0x80800000, then disassemble file 507 at `target - 0x80600000`.
- gSaveContext is at 0x8015E660 in ML's layout. For example, ML's Kokiri hook reads `ammo[6]` (the slingshot) at +0x92.
- No debug-ROM function symbol map exists: SoH's `SymbolMap_OoTMqDbg.txt` is empty and the decomp repo has none.

**Decoded so far (2026-09-24):** boot's last words are the loader entry {vrom 0x3922600, vram 0x80600000,
size 0x8000}, so every real hook target is below 0x80608000. 0x80602DE0 is the Blade Beam/sword routine (ported
as ovl_Ml_MThunder), 0x80600E00 is the Magic Hourglass rewind (NOT ported), and 0x80602200 is the Kokiri/Mido text
swap. ML's repurposed items: Bullet Bag 40 = Soul-Charged Kokiri Sword (slingshot ammo 40 is the flag), Boomerang =
Magic Hourglass, Red Potion = Deku Nut Milk, Stone of Agony = Birth Certificate, Gerudo Card = Deku Pass.
Player overlay = ML dma 102 (dma 120 is a dead copy); pause menu = dma 101 (dma 119 is dead).

**Inventory layout (2026-09-24, ported):** ML's item system is vanilla `code` reading ML's data (gItemSlots etc.) --
Item_Give is untouched -- so copying the tables into SoH reproduces it. The mask ML zeroed is UPG_STRENGTH (enum order
quiver, bomb bag, strength, scale), not scale.

**Full `code` patch pass done (2026-09-24):** tools/codediff.py (word diff by function) + tools/mlcustom.py
(disassemble dma 507). ML has NO item-restriction routine: restrictions are vanilla's sRestrictionFlags table with ML
data; 0x80607F20 sets world-map points, 0x806023E0 swaps Link animations, 0x80602DA0 hides the overworld minimap.
Many "rewritten" actors are vanilla source recompiled with edits (look for `beq $tN,$tN` and missing jal) -- port by
copying SoH's actor and applying the diff. Status table: MissingShip/docs/ENGINE_CHANGES.md ("ML code patches").
Tools: tools/modcheck.py (per-actor instruction diff), tools/aligndiff.py (shift-tolerant), tools/kaleido_arrays.py
(match C arrays by value), tools/checkhashes.py; inventory in MissingShip/docs/ML_ACTOR_INVENTORY.md (~45 todo).

Related: [[ml-custom-actors-actordb]], [[missing-link-soh-port]], [[missingship-goal]].
