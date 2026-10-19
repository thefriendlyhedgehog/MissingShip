/*
 * The Missing Link's Hollow Root Keep room 2 lift (ledger row 72), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_HakaLift),
 * registered through the mod API.
 *
 * Source overlay: ML dma 206, text 0x280, on ML's own object 0x72 (dma 22). Standing on it sends it 675 units to
 * the other end at 17 per frame; it waits there until Link steps off, and it doesn't come back by itself. It
 * starts at the top when Link loads in above it. While Link stands on it the camera uses setting 9.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "align_asset_macro.h"

#include <stdio.h>

#include "missinglink.h"

typedef struct MlHakaLift {
    /* 0x000 */ DynaPolyActor dyna;
    /* 0x164 */ f32 topY;
    /* 0x168 */ f32 bottomY;
    /* 0x16C */ s32 state; // 0 idle, 1 rising, -1 lowering, 2 arrived (until Link steps off)
    /* 0x170 */ s32 dir;   // direction of the next trip: 0 before the first, then 1 (up) or -1 (down)
} MlHakaLift;

#define dgMlHakaLiftCol "__OTR__objects/object_ml_hakalift/gMlHakaLiftCol"
static const ALIGN_ASSET(2) char gMlHakaLiftCol[] = dgMlHakaLiftCol;
#define dgMlHakaLiftDL "__OTR__objects/object_ml_hakalift/gMlHakaLiftDL"
static const ALIGN_ASSET(2) char gMlHakaLiftDL[] = dgMlHakaLiftDL;

static InitChainEntry sInitChain[] = {
    ICHAIN_F32(uncullZoneForward, 2000, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneScale, 600, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneDownward, 2000, ICHAIN_STOP),
};

static void MlHakaLift_Init(Actor* thisx, PlayState* play) {
    MlHakaLift* this = (MlHakaLift*)thisx;
    CollisionHeader* colHeader = NULL;

    Actor_SetScale(thisx, 0.1f);
    Actor_ProcessInitChain(thisx, sInitChain);
    DynaPolyActor_Init(&this->dyna, DPM_PLAYER);
    CollisionHeader_GetVirtual((void*)gMlHakaLiftCol, &colHeader);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, colHeader);
    this->bottomY = thisx->world.pos.y;
    this->topY = thisx->world.pos.y + 675.0f;
    this->state = 0;
    this->dir = 0;
    if (this->topY - 50.0f < GET_PLAYER(play)->actor.world.pos.y) {
        thisx->world.pos.y = this->topY;
        this->dir = -1;
    }
}

static void MlHakaLift_Destroy(Actor* thisx, PlayState* play) {
    MlHakaLift* this = (MlHakaLift*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
}

static void MlHakaLift_Update(Actor* thisx, PlayState* play) {
    MlHakaLift* this = (MlHakaLift*)thisx;
    s32 onTop = DynaPolyActor_IsPlayerOnTop(&this->dyna);

    if (onTop) {
        if (this->state == 0) {
            this->state = (this->dir == 0) ? 1 : this->dir;
        }
    } else if (this->state == 2) {
        this->state = 0;
        return;
    }

    if (this->state == 1) {
        thisx->world.pos.y += 17.0f;
        Audio_PlayActorSound2(thisx, NA_SE_EV_ELEVATOR_MOVE - SFX_FLAG);
        if (this->topY <= thisx->world.pos.y) {
            this->dir = -1;
            thisx->world.pos.y = this->topY;
            this->state = 2;
        }
    } else if (this->state == -1) {
        thisx->world.pos.y -= 17.0f;
        Audio_PlayActorSound2(thisx, NA_SE_EV_ELEVATOR_MOVE - SFX_FLAG);
        if (thisx->world.pos.y <= this->bottomY) {
            thisx->world.pos.y = this->bottomY;
            this->dir = 1;
            this->state = 2;
        }
    }

    if (onTop) {
        Camera_ChangeDataIdx(&play->mainCamera, 9);
    }
}

static void MlHakaLift_Draw(Actor* thisx, PlayState* play) {
    Gfx_DrawDListOpa(play, (Gfx*)gMlHakaLiftDL);
}

void MlHakaLift_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_HakaLift";
    definition.description = "The Missing Link: Hollow Root Keep lift";
    definition.category = ACTORCAT_BG;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = OBJECT_MORI_OBJECTS;
    definition.instanceSize = sizeof(MlHakaLift);
    definition.init = MlHakaLift_Init;
    definition.destroy = MlHakaLift_Destroy;
    definition.update = MlHakaLift_Update;
    definition.draw = MlHakaLift_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_HakaLift is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
