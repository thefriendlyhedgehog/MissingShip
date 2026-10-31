---
name: mod-api-2
description: "skijer's unbound-modapi is mod API 2 since late Oct 2026; hosts skip mods of another major silently — test local builds in build-mac"
metadata:
  node_type: memory
  type: project
  modified: 2026-10-28T14:29:44.266Z
---

skijer's unbound-modapi moved to `SOH_MOD_API_MAJOR 2` (commits 291dc26db / 35d178399, merged into ml-codemod-930 on 2026-10-28 with his shared game module). The SDK stamps `"modapi": 2` into each built package, and a host of another major skips the mod without a log line. The Oct 23 store build in mlplay-work/store-mac is API 1, so a mod built from the merged branch doesn't load there. His store ships both `<mod>-api1.o2r` and `-api2.o2r`.

**Why:** a missinglink 0.5 test silently ran without ML in store-mac (2026-10-28).

**How to apply:** test local mod builds in `../Unbound-skijer930/build-mac/soh/soh-macos` (the shared-module launcher; process name soh-macos). Build BOTH targets: `cmake --build build-mac --target soh` makes only libsoh.dylib, `--target soh-macos` the launcher. Until 2026-10-28 that path held an old monolithic binary (commit 2ac4ad6), so check the log's "Starting SoH ... Commit:" line with mlplay rebuilt from the same SDK; `mlplay-work/quests/run.sh` does this. UNBOUND_REF / CODEMOD_HOST stayed at 948bff278 (API 1). See [[mod-signing]], [[mac-tests-use-metal]].
