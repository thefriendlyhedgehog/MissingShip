/*
 * The Missing Link's changes to vanilla actors' InitVars, as a code mod.
 *
 * MissingShip's Unbound branch reads these as actor overrides from ML's layer (unbound/actors/<vanilla name>.json,
 * MissingShip tools/unbound_layer.py ML_OVERRIDES); stock Unbound refuses a vanilla name there ("already names an
 * actor"), so the MissingShip builder's --stock layer leaves them out and this mod applies them. Same table, same
 * effect: the flags change on the actor's ActorDB entry, so every copy spawns with them, including the Skulltulas ML's
 * crates and soil patch spawn; Bg_Treemouth's room placements are not spawned. Not in the layer's overrides: a
 * Skullwalltula is never spawned without its object, as on the N64 (ML's crates).
 */
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ActorDB.h"

#include <stdio.h>

#include "missinglink.h"

typedef struct {
    const char* name;
    u32 set;
    u32 clear;
} MlFlagOverride;

static const MlFlagOverride sFlagOverrides[] = {
    { "En_St", 0, ACTOR_FLAG_HOSTILE },                           // row 41: Skulltulas are not enemies (0x35 -> 0x31)
    { "En_Sw", 0, ACTOR_FLAG_HOSTILE },                           // row 41: nor Skullwalltulas (0x15 -> 0x11)
    { "Obj_Bombiwa", ACTOR_FLAG_UPDATE_CULLING_DISABLED, 0 },     // row 42 (0 -> 0x10)
    { "En_Vm", 0, ACTOR_FLAG_ATTENTION_ENABLED },                 // row 82: Beamos are never Z-targetable (0x11 -> 0x10)
    { "En_Karebaba", 0, ACTOR_FLAG_HOSTILE },                     // row 94 (5 -> 1); ML never places it
    { "En_Firefly", 0, ACTOR_FLAG_HOSTILE },                      // row 27: Keese are not enemies (0x5005 -> 0x5001)
    { "En_Dekubaba", 0, ACTOR_FLAG_HOSTILE },                     // row 27: nor Deku Babas (5 -> 1)
};

// Rows 8 and 122: ML guts the Deku Tree's mouth. Its room placements are dropped before the actor is allocated.
static void MlOverrides_SpawnEntry(bool* should, va_list args) {
    va_list copy;
    va_copy(copy, args);
    (void)va_arg(copy, ActorContext*);
    ActorEntry* entry = va_arg(copy, ActorEntry*);
    (void)va_arg(copy, PlayState*);
    Actor** spawned = va_arg(copy, Actor**);
    va_end(copy);

    if (entry->id == ACTOR_BG_TREEMOUTH) {
        *should = false;
        *spawned = NULL;
    }
}

// ML places every large crate (Obj_Kibako2) with params 0, which asks it to let out a Gold Skulltula when broken. On
// the N64 that spawn fails unless the room has object_st loaded, and none of ML's crate rooms do; SoH spawns an actor
// without its object anyway, so every crate let one out. Skullwalltulas keep vanilla's rule.
static void MlOverrides_SpawnWithoutObject(bool* should, va_list args) {
    va_list copy;
    va_copy(copy, args);
    s16 actorId = (s16)va_arg(copy, int); // an s16, promoted
    va_end(copy);

    if (actorId == ACTOR_EN_SW) {
        *should = false;
    }
}

void MlOverrides_Register(const SOHModApi* api) {
    char message[96];

    for (size_t i = 0; i < ARRAY_COUNT(sFlagOverrides); i++) {
        const MlFlagOverride* o = &sFlagOverrides[i];
        int id = ActorDB_RetrieveId(o->name);
        ActorDBEntry* entry = id < 0 ? NULL : ActorDB_Retrieve(id);

        if (entry == NULL || !entry->valid) {
            snprintf(message, sizeof(message), "[MissingLink] no actor %s to override", o->name);
            api->Log(message);
            continue;
        }
        entry->flags = (entry->flags | o->set) & ~o->clear;
    }
    api->RegisterVB(VB_SPAWN_ACTOR_ENTRY, MlOverrides_SpawnEntry);
    api->RegisterVB(VB_SPAWN_ACTOR_WITHOUT_OBJECT, MlOverrides_SpawnWithoutObject);
}
