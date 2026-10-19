/*
 * The Missing Link's Shadow Temple boss's projectile, as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_BossVaShot), registered through the mod API.
 *
 * Description: The Missing Link's Shadow Temple boss's projectile (ML actor 0x017)
 *
 * Source overlay: ML dma 109, text 0x310, on ML's object 0x8C. Vanilla leaves slot 0x17 unset. Spawned by
 * ML_BossVa 160 units in front of it. Flies 25 units a frame along its yaw (pitch world.rot.x, always 0), spinning,
 * and turns up to 0x80 a frame toward Link. Gone after 21 frames or when it touches a wall, floor, Link or a weapon.
 * Ported from MissingShip's SoH fork (ovl_Ml_BossVaShot).
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "align_asset_macro.h"

#include <stdio.h>

#include "missinglink.h"

// ML's layout (size 0x1A0), for reference against its overlay; SoH's own offsets differ.
typedef struct MlBossVaShot {
    /* 0x000 */ Actor actor;
    /* 0x14C */ ColliderCylinder collider;
    /* 0x198 */ s32 timer;
} MlBossVaShot;

#define dgMlBossVaShotDL "__OTR__objects/object_ml_bv/gMlBossVaShotDL"
static const ALIGN_ASSET(2) char gMlBossVaShotDL[] = dgMlBossVaShotDL;

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_2,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0xFFCFFFFF, 0x00, 0x08 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_WOOD,
        BUMP_ON,
        OCELEM_ON,
    },
    { 13, 13, 0, { 0, 0, 0 } },
};

static void MlBossVaShot_Init(Actor* thisx, PlayState* play) {
    MlBossVaShot* this = (MlBossVaShot*)thisx;

    Actor_SetScale(&this->actor, 0.01f);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    Collider_UpdateCylinder(&this->actor, &this->collider);
}

static void MlBossVaShot_Destroy(Actor* thisx, PlayState* play) {
    MlBossVaShot* this = (MlBossVaShot*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static void MlBossVaShot_Update(Actor* thisx, PlayState* play) {
    MlBossVaShot* this = (MlBossVaShot*)thisx;
    f32 cosPitch;

    // ML keeps running the rest of this frame after the kill, as here.
    if ((this->timer > 20) || (this->actor.bgCheckFlags & (1 | 8)) ||
        (this->collider.base.atFlags & AT_HIT) || (this->collider.base.acFlags & AC_HIT)) {
        Actor_Kill(&this->actor);
    }
    this->timer++;
    Actor_UpdateBgCheckInfo(play, &this->actor, 0.0f, 20.0f, 20.0f,
                            5);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    this->actor.flags |= ACTOR_FLAG_SFX_FOR_PLAYER_BODY_HIT;
    CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);

    cosPitch = Math_CosS(this->actor.world.rot.x);
    this->actor.world.pos.x += cosPitch * 25.0f * Math_SinS(this->actor.shape.rot.y);
    this->actor.world.pos.z += cosPitch * 25.0f * Math_CosS(this->actor.shape.rot.y);
    this->actor.world.pos.y += Math_SinS(this->actor.world.rot.x) * 25.0f;
    this->actor.shape.rot.x += 0x800;
    this->actor.shape.rot.z += 0xB00;
    Math_ScaledStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0x80);
}

static void MlBossVaShot_Draw(Actor* thisx, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gMlBossVaShotDL);
    CLOSE_DISPS(play->state.gfxCtx);
}

void MlBossVaShot_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_BossVaShot";
    definition.description = "The Missing Link: Shadow Temple boss's projectile";
    definition.category = ACTORCAT_PROP;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = OBJECT_BV;
    definition.instanceSize = sizeof(MlBossVaShot);
    definition.init = MlBossVaShot_Init;
    definition.destroy = MlBossVaShot_Destroy;
    definition.update = MlBossVaShot_Update;
    definition.draw = MlBossVaShot_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_BossVaShot is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
