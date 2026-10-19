/*
 * The Missing Link's Blade Beam Piece of Heart (ledger row 30), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_BeamHeart), registered through the mod API.
 *
 * A Piece of Heart revealed by the Blade Beam (ML's rewrite of En_Reeba, slot 0x1C)
 *
 * ML dma 166. Invisible. When a Blade Beam (actor 0x57, ACTORCAT_ITEMACTION) passes within 45 units it spawns a Piece
 * of Heart (En_Item00 params 0x106, collectible flag 1) at its position, which rises to world Y 176 at 8 units a
 * frame; then the spawner is done. Gone for good once collectible flag 1 is set.
 *
 * Ported from MissingShip's SoH fork (ovl_Ml_BeamHeart, ledger row 30).
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"


#include <stdio.h>

#include "missinglink.h"

typedef struct MlBeamHeart {
    Actor actor;
    Actor* heart; // the Piece of Heart it spawned
} MlBeamHeart;

static void MlBeamHeart_Init(Actor* thisx, PlayState* play) {
    if (Flags_GetCollectible(play, 1)) {
        Actor_Kill(thisx);
    }
}

static void MlBeamHeart_Destroy(Actor* thisx, PlayState* play) {
}

static void MlBeamHeart_Update(Actor* thisx, PlayState* play) {
    MlBeamHeart* this = (MlBeamHeart*)thisx;
    Actor* beam = Actor_FindNearby(play, thisx, ACTOR_EN_M_THUNDER, ACTORCAT_ITEMACTION, 45.0f);

    if (this->heart == NULL) {
        if (beam != NULL) {
            this->heart = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ITEM00, thisx->world.pos.x, thisx->world.pos.y,
                                      thisx->world.pos.z, 0, 0, 0, 0x106);
        }
        return;
    }
    // Not in ML: once the heart is collected it is freed, so stop before touching it again.
    if (Flags_GetCollectible(play, 1)) {
        Actor_Kill(thisx);
        return;
    }
    Math_StepToF(&this->heart->world.pos.y, 176.0f, 8.0f);
    if (this->heart->world.pos.y == 176.0f) {
        Actor_Kill(thisx);
    }
}

void MlBeamHeart_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_BeamHeart";
    definition.description = "The Missing Link: Blade Beam heart spawner";
    definition.category = ACTORCAT_PROP;
    definition.actorFlags = (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_UPDATE_CULLING_DISABLED);
    definition.objectId = OBJECT_GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(MlBeamHeart);
    definition.init = MlBeamHeart_Init;
    definition.destroy = MlBeamHeart_Destroy;
    definition.update = MlBeamHeart_Update;
    definition.draw = NULL;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_BeamHeart is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
