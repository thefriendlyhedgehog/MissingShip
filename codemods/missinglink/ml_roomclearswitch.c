/*
 * The Missing Link's room-clear switch (ledger row 69), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_RoomClearSwitch),
 * registered through the mod API.
 *
 * Source overlay: ML dma 164, text 0x260 (ML's own code in vanilla's unset slot 0x1A). Placed once, in HAKAdan
 * room 20. After 34 frames it sets switch 0x25 every frame until the room is cleared, then unsets it and removes
 * itself. It removes itself at once if the room was already cleared. Nothing is drawn.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"

#include <stdio.h>

#include "missinglink.h"

typedef struct MlRoomClearSwitch {
    /* 0x000 */ Actor actor;
    /* 0x14C */ s32 timer;
} MlRoomClearSwitch; // size = 0x150 (ML allocates 0x154)

#define ML_ROOM_CLEAR_SWITCH 0x25

static void MlRoomClearSwitch_Init(Actor* thisx, PlayState* play) {
    if (Flags_GetClear(play, thisx->room)) {
        Actor_Kill(thisx);
    }
}

static void MlRoomClearSwitch_Destroy(Actor* thisx, PlayState* play) {
}

static void MlRoomClearSwitch_Update(Actor* thisx, PlayState* play) {
    MlRoomClearSwitch* this = (MlRoomClearSwitch*)thisx;

    if (this->timer < 34) {
        this->timer++;
    } else if (Flags_GetClear(play, thisx->room)) {
        Flags_UnsetSwitch(play, ML_ROOM_CLEAR_SWITCH);
        Actor_Kill(thisx);
    } else {
        Flags_SetSwitch(play, ML_ROOM_CLEAR_SWITCH);
    }
}

void MlRoomClearSwitch_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_RoomClearSwitch";
    definition.description = "The Missing Link: room-clear switch";
    definition.category = ACTORCAT_BG;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    definition.objectId = OBJECT_GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(MlRoomClearSwitch);
    definition.init = MlRoomClearSwitch_Init;
    definition.destroy = MlRoomClearSwitch_Destroy;
    definition.update = MlRoomClearSwitch_Update;
    definition.draw = NULL;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_RoomClearSwitch is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
