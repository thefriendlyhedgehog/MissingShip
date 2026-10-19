/*
 * The Missing Link's thieves in Bmori1 (ledger row 87), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_Thief), registered through the mod API.
 *
 * The three thieves sheltering in Bmori1 (ML's rewrite of En_Nb, slot 0x0C3)
 *
 * Source overlay: ML dma 228, text 0x590, on ML's own object 0xB3 (dma 38, shipped as object_ml_nb). Placed three
 * times in Bmori1 room 0. params 1 stands guard (message 0x6019, the ReDeads outside) and spawns the two ReDeads by
 * the gate; the others idle (0x6069, "our treasure"). Once switch 7 is set they wait 45 frames, start the scene's
 * cutscene 0x350, turn around, and after 266 frames run off down -z, params 1 fastest. At 321 frames they vanish,
 * params 1 taking its ReDeads with it. With switch 7 already set they are gone on arrival.
 *
 * Ported from MissingShip's SoH fork (ovl_Ml_Thief, ledger row 87).
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "align_asset_macro.h"

#include <stdio.h>

#include "missinglink.h"

#define ML_THIEF_LIMB_MAX 23

// ML's layout (size 0x208), for reference against its overlay; SoH's own offsets differ.
typedef struct MlThief {
    /* 0x000 */ Actor actor;
    /* 0x14C */ ColliderCylinder collider;
    /* 0x198 */ SkelAnime skelAnime;
    /* 0x1DC */ Vec3s headRot;
    /* 0x1E2 */ Vec3s torsoRot;
    /* 0x1E8 */ Actor* redeads[2]; // params 1 only
    /* 0x1F0 */ f32 runSpeed;
    /* 0x1F4 */ void* eyeTexture;
    /* 0x1F8 */ s16 blinkTimer;
    /* 0x200 */ s32 csTimer;
    /* 0x204 */ s32 waitTimer;
    Vec3s jointTable[ML_THIEF_LIMB_MAX];
    Vec3s morphTable[ML_THIEF_LIMB_MAX];
} MlThief;

#define dgMlThiefSkel "__OTR__objects/object_ml_nb/gMlThiefSkel"
static const ALIGN_ASSET(2) char gMlThiefSkel[] = dgMlThiefSkel;
#define dgMlThiefIdleAnim "__OTR__objects/object_ml_nb/gMlThiefIdleAnim"
static const ALIGN_ASSET(2) char gMlThiefIdleAnim[] = dgMlThiefIdleAnim;
#define dgMlThiefRunAnim "__OTR__objects/object_ml_nb/gMlThiefRunAnim"
static const ALIGN_ASSET(2) char gMlThiefRunAnim[] = dgMlThiefRunAnim;
#define dgMlThiefGuardAnim "__OTR__objects/object_ml_nb/gMlThiefGuardAnim"
static const ALIGN_ASSET(2) char gMlThiefGuardAnim[] = dgMlThiefGuardAnim;
#define dgMlThiefEyeOpenTex "__OTR__objects/object_ml_nb/gMlThiefEyeOpenTex"
static const ALIGN_ASSET(2) char gMlThiefEyeOpenTex[] = dgMlThiefEyeOpenTex;
#define dgMlThiefEyeHalfTex "__OTR__objects/object_ml_nb/gMlThiefEyeHalfTex"
static const ALIGN_ASSET(2) char gMlThiefEyeHalfTex[] = dgMlThiefEyeHalfTex;
#define dgMlThiefEyeClosedTex "__OTR__objects/object_ml_nb/gMlThiefEyeClosedTex"
static const ALIGN_ASSET(2) char gMlThiefEyeClosedTex[] = dgMlThiefEyeClosedTex;
#define dgMlBmori1ThievesLeaveCs "__OTR__scenes/nonmq/Bmori1_scene/gMlBmori1ThievesLeaveCs"
static const ALIGN_ASSET(2) char gMlBmori1ThievesLeaveCs[] = dgMlBmori1ThievesLeaveCs;

static void* sEyeTextures[] = { (void*)gMlThiefEyeOpenTex, (void*)gMlThiefEyeHalfTex, (void*)gMlThiefEyeClosedTex };

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_NONE,
        OC1_ON | OC1_TYPE_PLAYER,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0xFFCFFFFF, 0x04, 0x05 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON,
        BUMP_ON,
        OCELEM_ON,
    },
    { 48, 36, 0, { 0, 0, 0 } },
};

// SoH difference: the ReDeads may be long dead (and freed) by the time params 1 leaves.
static s32 MlThief_IsLive(PlayState* play, Actor* actor) {
    Actor* it;

    if (actor == NULL) {
        return false;
    }
    for (it = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; it != NULL; it = it->next) {
        if (it == actor) {
            return true;
        }
    }
    return false;
}

