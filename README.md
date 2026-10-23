# MissingShip

**The Missing Link** (Kaze Emanuar, v2.0) on **Ship of Harkinian**, as a mod for
[SoH: Unbound](https://github.com/roborich/Shipwright/tree/unbound) (SoH 9.3), on Windows, Linux and macOS.

It is a port, not an emulator: ML's world is rebuilt from your own ROM, and ML's code is rewritten in C as a code
mod. No ML machine code runs, and no download holds Nintendo's or ML's data.

**Pre-release** ([`unbound-930-porter-1`](../../releases/tag/unbound-930-porter-1), 2026-10-23). All of The Missing
Link is ported: its 25 scenes, both dungeons and the boss, every changed or custom actor, Link's model and moves, the
title screen, credits and ending.

## What you need

- **Your own Ocarina of Time USA 1.0 ROM** (NTSC 1.0). None is included. Unbound can have Master Quest's
  `oot-mq.o2r` too.
- **Kaze's The Missing Link v2.0 patch**, from [his release](https://www.romhacking.net/hacks/5334/): the `.zip` as
  downloaded is fine. (A Missing Link v2.0 ROM you already patched works instead of the ROM and patch.)
- **SoH: Unbound with skijer's mod API**, started once with your USA 1.0 ROM. Until skijer publishes a release with
  it, download the newest build for your system (`soh-mac`, `soh-windows` or `soh-linux`) from the **Build Artifacts**
  list on [his Unbound mod API page](https://github.com/skijer/Shipwright/pull/11), no GitHub account needed. Not the
  builds under **UNSAFE, MODDING ONLY**, which are for modders. His NEI Unbound release of 2026-10-06 is on an older
  SoH, which ML doesn't run on.
- A controller, or the keyboard (SoH's default layout).

## Install

1. Get `missinglink.o2r`, ML's code mod, either way:
   - **In Unbound:** **Network → NEI-Store → Packs → Other mods**. Tick only `missinglink` (the others there start
     ticked), click **Install selected**, then **Close Unbound now**.
   - **Or by hand:** download it from [Releases](../../releases) into Unbound's `mods/` folder.
2. Start Unbound. It asks to make The Missing Link: choose **Yes**, then your OoT ROM, then Kaze's patch.
3. A few seconds later it asks to restart: choose **Close now** and open Unbound again. The Missing Link starts.

ML's world, `MissingLink_Unbound.o2r` (about 24 MB, in `mods/`), is made on your computer from your own files: nothing
of Nintendo's or ML's is downloaded. `missinglink.o2r` is signed, so Unbound loads it without asking. When a new
version comes out (NEI-Store's **Update**, or a new download), ML asks to update: choose the same two files, then
close and reopen Unbound. If your Unbound was set up from another release of Ocarina of Time, ML says what to do.

## Settings

ML runs inside SoH, so SoH's whole menu (**Esc**) is there: graphics (widescreen, resolution, frame rate), controls,
the HUD editor, cosmetics and the rest. Each setting has one place: ML adds only its own two options and uses SoH's
for everything else.

**Turn this on:** **Enhancements → Quality of Life → Saving → Remember Save Location.** ML resumes a save where it was
made; without it, a save made outside a dungeon loads at Link's House, which in ML is the prologue's stub.

**ML's own options**, under **Enhancements → Missing Link**:
- **B Cancels Choices** (on): on a Yes/No prompt, B picks the "no" or cancel answer, and holding B to skip text never
  answers a prompt.
- **No Bean Seller Flashing** (off): stops the flashing of the bean seller who trades the Bunny Hood.

**SoH settings ML follows:**
- **Text Speed:** until you set it, text runs at ML's own speed (twice vanilla's); once set, yours is used.
- **Skip Intro:** a new file skips ML's opening and starts where ML's own skip does.
- **HUD editor:** ML's button and counter shadows and its Start button follow the positions and margins you set.
- **Minimap:** as in ML, 20 scenes have no minimap; set a minimap position in the HUD editor to show it there too.
- **Minimal UI**, **Better Ammo Rendering**, **D-pad on the pause screen** and the **GameCube colour scheme** work
  with ML's HUD and pause menu.
- **Controllable Bombchus**, **Remove Spin Attack Darkness** (ML's Blade Beam stays), **Disable Grotto Rotation** and
  **Cucco Stay Duration** work with ML's versions of those actors.
- **Gameplay stats** and **Time Splits** use ML's items and names (the Magic Hourglass, the Soul-Charged Sword).

The randomizer and other options for vanilla's own items and areas don't apply to ML.

## Known differences from ML on N64

- SoH's boot logo replaces ML's two boot checks (the Expansion Pak and emulator-accuracy error screens), which don't
  apply here. ML's title screen itself is ported.

Found another? Open an issue with what you saw and where.

## More

- **Only young Link's Hylian Shield?** `childshield.o2r`
  ([`unbound-930-porter-1`](../../releases/tag/unbound-930-porter-1), or `childshield` in NEI-Store) is ML's shield
  rule alone, for any SoH: Unbound with skijer's mod API. It is signed, so Unbound loads it without asking. Not needed
  with The Missing Link, which has it.
- **Testers:** the builder ([`unbound-930-builder-3`](../../releases/tag/unbound-930-builder-3)) still makes ML from the
  command line or its window. Its layer updates in two steps, and its code mod crashes at the end of the prologue with
  **Skip Entrance Cutscenes** on; replace its `missinglink.o2r` with this one.
- **Developing:** how the port is built, tested and released, in [`DEVELOPMENT.md`](DEVELOPMENT.md).
- **Plan and history:** [`docs/ROADMAP.md`](docs/ROADMAP.md).
- **Every change ML makes to the game, and how it is ported:** [`docs/ENGINE_CHANGES.md`](docs/ENGINE_CHANGES.md).

## Credits

MissingShip is ported with Kaze Emanuar's permission.

### The Missing Link (v2.0)

| Who | Role |
|---|---|
| Kaze Emanuar | Producer. Hacking: game planning, direction and code; level design |
| Zel | Hacking: 3D model design, code, dialogue and cutscenes; level design, maps and writing |
| CDi-Fails | Level design, 3D models and maps, cutscenes, writing |
| Zerovolt0v, A6stringthing, Sauraen, Badub, DezZival | Music |
| Rankaisija, Ultra553 | Boss animations |
| OstrealavaO2 | Cutscenes |
| Aurora-Xatu, Ty Anderson | Concept art |
| Zerovolt0v, Sauraen, E-Gor, GTM, Collins, Ricar, Fig | Testing |
| Nokaubure, z64me, the OoT decomp team | Special thanks |

Credits from The Missing Link v2.0's release.

### Engine and tools

- Ship of Harkinian: HarbourMasters.
- SoH: Unbound: roborich. Its code mod API: skijer.
- The OoT decompilation: zeldaret.
