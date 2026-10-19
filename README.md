# MissingShip

**The Missing Link** (Kaze Emanuar, v2.0) on **Ship of Harkinian**, as a mod for
[SoH: Unbound](https://github.com/roborich/Shipwright/tree/unbound) (SoH 9.3), on Windows, Linux and macOS.

It is a port, not an emulator: ML's world is rebuilt from your own ROM, and ML's code is rewritten in C as a code
mod. No ML machine code runs, and no download holds Nintendo's or ML's data.

**Pre-release** ([`unbound-930-builder-3`](../../releases/tag/unbound-930-builder-3), 2026-10-17). All of The Missing
Link is ported: its 25 scenes, both dungeons and the boss, every changed or custom actor, Link's model and moves, the
title screen, credits and ending.

## What you need

- **Your own Ocarina of Time USA 1.0 ROM** (NTSC 1.0, not Master Quest). None is included.
- **Kaze's The Missing Link v2.0 patch**, from [his release](https://www.romhacking.net/hacks/5334/): the `.zip` as
  downloaded is fine. (A Missing Link v2.0 ROM you already patched works instead of the ROM and patch.)
- **SoH: Unbound on SoH 9.3.0 or later, with skijer's code mods**, started once with your OoT ROM. Until skijer
  publishes a release with them, that is his `unbound-modapi` build, downloadable without a GitHub account:
  [macOS](https://nightly.link/skijer/Shipwright/actions/runs/38509788047/soh-mac.zip),
  [Windows](https://nightly.link/skijer/Shipwright/actions/runs/38509788047/soh-windows.zip),
  [Linux](https://nightly.link/skijer/Shipwright/actions/runs/38509788047/soh-linux.zip) (until 2027-01-12). Not the
  `soh-unbound-unsafe-*` builds, which are for modders. His NEI Unbound release of 2026-10-06 is on an older SoH, and
  the builder refuses it.
- A controller, or the keyboard (SoH's default layout).

## Install

1. Download the package for your system from [Releases](../../releases): `MissingLink-Unbound-macos.zip`,
   `-windows.zip` or `-linux.tar.gz`. Unpack it anywhere.
2. Double-click **Build The Missing Link**: the `.app` on macOS, the `.cmd` on Windows, the `.sh` on Linux.
3. In the window, choose your Unbound (its folder, or the app on macOS), your OoT ROM and Kaze's patch, then click
   **Build The Missing Link**. It checks them first, then builds in under a minute, offline. On Linux it unpacks
   Unbound's AppImage first if that hasn't been done.
4. Start Unbound. When it asks about `missinglink.o2r` (ML's code mod, not signed yet), choose **Always load this
   version**.

The builder writes two files into Unbound's `mods/` folder: `MissingLink_Unbound.o2r` (ML's world, made from your
ROM) and `missinglink.o2r` (ML's code). Build again after updating Unbound or the package. Each package's
`README.txt` also gives the command line, for those who want it.

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

- **Only young Link's Hylian Shield?** [`childshield-2`](../../releases/tag/childshield-2) is ML's shield rule alone, for any
  SoH: Unbound 9.3 with skijer's code mods: `childshield.o2r` in `mods/`, no builder needed. It is signed, so Unbound
  loads it without asking.
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
