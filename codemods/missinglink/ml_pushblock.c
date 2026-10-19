/*
 * The Missing Link's Library push block (ledger row 91), as a code mod: its copy of Obj_Oshihiki (slot 0x17A, vanilla
 * Boss_Ganon2's).
 *
 * ML copied Obj_Oshihiki into Boss_Ganon2's slot and patched it by hand. It is placed once, in the Library
 * (kakariko3), with the readable note (En_Wonder_Talk params 7) on top. ML's changes: its own collision and a single
 * display list (object 0x153, shipped as object_ml_ganon2); the switch in params bits 8-15 no longer decides whether it
 * spawns but records that it was pushed to the far wall (set when a push stops against a wall; with it set the block
 * starts 80 units along -z); resting on a blue switch it moves only off a matching inverse switch; and every frame it
 * keeps note 7 at its own z. ML's copy also writes -80 into prevPos.z, a feet position and yawSin at Init; the first
 * two are unused here and Update recomputes yawSin, so those are left out. ML walks the prop list without a NULL check;
 * here the walk stops at the end of the list.
 *
 * MissingShip's Unbound branch runs Obj_Oshihiki's own functions with two hooks for it (its switch check and the push
 * stopping against a wall). Here it is vanilla's code (SoH 9.2.3) with both built in, its InitVars, Init and Draw left
 * out for ML's.
 */

#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "align_asset_macro.h"
#include "overlays/actors/ovl_Obj_Oshihiki/z_obj_oshihiki.h"
#include "overlays/actors/ovl_Obj_Switch/z_obj_switch.h"
#include "objects/gameplay_dangeon_keep/gameplay_dangeon_keep.h"
#include "soh/ResourceManagerHelpers.h"

#include <math.h>
#include <stdio.h>

#include "missinglink.h"

#define dgMlPushBlockCol "__OTR__objects/object_ml_ganon2/gMlPushBlockCol"
static const ALIGN_ASSET(2) char gMlPushBlockCol[] = dgMlPushBlockCol;
#define dgMlPushBlockDL "__OTR__objects/object_ml_ganon2/gMlPushBlockDL"
static const ALIGN_ASSET(2) char gMlPushBlockDL[] = dgMlPushBlockDL;

#define ML_PUSHBLOCK_SWITCH(this) (((this)->dyna.actor.params >> 8) & 0xFF)
#define ML_PUSHBLOCK_NOTE 7

static void MlPushBlock_SetupOnScene(ObjOshihiki* this, PlayState* play);
static void MlPushBlock_OnScene(ObjOshihiki* this, PlayState* play);
static void MlPushBlock_SetupOnActor(ObjOshihiki* this, PlayState* play);
static void MlPushBlock_OnActor(ObjOshihiki* this, PlayState* play);
static void MlPushBlock_SetupPush(ObjOshihiki* this, PlayState* play);
static void MlPushBlock_Push(ObjOshihiki* this, PlayState* play);
static void MlPushBlock_SetupFall(ObjOshihiki* this, PlayState* play);
static void MlPushBlock_Fall(ObjOshihiki* this, PlayState* play);

static f32 sScales[] = {
    (1 / 10.0f), (1 / 6.0f), (1 / 5.0f), (1 / 3.0f), (1 / 10.0f), (1 / 6.0f), (1 / 5.0f), (1 / 3.0f),
};

