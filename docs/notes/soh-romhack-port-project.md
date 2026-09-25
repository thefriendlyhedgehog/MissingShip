---
name: soh-romhack-port-project
description: "Kevin's project to port older OoT romhacks (Missing Link, Nimpize) into the modern decomp/SoH codebase — research already done, do not redo"
metadata: 
  node_type: memory
  type: project
  originSessionId: c90fe9a6-ad2e-4ae3-ba84-7caccc4ca425
  modified: 2026-09-18T00:14:41.218Z
---

Kevin wants to **port older OoT romhacks into the newer decomp-based code** (Ship of Harkinian /
zeldaret-oot lineage). Named targets: **The Missing Link** (Kaze Emanuar) and **Nimpize Adventure**
(Nede Nedawg). Stated 2026-09-17. This is a build project, NOT a "how do I play these" question —
he explicitly rejected the emulator route.

**He says a prior port already exists out there and will find it himself when he has time.** Do not
burn tokens hunting for it unprompted; when he asks, the promising angles are decomp-based romhack
bases (HackerN64/HackerOoT), Shipwright forks that added custom scene/actor support, and GameBanana's
SoH Hub (gamebanana.com/games/16121).

**Research already done 2026-09-17 — don't repeat it:**
- *Why a naive port is impossible:* SoH compiles decomp C; these hacks are byte-edits to the N64 ROM
  (SharpOcarina / Utility of Time / hand-written MIPS). No source exists to port. Missing Link is the
  hardest case — Kaze works in custom MIPS assembly + engine changes, and it was DMCA'd off GitHub so
  only patches circulate.
- *SoH hard-gates the ROM:* `soh/soh/Extractor/Extract.cpp:86` is a 21-entry `goodCrcs` array; line 134
  is "Rom CRC did not match the list of known compatible roms." Any modified ROM is rejected at import.
- *OTRMod (`xoascf/OTRMod`) is asset-only* — textures, music sequences, in-game text. Its flagship use
  is TRANSLATIONS (eduardo_a2j Spanish, pt-BR/it/ru). It carries NO scenes, actors, or logic. The name
  oversells it; do not treat it as a romhack-porting path.
- *Recompilation is the technically correct vehicle* (works on machine code, so custom ASM comes free)
  **but OoT recomp has NOT shipped.** Verified against the repo, not write-ups: Zelda64Recomp's
  description still reads "and soon Ocarina of Time", root configs are all MM (`us.rev1.toml`,
  `overlays.us.rev1.txt`, `aspMain.us.rev1.toml`), latest release v1.2.2 (Aug 2025) though the repo is
  active (pushed Aug 2026, has a `feature/metal` branch). **SEO listicles claiming OoT recomp already
  exists are WRONG** — several assert it; the repo contradicts them. Always check the repo.
- *Unconfirmed rumor:* a secondhand mention that a Missing Link PC version is in development. Two
  targeted searches found nothing. Treat as rumor.

**Stock SoH install (done 2026-09-16):** 9.2.3 "Ackbar Delta" at
`/Volumes/MacMiniExternal/AI_Projects/SOH/soh.app` — universal, ad-hoc signed, downloaded via curl so
it has NO quarantine attribute (no Gatekeeper dance needed). Its SHIP_HOME is the pre-existing
`~/Library/Application Support/com.shipofharkinian.soh/` with `oot.o2r` (Dec 2025), saves, a Holidays
in Hyrule mod pack, and `AudioBackend = sdl` — which is why stock SoH dodges the libultraship CoreAudio
sample-rate bug documented in [[comboship-macos-port]].

Note `/Volumes/MacMiniExternal/AI_Projects/` is ITSELF a git repo (remote: openFPGA-Genesis_savestates),
so `git` commands run from subdirs like SOH/ walk up to it. Those subdirs are not their own repos.

See [[comboship-macos-port]] (same SoH/libultraship stack, macOS build knowledge) and
[[ponytail-coding-philosophy]].

**Round 2 research (2026-09-17, Kevin also searching independently):**
- **Zelda64Recomp issue #646** (github.com/Zelda64Recomp/Zelda64Recomp/issues/646) — opened
  2025-08-11 by silbinarywolf, explicitly requesting Missing Link support. **Closed as "not planned."**
  This is the strongest verified negative signal: the one project with the right tech (static
  recompilation, works on machine code) was asked about Missing Link by name and declined.
- **N64Recomp's real requirement:** it needs an ELF file for metadata (README: "the only way to
  provide the required metadata is by passing an elf file"). For a romhack like Missing Link, that
  means needing Kaze's own build ELF/symbol map — which he doesn't release — or reverse-engineering
  symbols from scratch, which is close to a mini-decomp of just the hack's diff. Not a quick tool run.
- **No Shipwright fork adds romhack/custom-scene support** — checked Android, Switch, zh-CN forks,
  all just localization/platform ports.
