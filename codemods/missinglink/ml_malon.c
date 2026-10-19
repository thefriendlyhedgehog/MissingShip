/*
 * The Missing Link's young Malon (ledger row 34), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_Malon), registered through the mod API.
 *
 * Young Malon and her race (ML's rewrite of En_Ma1, slot 0xE7)
 *
 * ML dma 236, drawn from ML's redrawn object_ma1 (object_ml_ma1, in ML's layer). The race state is switch 0x11 of
 * scene 0x5A (the Gerudo Valley slot, where ML builds Deku Road), read from the save at Init. ML's entrance numbers are
 * the vanilla ones in the slots ML builds in (as ML_EventSpawner's): 0x058 ENTR_LON_LON_RANCH_ENTRANCE, 0x138
 * ENTR_GERUDO_VALLEY_OUTSIDE_TENT, 0x13C ENTR_LON_LON_RANCH_OUTSIDE_TALONS_HOUSE. Ported from MissingShip's SoH fork
 * (ovl_Ml_Malon, ledger row 34); placed by name where ML's rooms place 0xE7, and spawned by name by ML_EventSpawner.
 *   params 0xFFFF  the finish line: reach her (within 80, not below her) while the timer runs and the race ends --
 *                  switch 0x11, timer stopped, warp to ML entrance 0x13C.
 *   otherwise      the host. Offers text 0x204A (0x204F once the race is won, when she also releases a cucco at
 *                  (-778,-485,308), ML's recoloured one in slot 0x0D, params 4: ML_Cucco, spawned by name).
 *                  Choosing the first option of 0x204A answers 0x204C and warps to ML entrance 0x138, the race;
 *                  the other answers 0x204B.
 *                  Arriving by ML entrance 0x13C (the finish) she speaks first: 0x2050 on a loss, 0x204D on a
 *                  win, whose 0x204E line releases the cucco.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "align_asset_macro.h"
#include "soh/ActorDB.h"

#include <stdio.h>

#include "missinglink.h"

struct MlMalon;

typedef void (*MlMalonActionFunc)(struct MlMalon*, PlayState*);

typedef struct MlMalon {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[22];
    Vec3s morphTable[22];
    ColliderCylinder collider;
    MlMalonActionFunc actionFunc;
    Vec3s headRot;
    Vec3s torsoRot;
    s16 blinkTimer;
    void* eyeTex;
    void* mouthTex;
    s16 state; // 0 idle; 1 accepted the race (starts it when the text closes) / race cucco spawned; 2 declined
} MlMalon;

#define dgMlMalonSkel "__OTR__objects/object_ml_ma1/gMlMalonSkel"
static const ALIGN_ASSET(2) char gMlMalonSkel[] = dgMlMalonSkel;
#define dgMlMalonIdleAnim "__OTR__objects/object_ml_ma1/gMlMalonIdleAnim"
static const ALIGN_ASSET(2) char gMlMalonIdleAnim[] = dgMlMalonIdleAnim;
#define dgMlMalonTalkAnim "__OTR__objects/object_ml_ma1/gMlMalonTalkAnim"
static const ALIGN_ASSET(2) char gMlMalonTalkAnim[] = dgMlMalonTalkAnim;
#define dgMlMalonEyeOpenTex "__OTR__objects/object_ml_ma1/gMlMalonEyeOpenTex"
static const ALIGN_ASSET(2) char gMlMalonEyeOpenTex[] = dgMlMalonEyeOpenTex;
#define dgMlMalonEyeHalfTex "__OTR__objects/object_ml_ma1/gMlMalonEyeHalfTex"
static const ALIGN_ASSET(2) char gMlMalonEyeHalfTex[] = dgMlMalonEyeHalfTex;
#define dgMlMalonEyeClosedTex "__OTR__objects/object_ml_ma1/gMlMalonEyeClosedTex"
static const ALIGN_ASSET(2) char gMlMalonEyeClosedTex[] = dgMlMalonEyeClosedTex;
#define dgMlMalonMouthNeutralTex "__OTR__objects/object_ml_ma1/gMlMalonMouthNeutralTex"
static const ALIGN_ASSET(2) char gMlMalonMouthNeutralTex[] = dgMlMalonMouthNeutralTex;
#define dgMlMalonMouthSadTex "__OTR__objects/object_ml_ma1/gMlMalonMouthSadTex"
static const ALIGN_ASSET(2) char gMlMalonMouthSadTex[] = dgMlMalonMouthSadTex;
#define dgMlMalonMouthHappyTex "__OTR__objects/object_ml_ma1/gMlMalonMouthHappyTex"
static const ALIGN_ASSET(2) char gMlMalonMouthHappyTex[] = dgMlMalonMouthHappyTex;

#define ML_MALON_RACE_SCENE 0x5A
#define ML_MALON_WON_SWITCH 0x11

static void MlMalon_Host(MlMalon* this, PlayState* play);
static void MlMalon_HostWon(MlMalon* this, PlayState* play);
static void MlMalon_GreetLoss(MlMalon* this, PlayState* play);
static void MlMalon_GreetWin(MlMalon* this, PlayState* play);
static void MlMalon_FinishLine(MlMalon* this, PlayState* play);

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
        { 0x00000000, 0x00, 0x00 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON,
        OCELEM_ON,
    },
    { 23, 35, 0, { 0, 0, 0 } },
};

static void* sEyeTextures[] = { (void*)gMlMalonEyeOpenTex, (void*)gMlMalonEyeHalfTex, (void*)gMlMalonEyeClosedTex };

static void MlMalon_SetAnim(MlMalon* this, void* anim) {
    if (this->skelAnime.animation != anim) {
        Animation_MorphToLoop(&this->skelAnime, anim, -5.0f);
    }
}

// ML's recoloured cucco, ML's copy of En_Niw in En_Poh's slot, is ML_Cucco, spawned by name: if it is not registered,
// the spawn is skipped with an error.
static void MlMalon_SpawnCucco(PlayState* play) {
    s32 id = ActorDB_RetrieveId("ML_Cucco");

    if (id < 0) {
        LUSLOG_ERROR("[MissingLink] ML_Malon: %s is not registered; its spawn is skipped", "ML_Cucco");
        return;
    }
    Actor_Spawn(&play->actorCtx, play, id, -778.0f, -485.0f, 308.0f, 0, 0x1680, 0, 4);
}

static void MlMalon_SetupHost(MlMalon* this) {
    this->actor.flags &= ~ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
    this->mouthTex = (void*)gMlMalonMouthNeutralTex;
    this->actor.textId = 0x204A;
    this->state = 0;
    this->actionFunc = MlMalon_Host;
}

static void MlMalon_SetupHostWon(MlMalon* this) {
    this->actor.flags &= ~ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
    this->mouthTex = (void*)gMlMalonMouthNeutralTex;
    this->actor.textId = 0x204F;
    this->state = 0;
    this->actionFunc = MlMalon_HostWon;
}

static void MlMalon_Init(Actor* thisx, PlayState* play) {
    MlMalon* this = (MlMalon*)thisx;
    s32 won = (gSaveContext.sceneFlags[ML_MALON_RACE_SCENE].swch >> ML_MALON_WON_SWITCH) & 1;

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sCylinderInit);
    SkelAnime_InitFlex(play, &this->skelAnime, (void*)gMlMalonSkel, (void*)gMlMalonIdleAnim, NULL, NULL, 0);
    Animation_PlayLoop(&this->skelAnime, (void*)gMlMalonIdleAnim);
    MlMalon_SetAnim(this, (void*)gMlMalonIdleAnim);
    Actor_SetScale(thisx, 0.01f);
    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 20.0f);
    this->eyeTex = (void*)gMlMalonEyeOpenTex;

    if (gSaveContext.entranceIndex == ENTR_LON_LON_RANCH_OUTSIDE_TALONS_HOUSE) { // ML 0x13C
        // Back from the race: she speaks first.
        gSaveContext.entranceIndex = ENTR_LON_LON_RANCH_ENTRANCE; // ML 0x058
        MlMalon_SetAnim(this, (void*)gMlMalonTalkAnim);
        thisx->flags |= ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
        if (won) {
            this->mouthTex = (void*)gMlMalonMouthHappyTex;
            thisx->textId = 0x204D;
            this->actionFunc = MlMalon_GreetWin;
        } else {
            this->mouthTex = (void*)gMlMalonMouthSadTex;
            thisx->textId = 0x2050;
            this->actionFunc = MlMalon_GreetLoss;
        }
    } else if ((u16)thisx->params == 0xFFFF) {
        MlMalon_SetAnim(this, (void*)gMlMalonTalkAnim);
        this->mouthTex = (void*)gMlMalonMouthHappyTex;
        this->actionFunc = MlMalon_FinishLine;
    } else if (won) {
        MlMalon_SpawnCucco(play);
        MlMalon_SetupHostWon(this);
    } else {
        MlMalon_SetupHost(this);
    }
}

static void MlMalon_Destroy(Actor* thisx, PlayState* play) {
    MlMalon* this = (MlMalon*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static s32 MlMalon_OfferTalk(MlMalon* this, PlayState* play) {
    if (this->actor.xzDistToPlayer < 120.0f &&
        ABS((s16)(this->actor.yawTowardsPlayer - this->actor.shape.rot.y)) < 0x4300) {
        Actor_OfferTalkNearColChkInfoCylinder(&this->actor, play);
        MlMalon_SetAnim(this, Message_GetState(&play->msgCtx) != TEXT_STATE_NONE ? (void*)gMlMalonTalkAnim
                                                                                 : (void*)gMlMalonIdleAnim);
        return true;
    }
    return false;
}

static void MlMalon_Host(MlMalon* this, PlayState* play) {
    MlMalon_OfferTalk(this, play);

    if (Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE && Message_ShouldAdvance(play)) {
        if (play->msgCtx.choiceIndex == 0) {
            this->mouthTex = (void*)gMlMalonMouthHappyTex;
            Message_ContinueTextbox(play, 0x204C);
            this->state = 1;
        } else {
            this->mouthTex = (void*)gMlMalonMouthSadTex;
            Message_ContinueTextbox(play, 0x204B);
            this->state = 2;
        }
    } else if (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE) {
        if (this->state == 1) {
            func_80064520(play, &play->csCtx);
            play->nextEntranceIndex = ENTR_GERUDO_VALLEY_OUTSIDE_TENT; // ML 0x138
            play->transitionTrigger = TRANS_TRIGGER_START;
        } else {
            this->mouthTex = (void*)gMlMalonMouthNeutralTex;
            this->state = 0;
        }
    }
}

static void MlMalon_HostWon(MlMalon* this, PlayState* play) {
    MlMalon_OfferTalk(this, play);
}

static void MlMalon_GreetLoss(MlMalon* this, PlayState* play) {
    if (Message_GetState(&play->msgCtx) == TEXT_STATE_DONE) {
        MlMalon_SetupHost(this);
    }
    Actor_OfferTalk(&this->actor, play, 1000.0f);
}

static void MlMalon_GreetWin(MlMalon* this, PlayState* play) {
    if (Message_GetState(&play->msgCtx) == TEXT_STATE_DONE) {
        MlMalon_SetupHostWon(this);
        return;
    }
    Actor_OfferTalk(&this->actor, play, 1000.0f);
    if (play->msgCtx.textId == 0x204E && this->state == 0) {
        MlMalon_SpawnCucco(play);
        this->state = 1;
    }
}

static void MlMalon_FinishLine(MlMalon* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if ((this->actor.world.pos.y - 20.0f) < player->actor.world.pos.y && this->actor.xzDistToPlayer < 80.0f &&
        gSaveContext.timerSeconds > 0) {
        func_80064520(play, &play->csCtx);
        Flags_SetSwitch(play, ML_MALON_WON_SWITCH);
        gSaveContext.timerState = 10;
        play->nextEntranceIndex = ENTR_LON_LON_RANCH_OUTSIDE_TALONS_HOUSE; // ML 0x13C
        play->transitionTrigger = TRANS_TRIGGER_START;
    }
}

static void MlMalon_Update(Actor* thisx, PlayState* play) {
    MlMalon* this = (MlMalon*)thisx;

    this->actionFunc(this, play);
    Actor_UpdateBgCheckInfo(play, thisx, 20.0f, this->collider.dim.radius, this->collider.dim.height,
                            5);
    Collider_UpdateCylinder(thisx, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    SkelAnime_Update(&this->skelAnime);

    if (this->blinkTimer == 0) {
        this->blinkTimer = Rand_S16Offset(30, 30);
    }
    this->blinkTimer--;
    this->eyeTex = (this->blinkTimer < 2) ? sEyeTextures[(u8)(2 - this->blinkTimer)] : (void*)gMlMalonEyeOpenTex;
}

static s32 MlMalon_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx) {
    MlMalon* this = (MlMalon*)thisx;

    if (limbIndex == 14) {
        Actor_TrackPlayer(play, &this->actor, &this->headRot, &this->torsoRot, this->actor.world.pos);
        rot->x += this->torsoRot.y;
    }
    return false;
}

static void MlMalon_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    static Vec3f zeroVec = { 0.0f, 0.0f, 0.0f };
    MlMalon* this = (MlMalon*)thisx;

    if (limbIndex == 14) {
        Matrix_MultVec3f(&zeroVec, &this->actor.focus.pos);
    }
}

static void MlMalon_Draw(Actor* thisx, PlayState* play) {
    MlMalon* this = (MlMalon*)thisx;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(this->eyeTex));
    gSPSegment(POLY_XLU_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(this->eyeTex));
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(this->mouthTex));
    gSPSegment(POLY_XLU_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(this->mouthTex));
    SkelAnime_DrawSkeletonOpa(play, &this->skelAnime, MlMalon_OverrideLimbDraw, MlMalon_PostLimbDraw, this);

    CLOSE_DISPS(play->state.gfxCtx);
}

void MlMalon_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_Malon";
    definition.description = "The Missing Link: young Malon";
    definition.category = ACTORCAT_NPC;
    definition.actorFlags = (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED);
    definition.objectId = OBJECT_MA1;
    definition.instanceSize = sizeof(MlMalon);
    definition.init = MlMalon_Init;
    definition.destroy = MlMalon_Destroy;
    definition.update = MlMalon_Update;
    definition.draw = MlMalon_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_Malon is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