static Color_RGB8 sColors[][4] = {
    { { 110, 86, 40 }, { 110, 86, 40 }, { 110, 86, 40 }, { 110, 86, 40 } },         // deku tree
    { { 106, 120, 110 }, { 104, 80, 20 }, { 0, 0, 0 }, { 0, 0, 0 } },               // dodongos cavern
    { { 142, 99, 86 }, { 72, 118, 96 }, { 0, 0, 0 }, { 0, 0, 0 } },                 // forest temple
    { { 210, 150, 80 }, { 210, 170, 80 }, { 0, 0, 0 }, { 0, 0, 0 } },               // fire temple
    { { 102, 144, 182 }, { 176, 167, 100 }, { 100, 167, 100 }, { 117, 97, 96 } },   // water temple
    { { 232, 210, 176 }, { 232, 210, 176 }, { 232, 210, 176 }, { 232, 210, 176 } }, // spirit temple
    { { 135, 125, 95 }, { 135, 125, 95 }, { 135, 125, 95 }, { 135, 125, 95 } },     // shadow temple
    { { 255, 255, 255 }, { 255, 255, 255 }, { 255, 255, 255 }, { 255, 255, 255 } }, // ganons castle
    { { 232, 210, 176 }, { 232, 210, 176 }, { 232, 210, 176 }, { 232, 210, 176 } }, // Gerudo Training Ground
};

static s16 sScenes[] = {
    SCENE_DEKU_TREE,     SCENE_DODONGOS_CAVERN, SCENE_FOREST_TEMPLE,
    SCENE_FIRE_TEMPLE,   SCENE_WATER_TEMPLE,    SCENE_SPIRIT_TEMPLE,
    SCENE_SHADOW_TEMPLE, SCENE_GANONS_TOWER,    SCENE_GERUDO_TRAINING_GROUND,
};

static InitChainEntry sInitChain[] = {
    ICHAIN_F32(uncullZoneForward, 1800, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneScale, 500, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneDownward, 1500, ICHAIN_STOP),
};

// The vertices and center of the bottom face
static Vec3f sColCheckPoints[5] = {
    { 29.99f, 1.01f, -29.99f }, { -29.99f, 1.01f, -29.99f }, { -29.99f, 1.01f, 29.99f },
    { 29.99f, 1.01f, 29.99f },  { 0.0f, 1.01f, 0.0f },
};

static Vec2f sFaceVtx[] = {
    { -30.0f, 0.0f },
    { 30.0f, 0.0f },
    { -30.0f, 60.0f },
    { 30.0f, 60.0f },
};

static Vec2f sFaceDirection[] = {
    { 1.0f, 1.0f },
    { -1.0f, 1.0f },
    { 1.0f, -1.0f },
    { -1.0f, -1.0f },
};

static void MlPushBlock_InitDynapoly(ObjOshihiki* this, PlayState* play, CollisionHeader* collision, s32 moveFlag) {
    s32 pad;
    CollisionHeader* colHeader = NULL;
    s32 pad2;

    DynaPolyActor_Init(&this->dyna, moveFlag);
    CollisionHeader_GetVirtual(collision, &colHeader);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, &this->dyna.actor, colHeader);

}

static void MlPushBlock_RotateXZ(Vec3f* out, Vec3f* in, f32 sn, f32 cs) {
    out->x = (in->z * sn) + (in->x * cs);
    out->y = in->y;
    out->z = (in->z * cs) - (in->x * sn);
}

static s32 MlPushBlock_StrongEnough(ObjOshihiki* this) {
    s32 strength;

    if (this->cantMove) {
        return 0;
    }
    strength = Player_GetStrength();
    switch (this->dyna.actor.params & 0xF) {
        case PUSHBLOCK_SMALL_START_ON:
        case PUSHBLOCK_MEDIUM_START_ON:
        case PUSHBLOCK_SMALL_START_OFF:
        case PUSHBLOCK_MEDIUM_START_OFF:
            return 1;
            break;
        case PUSHBLOCK_LARGE_START_ON:
        case PUSHBLOCK_LARGE_START_OFF:
            return strength >= PLAYER_STR_BRACELET;
            break;
        case PUSHBLOCK_HUGE_START_ON:
        case PUSHBLOCK_HUGE_START_OFF:
            return strength >= PLAYER_STR_SILVER_G;
            break;
    }
    return 0;
}

static void MlPushBlock_ResetFloors(ObjOshihiki* this) {
    s32 i;

    for (i = 0; i < ARRAY_COUNT(this->floorBgIds); i++) {
        this->floorBgIds[i] = BGCHECK_SCENE;
    }
}

