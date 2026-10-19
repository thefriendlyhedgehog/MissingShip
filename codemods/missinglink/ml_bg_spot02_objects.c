/*
 * The Missing Link's Bg_Spot02_Objects, which ML's Kakariko places (ledger row 60), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_Bg_Spot02_Objects),
 * registered through the mod API.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "align_asset_macro.h"
#include "overlays/actors/ovl_Bg_Spot02_Objects/z_bg_spot02_objects.h"
#include "objects/object_spot02_objects/object_spot02_objects.h"

#include <stdio.h>

#include "missinglink.h"

ML_HOST_FN_DECL(func_808AC8FC);
ML_HOST_FN_DECL(func_808ACAFC);

// vanilla's, from z_bg_spot02_objects.c
void BgSpot02Objects_Destroy(Actor* thisx, PlayState* play);
void BgSpot02Objects_Update(Actor* thisx, PlayState* play);
void func_808AC8FC(BgSpot02Objects* this, PlayState* play);
void func_808AC908(BgSpot02Objects* this, PlayState* play);
void func_808ACAFC(BgSpot02Objects* this, PlayState* play);

// ML refilled object 0xA1 with a small model of its own; its layer ships it under this path
#define dgMlSpot02DL "__OTR__objects/object_ml_spot02/gMlSpot02DL"
static const ALIGN_ASSET(2) char gMlSpot02DL[] = dgMlSpot02DL;
#define dgMlSpot02Col "__OTR__objects/object_ml_spot02/gMlSpot02Col"
static const ALIGN_ASSET(2) char gMlSpot02Col[] = dgMlSpot02Col;

static void MlBgSpot02Objects_WaitSwitch(BgSpot02Objects* this, PlayState* play);

static InitChainEntry sInitChain[] = {
    ICHAIN_VEC3F_DIV1000(scale, 100, ICHAIN_STOP),
};

// ML hand-patched vanilla's Init. It keeps params whole (vanilla splits a switch flag off the high byte) and always
// runs the setup of types 0-2: 0 and 1 as vanilla, anything else with ML's collision. A non-zero high byte skips the
// collision. ML then stores 0x80 in colChkInfo.health and reads switch flag params & 0x1F: with the flag clear, a high
// byte of 0 sinks the object 200 units and zeroes health; with it set, a non-zero high byte removes it. ML's Kakariko
// places params 0x002, 0x202 and 0xF0F; vanilla's Init reads 0xF0F as a type it has no case for.
static void MlBgSpot02Objects_Init(Actor* thisx, PlayState* play) {
    BgSpot02Objects* this = (BgSpot02Objects*)thisx;
    CollisionHeader* colHeader = NULL;
    u8 high = (u16)thisx->params >> 8;

    DynaPolyActor_Init(&this->dyna, 0);
    Actor_ProcessInitChain(thisx, sInitChain);

    if (thisx->params == 0) {
        if (Flags_GetSwitch(play, this->unk_16B)) { // never set in ML, so switch flag 0
            ML_SET_HOST_FN(this->actionFunc, func_808AC8FC);
            thisx->world.pos.y += 255.0f;
        } else {
            ML_SET_HOST_FN(this->actionFunc, func_808ACAFC);
        }

        CollisionHeader_GetVirtual((void*)object_spot02_objects_Col_012BA4, &colHeader);
    } else if (thisx->params == 1) {
        ML_SET_HOST_FN(this->actionFunc, func_808AC8FC);
        CollisionHeader_GetVirtual((void*)object_spot02_objects_Col_0128D8, &colHeader);
        thisx->flags |= ACTOR_FLAG_IGNORE_POINTLIGHTS;
    } else {
        if (play->sceneNum == SCENE_GRAVEYARD) {
            this->actionFunc = func_808AC908;
        } else {
            ML_SET_HOST_FN(this->actionFunc, func_808AC8FC);
        }

        CollisionHeader_GetVirtual((void*)gMlSpot02Col, &colHeader);
    }

    if (high == 0) {
        this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, colHeader);

        if (((Flags_GetEventChkInf(EVENTCHKINF_DESTROYED_ROYAL_FAMILY_TOMB)) && (play->sceneNum == SCENE_GRAVEYARD) &&
             (thisx->params == 2)) ||
            (LINK_IS_ADULT && (thisx->params == 1))) {
            Actor_Kill(thisx);
        }
    }

    thisx->colChkInfo.health = 0x80;
    if (!Flags_GetSwitch(play, thisx->params & 0x1F)) {
        if (high == 0) {
            thisx->colChkInfo.health = 0;
            thisx->world.pos.y -= 200.0f;
        }
        // ML only reads the switch here, so its change shows after reloading the area. MissingShip's owner asked for
        // it to happen live (MissingShip MS-149): watch the switch and apply the same change when it is set.
        if (thisx->params >= 2) {
            this->actionFunc = MlBgSpot02Objects_WaitSwitch;
        }
    } else if (high != 0) {
        Actor_Kill(thisx);
    }
}

// Once the switch is set, a sunk object (high byte 0) rises back the 200 units Init took off, with vanilla's
// moving-wall sounds; any other kind is removed, as ML's Init would do on the next load.
static void MlBgSpot02Objects_WaitSwitch(BgSpot02Objects* this, PlayState* play) {
    Actor* thisx = &this->dyna.actor;

    if (!Flags_GetSwitch(play, thisx->params & 0x1F)) {
        return;
    }
    if (((u16)thisx->params >> 8) != 0) {
        Actor_Kill(thisx);
        return;
    }
    if (Math_StepToF(&thisx->world.pos.y, thisx->home.pos.y, 4.0f)) {
        Audio_PlayActorSound2(thisx, NA_SE_EV_STONEDOOR_STOP);
        thisx->colChkInfo.health = 0x80;
        ML_SET_HOST_FN(this->actionFunc, func_808AC8FC);
    } else {
        Actor_PlaySfx_Flagged(thisx, NA_SE_EV_WALL_MOVE_SP - SFX_FLAG);
    }
}

// ML replaced vanilla's Draw. A high byte of 0 draws ML's model and anything else draws nothing. (Other high bytes
// would scroll a texture in ML's object and draw its second display list, but only while (colChkInfo.health & 0x7F)
// is non-zero, and Init only ever stores 0x80 or 0, so that branch never draws and is left out.)
static void MlBgSpot02Objects_Draw(Actor* thisx, PlayState* play) {
    if (((u16)thisx->params >> 8) == 0) {
        Gfx_DrawDListOpa(play, (Gfx*)gMlSpot02DL);
    }
}

void MlBgSpot02Objects_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_Bg_Spot02_Objects";
    definition.description = "The Missing Link: Kakariko objects";
    definition.category = ACTORCAT_BG;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    definition.objectId = OBJECT_SPOT02_OBJECTS;
    definition.instanceSize = sizeof(BgSpot02Objects);
    definition.init = MlBgSpot02Objects_Init;
    definition.destroy = BgSpot02Objects_Destroy;
    definition.update = BgSpot02Objects_Update;
    definition.draw = MlBgSpot02Objects_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_Bg_Spot02_Objects is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
