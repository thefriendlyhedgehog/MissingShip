#ifndef ML_PATCH_H
#define ML_PATCH_H

/*
 * ML's changes to the game's own tables, put back while another quest's file is played (ml_patch.c). On a host with
 * mod quests ML's world covers the game's only while an ML file is played; its hooks then run only in that world, but
 * what it wrote into the game's tables stays unless it is put back here. Call one of these before the first write:
 * the bytes are kept as they are then (the game's), and swapped with ML's whenever the world changes.
 */

#include "soh/ModApi/ModApi.h"

#include <stddef.h>

// `size` bytes at `p`, a table that never moves
void MlPatch_Watch(void* p, size_t size);
#define ML_PATCH_WATCH(lvalue) MlPatch_Watch(&(lvalue), sizeof(lvalue))
// An actor's slot: its category, flags, object, size and functions (ActorDB moves its slots as actors are added)
void* MlPatch_Actor(int id);
// A row of gEntranceTable (SceneDB moves the table as scenes are added)
void MlPatch_Entrance(int index);
// Before ML's quest is registered: swaps the tables as ML's world comes and goes, once MlPatch_Start says the host
// took the quest (a host that didn't keeps ML's world on top for every file)
void MlPatch_Register(const SOHModApi* api);
void MlPatch_Start(void);

#endif
