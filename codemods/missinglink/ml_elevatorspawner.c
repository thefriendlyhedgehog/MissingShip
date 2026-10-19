/*
 * The Missing Link's spawner of HAKAdan's tall elevator (ledger row 70), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_ElevatorSpawner),
 * registered through the mod API.
 *
 * Source overlay: ML dma 184, text 0x100. Placed once, in HAKAdan room 19. Init looks for an ML_Elevator among the
 * BG actors; if there is none it spawns the tall one (params 1) at (-1154, -1685, -627), yaw 0x42D8, with room -1 so
 * it survives room changes. Either way it then removes itself. ML finds and spawns the elevator by its slot,
 * En_Kakasi's; here ML_Elevator is found and spawned by name.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ActorDB.h"

#include <stdio.h>

#include "missinglink.h"

typedef struct MlElevatorSpawner {
    /* 0x000 */ Actor actor;
    /* 0x14C */ u8 found;
} MlElevatorSpawner; // size = 0x150

static void MlElevatorSpawner_Init(Actor* thisx, PlayState* play) {
    MlElevatorSpawner* this = (MlElevatorSpawner*)thisx;
    s16 elevator = ActorDB_RetrieveId("ML_Elevator");
    Actor* actor;

    for (actor = play->actorCtx.actorLists[ACTORCAT_BG].head; actor != NULL; actor = actor->next) {
        if (actor->id == elevator) {
            this->found = true;
        }
    }
    if (!this->found) {
        actor = Actor_Spawn(&play->actorCtx, play, elevator, -1154.0f, -1685.0f, -627.0f, 0, 0x42D8, 0, 1);
        if (actor != NULL) { // ML doesn't check
            actor->room = -1;
        }
    }
    Actor_Kill(thisx);
}

static void MlElevatorSpawner_Destroy(Actor* thisx, PlayState* play) {
}

static void MlElevatorSpawner_Update(Actor* thisx, PlayState* play) {
}

void MlElevatorSpawner_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_ElevatorSpawner";
    definition.description = "The Missing Link: elevator spawner";
    definition.category = ACTORCAT_BG;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = OBJECT_GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(MlElevatorSpawner);
    definition.init = MlElevatorSpawner_Init;
    definition.destroy = MlElevatorSpawner_Destroy;
    definition.update = MlElevatorSpawner_Update;
    definition.draw = NULL;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_ElevatorSpawner is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