static ObjOshihiki* MlPushBlock_GetBlockUnder(ObjOshihiki* this, PlayState* play) {
    DynaPolyActor* dynaPolyActor;

    if ((this->floorBgIds[this->highestFloor] != BGCHECK_SCENE) &&
        (fabsf(this->dyna.actor.floorHeight - this->dyna.actor.world.pos.y) < 0.001f)) {
        dynaPolyActor = DynaPoly_GetActor(&play->colCtx, this->floorBgIds[this->highestFloor]);
        if ((dynaPolyActor != NULL) && (dynaPolyActor->actor.id == ACTOR_OBJ_OSHIHIKI)) {
            return (ObjOshihiki*)dynaPolyActor;
        }
    }
    return NULL;
}

static void MlPushBlock_UpdateInitPos(ObjOshihiki* this) {
    if (this->dyna.actor.home.pos.x < this->dyna.actor.world.pos.x) {
        while ((this->dyna.actor.world.pos.x - this->dyna.actor.home.pos.x) >= 20.0f) {
            this->dyna.actor.home.pos.x += 20.0f;
        }
    } else {
        while ((this->dyna.actor.home.pos.x - this->dyna.actor.world.pos.x) >= 20.0f) {
            this->dyna.actor.home.pos.x -= 20.0f;
        }
    }
    if (this->dyna.actor.home.pos.z < this->dyna.actor.world.pos.z) {
        while ((this->dyna.actor.world.pos.z - this->dyna.actor.home.pos.z) >= 20.0f) {
            this->dyna.actor.home.pos.z += 20.0f;
        }
    } else {
        while ((this->dyna.actor.home.pos.z - this->dyna.actor.world.pos.z) >= 20.0f) {
            this->dyna.actor.home.pos.z -= 20.0f;
        }
    }
}

// ML's: stuck on any blue switch, moving off an inverse one only when its flag is the block's
static s32 MlPushBlock_NoSwitchPress(ObjOshihiki* this, DynaPolyActor* dyna, PlayState* play) {
    s16 dynaSwitchFlag;

    if (dyna == NULL) {
        return 1;
    } else if (dyna->actor.id == ACTOR_OBJ_SWITCH) {
        dynaSwitchFlag = (dyna->actor.params >> 8) & 0x3F;
        switch (dyna->actor.params & 0x33) {
            case 0x30: // inverse blue switch
                return dynaSwitchFlag == ((this->dyna.actor.params >> 8) & 0x3F);
            case 0x20: // blue switch
                return 0;
            default:
                return 1;
        }
    }
    return 1;
}

static void MlPushBlock_SetScale(ObjOshihiki* this, PlayState* play) {
    Actor_SetScale(&this->dyna.actor, sScales[this->dyna.actor.params & 0xF]);
}

static void MlPushBlock_SetTexture(ObjOshihiki* this, PlayState* play) {
    switch (this->dyna.actor.params & 0xF) {
        case PUSHBLOCK_SMALL_START_ON:
        case PUSHBLOCK_MEDIUM_START_ON:
        case PUSHBLOCK_SMALL_START_OFF:
        case PUSHBLOCK_MEDIUM_START_OFF:
            this->texture = (void*)gPushBlockSilverTex;
            break;
        case PUSHBLOCK_LARGE_START_ON:
        case PUSHBLOCK_LARGE_START_OFF:
            this->texture = (void*)gPushBlockBaseTex;
            break;
        case PUSHBLOCK_HUGE_START_ON:
        case PUSHBLOCK_HUGE_START_OFF:
            this->texture = (void*)gPushBlockGrayTex;
            break;
    }
}

static void MlPushBlock_SetColor(ObjOshihiki* this, PlayState* play) {
    Color_RGB8* src;
    Color_RGB8* color = &this->color;
    s16 paramsColorIdx;
    s32 i;

    paramsColorIdx = (this->dyna.actor.params >> 6) & 3;

    for (i = 0; i < ARRAY_COUNT(sScenes); i++) {
        if (sScenes[i] == play->sceneNum) {
            break;
        }
    }

    if (i >= ARRAY_COUNT(sColors)) {
        color->r = color->g = color->b = 255;
    } else {
        src = &sColors[i][paramsColorIdx];
        color->r = src->r;
        color->g = src->g;
        color->b = src->b;
    }
}

