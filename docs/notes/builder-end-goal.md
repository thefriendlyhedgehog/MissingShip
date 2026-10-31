---
name: builder-end-goal
description: Owner end goal 2026-10-11 — players get our builder, bring ML ROM (or OoT ROM + patch), it writes ML for stock Unbound with skijer's code mods (Shipwright#11); then we only maintain the builder
metadata:
  type: project
---

Owner, 2026-10-11, the end-to-end flow:
1. A player sees they can play ML with Ship's features on PC.
2. They get the builder from the owner's MissingShip thread.
3. They download Kaze's patch and bring an OoT ROM, or bring an already patched ML ROM.
4. The builder turns that N64 file into something that runs in stock Unbound with code-mod support from
   https://github.com/skijer/Shipwright/pull/11.

Once that support is upstream, the builder is the only thing we maintain.

The owner also said: "Update the pack builder to 9.3 as we need to stay in parity." Done as follows:
- `tools/extractor_parity.py` compares our ZAPD's output with Torch's over the PAL MQ Debug ROM. 9.2.3 and 9.3.0
  agree except the boss title-card texture type; ML's card is now IA8.
- Run it on each SoH release.
- Porting the builder to Torch is the fallback, only if the two drift in format.

**Why:** the owner wants one maintained artefact (the builder). Players never receive Nintendo data or a forked game.

**How to apply:**
- Steer ML's C behaviour toward a #11 code mod: one `.o2r` holding a manifest, per-OS binaries and assets.
- Steer our Unbound data features toward roborich upstream.
- Delta packs only matter for handing out ML without the builder.
- #11 is on Unbound 0.9 (SoH 9.2.3).
- Gaps in #11: no room-load hook; on Windows only `HOST_DATA` globals are reachable.
- See [[ml-on-unbound]], [[held-upstream-offers]], [[unbound-930-wait]].

**Progress 2026-10-11:** `ML_TreeBridge` runs as a code mod on #11's base. The work is in the worktree
`/Volumes/MacMiniExternal/AI_Projects/Unbound-modapi`, branch `ml-codemod` (pushed to our Unbound fork), under
`mod-sdk/mods/missinglink`.
- The game build is in `build-mac`. `soh.o2r` needs `--target GenerateSohOtr`.
- Build the mod with `python3 mod-sdk/build_mods.py --mod missinglink --no-templates`.
- Test by booting a warp point (`gSettings.BootSequence` 4 plus a `WarpPoints` entry); this needs no input.
- Stock refuses vanilla-scene entries in `scenes.json`, so ML's Kokiri Forest fails to load (custom entrance in
  its exits). Fix this in the builder or upstream.
- **Stock scene data (2026-10-11):** `build_mod.py --unbound <bin> --stock` (not with `--pack`) uses
  `tools/unbound_stock.py`.
  - The layer ships no `scenes.json`. Exits to ML's added entrances are rewritten to vanilla ones.
  - 8 exits land at the scene entrance, not ML's spawn. Exact landing needs U1 upstream, or the code mod.
