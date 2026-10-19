/*
 * The Missing Link's switch-driven sliding block (ledger row 68), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_SlideBlock),
 * registered through the mod API.
 *
 * Source overlay: ML dma 234, text 0x490, hand-written. Placed 26 times in HAKAdan, all on room switch 0x20.
 * params: low byte = switch flag. High byte: bit 0 inverts the switch (and draws teal instead of orange),
 * bit 1 picks the second model and collision, bit 4 the third/fourth model, bit 5 scale 1.0 instead of 0.5.
 * When the switch (xor the invert bit) is set the block slides 10 frames of 20 units x scale along its own down
 * axis; when it clears, it slides back. It starts out already moved if the switch is set on load.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "align_asset_macro.h"

#include <stdio.h>

#include "missinglink.h"

typedef struct MlSlideBlock {
    /* 0x000 */ DynaPolyActor dyna;
    /* 0x180 */ s8 state;   // 0 idle, 1 sliding back, 2 sliding out
    /* 0x181 */ s8 timer;
    /* 0x182 */ s8 isOut;
    /* 0x18C */ Vec3f step; // one frame of travel along the block's own down axis
} MlSlideBlock;

#define dgMlSlideBlock0Col "__OTR__objects/object_ml_slideblock/gMlSlideBlock0Col"
static const ALIGN_ASSET(2) char gMlSlideBlock0Col[] = dgMlSlideBlock0Col;
#define dgMlSlideBlock0DL "__OTR__objects/object_ml_slideblock/gMlSlideBlock0DL"
static const ALIGN_ASSET(2) char gMlSlideBlock0DL[] = dgMlSlideBlock0DL;
#define dgMlSlideBlock1Col "__OTR__objects/object_ml_slideblock/gMlSlideBlock1Col"
static const ALIGN_ASSET(2) char gMlSlideBlock1Col[] = dgMlSlideBlock1Col;
#define dgMlSlideBlock1DL "__OTR__objects/object_ml_slideblock/gMlSlideBlock1DL"
static const ALIGN_ASSET(2) char gMlSlideBlock1DL[] = dgMlSlideBlock1DL;
#define dgMlSlideBlock2DL "__OTR__objects/object_ml_slideblock/gMlSlideBlock2DL"
static const ALIGN_ASSET(2) char gMlSlideBlock2DL[] = dgMlSlideBlock2DL;
#define dgMlSlideBlock3DL "__OTR__objects/object_ml_slideblock/gMlSlideBlock3DL"
static const ALIGN_ASSET(2) char gMlSlideBlock3DL[] = dgMlSlideBlock3DL;
#define dgMlSlideBlock4DL "__OTR__objects/object_ml_slideblock/gMlSlideBlock4DL"
static const ALIGN_ASSET(2) char gMlSlideBlock4DL[] = dgMlSlideBlock4DL;
#define dgMlSlideBlock5DL "__OTR__objects/object_ml_slideblock/gMlSlideBlock5DL"
static const ALIGN_ASSET(2) char gMlSlideBlock5DL[] = dgMlSlideBlock5DL;
#define dgMlSlideBlock6DL "__OTR__objects/object_ml_slideblock/gMlSlideBlock6DL"
static const ALIGN_ASSET(2) char gMlSlideBlock6DL[] = dgMlSlideBlock6DL;
#define dgMlSlideBlock7DL "__OTR__objects/object_ml_slideblock/gMlSlideBlock7DL"
static const ALIGN_ASSET(2) char gMlSlideBlock7DL[] = dgMlSlideBlock7DL;

#define ML_HB(thisx) (((thisx)->params >> 8) & 0xFF)

static InitChainEntry sInitChain[] = {
    ICHAIN_VEC3F_DIV1000(scale, 100, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneForward, 12800, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneScale, 1300, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneDownward, 1300, ICHAIN_STOP),
};

// Each ML display list sets the primitive colour, then branches to one of four models.
static Gfx* sDLists[] = {
    (Gfx*)gMlSlideBlock0DL, (Gfx*)gMlSlideBlock1DL, (Gfx*)gMlSlideBlock2DL, (Gfx*)gMlSlideBlock3DL,
    (Gfx*)gMlSlideBlock4DL, (Gfx*)gMlSlideBlock5DL, (Gfx*)gMlSlideBlock6DL, (Gfx*)gMlSlideBlock7DL,
};

