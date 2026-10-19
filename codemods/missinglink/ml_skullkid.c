/*
 * The Missing Link's Skull Kid on the Deku Road (ledger row 31), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_SkullKid), registered through the mod API.
 *
 * The Deku Road Skull Kid (ML's rewrite of En_Skj, slot 0x115)
 *
 * ML dma 250. Two variants, both keyed to switch 7:
 *   params 1  the Skull Kid you see: vanilla's skeleton idling with gSkullKidLookLeftAndRightAnim. It spawns two
 *             sideways bomb flowers (En_Bombf params 0xFFFF) at fixed positions and sets switch 7 once neither is
 *             targetable any more.
 *   params 2  an invisible speaker (focus 300 up). Before the switch: text 0x1029, auto-accepted when you step
 *             inside 40 units, camera setting 5 while talking within 100. After it: text 0x102A, then a Piece of
 *             Heart. Gone on later visits once the switch is set.
 *
 * Ported from MissingShip's SoH fork (ovl_Ml_SkullKid, ledger row 31).
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/object_skj/object_skj.h"
#include "soh/ResourceManagerHelpers.h"

#include <stdio.h>

#include "missinglink.h"

typedef struct MlSkullKid {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[19];
    Vec3s morphTable[19];
    Actor* flowers[2]; // params 1: the two bomb flowers it spawns
    s16 state;         // params 2: 0 offer talk, 1 talking, 2 event, 3 give heart, 4 done
    s16 inContact;     // params 2: the one-frame auto-accept pulse inside 40 units has fired
} MlSkullKid;

#define ML_SKJ_SWITCH 7

static void MlSkullKid_Init(Actor* thisx, PlayState* play) {
    MlSkullKid* this = (MlSkullKid*)thisx;
    s32 done = Flags_GetSwitch(play, ML_SKJ_SWITCH);

    this->inContact = false;
    switch (thisx->params) {
        case 1:
            Actor_SetScale(thisx, 0.01f);
            SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)gSkullKidSkel, (AnimationHeader*)gSkullKidLookLeftAndRightAnim,
                               this->jointTable, this->morphTable, 19);
            ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 25.0f);
            this->flowers[0] = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOMBF, 6001.0f, 556.0f, -671.0f, 0x4000, 0,
                                           0x4000, 0xFFFF);
            this->flowers[1] = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_BOMBF, 6001.0f, 556.0f, -488.0f, 0x4000, 0,
                                           0x4000, 0xFFFF);
            break;
        case 2:
            thisx->flags |= ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
            if (done) {
                Actor_Kill(thisx);
            }
            break;
    }
}

static void MlSkullKid_Destroy(Actor* thisx, PlayState* play) {
    MlSkullKid* this = (MlSkullKid*)thisx;

    if (thisx->params == 1) {
        ResourceMgr_UnregisterSkeleton(&this->skelAnime);
    }
}

// ML reads each flower's flags directly, even after it is freed; check it is still loaded first.
static s32 MlSkullKid_FlowerTargetable(PlayState* play, Actor* flower) {
    Actor* actor;

    if (flower == NULL) {
        return false;
    }
    for (actor = play->actorCtx.actorLists[flower->category].head; actor != NULL; actor = actor->next) {
        if (actor == flower) {
            return (flower->flags & ACTOR_FLAG_ATTENTION_ENABLED) != 0;
        }
    }
    return false;
}

static void MlSkullKid_UpdateSpeaker(MlSkullKid* this, PlayState* play, s32 done) {
    Actor* thisx = &this->actor;

    Actor_SetFocus(thisx, 300.0f);
    if (done) {
        thisx->textId = 0x102A;
        if (this->state < 3) {
            Camera_ChangeDataIdx(&play->mainCamera, 5);
        }
        switch (this->state) {
            case 0:
                thisx->flags |= ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
                Actor_OfferTalk(thisx, play, 300000.0f);
                Camera_ChangeDataIdx(&play->mainCamera, 5);
                if (Message_GetState(&play->msgCtx) != TEXT_STATE_NONE) {
                    this->state = 1;
                }
                break;
            case 1:
                if (Message_GetState(&play->msgCtx) == TEXT_STATE_DONE_FADING) {
                    this->state = 2;
                }
                break;
            case 2:
                if (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE) {
                    this->state = 3;
                }
                break;
            case 3:
                Actor_OfferGetItem(thisx, play, GI_HEART_PIECE, 30000.0f, 30000.0f);
                this->state = 4;
                break;
        }
        return;
    }

    thisx->textId = 0x1029;
    if (thisx->xzDistToPlayer < 40.0f) {
        if (!this->inContact) {
            thisx->flags |= ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
            this->inContact = true;
        } else {
            thisx->flags &= ~ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
        }
    } else {
        thisx->flags &= ~ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
        this->inContact = false;
    }
    Actor_OfferTalkNearColChkInfoCylinder(thisx, play);
    if (thisx->xzDistToPlayer < 100.0f && Message_GetState(&play->msgCtx) != TEXT_STATE_NONE) {
        Camera_ChangeDataIdx(&play->mainCamera, 5);
    }
}

static void MlSkullKid_Update(Actor* thisx, PlayState* play) {
    MlSkullKid* this = (MlSkullKid*)thisx;
    s32 done = Flags_GetSwitch(play, ML_SKJ_SWITCH);

    switch (thisx->params) {
        case 1:
            if (!MlSkullKid_FlowerTargetable(play, this->flowers[0]) &&
                !MlSkullKid_FlowerTargetable(play, this->flowers[1])) {
                Flags_SetSwitch(play, ML_SKJ_SWITCH);
            }
            Actor_UpdateBgCheckInfo(play, thisx, 20.0f, 20.0f, 20.0f, 5);
            SkelAnime_Update(&this->skelAnime);
            break;
        case 2:
            MlSkullKid_UpdateSpeaker(this, play, done);
            break;
    }
}

static void MlSkullKid_Draw(Actor* thisx, PlayState* play) {
    MlSkullKid* this = (MlSkullKid*)thisx;

    if (thisx->params == 1) {
        func_80034BA0(play, &this->skelAnime, NULL, NULL, thisx, 255);
    }
}

void MlSkullKid_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_SkullKid";
    definition.description = "The Missing Link: Deku Road Skull Kid";
    definition.category = ACTORCAT_NPC;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = OBJECT_SKJ;
    definition.instanceSize = sizeof(MlSkullKid);
    definition.init = MlSkullKid_Init;
    definition.destroy = MlSkullKid_Destroy;
    definition.update = MlSkullKid_Update;
    definition.draw = MlSkullKid_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_SkullKid is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
