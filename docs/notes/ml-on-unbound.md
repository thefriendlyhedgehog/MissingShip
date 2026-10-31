---
name: ml-on-unbound
description: Owner decision 2026-09-27 - ML becomes an SoH-Unbound mod; we build missing Unbound features ourselves in roborich's style, instead of more work on our SoH fork
metadata:
  type: project
---

On 2026-09-27 the owner chose to make The Missing Link a mod for **SoH: Unbound** (roborich/Shipwright, branch `unbound`). Where Unbound lacks a feature ML needs, we implement it ourselves, following roborich's coding and docs style, so it can potentially be contributed back to him. No request list for him; no further feature work on our own SoH engine fork (it stays as-is for the current beta).

Players still supply their own files: the OoT ROM plus Kaze's ML patch or a prepatched ML ROM. Building a base from the ML ROM alone (MS-157) is dropped.

Repo: private thefriendlyhedgehog/our Unbound fork (owner's choice, 2026-09-27), cloned at /Volumes/MacMiniExternal/AI_Projects/Unbound. Branch `unbound` tracks roborich's; our work is on `missinglink`. Remote `upstream` = roborich/Shipwright with pushes disabled; never push or open anything there without the owner.

Order the owner approved: (1) ML's world (scenes, text, entrances) loading as an Unbound mod, no custom behaviour; (2) map our 144 engine-ledger rows to Unbound features; (3) build features one at a time in his format, starting with `base`. Prelude of Light (browser editor) may be used for deeper manual scene fixes.

**Why:** a data-driven mod on a shared engine instead of a private fork; the author (roborich) is responsive and Unbound's goals (mods add scenes/entrances/actors as data, raised limits) match ML's needs.

**How to apply:** new engine work goes into the Unbound clone in his style (`// SOH [Unbound]` tags, SPEC.md as the contract, a how-doc per area). Contributing anything upstream is the owner's call. Related: [[soh-unbound-and-prelude]].
