/*
 * The Missing Link's Hollow Root Keep elevators (ledger row 71), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_Elevator),
 * registered through the mod API.
 *
 * Source overlay: ML dma 290, text 0x390, on ML's own object 0x154 (dma 380). params 0: rises 309 units at 10 per
 * frame (placed twice in room 16). params 1: rises 1784 units at 45 per frame (spawned by ML_ElevatorSpawner).
 * Standing on it sends it to the other end, where it waits until Link steps off. It comes back down when Link is
 * below its top, and the tall one also drops while a Magic Hourglass rewind plays with Link within 100 units.
 * While the tall one carries Link, the camera switches to setting 9.
 * ML's Update is branchy compiler output; the flow below follows it label by label.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "align_asset_macro.h"

#include <stdio.h>

#include "missinglink.h"

typedef struct MlElevator {
    /* 0x000 */ DynaPolyActor dyna;
    /* 0x168 */ f32 topY;
    /* 0x16C */ f32 bottomY;
    /* 0x170 */ s32 state; // 0 idle, 1 rising, -1 lowering, 2 arrived (until Link steps off)
    /* 0x174 */ s32 dir;   // direction of the next trip: 0 before the first, then 1 (up) or -1 (down)
} MlElevator;

#define dgMlElevatorCol "__OTR__objects/object_ml_elevator/gMlElevatorCol"
static const ALIGN_ASSET(2) char gMlElevatorCol[] = dgMlElevatorCol;
#define dgMlElevatorDL "__OTR__objects/object_ml_elevator/gMlElevatorDL"
static const ALIGN_ASSET(2) char gMlElevatorDL[] = dgMlElevatorDL;
#define dgMlElevatorTallDL "__OTR__objects/object_ml_elevator/gMlElevatorTallDL"
static const ALIGN_ASSET(2) char gMlElevatorTallDL[] = dgMlElevatorTallDL;

static InitChainEntry sInitChain[] = {
    ICHAIN_VEC3F_DIV1000(scale, 1000, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneForward, 3000, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneScale, 500, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneDownward, 1000, ICHAIN_STOP),
};

static void MlElevator_Init(Actor* thisx, PlayState* play) {
    MlElevator* this = (MlElevator*)thisx;
    CollisionHeader* colHeader = NULL;

    Actor_ProcessInitChain(thisx, sInitChain);
    DynaPolyActor_Init(&this->dyna, DPM_PLAYER);
    CollisionHeader_GetVirtual((void*)gMlElevatorCol, &colHeader);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, colHeader);
    this->state = 0;
    this->dir = 0;
    this->bottomY = thisx->world.pos.y;
    this->topY = thisx->world.pos.y + ((thisx->params != 0) ? 1784.0f : 309.0f);
}

static void MlElevator_Destroy(Actor* thisx, PlayState* play) {
    MlElevator* this = (MlElevator*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
}

static void MlElevator_Update(Actor* thisx, PlayState* play) {
    MlElevator* this = (MlElevator*)thisx;
    Player* player = GET_PLAYER(play);
    s32 onTop = DynaPolyActor_IsPlayerOnTop(&this->dyna);
    f32 speed = (thisx->params != 0) ? 45.0f : 10.0f;
    f32* y = &thisx->world.pos.y;

    if (onTop || ((this->bottomY < *y) && (player->actor.world.pos.y < this->topY - 50.0f))) {
        if (this->state == 0) {
            this->state = (this->dir == 0) ? 1 : this->dir;
        }
        if (this->state == 1) {
            goto rising;
        }
        goto lowering;
    }
    if (this->state == 2) {
        this->state = 0;
        goto recall;
    }
    if (this->state == 1) {
        goto recall;
    }

lowering:
    if (this->state != -1) {
        goto recall;
    }
lower:
    Audio_PlayActorSound2(thisx, NA_SE_EV_BUYOSTAND_FALL - SFX_FLAG);
    *y -= speed;
    if (*y <= this->bottomY) {
        this->dir = 1;
        *y = this->bottomY;
        this->state = 2;
        return;
    }
    goto tail;

rising:
    if (!onTop) {
        goto recall;
    }
    *y += speed;
    Audio_PlayActorSound2(thisx, NA_SE_EV_BUYOSTAND_RISING - SFX_FLAG);
    if (this->topY <= *y) {
        this->dir = -1;
        *y = this->topY;
        this->state = 2;
        return;
    }
    goto tail;

recall:
    // Called back down: Link is below its top, or (tall one) an Hourglass rewind plays nearby
    if ((this->bottomY < *y) && (player->actor.world.pos.y < this->topY - 50.0f)) {
        goto lower;
    }
    if ((MlHourglass_RewindStep() != 0) && (thisx->params != 0) && (this->bottomY < *y) &&
        (thisx->xzDistToPlayer < 100.0f)) {
        goto lower;
    }

tail:
    if (((this->state == 1) || (this->state == -1)) && onTop && (thisx->params == 1)) {
        Camera_ChangeDataIdx(&play->mainCamera, 9);
    }
}

static void MlElevator_Draw(Actor* thisx, PlayState* play) {
    Gfx_DrawDListOpa(play, (Gfx*)((thisx->params != 0) ? gMlElevatorTallDL : gMlElevatorDL));
}

void MlElevator_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_Elevator";
    definition.description = "The Missing Link: elevator";
    definition.category = ACTORCAT_BG;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = OBJECT_KA;
    definition.instanceSize = sizeof(MlElevator);
    definition.init = MlElevator_Init;
    definition.destroy = MlElevator_Destroy;
    definition.update = MlElevator_Update;
    definition.draw = MlElevator_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_Elevator is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
