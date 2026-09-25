# Drop-in proposals (MS-070)

What each row of [`ENGINE_CHANGES.md`](ENGINE_CHANGES.md) (rows 1-101 and the unnumbered rows) needs so that it can leave
the fork. Reviewed 2026-09-25 against SoH `develop` eccbc72cb (2026-09-24) and libultraship `main` 759b1340 (2026-09-02).

**Nothing goes upstream until the whole project is done** (owner decision, 2026-09-25). This is the list to work
from then. Every upstream PR needs a public fork to open from; the MissingShip repos stay private.

## 1. Bugfixes: nothing ML-specific in them

| Id | Rows | Fix | Upstream status |
|----|------|-----|-----------------|
| B1 | 23 | `z_bgcheck.c`: NULL-check `cameraDataList` (the `gSegments[0]` test never fires because SoH's `SEGMENTED_TO_VIRTUAL` is the identity) | Still unguarded on `develop` (`BgCheck_GetBgCamSettingImpl`, `func_80041B24`, ...). Any collision with no camera data crashes `Camera_Update` |
| B2 | 19 | `Oceff_Wipe`-`Wipe4`: widen the song cone by `aspect / (4/3)` | Still unfixed on `develop` |
| B3 | 44 | libultraship `GfxDpLoadBlock`: `word_size_shift` `uint32_t` → `int32_t` | Still `uint32_t` on `main`. One line |
| B4 | 64 | Place-name card: skip it when the scene has no title card, as vanilla did | Still unchecked on `develop` (the size line is commented out). Our fork hardcodes ML's titled slots; the upstream form should check the scene's title texture resource instead |
| B5 | — | ZAPD `ZAudio.cpp`: zero-size soundfont guard | Optional. Pipeline only |
| — | 11 | libultraship Metal: zero-size texture guard | **Already upstream:** Kenix3/libultraship#1040 (adaad35d, 2026-04-06) adds the same `width == 0 \|\| height == 0` return. Drop our copy when we rebase onto a SoH release that pins it |

## 2. Data-driven features: new resource types a mod `.o2r` could carry

Each one is general, so any mod could use it. Together they would take these rows out of C.

| Id | Feature | Absorbs rows |
|----|---------|--------------|
| F1 | **Entrance resource:** append entrances, override per-entrance flags (music continuity, title card, transitions) and remap a mod's entrance numbers onto SoH's | 1, 25 (+1 entrance), 28, 50, 84 (+8 entrances) |
| F2 | **Cutscene destination as a raw entrance** (`CS_DESTINATION` default case) and a `CS_BLACK_FILL` escape | 2, 84 (case 56) |
| F3 | **Entrance intro-cutscene table** (the scripts are already data) | 3 |
| F4 | **Grotto table** (`Door_Ana` destinations, return respawn on or off) | 25 |
| F5 | **New-save template** (inventory, equipment, songs and the starting entrance) | 13 |
| F6 | **Scene draw config in the scene resource** (overrides `scene_table.h`) | unnumbered draw-config row (17 slots) |
| F7 | **GetItem draw table** (draw function, DLs and colours for each GID) | 16 |
| F8 | **Pause-menu layout tables:** quest/equipment slot positions, cursor redirects, page colours, map tables and map markers | 24 (tables and redirects), 48 (tables), 49 |
| F9 | **Shop tables** (items, prices, conditions) | 32 (table), 67 |
| F10 | **Actor overrides in a scene or global resource:** InitVars flags, and "disable this actor" | 41, 42, 82, 94 (En_Karebaba); maybe 8 |
| F11 | Push-block colour table. Narrow, low value | 39 |

F1, F5, F6 and F8 matter most. Together they remove about 9 rows and every table ML changed in `code`.

## 3. Things to try on the data side first (no upstream needed)

| Id | Rows | Idea |
|----|------|------|
| D1 | 4 | Give ML's geometry-less intro stub scene a safe default layer, so a continue that lands there needs no guard |
| D2 | 38 | Ship ML's crack-wall DL and collision under the vanilla `gFireTemple*` paths (as the pickup ALIASES do) and drop the object swap |
| D3 | 100 | The Start button colour (223, 73, 73) is a stock cosmetics-editor setting. The drop shadows remain code |

## 4. Fork-only: drop or document for a drop-in build

- User settings and QoL: rows 5, 15, 45; the settings rows (`TextSpeed`, `SkipText`, D-pad options, `RememberSaveLocation`). Document the stock options for drop-in users.
- Row 47 (B cancels choices): propose it as a SoH enhancement after the project. Its exception table is per game.
- They go away with the rows they serve: 18 (with 17), 43 (the actors), 76 (the Hourglass).

## 5. Needs mod code (the hard wall)

Actor and player logic has no data form in stock SoH:

6, 7, 9, 10, 12, 14, 17, 20, 21, 22, 26, 27, 29, 30, 31, 33, 34, 35, 36, 37, 40, 46, 51, 52, 53, 54, 55, 56, 57, 58,
59, 60, 61, 62, 63, 65, 66, 68, 69, 70, 71, 72, 73, 74, 75, 77, 78, 79, 80, 81, 83, 85, 86, 87, 88, 89, 90, 91, 92, 93,
95, 96, 97, 98, 99, 100 (shadows), 101, plus parts of 24 (the vertical-move helper), 48 (the minimap index) and 94 (Obj_Ice_Poly).

That is 67 whole rows. The routes are unchanged (see "The hard wall" in the ledger): a mod-code mechanism upstream, narrow
data hooks (F10 covers only the flag-only rows), or split distribution. Every ML change found so far has a ledger row, so
the list should only grow if a new ML change turns up.

## Upstream order (once the project is done)

1. B3 (libultraship, one line) and B1 (a crash any collision can hit).
2. B2, B4.
3. F1, F5, F6 and F8, as a design discussion with HarbourMasters first, since they add resource types.
4. The others as they come up.
