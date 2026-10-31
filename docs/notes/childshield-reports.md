---
name: childshield-reports
description: "childshield player reports 2026-10-26/27 - 0.2 reflects nuts/rocks (signed, released porter-5); shield-on-back report likely a model pack; cs-reflect test home"
metadata:
  node_type: memory
  type: project
  modified: 2026-10-27T15:35:08.408Z
---

**Reflection (2026-10-27):** a player said the child's Hylian Shield didn't send things back. Vanilla En_Nutsball and
En_Okuta reflect only off the Deku Shield or an adult's Hylian Shield (LINK_IS_ADULT). childshield 0.2 (ml-codemod-930
ef74f9f22, MissingShip da43ff8) wraps both actors' updates and counts Link as adult during a projectile's update while
he holds the Hylian Shield. ML itself already reflects nuts (ml_vanilla_enemies.c) and has no Octoroks.
Fork commit not pushed (see [[actions-quota]]). Signed 2026-10-27 (owner OK; `/sign` on skijer/Shipwright#43, run
40532953100): in skijer's store `mods` as childshield-api1.o2r 0.2, so NEI-Store players get it. Verified signed in the
store build (loads with no prompt, both tests pass). Released 2026-10-27 as unbound-930-porter-5 (owner OK) with missinglink 0.4
unchanged; README links it.

Test home `mlplay-work/cs-reflect` (`run.sh old|new <script>`, nuts.txt in Sacred Forest Meadow warp 0x0FC, okuta.txt
at Lake Hylia 0x102 with a spawned Octorok). Old 0 reflected, new reflects and stuns/kills. run.sh sets
SkipCutscene.Intro/Entrances: without them, warping during the new-file intro leaves Link in a cutscene state.

**Shield on his back (2026-10-26):** another player's childshield stopped mid-session and stayed off. They run every
NEI-Store mod plus Djippi's 3DS experience, a Super Smash Bros Melee Link model + equipment pack and a 3DS animation
mod. Likely cause: the Melee pack replaces the adult Hylian Shield hand, so Build's shape check fails and the mod stays
off by design (Tab toggling alt assets would explain "mid-session"). Their log rotated (texture-null spam every frame);
asked for `Ship of Harkinian.1.log` and the Tab test. Unconfirmed.
