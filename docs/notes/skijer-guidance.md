---
name: skijer-guidance
description: skijer 2026-10-19 - link players to PR #11's safe Build Artifacts (permalink), host our own signed mods, raise Unbound needs as PRs
metadata:
  type: reference
---

skijer (Unbound mod API author) to the owner on Discord, 2026-10-19:
- **Downloads:** send players to the **Build Artifacts** list (safe builds: soh-mac, soh-windows, soh-linux) on https://github.com/skijer/Shipwright/pull/11. It's his permalink, rebuilt on each PR merge and every Monday. The **UNSAFE, MODDING ONLY** builds are for modders only. The README, the package READMEs (tools/package.py) and the current release notes link it, not a fixed run.
- **Hosting:** we host our own mods (signed ones too). "People trust more a signed mod" ([[mod-signing]]).
- **Unbound:** we may use our own Unbound fork, but anything we need in Unbound goes to him as a PR, so other modders get it too ([[held-upstream-offers]]).

Checked 2026-10-19: PR #11's build 7376c9f (run 39015228014) loads both signed mods with no prompt.