static void MlPushBlock_Destroy(Actor* thisx, PlayState* play) {
    s32 pad;
    ObjOshihiki* this = (ObjOshihiki*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
}

static void MlPushBlock_SetFloors(ObjOshihiki* this, PlayState* play) {
    s32 i;

    for (i = 0; i < 5; i++) {
        Vec3f colCheckPoint;
        Vec3f colCheckOffset;
        CollisionPoly** floorPoly;
        s32* floorBgId;

        colCheckOffset.x = sColCheckPoints[i].x * (this->dyna.actor.scale.x * 10.0f);
        colCheckOffset.y = sColCheckPoints[i].y * (this->dyna.actor.scale.y * 10.0f);
        colCheckOffset.z = sColCheckPoints[i].z * (this->dyna.actor.scale.z * 10.0f);
        MlPushBlock_RotateXZ(&colCheckPoint, &colCheckOffset, this->yawSin, this->yawCos);
        colCheckPoint.x += this->dyna.actor.world.pos.x;
        colCheckPoint.y += this->dyna.actor.prevPos.y;
        colCheckPoint.z += this->dyna.actor.world.pos.z;

        floorPoly = &this->floorPolys[i];
        floorBgId = &this->floorBgIds[i];
        this->floorHeights[i] =
            BgCheck_EntityRaycastFloor6(&play->colCtx, floorPoly, floorBgId, &this->dyna.actor, &colCheckPoint, 0.0f);
    }
}

static s16 MlPushBlock_GetHighestFloor(ObjOshihiki* this) {
    s16 highestFloor = 0;
    s16 temp = 1;
    f32 phi_f0 = this->floorHeights[temp];

    if (phi_f0 > this->floorHeights[highestFloor]) {
        highestFloor = temp;
    } else if ((this->floorBgIds[temp] == BGCHECK_SCENE) && ((phi_f0 - this->floorHeights[highestFloor]) > -0.001f)) {
        highestFloor = temp;
    }
    if (this->floorHeights[temp + 1] > this->floorHeights[highestFloor]) {
        highestFloor = temp + 1;
    } else if ((this->floorBgIds[temp + 1] == BGCHECK_SCENE) &&
               ((this->floorHeights[temp + 1] - this->floorHeights[highestFloor]) > -0.001f)) {
        highestFloor = temp + 1;
    }
    if (this->floorHeights[temp + 2] > this->floorHeights[highestFloor]) {
        highestFloor = temp + 2;
    } else if ((this->floorBgIds[temp + 2] == BGCHECK_SCENE) &&
               ((this->floorHeights[temp + 2] - this->floorHeights[highestFloor]) > -0.001f)) {
        highestFloor = temp + 2;
    }
    if (this->floorHeights[temp + 3] > this->floorHeights[highestFloor]) {
        highestFloor = temp + 3;
    } else if ((this->floorBgIds[temp + 3] == BGCHECK_SCENE) &&
               ((this->floorHeights[temp + 3] - this->floorHeights[highestFloor]) > -0.001f)) {
        highestFloor = temp + 3;
    }
    return highestFloor;
}

static void MlPushBlock_SetGround(ObjOshihiki* this, PlayState* play) {
    MlPushBlock_ResetFloors(this);
    MlPushBlock_SetFloors(this, play);
    this->highestFloor = MlPushBlock_GetHighestFloor(this);
    this->dyna.actor.floorHeight = this->floorHeights[this->highestFloor];
}

static s32 MlPushBlock_CheckFloor(ObjOshihiki* this, PlayState* play) {
    MlPushBlock_SetGround(this, play);

    if ((this->dyna.actor.floorHeight - this->dyna.actor.world.pos.y) >= -0.001f) {
        this->dyna.actor.world.pos.y = this->dyna.actor.floorHeight;
        return 1;
    }

    return 0;
}

static s32 MlPushBlock_CheckGround(ObjOshihiki* this, PlayState* play) {
    if (this->dyna.actor.world.pos.y <= BGCHECK_Y_MIN + 10.0f) {
        Actor_Kill(&this->dyna.actor);
        return 0;
    }
    if ((this->dyna.actor.floorHeight - this->dyna.actor.world.pos.y) >= -0.001f) {
        this->dyna.actor.world.pos.y = this->dyna.actor.floorHeight;
        return 1;
    }
    return 0;
}

static s32 MlPushBlock_CheckWall(PlayState* play, s16 angle, f32 direction, ObjOshihiki* this) {
    f32 maxDist = ((direction >= 0.0f) ? 1.0f : -1.0f) * (300.0f * this->dyna.actor.scale.x + 20.0f - 0.5f);
    f32 sn = Math_SinS(angle);
    f32 cs = Math_CosS(angle);
    s32 i;

    for (i = 0; i < 4; i++) {
        Vec3f faceVtx;
        Vec3f faceVtxNext;
        Vec3f posResult;
        Vec3f faceVtxOffset;
        s32 bgId;
        CollisionPoly* outPoly;

        faceVtxOffset.x = (sFaceVtx[i].x * this->dyna.actor.scale.x * 10.0f) + sFaceDirection[i].x;
        faceVtxOffset.y = (sFaceVtx[i].y * this->dyna.actor.scale.y * 10.0f) + sFaceDirection[i].y;
        faceVtxOffset.z = 0.0f;
        MlPushBlock_RotateXZ(&faceVtx, &faceVtxOffset, sn, cs);
        faceVtx.x += this->dyna.actor.world.pos.x;
        faceVtx.y += this->dyna.actor.world.pos.y;
        faceVtx.z += this->dyna.actor.world.pos.z;
        faceVtxNext.x = faceVtx.x + maxDist * sn;
        faceVtxNext.y = faceVtx.y;
        faceVtxNext.z = faceVtx.z + maxDist * cs;
        if (BgCheck_EntityLineTest3(&play->colCtx, &faceVtx, &faceVtxNext, &posResult, &outPoly, true, false, false,
                                    true, &bgId, &this->dyna.actor, 0.0f)) {
            return true;
        }
    }
    return false;
}

static s32 MlPushBlock_MoveWithBlockUnder(ObjOshihiki* this, PlayState* play) {
    s32 pad;
    ObjOshihiki* blockUnder = MlPushBlock_GetBlockUnder(this, play);

    if ((blockUnder != NULL) && (blockUnder->stateFlags & PUSHBLOCK_SETUP_PUSH) &&
        !MlPushBlock_CheckWall(play, blockUnder->dyna.unk_158, blockUnder->direction, this)) {
        this->blockUnder = blockUnder;
    }

    if ((this->stateFlags & PUSHBLOCK_MOVE_UNDER) && (this->blockUnder != NULL)) {
        if (this->blockUnder->stateFlags & PUSHBLOCK_PUSH) {
            this->underDistX = this->blockUnder->dyna.actor.world.pos.x - this->blockUnder->dyna.actor.prevPos.x;
            this->underDistZ = this->blockUnder->dyna.actor.world.pos.z - this->blockUnder->dyna.actor.prevPos.z;
            this->dyna.actor.world.pos.x += this->underDistX;
            this->dyna.actor.world.pos.z += this->underDistZ;
            MlPushBlock_UpdateInitPos(this);
            return true;
        } else if (!(this->blockUnder->stateFlags & PUSHBLOCK_SETUP_PUSH)) {
            this->blockUnder = NULL;
        }
    }
    return false;
}

static void MlPushBlock_SetupOnScene(ObjOshihiki* this, PlayState* play) {
    this->stateFlags |= PUSHBLOCK_SETUP_ON_SCENE;
    this->actionFunc = MlPushBlock_OnScene;
    this->dyna.actor.gravity = 0.0f;
    this->dyna.actor.velocity.x = this->dyna.actor.velocity.y = this->dyna.actor.velocity.z = 0.0f;
}

static void MlPushBlock_OnScene(ObjOshihiki* this, PlayState* play) {
    s32 pad;
    Player* player = GET_PLAYER(play);

    this->stateFlags |= PUSHBLOCK_ON_SCENE;
    if ((this->timer <= 0) && (fabsf(this->dyna.unk_150) > 0.001f)) {
        if (MlPushBlock_StrongEnough(this) &&
            !MlPushBlock_CheckWall(play, this->dyna.unk_158, this->dyna.unk_150, this)) {
            this->direction = this->dyna.unk_150;
            MlPushBlock_SetupPush(this, play);
        } else {
            player->stateFlags2 &= ~PLAYER_STATE2_MOVING_DYNAPOLY;
            this->dyna.unk_150 = 0.0f;
        }
    } else {
        player->stateFlags2 &= ~PLAYER_STATE2_MOVING_DYNAPOLY;
        this->dyna.unk_150 = 0.0f;
    }
}

static void MlPushBlock_SetupOnActor(ObjOshihiki* this, PlayState* play) {
    this->stateFlags |= PUSHBLOCK_SETUP_ON_ACTOR;
    this->actionFunc = MlPushBlock_OnActor;
    this->dyna.actor.velocity.x = this->dyna.actor.velocity.y = this->dyna.actor.velocity.z = 0.0f;
    this->dyna.actor.gravity = -1.0f;
}

static void MlPushBlock_OnActor(ObjOshihiki* this, PlayState* play) {
    s32 bgId;
    Player* player = GET_PLAYER(play);
    DynaPolyActor* dynaPolyActor;

    this->stateFlags |= PUSHBLOCK_ON_ACTOR;
    Actor_MoveXZGravity(&this->dyna.actor);

    if (MlPushBlock_CheckFloor(this, play)) {
        bgId = this->floorBgIds[this->highestFloor];
        if (bgId == BGCHECK_SCENE) {
            MlPushBlock_SetupOnScene(this, play);
        } else {
            dynaPolyActor = DynaPoly_GetActor(&play->colCtx, bgId);
            if (dynaPolyActor != NULL) {
                DynaPolyActor_SetActorOnTop(dynaPolyActor);
                DynaPolyActor_SetSwitchPressed(dynaPolyActor);

                if ((this->timer <= 0) && (fabsf(this->dyna.unk_150) > 0.001f)) {
                    if (MlPushBlock_StrongEnough(this) && MlPushBlock_NoSwitchPress(this, dynaPolyActor, play) &&
                        !MlPushBlock_CheckWall(play, this->dyna.unk_158, this->dyna.unk_150, this)) {

                        this->direction = this->dyna.unk_150;
                        MlPushBlock_SetupPush(this, play);
                    } else {
                        player->stateFlags2 &= ~PLAYER_STATE2_MOVING_DYNAPOLY;
                        this->dyna.unk_150 = 0.0f;
                    }
                } else {
                    player->stateFlags2 &= ~PLAYER_STATE2_MOVING_DYNAPOLY;
                    this->dyna.unk_150 = 0.0f;
                }
            } else {
                MlPushBlock_SetupOnScene(this, play);
            }
        }
    } else {
        bgId = this->floorBgIds[this->highestFloor];
        if (bgId == BGCHECK_SCENE) {
            MlPushBlock_SetupFall(this, play);
        } else {
            dynaPolyActor = DynaPoly_GetActor(&play->colCtx, bgId);

            if ((dynaPolyActor != NULL) && (dynaPolyActor->transformFlags & 1)) {
                DynaPolyActor_SetActorOnTop(dynaPolyActor);
                DynaPolyActor_SetSwitchPressed(dynaPolyActor);
                this->dyna.actor.world.pos.y = this->dyna.actor.floorHeight;
            } else {
                MlPushBlock_SetupFall(this, play);
            }
        }
    }
}

static void MlPushBlock_SetupPush(ObjOshihiki* this, PlayState* play) {
    this->stateFlags |= PUSHBLOCK_SETUP_PUSH;
    this->actionFunc = MlPushBlock_Push;
    this->dyna.actor.gravity = 0.0f;
}

static void MlPushBlock_Push(ObjOshihiki* this, PlayState* play) {
    Actor* thisx = &this->dyna.actor;
    Player* player = GET_PLAYER(play);
    f32 pushDistSigned;
    s32 stopFlag;

    this->pushSpeed = this->pushSpeed + (CVarGetInteger("gEnhancements.FasterBlockPush", 0) * 0.25) + 0.5f;
    this->stateFlags |= PUSHBLOCK_PUSH;
    this->pushSpeed = CLAMP_MAX(this->pushSpeed, 2.0f + (CVarGetInteger("gEnhancements.FasterBlockPush", 0) * 0.5));
    stopFlag = Math_StepToF(&this->pushDist, 20.0f, this->pushSpeed);
    pushDistSigned = ((this->direction >= 0.0f) ? 1.0f : -1.0f) * this->pushDist;
    thisx->world.pos.x = thisx->home.pos.x + (pushDistSigned * this->yawSin);
    thisx->world.pos.z = thisx->home.pos.z + (pushDistSigned * this->yawCos);

    if (!MlPushBlock_CheckFloor(this, play)) {
        thisx->home.pos.x = thisx->world.pos.x;
        thisx->home.pos.z = thisx->world.pos.z;
        player->stateFlags2 &= ~PLAYER_STATE2_MOVING_DYNAPOLY;
        this->dyna.unk_150 = 0.0f;
        this->pushDist = 0.0f;
        this->pushSpeed = 0.0f;
        MlPushBlock_SetupFall(this, play);
    } else if (stopFlag) {
        player = GET_PLAYER(play);
        if (MlPushBlock_CheckWall(play, this->dyna.unk_158, this->dyna.unk_150, this)) {
            Audio_PlayActorSound2(thisx, NA_SE_EV_BLOCK_BOUND);
            Flags_SetSwitch(play, ML_PUSHBLOCK_SWITCH(this)); // ML: the push to the wall is recorded
        }

        thisx->home.pos.x = thisx->world.pos.x;
        thisx->home.pos.z = thisx->world.pos.z;
        player->stateFlags2 &= ~PLAYER_STATE2_MOVING_DYNAPOLY;
        this->dyna.unk_150 = 0.0f;
        this->pushDist = 0.0f;
        this->pushSpeed = 0.0f;
        this->timer = 10 - ((CVarGetInteger("gEnhancements.FasterBlockPush", 0) * 3) / 2);
        if (this->floorBgIds[this->highestFloor] == BGCHECK_SCENE) {
            MlPushBlock_SetupOnScene(this, play);
        } else {
            MlPushBlock_SetupOnActor(this, play);
        }
    }
    Audio_PlayActorSound2(thisx, NA_SE_EV_ROCK_SLIDE - SFX_FLAG);
}

static void MlPushBlock_SetupFall(ObjOshihiki* this, PlayState* play) {
    this->stateFlags |= PUSHBLOCK_SETUP_FALL;
    this->dyna.actor.velocity.x = this->dyna.actor.velocity.y = this->dyna.actor.velocity.z = 0.0f;
    this->dyna.actor.gravity = -1.0f;
    MlPushBlock_SetGround(this, play);
    this->actionFunc = MlPushBlock_Fall;
}

static void MlPushBlock_Fall(ObjOshihiki* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    this->stateFlags |= PUSHBLOCK_FALL;
    if (fabsf(this->dyna.unk_150) > 0.001f) {
        this->dyna.unk_150 = 0.0f;
        player->stateFlags2 &= ~PLAYER_STATE2_MOVING_DYNAPOLY;
    }
    Actor_MoveXZGravity(&this->dyna.actor);
    if (MlPushBlock_CheckGround(this, play)) {
        if (this->floorBgIds[this->highestFloor] == BGCHECK_SCENE) {
            MlPushBlock_SetupOnScene(this, play);
        } else {
            MlPushBlock_SetupOnActor(this, play);
        }
        Audio_PlayActorSound2(&this->dyna.actor, NA_SE_EV_BLOCK_BOUND);
        Audio_PlayActorSound2(&this->dyna.actor, SurfaceType_GetSfx(&play->colCtx, this->floorPolys[this->highestFloor],
                                                                    this->floorBgIds[this->highestFloor]) +
                                                     SFX_FLAG);
    }
}

static void MlPushBlock_UpdateVanilla(Actor* thisx, PlayState* play) {
    s32 pad;
    ObjOshihiki* this = (ObjOshihiki*)thisx;

    this->stateFlags &=
        ~(PUSHBLOCK_SETUP_FALL | PUSHBLOCK_FALL | PUSHBLOCK_SETUP_PUSH | PUSHBLOCK_PUSH | PUSHBLOCK_SETUP_ON_ACTOR |
          PUSHBLOCK_ON_ACTOR | PUSHBLOCK_SETUP_ON_SCENE | PUSHBLOCK_ON_SCENE);
    this->stateFlags |= PUSHBLOCK_MOVE_UNDER;

    if (this->timer > 0) {
        this->timer--;
    }

    this->dyna.actor.world.rot.y = this->dyna.unk_158;

    this->yawSin = Math_SinS(this->dyna.actor.world.rot.y);
    this->yawCos = Math_CosS(this->dyna.actor.world.rot.y);

    if (this->actionFunc != NULL) {
        this->actionFunc(this, play);
    }
}

// Vanilla's Init with ML's collision and without its switch check
static void MlPushBlock_Init(Actor* thisx, PlayState* play) {
    ObjOshihiki* this = (ObjOshihiki*)thisx;

    if ((thisx->params & 0xF) <= PUSHBLOCK_HUGE_START_OFF) {
        MlPushBlock_InitDynapoly(this, play, (CollisionHeader*)gMlPushBlockCol, 1);
    }
    MlPushBlock_SetScale(this, play);
    MlPushBlock_SetTexture(this, play);
    Actor_ProcessInitChain(thisx, sInitChain);
    thisx->colChkInfo.mass = MASS_IMMOVABLE;
    MlPushBlock_SetColor(this, play);
    MlPushBlock_ResetFloors(this);
    MlPushBlock_SetupOnActor(this, play);

    if (Flags_GetSwitch(play, ML_PUSHBLOCK_SWITCH(this))) {
        thisx->world.pos.z += -80.0f;
        thisx->home.pos.z = thisx->world.pos.z;
    }
}

// Vanilla's Update, then the note on top follows the block along z
static void MlPushBlock_Update(Actor* thisx, PlayState* play) {
    Actor* note;

    MlPushBlock_UpdateVanilla(thisx, play);
    for (note = play->actorCtx.actorLists[ACTORCAT_PROP].head; note != NULL; note = note->next) {
        if ((note->id == ACTOR_EN_WONDER_TALK) && ((u16)note->params == ML_PUSHBLOCK_NOTE)) {
            note->world.pos.z = thisx->world.pos.z;
            break;
        }
    }
}

static void MlPushBlock_Draw(Actor* thisx, PlayState* play) {
    Gfx_DrawDListOpa(play, (Gfx*)gMlPushBlockDL);
}

void MlPushBlock_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_PushBlock";
    definition.description = "The Missing Link: Library push block";
    definition.category = ACTORCAT_PROP;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = OBJECT_GANON2;
    definition.instanceSize = sizeof(ObjOshihiki);
    definition.init = MlPushBlock_Init;
    definition.destroy = MlPushBlock_Destroy;
    definition.update = MlPushBlock_Update;
    definition.draw = MlPushBlock_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_PushBlock is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
