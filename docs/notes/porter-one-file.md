---
name: porter-one-file
description: Owner-approved 2026-10-20 - ML ships as one missinglink.o2r whose porter makes the layer in the game; USA 1.0 oot.o2r only; how to make and test the pack
metadata:
  node_type: memory
  type: project
  modified: 2026-10-23T21:39:54.285Z
---

skijer's idea (his demo came 2026-10-21); owner request, approved 2026-10-20: players install only `missinglink.o2r`. Its porter (`ml_porter.c`) asks for the ROM and patch, writes `MissingLink_Unbound.o2r` with SaveToModsFolder and restarts. On an update it clears the old layer first. Committed in ml-codemod-930 4c1127209/72159b6fe and MissingShip ac2b457.

- **Pack:** `tools/porterpack.py --unbound <PR #11 soh.app> --oot-o2r <its USA 1.0 oot.o2r> --also-base <9.2.3's USA 1.0 oot.o2r>`. PR #11's 1.0 `oot.o2r` is identical to `mlplay-work/bases/usa10-930.o2r`.
- **USA 1.0 only:** a variant for Rev A or Rev B would carry OoT bytes (Forest Temple, Kakariko rooms), and nodata fails it. The porter reads `oot.o2r`'s `version` file (01 + header CRC; 1.0 is EC7011B7) and tells the player what to do. The owner's real 9.1.1 `oot.o2r` is Rev B, so they will see that message.
- **Testing in the players' build:** `mlplay-work/porter-pr11` (`run.sh`, `t1` porter, `t2` newfile). `mlplay-work/porter-test/approve.py <mods>` pre-writes `mods/.unbound-mod-approvals.json` (BLAKE2b-512 content hash), so unsigned test mods load with no dialog. mlplay truncates long words, so keep test paths short (not scratch). The PR #11 app lives in `mlplay-work/pr11-mac/soh.app` (scratch was wiped 2026-10-21). `run.sh` takes extra CVars as `CVARS='{...}'` JSON.
- **nodata:** looks inside `.pack` files; the porter pack may hold up to 16 KB of ML's bytes (it has 3.7 KB).

**skijer's demo of the idea (2026-10-21, TML-Unbound.rar; the in-game build is his idea, so credit him):** a separate `missinglink_builder` mod built on our deltapack, with a stored recipe (4.9 MB, one base), .bps only, and the player deletes the old layer by hand. His copy of the code mod fixed `VB_PLAY_ENTRANCE_CS`'s missing third argument (the cutscene): with Skip Entrance Cutscenes on, the game crashed at the end of the prologue. Adopted in 5e0190e2a / MissingShip 60011a6. builder-3 and the signed codemod-2 still have the crash.

**Pre-release checks (2026-10-21, owner: verify everything before a sign request):** public MissingShip `.github/workflows/codemods.yml` builds `codemods/` + `testmods/` for all three OSes like sign-mod, unsigned, on free public minutes. Its artifact `universal-unsigned` holds the universal `.o2r`. `testmods/portercheck` (`PORTERCHECK="<layer>|<files>"`) runs the porter without dialogs. Windows: `mlplay-work/porter-win/run.sh` (D:\mlporter, task `mlporter`; delete both when done). Old Windows mlplay: `win/dl/mlplay-win`. Hands-on dialog test: `mlplay-work/porter-manual` (`start.sh`, `update.sh`, kit/missinglink-update.o2r). The Mac and Windows layers are identical, and the skip-on/off intros pass on both. Linux boxes were off on 2026-10-21.

**Updates (2026-10-21):** the owner's Mac run showed the update clears the old layer, but then needs a second start and a second file pick. The old layer hides the vanilla sources. ml-codemod-930 955467359 / MissingShip c812181: clearer two-step dialogs ("Finish updating", same files), and every question/answer logged. skijer/Shipwright#41 (Shipwright-VR `modapi-offer/read-base-archives`, worktree `../Unbound-pr-base`) adds `ReadGameArchiveFiles`. The one-step porter using it (ReadGame for the cache and OotIs10; `Make(true)` over LAYER_OLD, saved as `.pending`, one restart) lives only in `mlplay-work/sign-pr/mods/missinglink` (signing CMake against the PR tree). It can't go in the fork until #41 merges and `UNBOUND_REF` is bumped. Test: `mlplay-work/porter-onestep/run.sh <game> <name>`. The old layer is the real one with a changed mark. The PR build `mlplay-work/pr-mac/soh.app` made an identical layer; on PR #11 it fails as expected.