static void MlThief_Init(Actor* thisx, PlayState* play) {
    MlThief* this = (MlThief*)thisx;

    if (Flags_GetSwitch(play, 7)) {
        Actor_Kill(&this->actor);
    } else if (this->actor.params == 1) {
        this->redeads[0] = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_RD, -498.0f, 0.0f, -160.0f, 0, 0x2F20, 0, 0);
        this->redeads[1] = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_RD, -488.0f, 0.0f, 166.0f, 0, 0x5680, 0, 0);
    }
    this->runSpeed = 1.5f;
    Actor_SetScale(&this->actor, 0.01f);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    if (this->actor.params == 1) {
        this->actor.textId = 0x6019;
        SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)gMlThiefSkel, (AnimationHeader*)gMlThiefGuardAnim,
                           this->jointTable, this->morphTable, ML_THIEF_LIMB_MAX);
    } else {
        this->actor.textId = 0x6069;
        SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)gMlThiefSkel, (AnimationHeader*)gMlThiefIdleAnim,
                           this->jointTable, this->morphTable, ML_THIEF_LIMB_MAX);
    }
    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 23.0f);
    this->eyeTexture = sEyeTextures[0]; // drawn once before the first Update (MS-144)
}

static void MlThief_Destroy(Actor* thisx, PlayState* play) {
    MlThief* this = (MlThief*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static void MlThief_Update(Actor* thisx, PlayState* play) {
    MlThief* this = (MlThief*)thisx;

    Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, 20.0f, 60.0f, 0x1D);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    SkelAnime_Update(&this->skelAnime);

    if (this->blinkTimer == 0) {
        this->blinkTimer = Rand_S16Offset(30, 30);
    }
    this->blinkTimer--;
    this->eyeTexture = (this->blinkTimer < 2) ? sEyeTextures[2 - this->blinkTimer] : sEyeTextures[0];

    Actor_OfferTalkNearColChkInfoCylinder(&this->actor, play);

    if (!Flags_GetSwitch(play, 7)) {
        return;
    }
    if (this->waitTimer < 45) {
        this->waitTimer++;
        return;
    }
    if (this->csTimer == 0) {
        Cutscene_SetSegment(play, (void*)gMlBmori1ThievesLeaveCs);
        gSaveContext.cutsceneTrigger = 1;
        this->actor.shape.rot.y = -0x8000;
    }
    if (this->csTimer >= 266) {
        if (this->skelAnime.animation != (AnimationHeader*)gMlThiefRunAnim) {
            Animation_Change(&this->skelAnime, (AnimationHeader*)gMlThiefRunAnim, 1.0f, 0.0f,
                             Animation_GetLastFrame((void*)gMlThiefRunAnim), ANIMMODE_LOOP, -5.0f);
        }
        if (this->actor.params == 1) {
            this->runSpeed += 2.0f;
        } else if (this->actor.params == 2) {
            this->runSpeed += 1.3f;
        } else {
            this->runSpeed += 0.5f;
        }
        this->actor.world.pos.z -= this->runSpeed;
        if (this->csTimer >= 321) {
            if (this->actor.params == 1) {
                if (MlThief_IsLive(play, this->redeads[0])) {
                    Actor_Kill(this->redeads[0]);
                }
                if (MlThief_IsLive(play, this->redeads[1])) {
                    Actor_Kill(this->redeads[1]);
                }
            }
            Actor_Kill(&this->actor);
        }
    }
    this->csTimer++;
}

static s32 MlThief_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    MlThief* this = (MlThief*)thisx;

    if (limbIndex == 6) {
        Actor_TrackPlayer(play, &this->actor, &this->headRot, &this->torsoRot, this->actor.world.pos);
        rot->x += this->torsoRot.y;
    }
    return false;
}

static void MlThief_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };
    MlThief* this = (MlThief*)thisx;

    if (limbIndex == 6) {
        Matrix_MultVec3f(&sZeroVec, &this->actor.focus.pos);
    }
}

static void MlThief_Draw(Actor* thisx, PlayState* play) {
    MlThief* this = (MlThief*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    gDPSetEnvColor(POLY_OPA_DISP++, 0x8C, 0, 0, 0xFF);
    gDPSetEnvColor(POLY_XLU_DISP++, 0x8C, 0, 0, 0xFF);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(this->eyeTexture));
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          MlThief_OverrideLimbDraw, MlThief_PostLimbDraw, this);
    CLOSE_DISPS(play->state.gfxCtx);
}

void MlThief_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_Thief";
    definition.description = "The Missing Link: Bmori1 thief";
    definition.category = ACTORCAT_NPC;
    definition.actorFlags = (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_UPDATE_CULLING_DISABLED);
    definition.objectId = OBJECT_NB;
    definition.instanceSize = sizeof(MlThief);
    definition.init = MlThief_Init;
    definition.destroy = MlThief_Destroy;
    definition.update = MlThief_Update;
    definition.draw = MlThief_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_Thief is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
