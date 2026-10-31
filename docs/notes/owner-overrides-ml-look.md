---
name: owner-overrides-ml-look
description: "The owner may override ML-faithfulness for visual presentation; when they state how something should look, do it rather than argue from ML's data"
metadata:
  node_type: memory
  type: feedback
  modified: 2026-09-26T15:21:04.403Z
---

When the owner says how something should look (e.g. "the dark boxes behind the title text shouldn't be there"), implement it even if ML itself draws it that way. Record the divergence in the ledger as the owner's request. Don't push back by citing ML's code as proof; offer the evidence once at most.

**Why:** On 2026-09-26 I verified from ML's disassembly that the title bars were ML's design and recommended keeping them. The owner answered "That is a lie - I dont care if we have to divert from ML". They also told me to "stop and think" after I made several guessed fixes in a row.

**How to apply:** "Keep ML as the author intended" still governs gameplay and content. Presentation is the owner's call. When a regression is reported, A/B the builds at full zoom before claiming anything, and don't ship guessed fixes one after another.