**Automatic restart (owner asked 2026-10-21: no "Close now / Later"):** a second skijer offer, on Shipwright-VR branch `modapi-offer/restart-game` (821bf78e3, opened as skijer/Shipwright#42).
- What it does: main() relaunches after shutdown, execv on macOS/Linux; on Windows CreateProcess plus `SOH_RESTART_AFTER` wait. `RequestRestart` becomes "Restart now / Later", and the new `RestartGame` has no dialog.
- The local test branch `test/onestep-restart` merges both offers.
- Shipwright-VR `unbound` (5f86b600d, a fast-forward merge commit) builds all three OSes, run 39616627750.
- MissingShip branch `test/restart-game` builds the test mods against it (dispatch codemods.yml with the inputs unbound_repository/unbound_ref/soh_lib_run; `testmods/missinglink` overrides codemods'). Never merge it.
- The sign-pr porter uses RestartGame when present, with "then Unbound restarts into it".
- Owner hands-on, Mac, 10:29: update → files → restarted by itself in 0.6 s → layer ready. Homes: `porter-manual-1` (reset.sh, update.sh, start.sh); unattended: `mlplay-work/porter-restart.sh`.

Restart tests passed on all three, 2026-10-21 12:51-12:57 (Linux real AppImage, same pid; Windows new pid after the old exits). Runners: `porter-lin/restart.sh`, `porter-win/restart.sh <mods>` (builds in mlplay-work/pr-builds). Windows caveat: files scp'd in are owned by Administrators, so the game can't delete them and ApplyPending says "Access is denied". The runner grants mini full control (icacls). Opened as skijer/Shipwright#42 (owner OK, 2026-10-21).

**One-step update without Unbound changes (2026-10-21, after skijer asked for a way that leaves his exporter alone):** ml-codemod-930 30685ad6f / MissingShip a7d2886.
- The layer keeps the game's copy of each file it covers (`missinglink/game/<path>`: 1,026 files, 8.1 MB raw; the layer is about 24 MB) and `missinglink/layer-files.txt`.
- ReadGame reads those copies first. An update over a listed layer runs Make(true) → `.pending` → RequestRestart ("Close now").
- Paths are listed once per build with ListArchiveFiles, so lookups skip missing paths: about 4.6 s to install, about 3 s to update.
- Layers without the list (the builder's, or an older porter's) still update in two steps (Clear).
- Verified on PR #11 with `porter-onestep/run2.sh` (MOD=, OLD=) and `checkkept.py`. The hands-on kit is `porter-manual` (reset.sh, update.sh, start.sh).
- #41 isn't needed any more. The release no longer waits for skijer: the next steps are the owner's hands-on run, the three-OS checks, then the sign request.

**Ready to sign (2026-10-21 14:25):** UNBOUND_REF/CODEMOD_HOST is now skijer's 948bff278 (ListArchiveFiles came after eed5004cc), and ml-codemod-930 merged it (9c8b1e842). MissingShip 4424974's public codemods run 39650381324 is all green. Its universal mod gives byte-identical layers on Mac, Windows and Linux in PR #11, for both the first install (kept1) and the update (kept2). The owner's hands-on update in PR #11 passed ("working as intended"). Sign request #40 (da03954) was closed 2026-10-21 (owner OK). The owner files a new request for 4424974; check skijer/Shipwright issues for it. The w1/k1 homes were removed.

**Signed and verified (2026-10-23):** see [[mod-signing]]. The signed missinglink passes nodata and makes the identical layer in the store build, where ML's intros and a warp pass (mlplay t3). **Released 2026-10-23 as `unbound-930-porter-1`** (prerelease, owner OK; target 0de4e03, whose README gives the one-file install via NEI-Store "Packs → Other mods", untick the rest, or Releases). Assets: the signed missinglink.o2r and childshield.o2r from the store. Builder-3 stays for testers under README More.
- Owner, 2026-10-23: PR #11's links are fine ("still good until next year"), even though the GitHub API listed the artifacts as expiring 2026-10-23 22:23 UTC.
- The store shows the manifest's long technical description; a shorter one can come with the next /sign.

**Player report, 2026-10-23:** "couldn't be made: text/eng/messages.json: a file it copies from isn't the one it was made from". Log requested.
- Ruled out:
  - byte order: all 8 combinations pass in the store build (`mlplay-work/endian/make.py` makes .v64/.n64 copies and a little-endian bps);
  - store mods: none covers a game file the pack copies from;
  - Unbound's converter: unchanged from eed5004cc to the store build;
  - the store build itself: identical layer on Mac and Windows.
- Suspects: oot-mq.o2r next to oot.o2r, an oot.o2r from another SoH extraction, a non-standard 1.0 dump, or a mod covering text.
- Linux store test skipped: cachy was down.

**Left:** the owner's manual dialog run; Windows and Linux; a new sign-mod request (the owner submits it); README install text and a release (owner's OK). Related: [[builder-end-goal]], [[mod-signing]], [[kaze-permission]].

**Master Quest (2026-10-23; released as unbound-930-porter-2, missinglink 0.2 signed from a24eac5):** with oot-mq.o2r beside oot.o2r, Unbound converts the
text from the MQ ROM, so the pack's text tables get a last variant from the ML ROM's json/ view and ML is made anyway.
Owner rejected "move oot-mq.o2r out" as bad UX: make things just work. MQ alone (no USA 1.0 oot.o2r) can't work (no
extractor in the porter); it's told before the porter asks. Tests: mlplay-work/mq-home/mqfix.sh, mq-only.

