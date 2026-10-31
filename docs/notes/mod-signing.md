---
name: mod-signing
description: how our code mods get signed - skijer's sign-mod issue builds codemods/ from public MissingShip; first request needs his approval
metadata:
  type: project
---

Owner asked 2026-10-18 to "sign our mods". Unbound signs via skijer/Shipwright's **sign-mod** workflow, not with our own key. We never hold a signing key; skijer's secret signs.

- Sources are public in MissingShip `codemods/` (missinglink, childshield). `UNBOUND_REF` at the repo root = mspaths.CODEMOD_HOST (eed5004cc, the build players run: ask policy, key present). `tools/codemods.py` copies the sources from ml-codemod-930, and check.py's `codemods` check keeps them in step.
- Request: a "Sign a mod" issue (the form adds the `sign-mod` label, which an outside account can't add via the API). Fields: repository thefriendlyhedgehog/MissingShip, commit, folder `codemods`. Prefill URL: issues/new?template=sign-mod.yml&repository=…&commit=…&directory=codemods.
- The first request waits for skijer's `approved` label. The answer is a release with the signed .o2r files. Those replace codemod-1 (CODEMOD_RELEASE) and childshield-1, which needs an owner OK since it's a release; then drop the README's "Always load" step.
- Pre-checks that passed 2026-10-18: scan_mod_sources (both modes), cppcheck (fixed ml_maps.c calloc, 5dbdfc964), semgrep p/c, check_mod_binary, a signing CMake build on macOS.
- The built-in browser isn't signed in to GitHub; the owner submits the form themselves. Request filed 2026-10-19: skijer/Shipwright#39 (commit 33fdf8d89), labelled awaiting-approval.

Related: [[builder-end-goal]], [[held-upstream-offers]], [[release-priorities]].

**2026-10-19, signed** (skijer/Shipwright signed-mod-39-33fdf8d, run 39257948471). Verified in skijer's players' build (run 38509788047, ask policy): both load as signed with no prompt.
- childshield-2 is published, and the README links it.
- codemod-2 (private, signed missinglink.o2r) is published. CODEMOD_RELEASE=codemod-2 in engine 0103d767c.
- `tools/codemod.py --signed <tag>` fetches and checks signed packages.
- Builder texts drop "Always load". The builder no longer bundles ml_titlecard_banner.inc: unbound_layer reads it from the ML ROM (51eca0904). builder-3 shipped that banner.
- The macOS builder-4 package is made and launcher-tested (mlplay-work/gui-mac4; layer entry-identical to builder-3's).
- **(Dropped 2026-10-26: builder-4 won't ship, see [[builder-end-goal]].) Was waiting until 2026-11-01:** generate-builds run 39260130499 wasn't started (Actions billing), and the owner won't raise the limit ([[actions-quota]]). Local freezing isn't possible: no Python on the Windows box, no containers on the Mac, and a CachyOS freeze needs a newer glibc than CI's Ubuntu 22.04. Builder-3 stays up and current until builder-4 replaces it (owner 2026-10-19; its banner is fine, see [[kaze-permission]]). Its README step 4 (Always load) is still right for it. dist/MissingLink-Unbound-macos.zip is builder-4's macOS package (from 51eca0904).
- After CI: `package.py unbound-ci <run>`, launcher tests on the Windows and Linux boxes, release unbound-930-builder-4, then README step 4 (drop Always load) and the "Pre-release" line.

**2026-10-20/21: builder-4 paused.** The owner archived the session because major porter changes are planned ([[porter-one-file]]).
- Prefill URL for the second signing request (da03954): the owner was asked to submit it. Not confirmed filed. Re-check skijer/Shipwright issues before asking again. A new porter commit may need its own request instead.
- Windows/Linux builders via [[builder-repack]]: the Windows launcher test passed and its layer is identical to the Mac build. The Linux package is untested: both boxes were down on 2026-10-20.
- D:\mlplay-test\b4 and task mlb4 were removed.

**2026-10-23, skijer's store:** signing now answers in his store release `mods` as `<mod>-api<major>.o2r`, which NEI-Store installs in game. Each modder has one open issue; later commits are signed with `/sign <commit>`, and new permissions (ML's `files`) wait for his `approved` again.
- Ours is skijer/Shipwright#43 (open, approved): 4424974 was signed into `mods` (runs 39893195944 and 39892286252). #40 was closed.
- `tools/codemod.py --signed mods` fetches and checks both (MissingShip b6af5f3).
- Mod API major 1 (SOH_MOD_API_MAJOR) is stamped into each package, and hosts refuse other majors.
- PR #11's newest build is the store build, run 39864591861 at 291dc26db; the Mac copy is in mlplay-work/store-mac. There both signed mods load with no prompt and the porter's layer is identical. `mlplay-work/store-test.sh` runs that check.
