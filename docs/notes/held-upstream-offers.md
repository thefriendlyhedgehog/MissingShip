---
name: held-upstream-offers
description: "Unbound offers to skijer: skijer/Shipwright#33 merged 2026-10-14 (9.3.0 base); roborich libultraship #2/#3 still open; B8 ZAPDTR and delta files still held"
metadata:
  node_type: memory
  type: project
  modified: 2026-10-13T23:07:51.605Z
---

On 2026-10-13 the owner said to send all needed PRs to skijer instead of roborich ("his branch is the one with mod support",
"more responsive than robo"). Opened against `skijer/Shipwright` base `unbound-modapi`, head
`thefriendlyhedgehog/Shipwright-VR` (our fork in that network) branches `modapi-offer/*`, one commit each:
#23 child Hylian Shield VB, #24 wall push-out VB, #25 water speed motion scale, #26 stats count row VB,
#27 window labels API (table members SetLabel/SetTimeSplits/SetTimeSplitGroup/ClearTimeSplitGroups/SetItemIcon),
#28 B1 null camera data, #29 B2 widescreen song cone, #30 B7 VB_CONTROL_BOMBCHU, #31 B9 custom scene title card.
The offers worktree is `$scratch/offers` (detached worktree of Unbound-modapi, remote `prfork`).

Still elsewhere: B3/B6 open as roborich/libultraship#2/#3 (no reaction since 2026-09-29; skijer's branch takes that
submodule from roborich). B8 ZAPDTR fixes (`offer/empty-instrument-slots`, `offer/sample-past-table` in private
our ZAPDTR fork) and the delta files are held; delta files aren't needed on the builder route ([[builder-end-goal]]).

**Review round, 2026-10-13 evening (owner OK'd posting):** only #23 is needed for normal play (ML's child holds
its starting Hylian Shield in hand); #25 is fidelity, the rest cosmetic or bug fixes ML already works around.
#23 +5a8f553 (EAV sheath checks routed through the rule; ML answers always false, no-shield half dropped),
#25 +3426bf5 (skijer's guard), #26 screenshot posted (image on Shipwright-VR branch `pr-assets`), #27 answered.
#24 and #30 closed as not needed (fog wall's home/prevPos workaround; the A-press release). #28/#29 wait for his
9.3.0 update. skijer asked (Discord) for ONE combined PR once details settle: fold the kept branches into one PR
then (ask owner first: upstream). #29 is also on Shipwright-VR `hm-offer/widescreen-song-cone` (on hm/develop,
tag `SOH [Port]`), but opening the HarbourMasters PR was blocked by the permission check; the owner opens it or
grants permission. #31: owner said skip (skijer pointed it at roborich).
**Combined, 2026-10-13 20:00:** skijer closed #23-#31 unmerged right after the replies (his Discord ask: one PR).
Owner: merge into one and send. Opened skijer/Shipwright#32 from Shipwright-VR `modapi-offer/ml-host-needs`
(skijer's unbound-modapi tip 27bb6c445 + the 8 commits of #23, #25-#29); built and booted with ML on the Mac.
Review changes go on that branch as new commits.
**Moved to 9.3.0, 2026-10-13 night:** skijer moved `unbound-modapi` to Unbound 0.11 (SoH 9.3.0) + NEI 2.0 beta, so #32
was closed and redone as skijer/Shipwright#33 (Shipwright-VR `modapi-offer/ml-host-needs-930`; labels take TimeSplits
types). Shipwright-VR `unbound` is a build-only copy of that branch (skijer's generate-builds runs only on `unbound`
pushes; never use its workflow_dispatch, which publishes a release). **#33 MERGED 2026-10-14** (squash eed5004cc, tree = our b6e44007c); skijer's generate-builds run 38509788047 has its
soh.lib (expires 2027-01-12), which ml-codemod-930's build-missinglink links against until he releases 9.3.0. No
skijer 9.3.0 release yet; then switch the builder (tools/codemod.py --src) to the 9.3.0 checkout.
#33's first fork build failed on Windows only (an extern's array size from 9.2.3; MSVC links by type, Clang/GCC by name): fixed b6e44007c; build-check Windows before calling a host change done.
ML's 9.3.0 code mod: our Unbound fork
`ml-codemod-930` (worktree `Unbound-skijer930`). Windows mod CI there still needs a 9.3.0 soh.lib (skijer's releases are 9.2.3).

ML's side of #23-#27 is ready on Unbound-modapi `ml-codemod-hooks` (ml-codemod + the five PR branches + `fd5fae4e7`, pushed to origin, tested 2026-10-13). When they merge upstream, merge skijer into `ml-codemod`, then `ml-codemod-hooks`' mod commit.

**#41 CLOSED by us 2026-10-21 (owner: close it; skijer preferred no extractor change; the layer now keeps the game's files instead).** Was: #41 opened 2026-10-21 (owner: "yes, write the PR for skijer"): `ReadGameArchiveFiles` (O2rExtractor_ReadGameFiles), from Shipwright-VR `modapi-offer/read-base-archives` on 948bff278. It lets the porter update in one restart ([[porter-one-file]]). **#42 CLOSED by skijer 2026-10-21, security:** a mod that can restart the game (with or without a dialog) could rewrite its own files or a same-named mod and run them after approval. Never offer self-restart/relaunch again; restarts stay "Close now / Later" and the player reopens. He likes #41. Was: #42 opened 2026-10-21 (owner OK): automatic restart (execv / CreateProcess) plus `RestartGame` without a dialog, from `modapi-offer/restart-game`. #41 and #42 both append to SOHModApi (a one-line conflict). After both merge: bump UNBOUND_REF, move the sign-pr porter (ReadGame, Make(true), Restart()) into ml-codemod-930, and delete MissingShip branch test/restart-game.

**Why:** the owner wants ML's host needs merged where the mod API lives.

**How to apply:** don't poll. When a skijer PR merges, merge it into `ml-codemod` (per [[check-unbound-upstream]], fetch
skijer too) and switch the mod to the hook, dropping the workaround it replaces. If he asks for changes, make them on
the same branch as new commits (the branches are pushed, and we never force-push). Status table: docs/UNBOUND_MAP.md section E.
Related: [[ml-on-unbound]].

