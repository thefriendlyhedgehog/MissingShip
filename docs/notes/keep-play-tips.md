---
name: keep-play-tips
description: Owner's ML play knowledge (2026-10-17/18) - stairs need the Hourglass/blue switch; eyes by Bombchu; hooded rolls need the hood on at once and a full run-up; Sanctuary web/crate/pillar
metadata:
  type: reference
---

From the owner's own play (2026-10-17), for scripting the mlplay playthrough:
- Going up stairs in the keep mostly needs the Magic Hourglass, or the blue switch (crystal, room switch 0x20)
  active/raised.
- Room 16's skull eyes are hit by a Bombchu from the lower ledges (325, reached by the vines on each side), not
  the Blade Beam from the lids. ML's Bombchu takes two C presses (stance, then release); B leaves the stance.
- Room 1's elevator: ride it up from room 19, then the Hourglass at its top drops Link back into room 19.
- Room 14's blocks and room 24's ring gap: hooded roll long jumps ([[ml-hood-roll-gaps]]).
- Hooded rolls (owner, 2026-10-18): put the hood on right after a void-out / on entering the room (a void-out takes
  it off; mlplay `hood on` waits until Link can act and checks it took). Give the roll a full run-up on one floor: a
  step down on the way (room 5's z 15-30 step) makes Link full-jump and lose speed. Let go of the stick once A is in
  (`rollto ... 20`): steering toward the far point through the flight makes it fall short (room 9).
- Skulltulas hang over the keep's walkways (rooms 3, 6, 9): Blade Beams from a standing spot (`attack ... stand`).
- Sanctuary (Bmori1): the upper floor's pillar (0,-425) lowers on switch 1, under the crate at (585,-242) on room 0's
  east ledge. Reach it from the top door (0x169): a Blade Beam lights room 2's torch, a pot's Deku Stick lit there
  burns the web over the hole, drop through. The upper floor's torch (beam) slides the platform to the Peahat door.

**How to apply:** when a stretch is carried by a helper (mlplay PLACED), try these first; if stuck, use
`handover` to let the owner play it while mlplay records ("rec" lines), then script from the recording.
