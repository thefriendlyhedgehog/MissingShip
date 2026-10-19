/*
 * The Missing Link's music and sound trigger (ML's rewrite of Bg_Po_Event, ledger row 63), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_BgmTrigger), registered through the
 * mod API.
 * Source overlay: ML dma 213, text 0x170. Every frame, by params:
 *   0  silence the main BGM (sequence 0) until switch flag 7 of scene 3 is set (ML's Bmori1 event)
 *   1  once that flag is set, play Kakariko Village (Adult)
 *   2  play Inside the Deku Tree
 *   3  once, when Link is within 20 units and not above it: his child falling scream
 * Each music mode first checks the main BGM player's current sequence, so the command is sent only once.
 * Placed in spot01 (params 1, both rooms), hairal_niwa2 and night_shop.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"

#include <stdio.h>

#include "missinglink.h"

typedef struct MlBgmTrigger {
    Actor actor;
    u8 played;
} MlBgmTrigger; // ML instanceSize = 0x150

static void MlBgmTrigger_Init(Actor* thisx, PlayState* play) {
}

static void MlBgmTrigger_Destroy(Actor* thisx, PlayState* play) {
}

static void MlBgmTrigger_Update(Actor* thisx, PlayState* play) {
    MlBgmTrigger* this = (MlBgmTrigger*)thisx;
    u16 seqId = gAudioContext.seqPlayers[SEQ_PLAYER_BGM_MAIN].seqId;
    s32 eventDone = (gSaveContext.sceneFlags[SCENE_FOREST_TEMPLE].swch & (1 << 7)) != 0;
    Player* player;

    switch (thisx->params) {
        case 0:
            if ((seqId != 0) && !eventDone) {
                Audio_QueueSeqCmd(0);
            }
            break;
        case 1:
            if ((seqId != NA_BGM_KAKARIKO_ADULT) && eventDone) {
                Audio_QueueSeqCmd(NA_BGM_KAKARIKO_ADULT);
            }
            break;
        case 2:
            if (seqId != NA_BGM_INSIDE_DEKU_TREE) {
                Audio_QueueSeqCmd(NA_BGM_INSIDE_DEKU_TREE);
            }
            break;
        case 3:
            player = GET_PLAYER(play);
            if (!this->played && (thisx->xzDistToPlayer <= 20.0f) &&
                (player->actor.world.pos.y <= thisx->world.pos.y)) {
                Sfx_PlaySfxCentered(NA_SE_VO_LI_FALL_L_KID);
                this->played = true;
            }
            break;
    }
}

static void MlBgmTrigger_Draw(Actor* thisx, PlayState* play) {
}

void MlBgmTrigger_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_BgmTrigger";
    definition.description = "The Missing Link: music trigger";
    definition.category = ACTORCAT_PROP;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = OBJECT_GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(MlBgmTrigger);
    definition.init = MlBgmTrigger_Init;
    definition.destroy = MlBgmTrigger_Destroy;
    definition.update = MlBgmTrigger_Update;
    definition.draw = MlBgmTrigger_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_BgmTrigger is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
