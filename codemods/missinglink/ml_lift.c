/*
 * The Missing Link's switch-operated lift block (ledger row 12), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_Lift),
 * registered through the mod API.
 *
 * ML replaces vanilla's collapsing platform wholesale: new code (624 vs 2256 bytes) and a new model in
 * the same object slot (0x11D, 2080 vs 6144 bytes). This file implements ML's version under vanilla's
 * actor id (the actor table points slot 0x12C here), leaving vanilla's z_obj_lift.c untouched.
 *
 * Behaviour: a solid block that rests at its placed height ("up") or 160 units lower ("down"). Switch
 * flag params & 0xFF selects which; a nonzero params high byte inverts that. When the switch changes it
 * glides to the other height at 20 units/frame, with a metal-door sound as it starts.
 * Source overlay: ML dma for actor 0x12C; model from ML object dma 75 (pipeline/xml/objects/ml_lift.xml).
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "align_asset_macro.h"

#include <stdio.h>

#include "missinglink.h"

typedef struct MlLift {
    /* 0x0000 */ DynaPolyActor dyna;
    /* 0x0164 */ s8 state;
    /* 0x0168 */ f32 upY;
    /* 0x016C */ f32 downY;
} MlLift; // ML instanceSize = 0x1A0 (state/upY/downY at 0x180/0x184/0x188)

#define dgMlLiftCol "__OTR__objects/object_ml_lift/gMlLiftCol"
static const ALIGN_ASSET(2) char gMlLiftCol[] = dgMlLiftCol;
#define dgMlLiftDL "__OTR__objects/object_ml_lift/gMlLiftDL"
static const ALIGN_ASSET(2) char gMlLiftDL[] = dgMlLiftDL;

typedef enum {
    /* 0 */ ML_LIFT_IDLE,
    /* 1 */ ML_LIFT_RISING,
    /* 2 */ ML_LIFT_LOWERING,
} MlLiftState;

static InitChainEntry sInitChain[] = {
    ICHAIN_VEC3F_DIV1000(scale, 100, ICHAIN_STOP),
};

static s32 MlLift_WantsDown(MlLift* this, PlayState* play) {
    s32 switchSet = Flags_GetSwitch(play, this->dyna.actor.params & 0xFF);

    return ((this->dyna.actor.params >> 8) != 0) ? !switchSet : switchSet;
}

static void MlLift_Init(Actor* thisx, PlayState* play) {
    MlLift* this = (MlLift*)thisx;
    CollisionHeader* colHeader = NULL;

    Actor_ProcessInitChain(thisx, sInitChain);
    DynaPolyActor_Init(&this->dyna, DPM_UNK);
    CollisionHeader_GetVirtual((void*)gMlLiftCol, &colHeader);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, colHeader);

    this->upY = thisx->world.pos.y;
    this->downY = thisx->world.pos.y - 160.0f;
    if (MlLift_WantsDown(this, play)) {
        thisx->world.pos.y = this->downY;
    }
}

static void MlLift_Destroy(Actor* thisx, PlayState* play) {
    MlLift* this = (MlLift*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
}

static void MlLift_Update(Actor* thisx, PlayState* play) {
    MlLift* this = (MlLift*)thisx;

    switch (this->state) {
        case ML_LIFT_RISING:
            if (Math_StepToF(&thisx->world.pos.y, this->upY, 20.0f)) {
                this->state = ML_LIFT_IDLE;
            }
            break;
        case ML_LIFT_LOWERING:
            if (Math_StepToF(&thisx->world.pos.y, this->downY, 20.0f)) {
                this->state = ML_LIFT_IDLE;
            }
            break;
        default:
            if (MlLift_WantsDown(this, play)) {
                if (thisx->world.pos.y != this->downY) {
                    this->state = ML_LIFT_LOWERING;
                    SoundSource_PlaySfxAtFixedWorldPos(play, &thisx->world.pos, 30, NA_SE_EV_METALDOOR_OPEN);
                }
            } else if (thisx->world.pos.y != this->upY) {
                this->state = ML_LIFT_RISING;
                SoundSource_PlaySfxAtFixedWorldPos(play, &thisx->world.pos, 30, NA_SE_EV_METALDOOR_CLOSE);
            }
            break;
    }
}

static void MlLift_Draw(Actor* thisx, PlayState* play) {
    Gfx_DrawDListOpa(play, (Gfx*)gMlLiftDL);
}

void MlLift_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_Lift";
    definition.description = "The Missing Link: lift block";
    definition.category = ACTORCAT_PROP;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = OBJECT_D_LIFT;
    definition.instanceSize = sizeof(MlLift);
    definition.init = MlLift_Init;
    definition.destroy = MlLift_Destroy;
    definition.update = MlLift_Update;
    definition.draw = MlLift_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_Lift is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
