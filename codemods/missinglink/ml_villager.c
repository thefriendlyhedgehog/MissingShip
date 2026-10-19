/*
 * The Missing Link's Kakariko villager (ledger row 10), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_Villager),
 * registered through the mod API.
 *
 * ML's own overlay, in vanilla's unused slot 0x006 (ML dma 106, 912 bytes), ported as MissingShip's SoH fork has it
 * (ovl_Ml_Villager, ledger row 10). Placed 2x in ML's Kakariko (spot01 room 0). A small talking NPC that turns to face
 * the player, drawn from ML's replacement for object slot 0x0C (OBJECT_DODONGO in vanilla): a 10-limb flex skeleton
 * and a 30-frame idle loop, which ML's layer ships as objects/object_ml_villager.
 *
 * ML stores each villager's message id in the placement's rotation Z (0x605F "We are truly in your debt, hero..." and
 * 0x6063 "Now that everything is back to normal..."), and its tint in params.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "align_asset_macro.h"

#include <stdio.h>

#include "missinglink.h"

typedef struct MlVillager {
    Actor actor;
    SkelAnime skelAnime;
    ColliderCylinder collider;
} MlVillager; // ML instanceSize = 0x20C

#define dgMlVillagerSkel "__OTR__objects/object_ml_villager/gMlVillagerSkel"
static const ALIGN_ASSET(2) char gMlVillagerSkel[] = dgMlVillagerSkel;
#define dgMlVillagerIdleAnim "__OTR__objects/object_ml_villager/gMlVillagerIdleAnim"
static const ALIGN_ASSET(2) char gMlVillagerIdleAnim[] = dgMlVillagerIdleAnim;

// ML's collider enables AT and AC, but the actor only ever calls CollisionCheck_SetOC, so those
// halves are inert. Kept as-is for fidelity.
static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_ENEMY,
        AC_NONE,
        OC1_ON | OC1_TYPE_PLAYER,
        OC2_TYPE_2,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0xFFCFFFFF, 0x04, 0x05 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_ON,
        OCELEM_ON,
    },
    { 21, 23, 0, { 0, 0, 0 } },
};

static void MlVillager_Init(Actor* thisx, PlayState* play) {
    MlVillager* this = (MlVillager*)thisx;

    // The villagers only appear once switch flag 7 of scene 3 is set. ML's scene 3 is Bmori1,
    // which it builds in vanilla's Forest Temple slot, so they show up after that event.
    // ML does not return after killing the actor, so initialisation below still runs, as it does here.
    if (!(gSaveContext.sceneFlags[SCENE_FOREST_TEMPLE].swch & (1 << 7))) {
        Actor_Kill(thisx);
    }

    Actor_SetScale(thisx, 0.008f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)gMlVillagerSkel,
                       (AnimationHeader*)gMlVillagerIdleAnim, NULL, NULL, 0);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sCylinderInit);
    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 45.0f);

    thisx->textId = thisx->shape.rot.z;
    thisx->shape.rot.z = 0;
}

static void MlVillager_Destroy(Actor* thisx, PlayState* play) {
    MlVillager* this = (MlVillager*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
    // ML leaks the joint and morph tables SkelAnime_InitFlex allocates (it passes NULL for both);
    // free them here. No visible difference, but it stops repeated spawns from eating the arena.
    SkelAnime_Free(&this->skelAnime, play);
}

static void MlVillager_Update(Actor* thisx, PlayState* play) {
    MlVillager* this = (MlVillager*)thisx;

    SkelAnime_Update(&this->skelAnime);
    Actor_UpdateBgCheckInfo(play, thisx, 20.0f, 27.0f, 27.0f, 0x05);
    Collider_UpdateCylinder(thisx, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);

    if (thisx->xzDistToPlayer < 130.0f) {
        Actor_OfferTalkNearColChkInfoCylinder(thisx, play);
        Math_SmoothStepToS(&thisx->shape.rot.y, thisx->yawTowardsPlayer, 3, 0x400, 0x100);
    }
}

static void MlVillager_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

    if (limbIndex == 10) {
        Matrix_MultVec3f(&sZeroVec, &((Actor*)thisx)->focus.pos);
    }
}

static void MlVillager_Draw(Actor* thisx, PlayState* play) {
    MlVillager* this = (MlVillager*)thisx;

    OPEN_DISPS(play->state.gfxCtx);

    // Each villager's tint comes from the environment colour: params 0 is a warm orange,
    // anything else near-black.
    if (thisx->params != 0) {
        gDPSetEnvColor(POLY_OPA_DISP++, 36, 36, 36, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 36, 36, 36, 255);
    } else {
        gDPSetEnvColor(POLY_OPA_DISP++, 255, 104, 72, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 104, 72, 255);
    }

    CLOSE_DISPS(play->state.gfxCtx);

    // ML passes limbCount where dListCount belongs (over-allocating matrices) and keeps a
    // SkelAnime_DrawOpa fallback for limbCount == 0, which cannot happen after InitFlex. Use the
    // flex draw with the real count.
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          NULL, MlVillager_PostLimbDraw, this);
}

void MlVillager_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_Villager";
    definition.description = "The Missing Link: Kakariko villager";
    definition.category = ACTORCAT_NPC;
    definition.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = OBJECT_DODONGO;
    definition.instanceSize = sizeof(MlVillager);
    definition.init = MlVillager_Init;
    definition.destroy = MlVillager_Destroy;
    definition.update = MlVillager_Update;
    definition.draw = MlVillager_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_Villager is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
