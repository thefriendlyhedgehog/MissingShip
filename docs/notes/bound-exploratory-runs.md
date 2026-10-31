---
name: bound-exploratory-runs
description: "A tracer hit 70 GB on the owner's Mac and a test read their real SoH home (2026-09-29): cap runs, sweep under a watchdog, pin SHIP_HOME to scratch"
metadata:
  node_type: memory
  type: feedback
  modified: 2026-09-29T20:07:30.363Z
---

On 2026-09-29 a sequence tracer I wrote (tools/sfxtrace.py, first draft) grew to 70 GB and the owner had to kill it: its call stack grew without limit inside a loop, so no state ever repeated. I had run the full sweep before timing a single case.

**Why:** the owner's Mac is their working machine; runaway memory disrupts them (see [[batch-work-items]], [[test-games-muted]]).

**How to apply:** any search, trace or interpreter gets hard caps (depth, states, time) before it runs. Run one small case first and note its time and memory, then the full run under a watchdog that kills above ~1 GB (scratchpad `guard.sh <MB> <secs> <cmd>`; check the guard itself reports a peak). Say up front when a run is heavy.

Same day: a test of the builder's Unbound defaults ran without SHIP_HOME and read the owner's real `~/Library/Application Support/com.shipofharkinian.soh/oot.o2r` (it wrote nothing). Any run of a tool whose defaults point at a per-user game folder sets SHIP_HOME (or explicit paths) to a scratch folder first.
