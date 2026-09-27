# ML on SoH: Unbound — what each engine change becomes (step 2)

Every row of [`ENGINE_CHANGES.md`](ENGINE_CHANGES.md), placed against SoH: Unbound 0.8
(`roborich/Shipwright` branch `unbound`, 0382526, 2026-09-27) and its format spec (`unbound-docs/SPEC.md`, format
version 2). Rows 1–101 follow the groups already worked out in [`DROPIN_PROPOSALS.md`](DROPIN_PROPOSALS.md) (F1–F11,
B1–B5, D1–D3); rows 102–144 are placed here for the first time.

Step 1 is done: `tools/unbound_layer.py` turns ML's archive into an Unbound mod layer (25 scenes, 304 messages, ML's
objects, textures and audio), and Unbound loads it and draws ML's scenes. Everything below is what that layer does
*not* carry.

## A. Already covered by Unbound

| What ML needs | Rows | Unbound feature |
|---|---|---|
| ML's scenes and rooms, including the ones in unused vanilla slots | pipeline; 112 | JSON scenes (SPEC §4); a missing alternate setup falls back to setup 0 (§4.2), which is what row 112's bounds check did |
| ML's text | pipeline (`inject_messages.py`) | `text/<lang>/messages.json` merges per id (§5) |
| Animated scene materials for ML's scenes | draw-config row (part of F6) | `materialAnims` (§4.2), where ML's config is a scroll or colour cycle |
| Collision, room and actor limits | none needed so far | raised limits (§9) |

## B. Data features to build in Unbound (new format, spec'd in its style)

Each is general, so any mod could use it; each gets a SPEC section, a how-doc and a verification plan, as Unbound's
own features do.

| Id | Feature | Rows | Status against Unbound |
|---|---|---|---|
| U1 | **Vanilla entrance overrides**: a mod changes a vanilla entrance's flags (title card, music continuity, transitions) and destination spawn | 1, 28, 50 (F1) | Registry keys can't be vanilla scene names (§7.1): new entrances work, overriding vanilla ones doesn't |
| U2 | **Cutscene destinations by entrance name** (`CS_DESTINATION` to any registered entrance, and the black-fill escape) | 2, 84, 109 (F2) | Gap: cutscene commands still use vanilla destination numbers |
| U3 | **Entrance intro-cutscene table and conditional triggers** (which cutscene plays on which entrance and flag, and removing vanilla ones) | 3, 110, 114 (F3) | Gap |
| U4 | **New-save template** (inventory, equipment, songs, starting entrance, where Skip Intro lands) | 13, 115 (F5) | Gap |
| U5 | **Grotto table** (`Door_Ana` destinations and return respawn) | 25 (F4) | Gap |
| U6 | **GetItem draw table** | 16 (F7) | Gap |
| U7 | **Pause-menu and minimap tables** (slot positions, cursor redirects, colours, map tables, markers, the dungeon-minimap default) | 24, 48, 49, 138 (F8) | Gap; Unbound notes its own "no minimap for custom scenes" limit, the same table |
| U8 | **Shop tables** | 32, 67 (F9) | Gap |
| U9 | **Vanilla actor overrides**: InitVars flags, "don't spawn this actor" | 41, 42, 82, 94 (F10) | Gap. `base` (built, below) is per placement, and ML changed these actors everywhere: ML's crates and soil patches spawn Skulltulas (`En_Sw`) at run time, which no placement reaches, and SoH finds some of them by id (GS stats, the GS Targetable cheat). An override of the vanilla actor itself is the faithful form |
| U10 | **Game labels**: item and quest-item names, area names for stats and splits | 108, 111, 121, 137 | Gap; small, but touches SoH's UI tables |
| — | Push-block colours | 39 (F11) | Low value; fold into U9 if needed |

## C. Behaviour (actor and player logic in C)

The hard part: about 70 rows (DROPIN_PROPOSALS §5: 6, 7, 9, 10, 12, 14, 17, 20–22, 26, 27, 29–31, 33–37, 40, 46,
51–63, 65, 66, 68–81, 83, 85–101 and parts of 24, 48, 94) plus 103, 105, 107, 122, 123, 125, 126 and 127 here.
Unbound's custom actors are data-only today. Its planned route is `base` (a vanilla actor's code under a new type,
with asset swaps and text remaps) and then `script`.

| Kind | Rows (examples) | Route |
|---|---|---|
| Asset or text swap on a vanilla actor | 38 (breakable wall), 123 (Kokiri frown texture), 127 (scrub's sales line), part of the pickup ALIASES | **38 done** with `base` (`ml/crack_wall` in `tools/unbound_layer.py`, 2026-09-27): ML's two Deku Tree walls spawn from object 0xA3 with ML's model and collision, verified by log. 123 and 127 depend on save state (sword, a sale), so `base`'s fixed swaps don't express them |
| Small rule change in a vanilla actor | 105 (fog wall), 122 (tree mouth), 125 (title bars), 103 (bean seller flash) | `base` with a named hook per actor, or `script` |
| ML's new actors and Link's changes (triple jump, Magic Hourglass, Soul-Charged sword, Arborachnid, ...) | 51–63, 68–81, 107, ... | `script`, or C actors registered by name (SPEC §4.3 lets a mod place "actors a build adds in code") |

**Decision for the owner when we get here:** C actors registered by name in our Unbound branch make ML playable
soonest but are a fork again; a scripting design is the general answer and the largest piece of work. `base` comes
first either way.

## D. SoH-fork only: not needed on Unbound, or the owner decides

| What | Rows | Note |
|---|---|---|
| Menu, editor and preset curation (removed options, hidden cosmetics, retired CVars, Randomizer/Network tabs) | 106, 116–118, 120, 130–136 | Stock SoH menus on Unbound unless a generic "mod profile" feature is wanted; our rule of never mirroring stock settings still holds |
| Packaging, app name, first-launch builder | 140–144 | Revisit when ML on Unbound is playable: the builder would add `tools/unbound_layer.py` |
| Debug aids | 104 (trace log), 113 (console cutscene index) | Dev-only; 113 could be offered to Unbound |
| Removed rows | 102 | Nothing to carry |

## E. Bugfixes to offer Unbound (and SoH)

| Id | Rows | Fix |
|---|---|---|
| B1 | 23 | NULL-check `cameraDataList` in `z_bgcheck.c` |
| B2 | 19 | Song-cone width for widescreen (`Oceff_Wipe`-`Wipe4`) |
| B3 | 44 | libultraship `GfxDpLoadBlock` `word_size_shift` sign |
| B4 | 64 | Skip the place-name card when the scene has none |
| B6 | 124 | libultraship: texture-edge (`CVG_X_ALPHA`) modes stay alpha-tested when the blender reads memory |
| B7 | 126 | Controllable Bombchus: release control on rail walkways (general form: on a "no bombchu" surface) |
| B8 | — | ZAPD: two bytes per skeleton limb and one per soundfont drum entry are written uninitialized, so `oot.o2r` differs between platforms (found 2026-09-27) |

Pipeline rows (128 grass floors, 129 sky seams, 139 split samples) are data changes already inside the mod archive
and need nothing from Unbound.

## Order of work (step 3)

1. `base`: **built** on our Unbound branch (SPEC §7.3, `unbound-docs/based-actors.md`), tested on macOS and Linux; applied to row 38. Next, U9 as a vanilla actor override for the flag rows.
2. U1–U4 (entrances, cutscene destinations, intro cutscenes, new save): ML's story flow depends on them.
3. U7 and U10 (pause menu, minimap, labels), U5, U6, U8.
4. Behaviour beyond `base`: the owner's decision above.