**Mod quests (2026-10-28, owner relayed skijer: "a romhack quest or a mod quest ... select the list of mods you have ... leave the randomizer for later ... I would love [it] if you want to make that PR"):** first design on `modapi-offer/mod-quests` (f63583d92, worktree `../Unbound-modquests`; Mod Quests list, REPLACES_WORLD). The owner then chose (2026-10-28) **live world switching, no restart, ML as its own carousel stop with ML's title logo**: built on ml-codemod-930 as b7cf6c688 (host: per-quest `archive`+`logo`, SetArchives sinking, resource release, OTRMessage_Reload, audio permanent-pool drop, hooks gated by world, OnQuestWorldChange, labels per world, GetQuestWorld row) + de41719bf (ML: ml_patch.c journal of 470 table writes). Tested on Mac: run3.sh (one session, vanilla/ML new + old files, 11 PASS). Offer branch rebuilt 2026-10-28 as ONE commit 7420993e1 on skijer 597db6620 (live worlds + game-quest/seed switch + no boot switch without quest worlds; host only, no ML); PR text mlplay-work/quests/pr-mod-quests.md is current. Windows/Linux check = push a 'Build copy' commit (tree of the PR commit, parent prfork/unbound, fast-forward) to Shipwright-VR `unbound` (generate-builds, no release); the push needs the owner's OK (auto-mode blocked it). The Context::~Context abort at exit is upstream (no mods too), not in the PR. Owner: leave the stock Boss Rush timer-on-title bug alone. Build copy 80bfef6f1 (run 40762877072, 2026-10-28): Windows and Linux green; macOS job cancelled by the runner with no failed step (Mac built and tested locally). **Opened as skijer/Shipwright#45 on 2026-10-28 (owner OK)** from Shipwright-VR `modapi-offer/mod-quests` (7420993e1 on skijer 597db6620). Review changes go on that branch as new commits; when it merges, merge skijer into ml-codemod-930 and ship missinglink with the quest (porter).
