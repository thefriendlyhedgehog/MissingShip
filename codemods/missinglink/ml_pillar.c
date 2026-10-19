/*
 * The Missing Link's switch-raised pillars in Bmori1 (ledger row 86), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_Pillar), registered through the mod API.
 *
 * Switch-raised stone pillars in Bmori1 (ML's rewrite of En_Du, slot 0x098)
 *
 * Source overlay: ML dma 217, text 0x2E0, on ML's own object 0x9D (dma 30, shipped as object_ml_du). Placed three
 * times in Bmori1 room 0. params 0 (two of them): model 0x840, waits 100 frames after switch 7, then rises 100 units
 * at 20 a frame. params 1: model 0x720, on switch 1 holds a manual cutscene on camera setting 2, waits 60 frames, then
 * rises 117 units at 15 a frame and ends the cutscene. A pillar whose switch is already set starts raised.
 *
 * Ported from MissingShip's SoH fork (ovl_Ml_Pillar, ledger row 86).
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "align_asset_macro.h"

#include <stdio.h>

#include "missinglink.h"

// ML's layout (size 0x178), for reference against its overlay; SoH's own offsets differ.
typedef struct MlPillar {
    /* 0x000 */ DynaPolyActor dyna;
    /* 0x164 */ s32 switchFlag;
    /* 0x168 */ s32 switchSet;
    /* 0x16C */ s32 raised;
    /* 0x170 */ s32 timer;
    /* 0x174 */ f32 topY;
} MlPillar;

#define dgMlPillarCol "__OTR__objects/object_ml_du/gMlPillarCol"
static const ALIGN_ASSET(2) char gMlPillarCol[] = dgMlPillarCol;
#define dgMlPillarTallDL "__OTR__objects/object_ml_du/gMlPillarTallDL"
static const ALIGN_ASSET(2) char gMlPillarTallDL[] = dgMlPillarTallDL;
#define dgMlPillarDL "__OTR__objects/object_ml_du/gMlPillarDL"
static const ALIGN_ASSET(2) char gMlPillarDL[] = dgMlPillarDL;

static InitChainEntry sInitChain[] = {
    ICHAIN_F32(uncullZoneForward, 2000, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneScale, 600, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneDownward, 2000, ICHAIN_STOP),
};

static void MlPillar_Init(Actor* thisx, PlayState* play) {
    MlPillar* this = (MlPillar*)thisx;
    CollisionHeader* colHeader = NULL;

    Actor_SetScale(&this->dyna.actor, 0.1f);
    Actor_ProcessInitChain(&this->dyna.actor, sInitChain);
    DynaPolyActor_Init(&this->dyna, DPM_PLAYER);
    CollisionHeader_GetVirtual((void*)gMlPillarCol, &colHeader);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, &this->dyna.actor, colHeader);

    if (this->dyna.actor.params == 0) {
        this->topY = this->dyna.actor.world.pos.y + 100.0f;
        this->switchFlag = 7;
    } else {
        this->topY = this->dyna.actor.world.pos.y + 117.0f;
        this->switchFlag = 1;
    }
    this->switchSet = Flags_GetSwitch(play, this->switchFlag);
    if (this->switchSet) {
        this->dyna.actor.world.pos.y = this->topY;
        this->raised = true;
    }
}

static void MlPillar_Destroy(Actor* thisx, PlayState* play) {
    MlPillar* this = (MlPillar*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
}

static void MlPillar_Update(Actor* thisx, PlayState* play) {
    MlPillar* this = (MlPillar*)thisx;

    this->switchSet = Flags_GetSwitch(play, this->switchFlag);
    if (!this->switchSet || (this->raised == true)) {
        return;
    }

    if (this->dyna.actor.params != 0) {
        this->timer++;
        Camera_ChangeDataIdx(&play->mainCamera, 2);
        func_80064520(play, &play->csCtx);
        if (this->timer > 60) {
            this->dyna.actor.world.pos.y += 15.0f;
            Audio_PlayActorSound2(&this->dyna.actor, 0x205A);
            if (this->topY < this->dyna.actor.world.pos.y) {
                this->dyna.actor.world.pos.y = this->topY;
                func_80064534(play, &play->csCtx);
                this->raised = true;
            }
        }
        return;
    }

    this->timer++;
    if (this->timer > 100) {
        this->dyna.actor.world.pos.y += 20.0f;
        Audio_PlayActorSound2(&this->dyna.actor, 0x205A);
        if (this->topY < this->dyna.actor.world.pos.y) {
            this->dyna.actor.world.pos.y = this->topY;
            this->raised = true;
        }
    }
}

static void MlPillar_Draw(Actor* thisx, PlayState* play) {
    Gfx_DrawDListOpa(play, (Gfx*)((thisx->params == 0) ? gMlPillarDL : gMlPillarTallDL));
}

void MlPillar_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_Pillar";
    definition.description = "The Missing Link: Bmori1 pillar";
    definition.category = ACTORCAT_BG;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = OBJECT_DU;
    definition.instanceSize = sizeof(MlPillar);
    definition.init = MlPillar_Init;
    definition.destroy = MlPillar_Destroy;
    definition.update = MlPillar_Update;
    definition.draw = MlPillar_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_Pillar is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
