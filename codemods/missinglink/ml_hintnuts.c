/*
 * The Missing Link's talking Deku Scrubs (ledger row 26), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_HintNuts),
 * registered through the mod API.
 *
 * ML replaced the Deku Tree's hint scrubs (ML dma 300, text 0x7F0) with friendly NPCs. params select the variant:
 *   0x1017, 0x1018  the pair guarding the path to the Deku Grove in Deep Woods. They use ML's own object 0x005
 *                   (object_ml_scrub) and leave once Link owns the Deku Pass (the Gerudo Card quest slot).
 *   0x901B/0x901C,  scrubs on vanilla's object_hintnuts. The Deku Brothers' lines change once switch 5 is set;
 *   0x9024-0x9026,  0x502D answers differently to the Bunny Hood.
 *   0x502D
 *   10, 11          props the scrubs spawn under themselves: a flower (vanilla object) or a pedestal (object 5).
 * They pop up, face Link within 130 units and talk within 60, swapping to a talking animation while the textbox
 * is open. placed by name where ML's rooms place
 * 0x192. A scrub spawns its prop as another of itself (its own id), as ML spawns its own slot.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/object_hintnuts/object_hintnuts.h"
#include "align_asset_macro.h"
#include "soh/ResourceManagerHelpers.h"

#include <stdio.h>

#include "missinglink.h"

typedef struct MlHintNuts {
    Actor actor;
    SkelAnime skelAnime;
    s8 standing;    // the first-frame pop-up has run (vanilla-model variants)
    ColliderCylinder collider;
    s8 talkState;   // 0 idle, 1 conversation opened, 2 talking
    s8 inContact;   // inside 40 units: one-frame auto-accept pulse has fired
    void* eyeTex;
    s16 blinkTimer;
} MlHintNuts;

#define dgMlScrubSkel "__OTR__objects/object_ml_scrub/gMlScrubSkel"
static const ALIGN_ASSET(2) char gMlScrubSkel[] = dgMlScrubSkel;
#define dgMlScrubIdleAnim "__OTR__objects/object_ml_scrub/gMlScrubIdleAnim"
static const ALIGN_ASSET(2) char gMlScrubIdleAnim[] = dgMlScrubIdleAnim;
#define dgMlScrubTalkAnim "__OTR__objects/object_ml_scrub/gMlScrubTalkAnim"
static const ALIGN_ASSET(2) char gMlScrubTalkAnim[] = dgMlScrubTalkAnim;
#define dgMlScrubPedestalDL "__OTR__objects/object_ml_scrub/gMlScrubPedestalDL"
static const ALIGN_ASSET(2) char gMlScrubPedestalDL[] = dgMlScrubPedestalDL;
#define dgMlScrubEyeOpenTex "__OTR__objects/object_ml_scrub/gMlScrubEyeOpenTex"
static const ALIGN_ASSET(2) char gMlScrubEyeOpenTex[] = dgMlScrubEyeOpenTex;
#define dgMlScrubEyeHalfTex "__OTR__objects/object_ml_scrub/gMlScrubEyeHalfTex"
static const ALIGN_ASSET(2) char gMlScrubEyeHalfTex[] = dgMlScrubEyeHalfTex;
#define dgMlScrubEyeClosedTex "__OTR__objects/object_ml_scrub/gMlScrubEyeClosedTex"
static const ALIGN_ASSET(2) char gMlScrubEyeClosedTex[] = dgMlScrubEyeClosedTex;

#define ML_HINTNUTS_FLOWER 10
#define ML_HINTNUTS_PEDESTAL 11
#define ML_HINTNUTS_IS_GUARD(params) ((u16)(params) == 0x1017 || (u16)(params) == 0x1018)
#define ML_HINTNUTS_IS_PROP(params) ((u16)(params) == ML_HINTNUTS_FLOWER || (u16)(params) == ML_HINTNUTS_PEDESTAL)

// ML overlay data +0x830.
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
        TOUCH_ON,
        BUMP_ON,
        OCELEM_ON,
    },
    { 35, 32, 0, { 0, 0, 0 } },
};

static void* sGuardEyeTextures[] = { (void*)gMlScrubEyeOpenTex, (void*)gMlScrubEyeHalfTex, (void*)gMlScrubEyeClosedTex };

static void MlHintNuts_Init(Actor* thisx, PlayState* play) {
    MlHintNuts* this = (MlHintNuts*)thisx;
    u16 params = thisx->params;

    if (params == ML_HINTNUTS_PEDESTAL) {
        thisx->flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
        Actor_SetScale(thisx, 0.013f);
    } else if (params == ML_HINTNUTS_FLOWER) {
        thisx->flags &= ~(ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE);
    } else if (ML_HINTNUTS_IS_GUARD(params)) {
        if (CHECK_QUEST_ITEM(QUEST_GERUDO_CARD)) { // ML's Deku Pass
            Actor_Kill(thisx);
            return;
        }
        SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)gMlScrubSkel, (AnimationHeader*)gMlScrubIdleAnim,
                       NULL, NULL, 0);
        Animation_PlayOnceSetSpeed(&this->skelAnime, (AnimationHeader*)gMlScrubIdleAnim, 0.0f);
        Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)gMlScrubIdleAnim, -5.0f);
        Actor_SpawnAsChild(&play->actorCtx, thisx, play, thisx->id, thisx->world.pos.x, thisx->world.pos.y,
                           thisx->world.pos.z, 0, thisx->shape.rot.y, 0, ML_HINTNUTS_PEDESTAL);
        Actor_SetScale(thisx, 0.013f);
        Actor_SetFocus(thisx, 35.0f);
    } else {
        Actor_SetFocus(thisx, 20.0f);
        SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)gHintNutsSkel, (AnimationHeader*)gHintNutsStandAnim, NULL, NULL, 0);
        Animation_PlayOnceSetSpeed(&this->skelAnime, (AnimationHeader*)gHintNutsUpAnim, 0.0f);
        Actor_SpawnAsChild(&play->actorCtx, thisx, play, thisx->id, thisx->world.pos.x, thisx->world.pos.y,
                           thisx->world.pos.z, 0, thisx->shape.rot.y, 0, ML_HINTNUTS_FLOWER);
    }

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sCylinderInit);
    this->collider.dim.height = 5;
}

static void MlHintNuts_Destroy(Actor* thisx, PlayState* play) {
    MlHintNuts* this = (MlHintNuts*)thisx;

    if (!ML_HINTNUTS_IS_PROP(thisx->params)) {
        Collider_DestroyCylinder(play, &this->collider);
        if (this->skelAnime.skeleton != NULL) { // a guard that left at Init never built its skeleton
            ResourceMgr_UnregisterSkeleton(&this->skelAnime);
        }
    }
}

// ML +0x398: the vanilla-model scrubs pop out of the ground on their first update.
static void MlHintNuts_StandUp(MlHintNuts* this) {
    f32 frame = this->skelAnime.curFrame;

    if (frame < 9.0f) {
        this->collider.dim.height = 5;
    } else if (frame > 12.0f) {
        this->collider.dim.height = 32;
    } else {
        this->collider.dim.height = (s16)((frame - 9.0f) * 9.0f + 5.0f);
    }
    Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)gHintNutsStandAnim, -5.0f);
    this->standing = true;
}

static u16 MlHintNuts_GetTextId(MlHintNuts* this, PlayState* play) {
    switch ((u16)this->actor.params) {
        case 0x901B:
            return Flags_GetSwitch(play, 5) ? 0x101D : 0x101B;
        case 0x901C:
            return Flags_GetSwitch(play, 5) ? 0x101E : 0x101C;
        case 0x9024:
            return 0x1024;
        case 0x9025:
            return 0x1025;
        case 0x9026:
            return 0x1026;
        case 0x1017:
            return 0x1017;
        case 0x1018:
            return 0x1018;
        case 0x502D:
            return (Player_GetMask(play) == PLAYER_MASK_BUNNY) ? 0x102E : 0x102D;
        default:
            return this->actor.textId;
    }
}

static void MlHintNuts_Update(Actor* thisx, PlayState* play) {
    MlHintNuts* this = (MlHintNuts*)thisx;
    bool guard = ML_HINTNUTS_IS_GUARD(thisx->params);

    if (ML_HINTNUTS_IS_PROP(thisx->params)) {
        return;
    }

    if (!this->standing && !guard) {
        MlHintNuts_StandUp(this);
    }

    Collider_UpdateCylinder(thisx, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);

    thisx->textId = MlHintNuts_GetTextId(this, play);

    if (thisx->xzDistToPlayer < 60.0f) {
        if (this->talkState == 1) {
            if (guard) {
                Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)gMlScrubTalkAnim, -5.0f);
                Player_PlaySfx(thisx, 0x387E);
            } else {
                Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)gHintNutsTalkAnim, -5.0f);
            }
            this->talkState = 2;
        } else if (this->talkState == 2) {
            if (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE) {
                if (guard) {
                    Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)gMlScrubIdleAnim, -5.0f);
                } else {
                    Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)gHintNutsStandAnim, -5.0f);
                }
                Player_PlaySfx(thisx, 0x3880);
                this->talkState = 0;
            }
        } else if (Message_GetState(&play->msgCtx) != TEXT_STATE_NONE) {
            this->talkState = 1;
        }

        // Stepping inside 40 units auto-accepts the talk for exactly one frame.
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
    }

    if (thisx->xzDistToPlayer < 130.0f) {
        Math_SmoothStepToS(&thisx->shape.rot.y, thisx->yawTowardsPlayer, 3, 0x400, 0x100);
    }

    if (this->skelAnime.animation != (AnimationHeader*)gMlScrubTalkAnim ||
        this->skelAnime.curFrame != this->skelAnime.endFrame) {
        SkelAnime_Update(&this->skelAnime);
    }

    if (guard) {
        s16 frame;

        if (this->blinkTimer == 0) {
            this->blinkTimer = Rand_S16Offset(30, 30);
        }
        this->blinkTimer--;
        frame = (this->blinkTimer < 2) ? (2 - this->blinkTimer) : 0;
        this->eyeTex = sGuardEyeTextures[frame];
    }
}

static void MlHintNuts_Draw(Actor* thisx, PlayState* play) {
    MlHintNuts* this = (MlHintNuts*)thisx;

    if (this->eyeTex != NULL) {
        OPEN_DISPS(play->state.gfxCtx);
        gSPSegment(POLY_OPA_DISP++, 0x08, this->eyeTex);
        gSPSegment(POLY_XLU_DISP++, 0x08, this->eyeTex);
        CLOSE_DISPS(play->state.gfxCtx);
    }

    switch ((u16)thisx->params) {
        case ML_HINTNUTS_FLOWER:
            Gfx_DrawDListOpa(play, (Gfx*)gHintNutsFlowerDL);
            break;
        case ML_HINTNUTS_PEDESTAL:
            Gfx_DrawDListOpa(play, (Gfx*)gMlScrubPedestalDL);
            break;
        default:
            SkelAnime_DrawOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, NULL, NULL, this);
            break;
    }
}

void MlHintNuts_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_HintNuts";
    definition.description = "The Missing Link: talking Deku Scrub";
    definition.category = ACTORCAT_NPC;
    definition.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED;
    definition.objectId = OBJECT_HINTNUTS;
    definition.instanceSize = sizeof(MlHintNuts);
    definition.init = MlHintNuts_Init;
    definition.destroy = MlHintNuts_Destroy;
    definition.update = MlHintNuts_Update;
    definition.draw = MlHintNuts_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_HintNuts is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