**Master Quest save files (2026-10-23, MissingShip aa978cc / ml-codemod-930 957ef0686, unsigned):** a Discord player on
an MQ file dropped into the real MQ Deku Tree from Lost Woods and exited to Kokiri Forest (0x209): Unbound loads
scenes/<dir>_mq/scene.json for MQ files. The layer now writes ML's scene.json under ydan_mq, Bmori1_mq, HAKAdan_mq
(rooms use full scenes/<dir>/ paths, so no copies). mlplay `newfile mq` (Unbound-modapi ml-playtest 89c8343d3).
Verified in the store build: Mac (mq-home/play.sh mqsave.txt; stripped layer reproduces the bug) and Windows (universal
CI mod, layer byte-identical; old win/dl/mlplay-win plays the Mac MQ save). Players need the next signed build.

**Fit check (2026-10-23, MissingShip 2595fe6 / ml-codemod-930 f3b0b2875, unsigned):** a second player's prebuilt 1.0
oot.o2r failed on messages.json (Unbound takes any 9.x oot.o2r; the pack covers only 9.3.0 and 9.2.3 extractions).
Before asking, the porter now dry-runs the pack against the game's files (ML ROM sources count as there) and tells the
player which file doesn't fit and to have Unbound make oot.o2r again; the log names ROM CRCs, the portVersion SoH, MQ
mounted, and per variant the source, size and CRCs. Runner: mlplay-work/fitcheck/run.sh (a-e cases, zz_cover.o2r covers
audio/audio). The in-game dialog itself (PorterAsks 1) is untested; no older-than-9.2.3 USA 1.0 extraction on hand.

Audio crashes in a player's log came from a vanilla custom music pack (soh-z64-song-pack.otr; crashed without ML too); removing it fixed it. A guard in Unbound's mixer needs the pack to reproduce (not public).

**0.3 (2026-10-24, unbound-930-porter-3, signed from 684c28a):** ml_diag.c logs music (seq, name, font), scenes (MQ/age) and exits (entrance, pos), with or without the layer; the porter checks game files before asking. Pack unchanged from 0.2, so 0.2 layers stay ready. Bump manifest version for each /sign.

**Held for the next release (2026-10-24, owner: "hold the release until more fixes are in"):** ml-codemod-930 07c46448f / MissingShip 0f5a22e, unsigned. ml_othermods.c logs the other mods at every start (code mods from `bin/*`, `custom/` music, `alt/` texture or model packs) and tells the player once per change (CVar `gEnhancements.MissingLink.OtherModsSeen`; not when PorterAsks is 0). MM masks crashes ML. Tested on the Mac store build with childshield and a fake alt/ pack. A real music pack is untested. At release: bump the manifest version, and add the popup to the README's "Other mods" paragraph (left out until then). Owner 2026-10-24: the popup stays a warning only, with no "turn the others off" choice (Unbound can't disable a mod; no upstream PR, no moving files). Store baseline 2026-10-24 (mlplay-work/storecompat, results.tsv): 70 of 72 store mods load and pass a smoke run with ML, and ML's log matches ML alone; pak_loader fails on vanilla too; mm_assets alone waits for an MM ROM. The MM-masks crash wasn't reproduced (it needs a mask worn).

**0.4 (2026-10-24, unbound-930-porter-4, signed from b634183 / ml-codemod-930 23b5fdda3):** the other-mods warning popup (warning only), README Other mods paragraph names it. Store build: signed load with no prompt, layer = 0.2/0.3's, mm_wearables smoke ok. mlplay-work/signed-43 and signed-43c now hold 0.4 (0.3 kept as *.0.3).

**porter-5 (2026-10-27, unbound-930-porter-5, owner OK; tag on da43ff8, README a7bfcf4):** childshield 0.2 (reflects nuts/rocks, signed from da43ff8) plus the unchanged missinglink 0.4. Assets are byte-identical to the store's. See [[childshield-reports]].

**0.5 held (2026-10-28, owner: "Wait for skijer"):** missinglink 0.5 (ml_quests.c: a new file is Original Quest, and files of other quests (Rando, MQ, Boss Rush, Speedrun) don't open) is committed as MissingShip 0163b75 / ml-codemod-930 51513cc4d. It is unsigned and unreleased. The owner is discussing a real ML quest entry (RegisterQuest + archives mounted per quest) with skijer on Discord. Don't /sign or release 0.5 until the owner says how that went: skijer's API may replace the guard. Mac tests: mlplay-work/quests/run.sh new|rando|ml (see [[mod-api-2]]).
Update 2026-10-28: ml-codemod-930 629d6af96 / MissingShip e19951e: on a host with RegisterQuest, ML registers "The Missing Link" (label ML, replaces world, adopts Original Quest files) and the host does the guarding; built against UNBOUND_REF (no `SOH_MOD_QUEST_REPLACES_WORLD`) the 0.5 guard stays. Still unsigned; wait for the owner and skijer's merge.
