# Young Link's Hylian Shield

One rule from The Missing Link, alone, for stock Unbound on SoH 9.3.0 with skijer's mod API: young Link holds the
Hylian Shield like any shield. He guards and Z-targets with it raised in his right hand, and holds it there with his
sword out, as he does the Deku Shield, instead of ducking under it on his back. Nothing else of ML comes with it.

- The rule: `VB_BE_CHILD_WITH_HYLIAN_SHIELD` answers false (skijer/Shipwright#33), as ML's nop of vanilla's
  `^ PLAYER_SHIELD_HYLIAN` does.
- The hand: vanilla has no child hand holding the Hylian Shield, so the mod builds one when it starts, from the
  player's own `oot.o2r`. It copies vanilla's adult hand holding the shield without the hand's and gauntlet's
  triangles, draws it scaled by 0.8 and moved into the child's fist, then the child's closed right hand. That is the
  transform ML's own hand uses (its handle is the adult's x 0.8 + (-10.8, -54.8, -7.9), within a unit). The package
  holds compiled code only: no model, texture or other game data.
- If a model pack has replaced the adult shield hand (the list isn't vanilla's shape), the mod logs it and changes
  nothing, so the shield stays on his back as in vanilla.

Not needed with The Missing Link's own code mod, which does the same.
