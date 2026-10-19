/*
 * The Missing Link's Mido (ledger row 14), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_En_Md), registered through the mod API.
 * He keeps vanilla En_Md's instance and runs its Destroy, Draw and helpers; Init, Update and the text choice are ML's:
 * he stands where ML's Kokiri Forest places him and only talks (0x1046, "The path collapsed", until the slingshot has
 * ammo, then 0x1044). ML's rooms place him by name, so vanilla's Mido is unchanged.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Md/z_en_md.h"
#include "objects/object_md/object_md.h"
#include "overlays/actors/ovl_En_Elf/z_en_elf.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#include <stdio.h>

#include "missinglink.h"

ML_HOST_FN_DECL(EnMd_BlockPath);
ML_HOST_FN_DECL(EnMd_ListenToOcarina);
ML_HOST_FN_DECL(EnMd_Walk);
ML_HOST_FN_DECL(EnMd_Watch);

// vanilla's, from z_en_md.c
void EnMd_Destroy(Actor* thisx, PlayState* play);
void EnMd_Draw(Actor* thisx, PlayState* play);
u8 EnMd_ShouldSpawn(EnMd* enMd, PlayState* play);
void EnMd_UpdateEyes(EnMd* enMd);
void EnMd_UpdateAlphaByDistance(EnMd* enMd, PlayState* play);
u16 EnMd_GetTextKokiriHome(PlayState* play, EnMd* enMd);
u16 EnMd_GetTextLostWoods(PlayState* play, EnMd* enMd);
s16 func_80AAAF04(PlayState* play, Actor* thisx);
void EnMd_Watch(EnMd* enMd, PlayState* play);
void EnMd_BlockPath(EnMd* enMd, PlayState* play);
void EnMd_ListenToOcarina(EnMd* enMd, PlayState* play);
void EnMd_Walk(EnMd* enMd, PlayState* play);

// Vanilla's, which its file keeps static
static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        AC_NONE,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_2,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_NONE,
        OCELEM_ON,
    },
    { 36, 46, 0, { 0, 0, 0 } },
};

static CollisionCheckInfoInit2 sColChkInfoInit = { 0, 0, 0, 0, MASS_IMMOVABLE };

// Vanilla's first animation entry (ENMD_ANIM_0), the only one Init plays
static AnimationInfo sIdleAnimationInfo[] = {
    { (AnimationHeader*)gMidoHandsOnHipsIdleAnim, 0.0f, 0.0f, -1.0f, ANIMMODE_LOOP, 0.0f },
};

// ML retargets vanilla's Kokiri Forest case to the Mido's House lines (0x1046 "The path collapsed", 0x1028), so the
// sword-and-shield exchange that makes him step aside can never start.
static u16 MlEnMd_GetText(PlayState* play, Actor* thisx) {
    EnMd* this = (EnMd*)thisx;

    switch (play->sceneNum) {
        case SCENE_KOKIRI_FOREST:
        case SCENE_MIDOS_HOUSE:
            return EnMd_GetTextKokiriHome(play, this);
        case SCENE_LOST_WOODS:
            return EnMd_GetTextLostWoods(play, this);
        default:
            return 0;
    }
}

// Vanilla's EnMd_UpdateTalking, which names vanilla's text choice; here it names ML's
static void MlEnMd_UpdateTalking(EnMd* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 absYawDiff;
    s16 trackingMode;
    s16 canUpdateTalking;
    s16 yawDiff;

    if (this->actor.xzDistToPlayer < 170.0f) {
        yawDiff = (f32)this->actor.yawTowardsPlayer - this->actor.shape.rot.y;
        absYawDiff = ABS(yawDiff);

        trackingMode =
            absYawDiff <= Npc_GetTrackingPresetMaxPlayerYaw(2) ? NPC_TRACKING_HEAD_AND_TORSO : NPC_TRACKING_NONE;
        canUpdateTalking = true;
    } else {
        trackingMode = NPC_TRACKING_NONE;
        canUpdateTalking = false;
    }

    if (this->interactInfo.talkState != NPC_TALK_STATE_IDLE) {
        trackingMode = NPC_TRACKING_FULL_BODY;
    }

    if (ML_IS_HOST_FN(this->actionFunc, EnMd_Walk)) {
        trackingMode = NPC_TRACKING_NONE;
        canUpdateTalking = false;
    }
    if (ML_IS_HOST_FN(this->actionFunc, EnMd_Watch)) {
        trackingMode = NPC_TRACKING_FULL_BODY;
        canUpdateTalking = true;
    }

    if ((play->csCtx.state != CS_STATE_IDLE) || gDbgCamEnabled) {
        this->interactInfo.trackPos = play->view.eye;
        this->interactInfo.yOffset = 40.0f;
        trackingMode = NPC_TRACKING_HEAD_AND_TORSO;
    } else {
        this->interactInfo.trackPos = player->actor.world.pos;
        this->interactInfo.yOffset = (gSaveContext.linkAge > 0) ? 0.0f : -18.0f;
    }

    Npc_TrackPoint(&this->actor, &this->interactInfo, 2, trackingMode);
    if (!ML_IS_HOST_FN(this->actionFunc, EnMd_ListenToOcarina)) {
        if (canUpdateTalking) {
            Npc_UpdateTalking(play, &this->actor, &this->interactInfo.talkState, this->collider.dim.radius + 30.0f,
                              MlEnMd_GetText, func_80AAAF04);
        }
    }
}

// Vanilla's Init, with ML's one patch: the stand-at-home condition is forced true (bne -> beq $a0,$a0), so Mido
// always stays at his placed position. Vanilla would otherwise move him to the end of the scene's path 0, which ML's
// Kokiri Forest does not have.
static void MlEnMd_Init(Actor* thisx, PlayState* play) {
    EnMd* this = (EnMd*)thisx;

    ActorShape_Init(&this->actor.shape, 0.0f, ActorShadow_DrawCircle, 24.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)gMidoSkel, NULL, this->jointTable, this->morphTable, 17);

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    CollisionCheck_SetInfo2(&this->actor.colChkInfo, NULL, &sColChkInfoInit);
    if (!GameInteractor_Should(VB_MIDO_SPAWN, EnMd_ShouldSpawn(this, play), this)) {
        Actor_Kill(&this->actor);
        return;
    }

    Animation_ChangeByInfo(&this->skelAnime, sIdleAnimationInfo, 0);
    Actor_SetScale(&this->actor, 0.01f);
    this->actor.targetMode = 6;
    this->alpha = 255;
    Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_EN_ELF, this->actor.world.pos.x,
                       this->actor.world.pos.y, this->actor.world.pos.z, 0, 0, 0, FAIRY_KOKIRI);

    this->actor.home.pos = this->actor.world.pos;
    ML_SET_HOST_FN(this->actionFunc, EnMd_BlockPath); // never run: ML's Update does not call the action function
}

// Vanilla's Update, with ML's patch: the action call is replaced by a jump into ML's own code (shared with En_Ko), so
// Mido never runs BlockPath or Walk and only talks. Once slingshot ammo is non-zero, ML swaps his line for the one
// after the "demon".
static void MlEnMd_Update(Actor* thisx, PlayState* play) {
    EnMd* this = (EnMd*)thisx;

    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    SkelAnime_Update(&this->skelAnime);
    EnMd_UpdateEyes(this);
    EnMd_UpdateAlphaByDistance(this, play);
    Actor_MoveXZGravity(&this->actor);
    MlEnMd_UpdateTalking(this, play);
    Actor_UpdateBgCheckInfo(play, &this->actor, 0.0f, 0.0f, 0.0f, 4);
    if (gSaveContext.inventory.ammo[SLOT_SLINGSHOT] != 0 && this->actor.textId == 0x1046) {
        this->actor.textId = 0x1044;
    }
}

void MlEnMd_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_En_Md";
    definition.description = "The Missing Link: Mido";
    definition.category = ACTORCAT_NPC;
    definition.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
                            ACTOR_FLAG_UPDATE_DURING_OCARINA;
    definition.objectId = OBJECT_MD;
    definition.instanceSize = sizeof(EnMd);
    definition.init = MlEnMd_Init;
    definition.destroy = EnMd_Destroy;
    definition.update = MlEnMd_Update;
    definition.draw = EnMd_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_En_Md is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