- **readonlymemo.com aggregator, Sept 2026 snapshot:** confirms OoT recomp still NOT shipped (only
  SoH's decomp-based 9.2.3 listed under Ocarina of Time); no OoT romhack recompilation anywhere.
  15 other N64 games have shipped Recompiled ports (Banjo-Kazooie, DK64, Star Fox 64, etc.) — the
  tooling clearly works, it's just never been pointed at an OoT romhack successfully.
- I initially second-guessed my own "Missing Link is binary/ASM, not decomp-based" claim after a
  WebSearch summary asserted the opposite ("uses the OoT Decomp as its foundation") — COULD NOT
  VERIFY that via primary source (tml.z64.me doesn't resolve, fandom wiki paywalled) and it
  contradicts Kaze's well-documented public methodology (assembly/binary-level hacking, distinct
  from his separate HackerSM64 decomp-base project for OTHERS to build on). Treat that summary
  claim as a likely search-hallucination, not fact.
- **Net result of round 2: still no known existing port of Missing Link or Nimpize to SoH/decomp/
  recomp code found anywhere public.** Kevin is searching in parallel for the specific prior-art
  reference he recalls; do not re-run these same searches next time — ask him what he found first.

**BREAKTHROUGH (2026-09-17, later same session) — patched ROM's `code` VRAM base found: `0x8000b2e0`.**
Verified by direct empirical test (clean disassembly: a real setter function at the boundary, a real
matrix/vector-transform function deep in the middle) after Kevin proposed the key idea: don't guess
addresses from boot's internals — diff the two DECOMPRESSED `code` blobs directly against each other.

**The technique (reusable for future overlay work):**
1. Both ROMs' `code` segments were already isolated: vanilla at VROM `0xa62840-0xafd890` (compressed),
   patched at VROM `0x935120-0x9e8890` (compressed) — both Yaz0, verified via magic bytes + exact
   declared-decompressed-size match. Decompressed with a from-scratch Yaz0 decoder (public LZ77-style
   format, `scratchpad/yaz0.py`). Vanilla decompressed size 0x103d30 (1,064,240 B), patched 0x13af30
   (1,290,032 B).
2. Chunk-hash matching (64-byte windows, 16-byte stride, **must exclude low-entropy/degenerate chunks**
   — repetitive padding causes massive spurious matches, e.g. a bogus `0x7fff2420` "candidate" got 284
   false votes from one padding run before filtering) found ~7,900 matched offset-pairs between the two
   blobs.
3. **The address-solving trick:** MIPS `jal` encodes an absolute 26-bit target; decoding it only needs
   the caller's own top 4 address bits (always `1000` for any `0x8xxxxxxx` addr), not the full address.
   So if a `jal` instruction's raw bytes are IDENTICAL between vanilla (known address) and patched
   (unknown base), its decoded target is the *same number* in both, regardless of patched's actual load
   address. Cross-referencing that target against a SECOND independently-matched offset pair gives a
   direct equation: `base = target_addr - patched_offset_of_callee`. Every matching `jal` "votes" for a
   candidate base; the correct one wins by a landslide once padding-driven noise is filtered out.
4. Sanity check that mattered: the boot-truncation insight. `boot`'s dmadata-implied size was ~3x
   vanilla's, which looked like a contradiction (0x8000b2e0 sits INSIDE that range) until realizing
   `boot` is a one-shot initializer whose memory is normal to reclaim/overwrite once `code` takes over
   — standard N64-era pattern. Confirmed structurally: debug strings `"link_animetion"`,
   `"icon_item_static"` (a DMA segment-name debug table, `ActorOverlayTable_LogPrint`-style) sit right
   at the `code` boundary — Missing Link has far more files/segments than vanilla (matches the earlier
   BPS-diff finding of large new asset regions), which alone explains boot's inflated dmadata size
   without its actual code growing.

**Current verified memory map (both programs, in the live Ghidra project "A Hub"):**
| | vanilla NTSC 1.0 | patched (Missing Link 2.0) |
|---|---|---|
| `boot` VRAM | 0x80000460 | 0x80000460 (block truncated to 0x8000b2e0 to avoid overlap) |
| `code` VRAM | 0x800110a0 | **0x8000b2e0** |

**Correction logged earlier this session, still valid:** the original "15 new functions in boot" claim
(from before the real memory map existed) was WRONG — a mapping artifact. Re-scanning boot with the
CORRECT map found 0 new-content hits there. The real work — and the actual new-content-vs-code scan —
targets `code`, not `boot`.

**Toolchain notes worth not re-deriving:**
- Ghidra 12.1.3's Script Manager needs a FULL Ghidra process restart to discover brand-new `.py`
  script files (editing an EXISTING already-discovered script does NOT need a restart, just rerun it).
- `.py` GhidraScripts require launching via `pyghidraRun` (not `ghidraRun`) — Ghidra 12.x has no Jython
  fallback anymore. `pyghidraRun` bootstraps its own venv on first run (answer 2x `y` prompts, or the
  venv persists across future launches once created — subsequent runs are silent).
- `~/ghidra_scripts` is the default script dir; it needed a full restart before its "file not found"
  cache entry in the Script Manager's Bundle Manager cleared (created after Ghidra was already running).
- `toAddr()` in PyGhidra needs a HEX STRING (`toAddr("8000b2e0")`), not a raw int — `0x80000000`+ ints
  overflow Java's signed 32-bit int and throw `OverflowError`.
- The small "Disassembled View" panel at the bottom of the CodeBrowser is just a live preview that
  decodes whatever raw bytes are under the cursor AS IF they were instructions — it does NOT reflect
  whether that address is actually analyzed/defined as code in the program's real database. Don't trust
  it as evidence of successful analysis; check the main Listing panel or run `listing.getInstructions()`.
- Memory remapping done via `memory.createInitializedBlock(name, addr, fileBytes, fileOffset, size,
  False)` sourcing from `memory.getAllFileBytes()[0]` (the original import) for raw/uncompressed
  segments, or `memory.createFileBytes(name, 0, len, ByteArrayInputStream(bytearray(data)), monitor)`
  to register a NEW decompressed byte blob as its own FileBytes source for compressed segments.

See [[read-only-audit-first]] — extensive empirical verification at every step (never trusted a single
technique's output without a second, independent check) is exactly why the wrong "15 functions" claim
got caught and fixed rather than compounding into further wasted effort.

**Version Tracking session set up and run (2026-09-17, later same session) — "TML_vs_vanilla_code".**
Reopenable via Ghidra project "A Hub" → Tools → Run Tool → Version Tracking → File → Open Session.
Source = oot_ntsc10_vanilla.z64, Destination = tml_2_0.z64. Ran File → Automatic Version Tracking
(8-stage default correlator pipeline) → **5,825 total matches** (functions + data + labels).

**Cross-validated finding: `code` is genuinely, substantially different from vanilla — not just
recompilation noise.** Two independent methods landed in the same ballpark:
- Naive byte-hashing (64-byte chunks, degenerate/padding filtered): 88.6% of `code` unmatched
- Ghidra's real function-aware correlator (Version Tracking): **3,145 of 3,793 functions in `code`
  (83%) have NO correlation to vanilla at all** (only 724 distinct destination functions matched)

Caveat that still applies: some of the 83% is inevitably compiler/register-allocation noise rather
than genuine content change (recompiling identical source with a different toolchain still produces
different bytes) — Version Tracking's correlators handle relocation better than raw byte-hashing but
not register-renaming. Treat 83% as "a strong majority of `code` doesn't map cleanly to vanilla," not
as a precise measurement of how much is hand-written new logic.

**The actual worklist — top 30 largest unmatched functions in patched `code`, sorted by size (bytes).
These are the highest-value candidates for genuinely new/rewritten engine logic; read them in the
Decompiler in the already-open `tml_2_0.z64` CodeBrowser (memory map already correct, just navigate
to each address):**
```
0x8006acd0  5220    0x80071588  3856    0x8005f408  3584    0x80011d1c  3316
0x800ba0e0  2632    0x8009c740  2576    0x800bd28c  2464    0x8004766c  2144
0x80064db4  2144    0x800f1730  1968    0x800a3c9c  1828    0x8005134c  1784
0x800b1464  1656    0x800d88c0  1656    0x800f5e90  1604    0x8007f1a0  1552
0x80076934  1512    0x80025960  1500    0x80010e90  1288    0x8001eab0  1272
0x8007713c  1220    0x800b0980  1220    0x800af6d8  1204    0x80099e6c  1160
0x800d3574  1156    0x800912f0  1148    0x800bdc50  1136    0x8000e900  1116
0x800fb458  1112    0x800203d0  1096
```
Two spot-checked earlier (not in this top-30, checked before Version Tracking ran, still valid):
`0x80073074` is a genuine text/dialogue control-code parser (branches on `\x86`/`Y`/`-1` byte values,
calls helper functions) — concrete, non-trivial confirmed-real logic, a good first read if picking
just one place to start.

**How to regenerate/extend this analysis (scripts + data all still in scratchpad if that session's
scratchpad still exists, otherwise re-derive from this recipe):**
1. `~/ghidra_scripts/DumpDestFunctions.py` — dumps every function in the current program's `code`
   block to JSON (address, name, size). Edit `OUT_PATH`, run against `tml_2_0.z64`.
2. Export the VT session's Matches table to CSV: select all rows (Cmd+A) in the "Version Tracking
   Matches" panel first — **exporting without selecting all only exports the one selected row**, a
   trap that cost a wasted export earlier this session.
3. Cross-reference: for each dumped function, check whether its address (normalized, strip `0x` and
   leading zeros) appears in the CSV's `Dest Address` column where `Type == 'Function'`. Unmatched =
   not present. Sort by `size` descending for a prioritized worklist.

See [[read-only-audit-first]] and the earlier entries in this file for the full segment-table /
Yaz0 / base-address-solving work this analysis builds on.

**GUI freeze workaround + first real function read (2026-09-17, later same session).**
The interactive CodeBrowser's Listing/Decompiler panels got persistently stuck — status bar and
Window-menu tracked the correct current function correctly, but the visible Listing/Decompiler panes
would not repaint, surviving multiple full process kills (`kill -9` on the JVM AND its two spawned
`.../Decompiler/os/mac_arm_64/decompile` helper subprocesses), program close/reopen, and a window
resize. Root cause never fully identified; a real, reproducible Swing rendering bug, not a one-off
hang. **Workaround that fully solves it: don't rely on the GUI at all.**

`~/ghidra_scripts/DecompileOne.py` — decompiles ONE function directly via Ghidra's `DecompInterface`
Java API and writes clean C to a file. Completely bypasses the stuck Decompiler pane; works from a
plain Script Manager run regardless of the interactive GUI's state. Edit `TARGET_ADDR` and `OUT_PATH`
at the top, rerun. **This is now the standard way to read any function on the worklist** — don't
fight the interactive Decompiler pane again if it freezes, just use this script.

**First worklist function actually read: `0x8006acd0` (5,220 bytes, the largest unmatched function).**
It's a large event/cutscene command dispatcher: dozens of branches keyed off a numeric command ID
(param_3) ranging from ~1000 to ~9,908, each setting a data pointer + small type code then calling a
shared queue/dispatch function (`FUN_800c0808`), matching OoT's cutscene-command-queue pattern. Several
branches reference actual embedded debug-assert source-file-path strings baked into the binary,
including OoT's real get-item actor file (twice), plus effect functions (blur, shield particle) and
audio-trigger calls. The command-ID range is far denser/larger than vanilla's own event system would
need. **Working hypothesis: this is Missing Link's cutscene/event script interpreter** — the actual
mechanism driving its new story content, layered on top of the existing engine's dispatch machinery
rather than a modified vanilla function. This is the strongest concrete evidence yet that `code`'s
83%-unmatched figure reflects genuine new logic, not just compiler noise.

Worth a follow-up pass: this function calls many other unmatched-list candidates and helpers
(FUN_800c0808, FUN_800c0874, FUN_800c04d8, FUN_8007c680, FUN_8002eee4, FUN_80092f88/dac/e70/df0 —
looks audio-related, FUN_800fd9ac, FUN_8007c820, and others) — cross-referencing which of ITS callees
are also on the unmatched list vs. shared/vanilla helpers would map out how much of the surrounding
system is new too, not just this one dispatcher.

Full decompiled C saved at (session-local scratchpad, may not persist across sessions — rerun
DecompileOne.py if gone): `scratchpad/decompiled_8006acd0.c`

**Worklist progress (2026-09-17, later same session) — 9 of 30 functions read, batch pipeline built.**

Corrected the worklist first: re-ran `DumpDestFunctions.py` and confirmed `0x800a2010` (originally
listed at 30,128 bytes, position 4) was a stale function-boundary artifact — it's now absorbed into
a real, tiny function `FUN_800a2000` (a 3-component vector add). Every other entry in the original
top-30 held stable on re-scan. Corrected list shifts up one slot; the actual next-largest after the
first 5 is `0x80011d1c` (3,316 bytes), not `0x800a2010`.

**`~/ghidra_scripts/BatchDecompile.py` — new script, decompiles a whole list of addresses in ONE
Ghidra run**, each to its own file under `scratchpad/batch/decompiled_<addr>.c` plus a `_summary.txt`.
Far more efficient than one GUI round-trip per function (`DecompileOne.py` still exists for single
ad-hoc lookups). Edit the `TARGETS` list at the top, rerun after the usual new-script restart.

**Individually read and categorized so far (9 functions):**

*Confirmed members of ONE cohesive new subsystem (a story/companion-tracking system, working name
"the 4-slot subsystem" — shares global state cluster `DAT_8015e6c8` mode byte + `DAT_8015fa42-45`
backup slots + `DAT_8015fa4a` flag, and a 4-element slot array at `DAT_8015e660`):*
- `0x8006acd0` (5,220B) — event/cutscene command dispatcher, ~35 numbered command IDs (1000-9908),
  references OoT's real get-item actor file path twice, plus blur/shield-particle effects and audio.
  Leading hypothesis: Missing Link's cutscene/event script interpreter.
- `0x80071588` (3,856B) — mode state-machine for the same `DAT_8015e6c8` byte; steps through states
  (-1/0x03/0x06/0x09/';'/'<'/'='/'U'/'Y'), scans the 4-slot array checking each slot's "type" byte
  against numeric ranges, toggling per-slot availability flags.
- `0x8007713c` (17,096B decompiled / large) — **builds raw N64 F3D/RDP display-list commands directly**
  (recognizable opcodes: 0xE7=EndDL, 0xFA=SetPrimColor, 0xE4=TexRect, 0xFC=SetCombine, etc.), loops
  over the SAME 4-slot array (`DAT_8015e660`), and draws per-slot icons keyed by real OoT actor names
  embedded as string literals: `"Obj_Mure"`, `"Bg_Po_Event"`. Strong candidate: the HUD/icon-rendering
  routine for whatever the 4 tracked slots represent (party members? companion status? quest-item
  tracker?). This is the clearest evidence yet of a deliberate, multi-function new system, not
  scattered unrelated additions.

*Likely vanilla infrastructure, recompiled/shifted rather than genuinely new (compiler-noise effect,
not hand-written new logic) — all touch the SAME generic environment-context struct `_DAT_8015fa90`,
distinct from the dispatcher's own state cluster:*
- `0x8005f408` (3,584B) — lighting/environment color blend between two presets over time.
- `0x80011d1c` (3,316B) — clean 3-keyframe RGBA color interpolator.
- `0x800ba0e0` (2,632B) — line-segment vs. axis-aligned-box collision test (12-triangle box, looks
  like a Möller–Trumbore-style ray/segment-triangle helper underneath) — textbook `BgCheck`-style math.
- Also touch `_DAT_8015fa90` without being individually read yet (batch-decompiled, sitting in
  `scratchpad/batch/`, worth a read pass): `0x8001eab0`, `0x8004766c`, `0x800203d0`, `0x800b1464`,
  `0x800f5e90`.

*Ambiguous / not yet resolved:*
- `0x80076934` — sits near-but-not-exactly the dispatcher's state cluster (`DAT_8015e693`,
  `_DAT_8015fa50/54/58` — close addresses, different exact variables). Also builds raw display-list
  commands. Worth reading in full to determine if it's a 4th subsystem member or unrelated.
- `0x8007f1a0` — references several real, DIFFERENT vanilla actor names as a group (`En_Skj`,
  `En_Skjneedle`, `En_Tubo_Trap`, `Obj_Lift`, `Bg_Po_Syokudai`, `Demo_Ext`) — reads like a generic
  "manage these actor types during a scene/cutscene" list, plausibly vanilla, not yet fully read.
- `0x80025960` — has an unusually heavy if/else chain (12 branches) relative to its peers, flagged but
  not yet read.

**Batch-decompiled but not yet read/categorized at all** (files exist in `scratchpad/batch/`, ready
to read immediately, no more Ghidra interaction needed): `0x8009c740`, `0x800bd28c`, `0x80064db4`,
`0x800f1730`, `0x800a3c9c`, `0x8005134c`, `0x800b0980`, `0x800af6d8`, `0x80099e6c`, `0x800d3574`,
`0x800912f0`, `0x800bdc50`, `0x8000e900`, `0x800fb458`.

**GUI note carried forward:** the interactive CodeBrowser Listing/Decompiler panes went stuck-again
partway through this batch (same rendering freeze as the earlier documented one) — did NOT bother
troubleshooting it again since the scripted pipeline (Script Manager running `.py` scripts) works
fine regardless of whether the interactive panels are rendering. If picking this up again and the
GUI looks frozen, don't waste time on it — just run scripts.

**WORKLIST COMPLETE (2026-09-17, later same session) — all 30 largest unmatched functions in `code`
now read and categorized. Headline finding: the top-30-by-size worklist was the WRONG lens for
finding new content — size correlates with generic engine complexity, not with hand-written
additions. ~90% of even the LARGEST unmatched functions are vanilla infrastructure.**

**Final tally of all 30:**
- **Confirmed new subsystem (4 functions, all reference the shared `DAT_8015e660` 4-slot array /
  `DAT_8015e6c8` mode byte / `DAT_8015fa42-45` cluster):** `0x8006acd0` (cutscene/event command
  dispatcher, real get-item actor path embedded twice), `0x80071588` (mode state-machine over the
  4 slots), `0x8007713c` (F3D display-list icon renderer, embeds real actor name strings
  `"Obj_Mure"`/`"Bg_Po_Event"`), and now also `0x80076934` (a second icon/quad display-list builder
  referencing adjacent-but-distinct globals in the same address neighborhood — very likely a 4th
  member, e.g. a second icon variant or slot-highlight draw). **This 4-function cluster is still the
  single strongest, most concrete evidence of new Missing Link content found in this whole pass** —
  working theory unchanged: a story/quest/companion-tracking system with its own HUD icon rendering.
- **Vanilla infrastructure (24 of 30), recompiled/shifted rather than genuinely new — grouped by
  subsystem for anyone auditing this later:**
  - *Collision (`BgCheck`-style):* `0x800ba0e0` (line-vs-box), `0x8005134c` (line-vs-line
    intersection), `0x800bdc50` (triangle/wall closest-point push-out), `0x800bd28c` (line-vs-cylinder),
    `0x8004766c` (floor-height check — confirmed via OoT's real `-32000.0` "no floor" sentinel value).
  - *Lighting/environment/HUD color blend (share `DAT_8011fdcc/fdd0/fdd4` or `_DAT_8015fa90`):*
    `0x8005f408`, `0x80011d1c`, `0x80064db4`, plus untouched-in-depth `0x8001eab0`, `0x800203d0` is
    texture not lighting (see below).
  - *Camera:* `0x800a3c9c` (bit-tested analog-stick/rumble control), `0x800fb458` (smooth eye/at/roll
    point interpolation + state-step counter), `0x800f5e90` (sibling interpolation + reticle/icon
    display-list draw).
  - *Graphics/viewport/texture:* `0x80099e6c` (scissor+viewport+ortho/perspective matrix display-list
    builder), `0x8009c740` & `0x800af6d8` (tiled/scissor textured-rect display-list builders),
    `0x800203d0` (texture LOD/filter setup — confirmed via real libultra macro name strings
    `"G_TF_AVERAGE"`, `"G_CD_MAGICSQ"` appearing as literal debug-log arguments), `0x800f1730`
    (environment-map UV/tangent-space vertex setup).
  - *Texture filtering (RGBA5551 decode + median-filter sorting network, near-identical code shape):*
    `0x800b0980`, `0x800b1464`.
  - *Animation:* `0x800912f0` (keyframe vertex-array blend/copy).
  - *Audio:* `0x800d3574` (sequence-player channel/track bookkeeping — matches OoT's known-ugly audio
    engine code shape), `0x800d88c0` (pitch/frequency scaling math — **confirmed vanilla-decomp
    provenance**: contains the literal string `"../z_vimode.c"`, OoT decomp's real source filename,
    surfaced as a decompiler code/data-overlap artifact reading a jump/format table as a string).
  - *Actor management:* `0x8001eab0` (actor linked-list traversal/update loop, `Actor_UpdateAll`-style).
  - *Scene-specific vanilla cutscene logic:* `0x8007f1a0` — **ambiguity resolved**: references a group
    of real, otherwise-unrelated vanilla actor names (`Bg_Po_Syokudai`, `En_Skj`, `En_Skjneedle`,
    `En_Tubo_Trap`, `Obj_Lift`, `Demo_Ext`) consistent with a single dungeon/area's own demo-management
    code, not new content.
  - *Separate vanilla dialogue/state-machine system:* `0x80025960` — **ambiguity resolved**: a large
    hex-command-ID dispatcher (IDs `0x1035`-`0x703f`) structurally similar to the confirmed new
    dispatcher's pattern but calling entirely different helpers (`FUN_80035b18`/`ba4`/`b4c`/`bcc`) over
    a disjoint ID range — a distinct, likely-vanilla NPC-talk/state-graph system, not the same system.
  - *Unclear subsystem, still vanilla-shaped, low confidence:* `0x8000e900` (fixed-record queue/list
    rotation with a shift-forward-and-decrement-counter pattern — generic enough that the exact
    caller/subsystem wasn't identifiable from the function body alone).

**Conclusion / recommendation for the next session:** brute-force "read the biggest unmatched
functions" is exhausted and was low-yield — 24/30 vanilla, only a tight 4-function cluster confirmed
new. Don't extend this same approach further down the size-sorted list; diminishing returns are
likely to continue (more vanilla engine code, further down in size). **Better next angles, not yet
tried:**
1. Read the callees of the confirmed 4-function cluster (`0x8006acd0`, `0x80071588`, `0x8007713c`,
   `0x80076934`) and check which of THEIR callees are also unmatched — walk outward from a known-good
   seed instead of sorting by raw size. `0x8006acd0`'s own callee list is already recorded above
   (FUN_800c0808, FUN_800c0874, FUN_800c04d8, FUN_8007c680, FUN_8002eee4, FUN_80092f88/dac/e70/df0,
   FUN_800fd9ac, FUN_8007c820) and is the natural starting point.
2. Search the unmatched set for functions referencing embedded strings that are NOT real OoT decomp
   file/actor names (the opposite signal used successfully above) — genuinely new code is more likely
   to reference new asset/actor names never seen in vanilla, whereas every vanilla-infra function found
   this pass either had no interesting strings or embedded REAL, verifiable vanilla names/paths.
3. Consider diffing which VANILLA functions are missing/replaced entirely in patched `code` (not just
   which patched functions are unmatched) — that's a different, possibly more targeted signal for
   "what got removed or fully rewritten" vs. "what differs due to recompilation."

**Angle 1 tried (2026-09-17, next session) — walked the callees of `0x8006acd0`. Dead-ended into
vanilla plumbing, as expected in hindsight.** All 11 callees decompiled via `BatchDecompile.py`
(edit `TARGETS`/`OUT_DIR`, rerun — no restart needed, confirms the script-based workflow still works
across sessions as long as Ghidra/PyGhidra is left running). Result: **0 of 11 are new content.**
- `0x800c0808`, `0x800c04d8` — vector normalize/scale math utilities (vanilla, feeds the collision system).
- `0x8002eee4` — 50-slot nearby-actor/target search loop (squared-distance-vs-radius check). Vanilla.
- `0x80092f88`, `0x80092dac`, `0x80092e70`, `0x80092df0`, `0x800fd9ac` — sound-channel pitch/volume/pan
  setup calls. Vanilla audio playback plumbing.
- `0x8007c680` — charge/timer state machine embedding the literal symbol `"Magic_Wind"`, a real vanilla
  asset/debug name — confirms vanilla provenance.
- `0x800c0874`, `0x8007c820` — trivial one-line setters/no-ops, register-allocation artifacts.

**Interpretation:** this is a coherent, expected result, not a dead end for the overall theory — new
orchestration/dispatcher code calling into UNCHANGED vanilla services (audio, vector math, actor
search) is exactly what you'd expect one hop out from a new system's entry point. **Don't repeat
"walk callees of the confirmed cluster" as a strategy — one hop out is already vanilla, so a second
hop is unlikely to do better.** The two untried angles (search unmatched functions for non-vanilla
embedded strings; diff for vanilla functions REMOVED/replaced in patched `code`) are more promising
and still open for a future session. `~/ghidra_scripts/BatchDecompile.py`'s `TARGETS`/`OUT_DIR` are
currently still set to this callee batch (`scratchpad/batch_callees/`) — edit before reusing.

**Angle 2 tried (2026-09-17, next session) — embedded-string diff. SUCCEEDED: found a real, concrete,
byte-verified Kaze-authored change.** Extracted all printable strings (`strings -n 5`) from both
decompressed `code` blobs, diffed for text present in patched but absent in vanilla (2,007 candidates),
then filtered out actor-name-table noise, known vanilla `.c` file paths, and printf-format strings —
496 lines survived. **Critical realization made along the way, worth not re-deriving:** the ~2,000
"patched-only" strings are overwhelmingly a DEBUG-BUILD-vs-RELEASE-BUILD artifact, not new content.
Vanilla NTSC 1.0's `code` has **zero** actor-name-shaped strings and no `z_*.c` debug file paths at
all (confirmed: `grep -c "En_Zl1|En_Wallmas|Boss_Dodongo|z_actor.c" vanilla_code.bin` = 0 for all);
patched has 414 real vanilla actor names (`En_Zl1`, `Boss_Ganon`, `Bg_Spot17_Funen`, etc. — checked
every single one, all are genuine documented vanilla OoT actors, none new) plus ~50 real `../z_*.c`
decomp source paths and dozens of real libultra/F3D macro-name strings (`G_TF_AVERAGE`, `CVG_DST_WRAP`,
etc.) and real decomp assert-message text (`CollisionCheck_setAC():`, `EffectSparkInfo_proc():`, the
famous `CONGRATURATIONS!` credits typo). **Conclusion: Missing Link's build keeps debug
symbols/asserts/actor-name tables that a retail-mode vanilla ROM strips — this is a major, previously
uncaptured confound behind the whole "83% of `code` unmatched" figure, likely bigger than ordinary
recompiler/register-allocation noise.** Don't re-flag any bare vanilla actor name or `z_*.c`/libultra
string as "new content" — it isn't; it's the debug build exposing normally-stripped vanilla data.

**After filtering that whole confound out, exactly ONE non-vanilla marker survived: the literal
string `"TMLDBG"`** (`grep -iE "kaze|tml|missing"` across all 2,007 patched-only strings found nothing
else). Traced and byte-verified as a genuine authored change:
- Patched `code` (decompressed) offset `0x10d754` = `"TMLDBG"`. Vanilla's `code` has the **exact same
  36-byte record slot**, at offset `0xeaf34`, containing `"ZELDAZ"` instead. Byte-diffed the two
  records directly (script inline, not saved as a file — trivial to rerun: `open both .bin files,
  slice 36 bytes at each offset, compare`): **only bytes [0:16] differ** — the 8-byte name field
  (`"ZELDAZ\0\0"` → `"TMLDBG\0\0"`) and an adjacent 8-byte flag/color-tag field (vanilla `81 87 61 df
  df df df df` → patched `15 12 17 14 3e 3e 3e 3e`). **Bytes [16:36] — the actual numeric parameters
  (0xe0, 0xe0, 0x30, 0x96, 0x08, 0x00, 0x01, 0x00, all as 4-byte fields) — are byte-for-byte
  IDENTICAL between vanilla and patched.**
- This table has exactly 2 records in both ROMs: a first record named `"ZELDA"` (unchanged in both)
  and this second record (`"ZELDAZ"` in vanilla, renamed to `"TMLDBG"` in patched). Strongly believed
  to be OoT's real, documented **VI (video interface) mode config table from `z_vimode.c`** — ties
  directly to the earlier-catalogued `0x800d88c0` finding (already in this file, under "vanilla audio"
  — **note for later: that categorization was probably too narrow; `0x800d88c0` references the literal
  string `"../z_vimode.c"` as if indexing into adjacent float data, meaning it's reading THIS SAME
  table/blob, not narrowly "audio pitch math" as first described**).
- **Interpretation:** Kaze renamed vanilla's second VI-mode/debug-display slot to `"TMLDBG"` (The
  Missing Link DeBuG) and changed its highlight/flag bytes, while deliberately leaving the actual
  width/height/geometry parameters untouched — consistent with repurposing an existing debug-display
  mode slot for his own build's on-screen debug output, not writing a new subsystem from scratch. This
  is a genuine, deliberate, human-authored change — the strongest fully-byte-verified "Kaze was here"
  evidence found in this whole investigation — but it's a **dev-tool/debug-display customization, not
  new gameplay content**. Don't overstate it as evidence of new gameplay logic.
- **Not yet done, still open:** identify what CODE reads this table (tells you when/how the debug
  display gets triggered). Tried the same `lui`+`addiu`/`ori` byte-pattern search that worked for the
  earlier base-address-solving — searched for a direct reference to both the record's own address and
  the table's base (first-record) address, in both vanilla and patched, using `~/ghidra_scripts/
  BatchDecompile.py` (temporarily repurposed as `getReferencesTo()` xref lookup — see next paragraph)
  and a manual Python word-scan. **Both came up with zero hits** — Ghidra's own auto-analysis found no
  xrefs either, meaning the access is NOT a simple 2-instruction `lui`/`addiu` pair (likely GP-relative
  small-data addressing, a computed/indexed pointer, or accessed from a segment other than `code`).
  Not pursued further given diminishing returns — the byte-diff finding stands on its own without it.

**Reusable technique note:** `~/ghidra_scripts/BatchDecompile.py` was repurposed mid-session from
"decompile a list of addresses" into "find xrefs to one data address and decompile the referencing
function(s)" by just overwriting its contents (same filename) — confirmed this avoids the
"new-script-needs-a-restart" Ghidra quirk since the Script Manager already had this filename indexed.
**Prefer editing this one file's contents over creating new `.py` scripts** for any similar one-off
Ghidra query — only make a new file if you need to keep both versions available simultaneously.
Its `TARGETS`/behavior currently reflect the xref-lookup version, not the original batch-decompile
version — rewrite before reusing for straightforward batch decompiling again.

**Angle 3 tried (2026-09-17, next session) — removed/heavily-rewritten VANILLA functions. Succeeded:
found strong evidence Kaze REWROTE vanilla's own cutscene/event dispatcher rather than building a
parallel new one, refining the earlier "confirmed new subsystem" conclusion.**

**How to reopen the vanilla program if picking this up again:** the vanilla program
(`oot_ntsc10_vanilla.z64`) is a separate Ghidra "Tool" instance from the patched one, and does NOT
open by double-clicking its row in the Project window (AX-based double-click on that JTable silently
fails — clicks land on the wrong element per the accessibility tree, a real quirk, not user error).
**Working method:** bring any CodeBrowser tool forward, File → Open... → pick `oot_ntsc10_vanilla.z64`
from the project tree in the "Open Program" dialog → OK. This opens it in that SAME tool instance
(confirmed via the title bar changing to `CodeBrowser(2): A Hub:/oot_ntsc10_vanilla.z64` and Window →
Script Manager opening a NEW Script Manager scoped to that program specifically — the original
Script Manager window stays scoped to whichever program was active when IT was first opened, so you
must open a fresh Script Manager from the now-vanilla-focused tool's own Window menu, not reuse the
old one). Also hit the recurring "Zen is not in the allowed applications and is currently in front"
focus-steal issue again mid-session — same fix as before, `open_application` on `org.python.python`.

**Method:** dumped every function in vanilla's `code` block the same way as `dest_functions.json`
(script now named/reused as `BatchDecompile.py` again, temporarily repointed at
`vanilla_functions.json`; **2,588 total vanilla functions in `code`**), then diffed against the
Source Address column of `vt_matches.csv` (Type=='Function' rows) to find vanilla functions with NO
correlated match on the patched side at all: **2,001 of 2,588 (77%) unmatched** — consistent in scale
with the patched-side 83% figure, saved to `scratchpad/vanilla_unmatched.json`.

**Key observation before reading anything:** the SIZE DISTRIBUTION is asymmetric in a meaningful way.
Vanilla's largest unmatched functions run up to **14,216 raw bytes** (`0x80067e48`), with several
more above 9,000 bytes — far bigger than ANY unmatched function found on the patched side (patched's
largest unmatched, `0x8006acd0`, was only 5,220 bytes). Big monolithic vanilla functions are the most
fragile to correlation (any edit shifts every subsequent branch offset throughout the whole function),
so size alone doesn't prove removal — but it was worth reading the biggest ones to check.

**Decompiled and read the top 20 vanilla-unmatched-by-size functions**
(`scratchpad/vanilla_unmatched_batch/decompiled_*.c` — all 20 succeeded). None contained embedded
strings (unlike the patched-side top-30, which had plenty) — these are pure-logic/math functions, not
error/assert paths. Clustered them by shared struct-offset/callee patterns (cheap grep, not full reads
of all 20):
- **The single most important find: `0x80067e48` (14,216 bytes) is VANILLA'S OWN giant numeric-
  command-ID cutscene/event dispatcher** — structurally identical in shape to the CONFIRMED-new
  dispatcher already catalogued on the patched side (`0x8006acd0`, part of the "4-slot subsystem"
  cluster): a huge if/else chain keyed on a numeric ID (`iVar4`/`param_3`), calling text-trigger and
  actor-spawn-at-position helpers. Extracted all 18 of its case IDs: they range from **`0x3f2` (1010
  decimal) to `0x25e9` (9705 decimal)** — this range is an almost exact match for the ID range already
  documented for patched's dispatcher ("~1000 to ~9,908"). **This is strong evidence that patched's
  dispatcher isn't a brand-new parallel system layered on top of vanilla's engine — it's Kaze's
  REWRITE of THIS EXACT vanilla dispatcher, extended with new command IDs sharing the same numeric ID
  space.** This refines (doesn't contradict) the earlier "confirmed new subsystem" finding: treat the
  cutscene/event dispatcher as a modified-and-extended vanilla system for porting purposes, not a
  from-scratch new one — **relevant for the actual porting work down the line: the port should target
  "add new command IDs to OoT's existing cutscene dispatcher," not "build a new interpreter."**
- **A second cluster — CORRECTED after reading `0x800dda6c` and `0x800756f0` in full (2026-09-17,
  later session): this is NOT a generic HUD/`Interface_Draw` family, it's OoT's entire FILE SELECT
  SCREEN system.** Shares struct offset `0x1c44` and/or callees `FUN_8006fdcc`/`FUN_8006fb50`/
  `FUN_8006e340`: `0x800dda6c` (12,104B), `0x800756f0` (11,116B), `0x80052f34` (7,448B), `0x8006fdcc`
  (5,660B), `0x8006e340` (3,824B), `0x8003ebc4` (3,580B, bridges into a third cluster too). Zero match
  on patched side for any of them.
  - **`0x800dda6c` is the Name-Entry keyboard screen's state machine** — ~46 sequential states
    (case values 1 through 0x37) covering: New/Copy/Erase file-menu flow, reading typed characters
    from a controller-driven on-screen keyboard into a per-slot char buffer (`DAT_80112e8b`/
    `DAT_80112e8c`, an 0xff-terminated array — a null-terminated string, essentially), and confirm/
    cancel sound-effect dispatch (`FUN_800c806c` with codes like `0x4807`/`0x4827`/`0x4836`/`0x4842`).
    The back half of the function draws each typed character glyph one at a time via cursor-position
    tracking with per-character fade/blink color animation (`DAT_80112e98` timer array) — this is OoT's
    real on-screen keyboard used to type Link's/the save file's name.
  - **`0x800756f0` is the File Select main-screen draw function** — builds the display list for the
    rotating 3D model shown behind the save slots (matrix/rotation setup via `FUN_800aa7f4`/
    `FUN_800aa8fc`/`FUN_800aa9e0`/`FUN_800ab900`, rotation angle from `param_1[0x41b9] / DAT_80108800`
    — this is vanilla's rotating Deku Tree/Kokiri Forest model), draws quest-status icons, and contains
    a title-logo fade-in/fade-out timer (`_DAT_8011b99e`/`_DAT_8011b9a2` state machines counting down
    fixed frame intervals like `0x14`/`0x28`, driving alpha values `_DAT_8011b9a6`/`_DAT_8011b9a8` from
    ~0x8c/0x50 toward zero) with synchronized countdown tick/ding sound effects (`0x481a`/`0x4819`/
    `0x4804`) and a 3-digit counter display (splitting a value into hundreds/tens/ones via repeated
    subtraction, `_DAT_8011bf2e`/`_DAT_8011bf2a`/`_DAT_8011bf28`) — this is the "N64"/copyright/title
    logo fade-in sequence and its accompanying timer shown before the save-file list appears.
  - **Conclusion: this is a CONFIRMED, sensible, and expected removed/replaced vanilla subsystem** —
    unlike the dispatcher case (which was a rewrite-in-place using the same ID space), a full
    total-conversion romhack with its own title and branding ("The Missing Link" vs. "The Legend of
    Zelda") essentially HAS to replace the file-select title logo, and plausibly the rotating model and
    countdown-timer cosmetics too. This is much more confidently "genuinely different/replaced content"
    than the borderline "could just be compiler fragility" functions found earlier, precisely because
    the real-world reason for the difference (different game title/branding) is obvious and necessary,
    not merely inferred from code shape. **For the porting project: treat the File Select screen as a
    known, deliberately-customized area, not something to port byte-for-byte from vanilla — expect
    Missing Link's own title logo asset and possibly a different rotating model/scene.**
**The two smaller clusters — both read in full (2026-09-17, later same session) and identified
confidently, though with a caveat on how meaningful the difference actually is:**

- **Camera system cluster (`z_camera.c`): `0x8003d400`, `0x80042f54`, `0x8003ebc4`** (share callees
  `FUN_80035724`/`FUN_8003569c`, angle/float interpolation helpers used throughout OoT's real camera
  code). All three read in full:
  - `0x80042f54` processes a **cutscene keyframe camera array** — walks a list of records read via
    `param_1[1] + index*0x28` (40 bytes/record, matching OoT's real `CsCameraPoint` struct size),
    each with eye/at/fov/roll fields and continuation-flag bytes, driving multiple interpolation-curve
    modes (a switch on `*pbVar4 & 0x1f`, case values 1-0x18 = distinct easing curves). This is
    vanilla's cutscene-camera keyframe interpolator.
  - `0x8003ebc4` is a **per-camera-setting-ID initializer** — a giant switch on a small sequential ID
    (1 to 0x5b, i.e. ~90 values) where EVERY case hardcodes its own literal angle/distance/FOV
    constants (10.0, -20.0, 80.0, 25.0, etc.) — this is OoT's real numbered `CAM_SET_*` camera-setting
    table (vanilla decomp has on the order of 80-90 such settings, matching the ID range found here).
  - `0x8003d400` is a regular **gameplay camera-mode function** (not cutscene-specific) — eye/at
    interpolation toward the player actor, FOV blending, roll handling.
  - **Caveat, unlike the File Select finding: no obvious "this HAS to differ" narrative here.** These
    are dense, float-constant-table-heavy functions — exactly the shape most fragile to ANY unrelated
    change in the surrounding data segment (a single float constant added/removed anywhere shifts every
    subsequent `DAT_..` reference and byte-diffs the whole function even with zero functional change).
    Plausible interpretation: since the cutscene *dispatcher* itself was confirmed rewritten-in-place
    (same ID space, Angle 3's headline finding), it's reasonable that cutscene-camera behavior needed
    matching adjustments too — but this is inference, not verified the way File Select was. **Treat as
    "vanilla camera code, unmatched, plausibly touched but not confirmed changed" — lower confidence
    than the other two angle-3 findings.**

- **Text/dialogue box cluster (`z_message.c`/`z_message_PAL.c`): `0x800d7b00`, `0x800d8ef4`**
  (share callees `FUN_800d6470`/`FUN_800d6ba8`/`FUN_800d75a4`/`FUN_800d78f0`). Both read in full:
  - **`0x800d7b00` is OoT's real dialogue text-box control-code decoder** — walks a message-text byte
    stream (`iVar8 + offset + 0xe306`) interpreting values as either printable-character width advances
    or control codes. The control codes found (`0x8140`-`0x81cb` range: box-break, color-change, icon
    display, A-button/B-button prompt icons, ocarina trigger, sound-effect trigger, fade-out, end-of-
    message, plus `0x86b3`/`0x86c7`-`0x86c9` likely JP-text-mode codes) match REAL, extensively
    documented vanilla OoT text control codes almost exhaustively — no unrecognized/novel codes found.
  - **`0x800d8ef4` is the SAME decoder's alternate-encoding sibling**, not a separate subsystem —
    reads single BYTE codes instead of shorts, dispatched via a language/region flag (`DAT_8010a938`),
    exactly mirroring the `if (DAT_8011b9d9==0) FUN_800d7b00 else FUN_800d8ef4` branch pattern already
    seen calling into this pair from the File Select cluster.
  - **Interpretation: this one leans MORE toward "recompiler noise, not deliberate change" than the
    camera cluster.** Missing Link definitely has new dialogue TEXT (any romhack does), but that text
    lives in a separate data table, not in this decoder function — the decoder's CODE only needs to
    change if new/different control codes were added, and none were found here. The byte-mismatch is
    most plausibly explained by the same float/data-table-shift fragility as the camera cluster, not by
    an actual logic change. **Lower-confidence, likely-noise finding — don't treat this as evidence of
    a modified text engine.**

**Overall conclusion for Angle 3, now complete across all 4 clusters:** the angle succeeded at its
strongest with the cutscene dispatcher (confirmed rewrite-in-place, high confidence) and File Select
screen (confirmed replacement, high confidence, obvious real-world justification). The camera and
text-decoder clusters are correctly IDENTIFIED (high confidence on what the code does) but their
removed/unmatched status is most likely explained by ordinary recompiler fragility on dense,
constant-heavy vanilla functions rather than deliberate content changes (lower confidence on WHY they
differ). **Net takeaway for the porting project: don't assume every large unmatched vanilla function
represents new work to port — weigh "is there an obvious external reason this had to change" (branding,
new content needing new codes) against "is this just a huge float/switch-heavy function that any
recompile would byte-diff regardless."**