- **Stock actor data (2026-10-11):** `--stock` writes no `unbound/actors/*.json` (stock refuses all 10). The code mod
  carries them: `ml_overrides.c` (ActorDB flag patches, Bg_Treemouth via `VB_SPAWN_ACTOR_ENTRY`), `ml_grotto.c`,
  `ml_crack_wall.c`, plus `ml_scenes.c` (ML's 25 draw configs at `OnSceneInit`).
  - Lesson: "Scene Init" in the log is not "loads". Without the draw configs, 5 scenes crashed on their first
    frame. Run each test past the first frames and check for `[critical]` lines and an early exit.
  - With the mod, all 25 scenes run 15 s clean; the only errors are unported ML actors (ML_Fishpond,
    ML_EventSpawner, ...).
  - The mod's tables mirror the builder's (`ML_OVERRIDES`, the fork's `sGrottoEntrances`, `ml_scene_map.json`
    configs); change both together.
- **Kokiri Forest complete (2026-10-11):** `ml_en_md.c` and `ml_songblock.c` ported; the scene loads with no errors.
  **All 30 room-placed ML actors ported (2026-10-12, ml-codemod 026815fd9).** **All ML edits inside vanilla actors
  ported (2026-10-12, ml-codemod 4c587072c)**: ml_vanilla_npcs/enemies/objects.c, ml_en_mag.c, ml_en_rd.c (whole
  9.2.3 copy), plus the earlier Blade Beam, Business Scrubs, En_Elf, push-block colours. Left for stock: player code
  (Magic Hourglass, triple jump, sword model, child hands, no talk-to-Saria), non-actor code patches (text speed,
  ocarina note, HUD colours, place-name card), Controllable Bombchus release on the rail, unread registries.
  Tricks that worked: wrap entry init/update saving the previous pointer; swap actionFunc before vanilla Update;
  revert an action vanilla sets (WonderTalk) after Update; hold an event flag raw around vanilla Init (Ossan, no
  hooks fire); refuse a cutscene via RegisterVB(VB_PLAY_ONEPOINT_CS) only while that actor updates (file-local
  statics can't be copied); grow instanceSize for extra fields. Off-screen actors don't update (no
  UPDATE_CULLING_DISABLED), so probes must pick ones in view. Unbound's Cylinder16 dims are f32. Probe chaining
  scenes in one run: set play->nextEntranceIndex + transitionTrigger at a frame (MLSEQ env). (The red wedge seen in Bmori1 room 0 was the test camera inside
  ML's vanilla red ice from a warp at spawn 4, which no exit uses: not a bug.) Porting recipe: copy the
  branch's `ovl_ML_*` code, make its functions static, declare the vanilla helpers it calls (they're exported), and
  register with the flags/object from `MlActors.cpp`. Use a TEMPDEBUG probe to force switches when no input is allowed.
  - Where the branch hooks inside vanilla code: wrap vanilla's functions when the change sits before/after them
    (DnsShop), else copy vanilla's 9.2.3 code with the change built in (Torch Update, PushBlock). No actor reset
    func in the API: use the OnLoadGame hook. Player keeps last frame's position in home.pos (copied to prevPos).
  - 9.3.0→9.2.3 renames hit when porting: Actor_PlaySfx_Flagged = func_8002F974,
    Actor_OfferTalkNearColChkInfoCylinder = func_8002F2F4, Actor_TrackPlayer = func_80038290,
    Actor_SetPlayerKnockbackLarge = func_8002F6D4, Room_RequestNewRoom = func_8009728C, Room_FinishRoomChange =
    func_80097534, roomCtx.activeBufPage = unk_30, play->bgCoverAlpha = unk_11E18, CAM_ID_MAIN = MAIN_CAM,
    sceneLayer = sceneSetupIndex. The SDK has no CVAR_* prefixes (use "gEnhancements.X") and no game-print log
    level (drop osSyncPrintf).
  - Test harness: never `rm -rf` a test home or pass `env "$@"` (the safety check blocks both); cm3.sh refuses an
    existing home, so give each run a new name.
  - The scratchpad can be wiped between days: the test kit (guard.sh, winids.py, ext.sh, cm3.sh, the usa10-923 base
    from build-ub, the --stock layer) is rebuilt in a few minutes from this session's transcript.

- **Builder delivers the code mod (2026-10-13, MissingShip 08d8532):** `python3 tools/codemod.py` (needs the
  `ml-codemod` checkout clean in mod-sdk/mods/missinglink, pushed, and its build-missinglink CI run green) writes
  `dist/codemod/missinglink.o2r` (all 3 OSes) + `codemod.txt`. `freeze_builder.py` bundles it into
  `_internal/codemod/`; `build_mod.py --stock` writes `mods/missinglink.o2r` beside the layer (`--code-mod` /
  `MS_CODEMOD` override). Rerun codemod.py, then re-freeze, whenever the code mod changes. Freeze with the
  scratchpad venv's Python (`$scratch/pyi-venv`, PyInstaller 6.22.3, owner OK'd the install 2026-10-13); no system
  Python has PyInstaller. Frozen `--stock` verified (d1ff0ad): same layer entries and code mod as the checkout.

**9.3.0 only, owner 2026-10-14:** "update builder to only use 9.3 since people cant play it with the 9.2.3 build
anyway". The builder packs the code mod from `ml-codemod-930` (mspaths.CODEMOD_SRC = ../Unbound-skijer930;
tools/codemod.py refuses a checkout without CODEMOD_HOST eed5004cc) and refuses `--stock` for an Unbound whose soh.o2r
portVersion is below 9.3.0. The 9.2.3 `ml-codemod` branch is retired. Windows/Linux frozen builders must be re-frozen
on those OSes to carry it.
**Release unbound-930-builder-1 (2026-10-14, pre-release, private MissingShip repo):** macOS/Windows/Linux builders +
code mod. Windows/Linux builders come from the engine fork's generate-builds (builder-inputs job: our Unbound fork at
UNBOUND_COMPARE + code mod from the release named by CODEMOD_RELEASE; owner added our Unbound fork to
SUBMODULES_TOKEN) and `tools/package.py unbound-ci <run>`. A new code mod means a new release, then bump
CODEMOD_RELEASE before re-running CI. Linux builder + code mod passed segments 01-04 on bc250 (2026-10-14, same as macOS; layer and oot.o2r entry-identical to the Mac's); Windows not yet run. **Release unbound-930-builder-2 (2026-10-16, owner OK'd):** code mod dd9a7e4f3 (Windows function-address fix), CODEMOD_RELEASE bumped, builders from engine run 38990802002 + local macOS freeze (scratch pyi-venv); all three builders' layers entry-identical, Windows played 01-14. Order: release with missinglink.o2r+codemod.txt first, bump CODEMOD_RELEASE, run CI, package, test, upload, then notes. Linux recipe: AppImage --appimage-extract, run squashfs-root/usr/bin/soh.elf in its folder (portable home = cwd), mlplay builds on the box with mod-sdk/build_mods.py from an rsynced source tree.

**Release unbound-930-builder-3 (2026-10-17, owner OK'd):** adds the builder window (pipeline/builder_gui.py, Tk, offline; no args or --gui opens it; MS_GUI_AUTOTEST=<json> test hook) and launchers (.app / .cmd / .sh from tools/package.py). Same code mod as builder-2 (CODEMOD_RELEASE stays builder-2). Builders from engine run 39035094801 + local macOS freeze with dist/venv-builder (PyInstaller 6.22.3). Tested through the launchers on all three; layers identical.

**Code mod release moved, 2026-10-18:** the public MissingShip releases no longer carry missinglink.o2r. CI's builders now download it from the private Unbound fork's pre-release `codemod-1` (50dabfcf2, CI run 39175561462; tagged at that commit). CODEMOD_RELEASE=codemod-1 is set in engine commit 3c95e7533. The SUBMODULES_TOKEN reads it (builder-inputs passed in run 39176943139, which was then cancelled). For a new code mod: run tools/codemod.py, publish `codemod-N` on the fork with missinglink.o2r and codemod.txt, then bump CODEMOD_RELEASE.

**Dropped 2026-10-26 (owner):** builder-4 won't ship and the builder isn't updated any more: players get ML from the one-file porter ([[porter-one-file]], `missinglink.o2r` in NEI-Store). builder-3 stays up for testers as it is. Don't spend Actions minutes or repack work on builders.
