---
name: pack-rom-releases
description: "ML pack is made over 7 bases (USA 1.0/1.1/1.2 × SoH 9.2.3 and 9.3.0, + 9.1.1's 1.2); where ROMs and the 9.3.0 build live"
metadata:
  node_type: memory
  type: project
  modified: 2026-10-13T12:16:45.225Z
---

Delta packs depend on both the ROM release and the extractor version. Since 2026-10-09 the shipped ML pack is made
over seven bases:
- USA 1.0, 1.1 (Rev A) and 1.2 (Rev B), each by our Unbound build (SoH 9.2.3, ZAPD);
- the same three by stock SoH 9.3.0 (Torch);
- the owner's real USA 1.2 `oot.o2r` made by SoH 9.1.1 (in ~/Library/Application Support/com.shipofharkinian.soh).

Since the 0.11 merge (2026-10-11) the main base (`--oot-o2r`) is USA 1.0 from our Unbound 0.11 build (`Unbound/build-930`, Torch, byte-for-byte stock 9.3.0's); the other six go in as `--also-base`. Packaging takes Unbound's app from `build-930` (cpack). PAL and GameCube releases
are not covered.

- **ROMs:** `/Volumes/MacMiniExternal/Games/roms/n64/the-legend-of-zelda-ocarina-of-time-usa_202012/` (USA, Rev A,
  Rev B). Read them in place; never copy them into a repo ([[no-copyrighted-data-in-git]]).
  `MissingShip/roms/oot-usa10.z64` is a git-ignored symlink to the USA 1.0 ROM (2026-10-17): mspaths OOT10_ROM, read by
  tools/nodata.py (MS-156), which package.py runs on the Unbound packages.
- **ML v2.0:** `MissingShip/pipeline/tml_2_0.z64` (untracked; also `dist/*-test/tml_z64_2_0_big_endian.bps`). The
  `.bps` in `Games/Zelda Stuff` is an older ML and the builder refuses it.
- **SoH 9.3.0:** a stock build is at `/Volumes/MacMiniExternal/AI_Projects/soh-9.3.0/build/soh/soh-macos`.

**Why:** a player whose `oot.o2r` the pack doesn't cover gets the refusal popup instead of ML. SoH 9.3.0's Torch
writes 58 files differently (sound fonts, boss title cards), and the pack was refused over them until 9.3.0's bases
were added.

**How to apply:** scratch gets wiped, so re-extract the bases before rebuilding (2026-10-13 run: 1.0 by `Unbound/build-930` Metal 3, 1.1/1.2 by stock `soh-9.3.0` Metal 3, the 9.2.3 three by `Unbound/build-ub` Metal 2, ~10 s each; then `build_mod.py ... --pack` from the main checkout ~85 s). Before `cpack`, run `cmake build-930` so the app's commit label (set at configure time) matches HEAD. Run `<soh-macos> <ROM>` in a muted,
Metal scratch home; it needs no clicks. Wait for the "version file" line in game.out, then stop it. A new SoH release
means adding its three bases.
