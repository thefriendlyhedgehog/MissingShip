---
name: gentle-builds
description: owner works on the Mac mini while I build; parallel -j10 host build + mod builds froze it (2026-10-28)
metadata:
  type: feedback
---

On 2026-10-28 a `cmake --build build-mac -j 10` host build running beside mod builds froze the owner's workstation
("My workstation is freezing now"); they then said "go ahead, just be mindful I am using the machine".

**Why:** the Mac mini is the owner's own machine (they also use it via RustDesk); my builds compete with their work.

**How to apply:** on the Mac, one heavy job at a time: `nice -n 15 cmake --build ... -j 3`, mod builds niced and
never alongside a host build, test games niced and one at a time. Don't pop up game windows unasked while they work:
prepare a home and a launcher script instead. Related: [[batch-work-items]], [[mod-api-2]].