static s32 MlSlideBlock_SwitchOn(MlSlideBlock* this, PlayState* play) {
    s32 flag = Flags_GetSwitch(play, this->dyna.actor.params & 0xFF);

    return (ML_HB(&this->dyna.actor) & 1) ? !flag : flag;
}

static void MlSlideBlock_Init(Actor* thisx, PlayState* play) {
    MlSlideBlock* this = (MlSlideBlock*)thisx;
    s32 hb = ML_HB(thisx);
    CollisionHeader* colHeader = NULL;
    f32 side;
    f32 back;

    Actor_ProcessInitChain(thisx, sInitChain);
    DynaPolyActor_Init(&this->dyna, DPM_PLAYER);
    Actor_SetScale(thisx, (hb & 0x20) ? 1.0f : 0.5f);
    CollisionHeader_GetVirtual((void*)((hb & 2) ? gMlSlideBlock1Col : gMlSlideBlock0Col), &colHeader);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, colHeader);

    // The block's local down axis, 20 units long at scale 1.0
    side = Math_SinS(thisx->shape.rot.z) * 20.0f;
    back = Math_SinS(thisx->shape.rot.x) * 20.0f;
    this->step.x = (side * Math_SinS(thisx->shape.rot.y + 0x3FFF) + back * Math_SinS(thisx->shape.rot.y + 0x7FFF)) *
                   thisx->scale.x;
    this->step.y = Math_CosS(thisx->shape.rot.z) * (-20.0f * thisx->scale.x) * Math_CosS(thisx->shape.rot.x);
    this->step.z = (side * Math_CosS(thisx->shape.rot.y + 0x3FFF) + back * Math_CosS(thisx->shape.rot.y + 0x7FFF)) *
                   thisx->scale.x;

    if (MlSlideBlock_SwitchOn(this, play)) {
        thisx->world.pos.x += this->step.x * 10.0f;
        thisx->world.pos.y += this->step.y * 10.0f;
        thisx->world.pos.z += this->step.z * 10.0f;
        this->isOut = true;
    }
}

static void MlSlideBlock_Destroy(Actor* thisx, PlayState* play) {
    MlSlideBlock* this = (MlSlideBlock*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
}

static void MlSlideBlock_Update(Actor* thisx, PlayState* play) {
    MlSlideBlock* this = (MlSlideBlock*)thisx;

    if (this->state == 1 || this->state == 2) {
        f32 dir = (this->state == 2) ? 1.0f : -1.0f;

        thisx->world.pos.x += this->step.x * dir;
        thisx->world.pos.y += this->step.y * dir;
        thisx->world.pos.z += this->step.z * dir;
        if (--this->timer == 0) {
            this->isOut = (this->state == 2);
            this->state = 0;
        }
    } else if (MlSlideBlock_SwitchOn(this, play)) {
        if (!this->isOut) {
            this->state = 2;
            this->timer = 10;
            SoundSource_PlaySfxAtFixedWorldPos(play, &thisx->world.pos, 30, NA_SE_EV_METALDOOR_OPEN);
        }
    } else if (this->isOut) {
        this->state = 1;
        this->timer = 10;
        SoundSource_PlaySfxAtFixedWorldPos(play, &thisx->world.pos, 30, NA_SE_EV_METALDOOR_CLOSE);
    }
}

static void MlSlideBlock_Draw(Actor* thisx, PlayState* play) {
    s32 hb = ML_HB(thisx);

    Gfx_DrawDListOpa(play, sDLists[(hb & 1) * 2 + ((hb >> 1) & 1) + ((hb >> 4) & 1) * 4]);
}

void MlSlideBlock_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_SlideBlock";
    definition.description = "The Missing Link: sliding block";
    definition.category = ACTORCAT_BG;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = OBJECT_TW;
    definition.instanceSize = sizeof(MlSlideBlock);
    definition.init = MlSlideBlock_Init;
    definition.destroy = MlSlideBlock_Destroy;
    definition.update = MlSlideBlock_Update;
    definition.draw = MlSlideBlock_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_SlideBlock is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
