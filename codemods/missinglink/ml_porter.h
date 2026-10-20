#ifndef ML_PORTER_H
#define ML_PORTER_H

// The porter: ML's code mod makes ML's layer, MissingLink_Unbound.o2r, in the game, from the player's own OoT ROM and
// Kaze's patch, and puts it in the mods folder (see ml_porter.c).

#include "soh/ModApi/ModApi.h"

// Called first from ModInit. Returns true if a layer is mounted, so the rest of ML may start; either way it offers to
// make the layer (or the newer one this mod carries) on the game's first frame.
bool MlPorter_Register(const SOHModApi* api);

#endif
