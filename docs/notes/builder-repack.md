---
name: builder-repack
description: "Windows/Linux builders without CI - patch builder-3's CI-frozen builds with a fresh macOS freeze (mlplay-work/repack)"
metadata:
  node_type: memory
  type: project
  modified: 2026-10-20T16:42:58.856Z
---

Since 2026-10-20, Windows/Linux builders are made without Actions ([[actions-quota]]) by patching the CI-frozen builder-3 builds (dist/run-39035094801-builders, Python 3.13, PyInstaller 6.x, Ubuntu 22.04 glibc baseline). The patch takes a fresh macOS freeze at HEAD (`dist/venv-builder`, Python 3.13 too).

Script: `mlplay-work/repack/repack.py <ci builder> <mac builder> <out>`. It swaps in:
- `_internal/{pipeline,soh,tools,unbound,codemod}`;
- the version, unbound and codemod `.txt` files;
- the PYZ modules `builder_gui`, `mspaths`, `bps_apply`, and the `build_mod` entry script.

It rewrites the PKG archive: the ELF `pydata` section on Linux, or the archive appended to the exe on Windows. Then run `package.py unbound-ci <out parent>`.

Valid while ZAPD, the stdlib imports and Python 3.13 stay the same: ZAPDTR's tree at CI equals current. A genuine freeze on a CachyOS box would need a newer glibc than players have.

Windows launcher test (2026-10-20): D:\mlplay-test\b4 (unbound = a copy of game + oot.o2r, t.json for MS_GUI_AUTOTEST, run.cmd, task mlb4). It found WinError 6: the freed console's stdin was inherited, fixed with stdin=DEVNULL in builder_gui.

Related: [[mod-signing]], [[builder-end-goal]], [[windows-test-box]].

Not needed since 2026-10-26: builder-4 was dropped ([[builder-end-goal]]).
