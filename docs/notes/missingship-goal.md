---
name: missingship-goal
description: "MissingShip's governing goal (set by the user 2026-09-23): the ENTIRE game working on the SoH engine, and long-run a drop-in .o2r for stock SoH -- which means minimising and upstreaming every engine change"
metadata:
  node_type: memory
  type: project
  originSessionId: c90fe9a6-ad2e-4ae3-ba84-7caccc4ca425
  modified: 2026-09-23T23:39:08.125Z
---

**The user's stated goal (2026-09-23):** "ensure everything we do is working on getting the entire game
working on the SOH engine, and then in the long run getting it to run as a mod people can drop into SOH."

**Why this constrains the work:** a drop-in mod for stock SoH can only be DATA (.o2r). Stock SoH 9.2.3 has
no runtime mod-code path -- no dlopen/plugin API, no scripting (verified by grep), and HarbourMasters'
own code-modding guide describes only fork-and-compile. So every C change we make is a drop-in blocker.

**How to apply -- two tracks, every change judged against both:**
1. *Now:* get the whole game working in the MissingShip fork (`Shipwright/` + `MissingShip/`).
2. *Long run:* shrink the engine delta to zero. For every engine change, prefer, in order:
   - a **data-side** fix in the pipeline (best -- e.g. the sample-medium and palette fixes were data);
   - a **generic, data-driven SoH feature** worth upstreaming (e.g. "mods may override the entrance
     cutscene table / append entrances from a resource") so ML's specifics become data;
   - a **bugfix** upstreamable as-is (e.g. the Metal zero-dimension texture guard);
   - only then an ML-specific C change -- isolated, commented `MissingShip:`, and logged.
The hard wall is **actor logic** (ML rewrote many vanilla actors and added its own): that needs either
an upstream mod-code mechanism or stays fork-only. Be honest with the user about that boundary.

**The ledger is `MissingShip/docs/ENGINE_CHANGES.md`** -- keep it updated with every engine change and
its drop-in path. Related: [[missing-link-soh-port]], [[ml-custom-actors-actordb]].

**User scope decisions (2026-09-23):** randomizer is OUT of scope ("randomizer will likely never work with
logic" -- ML replaces too much vanilla item/flag logic). SoH QoL enhancements (fast text etc.) are IN scope
and should keep working. Engine-level QoL (text speed) is ML-agnostic; the risk is enhancements keyed to
VANILLA flags/entrances/actors (cutscene skips, time savers) colliding with ML's rewritten logic -- audit
those as their own pass. Note: with DebugEnabled on, START during a cutscene triggers SoH's debug skip.

**Expanded vision (user, 2026-09-25):** work as a TEAM, not solo -- plan and track through GitHub Issues
(milestones M0-M6, Platforms, Porting kit; see [[missingship-github]]). Long-term: **Windows + Linux support like
SoH** (macOS is the only test platform for now). Beyond ML: build up a knowledge base and the tools into a
**reusable porting kit** -- the user will hand over more romhacks to port to the SoH engine "until we have a ready
made porting option, or they are all done". So prefer generic, configurable tools over ML-hardcoded ones.
