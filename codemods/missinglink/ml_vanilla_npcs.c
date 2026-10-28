/*
 * The Missing Link's small edits to vanilla NPCs, as a code mod: the carpenters (En_Daiku), the cucco-pen man
 * (En_Hs), the ranch woman's Birth Certificate (En_Hy), the Kokiri (En_Ko), the Bunny Hood trader (En_Ms), Anju
 * (En_Niw_Lady), the shopkeepers (En_Ossan), the cursed men (En_Ssh) and the readable notes (En_Wonder_Talk).
 *
 * MissingShip's Unbound branch patches these into each actor's code while ML is mounted (ledger rows 6, 7, 22, 46,
 * 53, 74, 87, 103, 123 and the Bmori1 batch; MissingShip docs/UNBOUND_MAP.md). The game spawns these actors by number,
 * so, as the Business Scrubs do, this takes each one's slot (its ActorDB entry) and goes around vanilla's code: it
 * runs ML's rules before or after vanilla's Init and Update, and where ML changed one action function it swaps that
 * function for ML's copy in Update before vanilla's runs.
 *
 * Left out, as no-ops for ML's placements: the carpenters' Thieves' Hideout setup (ML places none there), the Kokiri's
 * spawn-scene and fading rules for scenes other than Kokiri Forest (ML places Kokiri only there).
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ActorDB.h"
#include "soh/OTRGlobals.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "overlays/actors/ovl_En_Daiku/z_en_daiku.h"
#include "overlays/actors/ovl_En_Hs/z_en_hs.h"
#include "overlays/actors/ovl_En_Hy/z_en_hy.h"
#include "overlays/actors/ovl_En_Ko/z_en_ko.h"
#include "overlays/actors/ovl_En_Ms/z_en_ms.h"
#include "overlays/actors/ovl_En_Niw_Lady/z_en_niw_lady.h"
#include "overlays/actors/ovl_En_Ossan/z_en_ossan.h"
#include "overlays/actors/ovl_En_Ssh/z_en_ssh.h"
#include "overlays/actors/ovl_En_Wonder_Talk/z_en_wonder_talk.h"
#include "objects/object_kw1/object_kw1.h"

#include <string.h>

#include "missinglink.h"

ML_HOST_FN_DECL(EnDaiku_TentIdle);
ML_HOST_FN_DECL(EnMs_Sell);
ML_HOST_FN_DECL(EnMs_Talk);
ML_HOST_FN_DECL(EnMs_TalkAfterPurchase);
ML_HOST_FN_DECL(EnMs_Wait);
ML_HOST_FN_DECL(EnOssan_MainActionFunc);
ML_HOST_FN_DECL(EnSsh_Idle);
ML_HOST_FN_DECL(EnSsh_Talk);
ML_HOST_FN_DECL(func_80ABA244);
ML_HOST_FN_DECL(func_80ABA654);
ML_HOST_FN_DECL(func_80ABAC00);
ML_HOST_FN_DECL(func_80ABAC84);
ML_HOST_FN_DECL(func_80B3943C);
ML_HOST_FN_DECL(func_80B395F0);

// ML's Bmori1 event: switch flag 7 of the Forest Temple's slot, where ML builds Bmori1
#define ML_BMORI1_EVENT_DONE() (gSaveContext.sceneFlags[SCENE_FOREST_TEMPLE].swch & (1 << 7))

// Sets or clears an event flag without Flags_SetEventChkInf's hooks
#define ML_EVENTCHKINF_SET_RAW(flag, on)                                              \
    do {                                                                              \
        if (on) {                                                                     \
            gSaveContext.eventChkInf[(flag) >> 4] |= (1 << ((flag) & 0xF));           \
        } else {                                                                      \
            gSaveContext.eventChkInf[(flag) >> 4] &= ~(1 << ((flag) & 0xF));          \
        }                                                                             \
    } while (0)

// vanilla's
void EnDaiku_Update(Actor* thisx, PlayState* play);
void EnDaiku_TentIdle(EnDaiku* this, PlayState* play);
void EnHs_Update(Actor* thisx, PlayState* play);
void EnHy_Update(Actor* thisx, PlayState* play);
void EnKo_Update(Actor* thisx, PlayState* play);
void EnMs_Init(Actor* thisx, PlayState* play);
void EnMs_Update(Actor* thisx, PlayState* play);
void EnMs_Wait(EnMs* this, PlayState* play);
void EnMs_Talk(EnMs* this, PlayState* play);
void EnMs_Sell(EnMs* this, PlayState* play);
void EnMs_TalkAfterPurchase(EnMs* this, PlayState* play);
void EnNiwLady_Update(Actor* thisx, PlayState* play);
void func_80ABA244(EnNiwLady* this, PlayState* play);
void func_80ABA654(EnNiwLady* this, PlayState* play);
void func_80ABAC00(EnNiwLady* this, PlayState* play);
void func_80ABAC84(EnNiwLady* this, PlayState* play);
void EnOssan_Init(Actor* thisx, PlayState* play);
void EnOssan_Update(Actor* thisx, PlayState* play);
void EnOssan_MainActionFunc(EnOssan* this, PlayState* play);
void EnSsh_Update(Actor* thisx, PlayState* play);
void EnSsh_Talk(EnSsh* this, PlayState* play);
void EnSsh_Idle(EnSsh* this, PlayState* play);
void EnSsh_Bob(EnSsh* this, PlayState* play);
void EnWonderTalk_Update(Actor* thisx, PlayState* play);
void func_80B3943C(EnWonderTalk* this, PlayState* play);
void func_80B395F0(EnWonderTalk* this, PlayState* play);

/* En_Daiku: ML's Monktown Valley carpenters (Bmori1 batch) */

#define ENDAIKU_STATE_CAN_TALK 0
#define ENDAIKU_STATE_TALKING 2

// Vanilla's EnDaiku_UpdateTalking, tent branch, which ML plays in Kakariko's slot (its Monktown Valley)
static s32 MlEnDaiku_UpdateTalking(EnDaiku* this, PlayState* play) {
    if ((Message_GetState(&play->msgCtx) == TEXT_STATE_DONE) && Message_ShouldAdvance(play)) {
        switch (this->actor.textId) {
            case 0x6061:
                Flags_SetInfTable(INFTABLE_176);
                break;
            case 0x6064:
                Flags_SetInfTable(INFTABLE_178);
                break;
        }
        return ENDAIKU_STATE_CAN_TALK;
    }
    return ENDAIKU_STATE_TALKING;
}

// Vanilla's EnDaiku_UpdateText, tent branch, in Kakariko's slot
static void MlEnDaiku_UpdateText(EnDaiku* this, PlayState* play) {
    s16 x;
    s16 y;

    if (this->talkState == ENDAIKU_STATE_TALKING) {
        this->talkState = MlEnDaiku_UpdateTalking(this, play);
    } else if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->talkState = ENDAIKU_STATE_TALKING;
    } else {
        Actor_GetScreenPos(play, &this->actor, &x, &y);
        if ((x >= 0) && (x <= 320) && (y >= 0) && (y <= 240) && (this->talkState == ENDAIKU_STATE_CAN_TALK) &&
            (Actor_OfferTalk(&this->actor, play, 100.0f) == 1)) {
            switch (this->actor.params & 3) {
                case 0:
                    this->actor.textId = CHECK_QUEST_ITEM(QUEST_MEDALLION_SPIRIT) ? 0x6060 : 0x605F;
                    break;
                case 1:
                    if (CHECK_QUEST_ITEM(QUEST_MEDALLION_SPIRIT)) {
                        this->actor.textId = 0x6063;
                    } else {
                        this->actor.textId = !Flags_GetInfTable(INFTABLE_176) ? 0x6061 : 0x6062;
                    }
                    break;
                case 2:
                    if (CHECK_QUEST_ITEM(QUEST_MEDALLION_SPIRIT)) {
                        this->actor.textId = 0x6066;
                    } else {
                        this->actor.textId = !Flags_GetInfTable(INFTABLE_178) ? 0x6064 : 0x6065;
                    }
                    break;
                case 3:
                    this->actor.textId = CHECK_QUEST_ITEM(QUEST_MEDALLION_SPIRIT) ? 0x6068 : 0x6067;
                    break;
            }
        }
    }
}

static void MlEnDaiku_TentIdle(EnDaiku* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    MlEnDaiku_UpdateText(this, play);
}

static ActorFunc sEnDaikuUpdate;

static void MlEnDaiku_Update(Actor* thisx, PlayState* play) {
    EnDaiku* this = (EnDaiku*)thisx;

    if (ML_IS_HOST_FN(this->actionFunc, EnDaiku_TentIdle) && (play->sceneNum == SCENE_KAKARIKO_VILLAGE)) {
        this->actionFunc = MlEnDaiku_TentIdle;
    }
    sEnDaikuUpdate(thisx, play);

    // ML removes the carpenters every frame until its Bmori1 event; then params 0 becomes 4. ML next checks whether
    // Kakariko (Adult) music is playing, but the call that would start it is nop'd out, so nothing else happens
    if (!ML_BMORI1_EVENT_DONE()) {
        Actor_Kill(thisx);
    } else if (thisx->params == 0) {
        thisx->params = 4;
    }
}

/* En_Hs: removed every frame until ML's Bmori1 event, which ML_Villager also waits for (ledger row 57) */

static ActorFunc sEnHsUpdate;

static void MlEnHs_Update(Actor* thisx, PlayState* play) {
    sEnHsUpdate(thisx, play);
    if (!ML_BMORI1_EVENT_DONE()) {
        Actor_Kill(thisx);
    }
}

/*
 * En_Hy: ML's Birth Certificate (ledger row 6). ML reskins the Stone of Agony as the Birth Certificate. The ranch
 * woman's line 0x709D ("would you mind delivering this Birth Certificate") offers it, as does 0x709F, which shares the
 * case; vanilla's lost-dog chime on 0x709E and 0x709F is gone. Event flag 0x08 (unused in vanilla) records the errand
 * done. ML does this in En_Hy's talk-state callback when the text has faded in; here it runs just before vanilla's
 * Update, which reaches that callback, and marks the chime as already played.
 */

#define ML_EVENTCHKINF_CERTIFICATE_DELIVERED 0x08

static ActorFunc sEnHyUpdate;

static void MlEnHy_Update(Actor* thisx, PlayState* play) {
    EnHy* this = (EnHy*)thisx;

    if ((this->interactInfo.talkState != NPC_TALK_STATE_IDLE) &&
        (Message_GetState(&play->msgCtx) == TEXT_STATE_DONE_FADING)) {
        switch (this->actor.textId) {
            case 0x709D:
            case 0x709F:
                if (!Flags_GetEventChkInf(ML_EVENTCHKINF_CERTIFICATE_DELIVERED) &&
                    !CHECK_QUEST_ITEM(QUEST_STONE_OF_AGONY)) {
                    Actor_OfferGetItem(&this->actor, play, GI_STONE_OF_AGONY, 120.0f, 10.0f);
                }
                this->playedSfx = true;
                break;
            case 0x709E:
                this->playedSfx = true;
                break;
        }
    }
    sEnHyUpdate(thisx, play);
}

/*
 * En_Ko: ML's hook at the end of Update (ledger rows 53 and 123). Once Link has the Soul-Charged Kokiri Sword
 * (slingshot ammo != 0), these lines give way to their after-the-sword versions. ML's loop runs on over 13 more
 * "pairs" that are its own code; the only one that could match, 0x1040 -> 0x0003, lands after that line is already
 * on screen and no case reads it, so they are left out.
 */
static const u16 sMlSwordTextSwaps[][2] = {
    { 0x1005, 0x1000 }, { 0x1006, 0x1000 }, { 0x100A, 0x101F }, { 0x100B, 0x101F }, { 0x100C, 0x1011 },
    { 0x100D, 0x1011 }, { 0x100E, 0x1012 }, { 0x1019, 0x1012 }, { 0x10D7, 0x1045 }, { 0x10D8, 0x1045 },
    { 0x1046, 0x1044 },
};

// The same hook then patches rows 12-18 of object_kw1's mouth texture in object memory, turning the Kokiri smile into
// a frown, when this Kokiri's legs come from object_kw1. ML's layer ships the frown as its own texture; like ML, it is
// copied over the mouth texture's pixels (the smile is kept to put back once a save without the sword is played). A
// blended-texture swap draws garbage on this CI8 texture
static u8 sMlSmile[32 * 32];
static s32 sMlFrown = false;

static void MlEnKo_SetFrown(s32 frown) {
    u8* mouth = (u8*)ResourceMgr_LoadTexOrDListByName(gKw1MouthTex);

    if (mouth == NULL) {
        return;
    }
    if (frown) {
        memcpy(sMlSmile, mouth, sizeof(sMlSmile));
        memcpy(mouth, ResourceMgr_LoadTexOrDListByName("__OTR__objects/object_kw1/gMlKw1MouthFrownTex"),
               sizeof(sMlSmile));
    } else {
        memcpy(mouth, sMlSmile, sizeof(sMlSmile));
    }
    Gfx_TextureCacheDelete(mouth);
    sMlFrown = frown;
}

// Leaving ML's world: the smile goes back on the game's texture, which stays loaded
void MlVanillaNpcs_LeaveWorld(void) {
    if (sMlFrown) {
        MlEnKo_SetFrown(false);
    }
}

static ActorFunc sEnKoUpdate;

static void MlEnKo_Update(Actor* thisx, PlayState* play) {
    EnKo* this = (EnKo*)thisx;
    s32 i;

    sEnKoUpdate(thisx, play);
    if (thisx->update == NULL) {
        return;
    }
    if (gSaveContext.inventory.ammo[SLOT_SLINGSHOT] == 0) {
        if (sMlFrown) {
            MlEnKo_SetFrown(false);
        }
        return;
    }
    for (i = 0; i < ARRAY_COUNT(sMlSwordTextSwaps); i++) {
        if (this->actor.textId == sMlSwordTextSwaps[i][0]) {
            this->actor.textId = sMlSwordTextSwaps[i][1];
            break;
        }
    }
    if (!sMlFrown && (this->legsObjectBankIdx >= 0) &&
        (play->objectCtx.status[this->legsObjectBankIdx].id == OBJECT_KW1) &&
        ResourceMgr_FileExists("objects/object_kw1/gMlKw1MouthFrownTex")) {
        MlEnKo_SetFrown(true);
    }
}

/*
 * En_Ms: ML turned the bean salesman into a Bunny Hood trader paid in Deku Nuts (ML dma 229, ledger row 103 and the
 * code patches of 2026-09-29): 20 nuts buy the hood and he takes every nut carried; once the hood is in its slot he
 * offers 0x405F and asks 100, more than a nut bag holds. He flashes every 52 frames for 15, counting in
 * colChkInfo.health. ML's option "No Bean Seller Flashing" (gEnhancements.MissingLink.NoBeanSellerFlash, off by
 * default, in Enhancements > Missing Link: ml_text.c) stops the flash.
 */

#define ML_MS_HAS_BUNNY_HOOD() (gSaveContext.inventory.items[SLOT(ITEM_MASK_BUNNY)] == ITEM_MASK_BUNNY)

static void MlEnMs_SetOfferText(EnMs* this, PlayState* play) {
    this->actor.textId = Text_GetFaceReaction(play, 0x1B);
    if (this->actor.textId == 0) {
        this->actor.textId = ML_MS_HAS_BUNNY_HOOD() ? 0x405F : 0x405E;
    }
}

static void MlEnMs_Talk(EnMs* this, PlayState* play);
static void MlEnMs_Sell(EnMs* this, PlayState* play);

// Vanilla's EnMs_Wait with ML's offer text
static void MlEnMs_Wait(EnMs* this, PlayState* play) {
    s16 yawDiff = this->actor.yawTowardsPlayer - this->actor.shape.rot.y;

    MlEnMs_SetOfferText(this, play);
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->actionFunc = MlEnMs_Talk;
    } else if ((this->actor.xzDistToPlayer < 90.0f) && (ABS(yawDiff) < 0x2000)) {
        Actor_OfferTalk(&this->actor, play, 90.0f);
    }
}

// Vanilla's EnMs_Talk with ML's price in Deku Nuts and the Bunny Hood
static void MlEnMs_Talk(EnMs* this, PlayState* play) {
    u8 dialogState = Message_GetState(&play->msgCtx);

    if (dialogState != TEXT_STATE_CHOICE) {
        if ((dialogState == TEXT_STATE_DONE) && Message_ShouldAdvance(play)) {
            this->actionFunc = MlEnMs_Wait;
        }
    } else if (Message_ShouldAdvance(play)) {
        switch (play->msgCtx.choiceIndex) {
            case 0: // yes
                if (AMMO(ITEM_NUT) < (ML_MS_HAS_BUNNY_HOOD() ? 100 : 20)) {
                    Message_ContinueTextbox(play, 0x4069); // not enough
                    return;
                }
                Actor_OfferGetItem(&this->actor, play, GI_MASK_BUNNY, 90.0f, 10.0f);
                this->actionFunc = MlEnMs_Sell;
                return;
            case 1: // no
                Message_ContinueTextbox(play, 0x4068);
            default:
                return;
        }
    }
}

// Vanilla's EnMs_Sell: ML takes every nut rather than the price
static void MlEnMs_Sell(EnMs* this, PlayState* play) {
    if (Actor_HasParent(&this->actor, play)) {
        AMMO(ITEM_NUT) = 0;
        this->actor.parent = NULL;
        ML_SET_HOST_FN(this->actionFunc, EnMs_TalkAfterPurchase);
    } else {
        Actor_OfferGetItem(&this->actor, play, GI_MASK_BUNNY, 90.0f, 10.0f);
    }
}

static ActorFunc sEnMsInit;
static ActorFunc sEnMsUpdate;

static void MlEnMs_Init(Actor* thisx, PlayState* play) {
    EnMs* this = (EnMs*)thisx;

    sEnMsInit(thisx, play);
    if (thisx->update == NULL) {
        return; // killed by vanilla's Init
    }
    MlEnMs_SetOfferText(this, play);
    thisx->colChkInfo.health = 0x2B; // ML's flash counter
}

static void MlEnMs_Update(Actor* thisx, PlayState* play) {
    EnMs* this = (EnMs*)thisx;

    if (ML_IS_HOST_FN(this->actionFunc, EnMs_Wait)) {
        this->actionFunc = MlEnMs_Wait;
    } else if (ML_IS_HOST_FN(this->actionFunc, EnMs_Talk)) {
        this->actionFunc = MlEnMs_Talk;
    } else if (ML_IS_HOST_FN(this->actionFunc, EnMs_Sell)) {
        this->actionFunc = MlEnMs_Sell;
    }
    sEnMsUpdate(thisx, play);

    // ML's flash: colour filter 0x1F78 for 15 frames every 52
    if (++thisx->colChkInfo.health == 0x34) {
        thisx->colChkInfo.health = 0;
        if (!CVarGetInteger("gEnhancements.MissingLink.NoBeanSellerFlash", 0)) {
            thisx->colorFilterParams = 0x1F78;
            thisx->colorFilterTimer = 15;
        }
    }
}

/*
 * En_Niw_Lady: Anju's side of the Birth Certificate (ledger row 7). She no longer counts her cuccos: carrying the
 * certificate counts as all seven returned, which sends her down vanilla's reward path, and the reward is a Piece of
 * Heart instead of the bottle, setting event flag 0x08; after that she says 0x503D ("Cuccos are the best!") instead of
 * her first line. ML sets that line by overwriting her text table; here it is picked where she chooses it.
 */

static s16 sMlMissingCuccoTextIds[] = {
    0x5036, 0x5070, 0x5072, 0x5037, 0x5038, 0x5039, 0x503A, 0x503B, 0x503D, 0x503C,
};

// Vanilla's func_80ABA244 without the pen loop: the certificate is the count
static void MlEnNiwLady_CountCuccos(EnNiwLady* this, PlayState* play) {
    s32 delivered = Flags_GetEventChkInf(ML_EVENTCHKINF_CERTIFICATE_DELIVERED);
    s32 phi_s1;

    if (GameInteractor_Should(VB_SET_CUCCO_COUNT, true, this)) {
        this->cuccosInPen = (!delivered && CHECK_QUEST_ITEM(QUEST_STONE_OF_AGONY)) ? 7 : 0;
    }
    if (BREG(7) != 0) {
        this->cuccosInPen = BREG(7) - 1;
    }
    phi_s1 = this->cuccosInPen;
    if ((Message_GetState(&play->msgCtx) == TEXT_STATE_NONE) || (Message_GetState(&play->msgCtx) == TEXT_STATE_DONE)) {
        this->unk_26E = 101;
    }
    if (this->cuccosInPen >= 7) {
        phi_s1 = 8;
        if ((this->unk_26C < 2) && (this->unk_26C == 0)) {
            phi_s1 = 7;
        }
    }
    if ((this->unk_26C != 0) && (phi_s1 < 7)) {
        phi_s1 = 9;
    }

    this->actor.textId = ((phi_s1 == 0) && delivered) ? 0x503D : sMlMissingCuccoTextIds[phi_s1];
    if (Text_GetFaceReaction(play, 8) != 0) {
        this->actor.textId = Text_GetFaceReaction(play, 8);
        this->unk_262 = TEXT_STATE_DONE;
    }
    if ((this->unk_26C != 0) && (phi_s1 != 9)) {
        phi_s1 = 10;
        this->unk_26E = 11;
    }
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        if (Text_GetFaceReaction(play, 8) == 0) {
            if (this->actor.textId == 0x503C) {
                Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
                this->unk_26C = 2;
                this->unk_262 = TEXT_STATE_EVENT;
                ML_SET_HOST_FN(this->actionFunc, func_80ABA654);
                return;
            }
            this->unk_26E = phi_s1 + 1;
            if (phi_s1 == 7) {
                Sfx_PlaySfxCentered(NA_SE_SY_TRE_BOX_APPEAR);
                this->unk_26C = 1;
                this->unk_262 = TEXT_STATE_EVENT;
                this->unk_26A = this->cuccosInPen;
                gSaveContext.infTable[25] &= 0x1FF;
                ML_SET_HOST_FN(this->actionFunc, func_80ABA654);
                return;
            }
            if (this->unk_26A != this->cuccosInPen) {
                if (this->cuccosInPen < this->unk_26A) {
                    Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
                } else if (phi_s1 + 1 < 9) {
                    Sfx_PlaySfxCentered(NA_SE_SY_TRE_BOX_APPEAR);
                }
            }
            if (this->unk_26A < this->cuccosInPen) {
                this->unk_26A = this->cuccosInPen;
                return;
            }
        }
    } else {
        Actor_OfferTalk(&this->actor, play, 100.0f);
    }
}

// Vanilla's func_80ABA654 with ML's reward: ML leaves the stored item at the bottle, which a missed offer would hand
// out on a later frame; here it is the Piece of Heart
static void MlEnNiwLady_Reward(EnNiwLady* this, PlayState* play) {
    if ((this->unk_262 == Message_GetState(&play->msgCtx)) && Message_ShouldAdvance(play)) {
        Message_CloseTextbox(play);
        this->unk_26E = 0xB;
        if (!Flags_GetItemGetInf(ITEMGETINF_0C)) {
            this->actor.parent = NULL;

            if (GameInteractor_Should(VB_GIVE_ITEM_FROM_ANJU_AS_CHILD, true, this)) {
                this->getItemId = GI_HEART_PIECE;
                Actor_OfferGetItem(&this->actor, play, GI_HEART_PIECE, 100.0f, 50.0f);
                Flags_SetEventChkInf(ML_EVENTCHKINF_CERTIFICATE_DELIVERED);
            } else {
                ML_SET_HOST_FN(this->actionFunc, func_80ABAC84);
                return;
            }
            ML_SET_HOST_FN(this->actionFunc, func_80ABAC00);
            return;
        }
        if (this->unk_26C == 1) {
            this->getItemId = GI_RUPEE_PURPLE;
            Actor_OfferGetItem(&this->actor, play, GI_RUPEE_PURPLE, 100.0f, 50.0f);
            ML_SET_HOST_FN(this->actionFunc, func_80ABAC00);
        }
        ML_SET_HOST_FN(this->actionFunc, func_80ABA244);
    }
}

static ActorFunc sEnNiwLadyUpdate;

static void MlEnNiwLady_Update(Actor* thisx, PlayState* play) {
    EnNiwLady* this = (EnNiwLady*)thisx;

    if (ML_IS_HOST_FN(this->actionFunc, func_80ABA244)) {
        this->actionFunc = MlEnNiwLady_CountCuccos;
    } else if (ML_IS_HOST_FN(this->actionFunc, func_80ABA654)) {
        this->actionFunc = MlEnNiwLady_Reward;
    }
    sEnNiwLadyUpdate(thisx, play);
}

/*
 * En_Ossan: ML wraps the whole Init (Bmori1 batch): once ML's Bmori1 event has happened the shop music starts; before
 * it the shopkeeper is removed. The Bombchu Shop is always open (vanilla removes its keeper until Dodongo's Cavern's
 * blue warp; here the flag is held set while vanilla's Init runs). Link stays drawn when a conversation starts, and
 * the cursor keeps the green it starts with and a fixed blue of 255, so it doesn't pulse.
 */

static ActorFunc sEnOssanInit;
static ActorFunc sEnOssanUpdate;

static void MlEnOssan_Init(Actor* thisx, PlayState* play) {
    s32 dcWarp = Flags_GetEventChkInf(EVENTCHKINF_USED_DODONGOS_CAVERN_BLUE_WARP);

    if (thisx->params == OSSAN_TYPE_BOMBCHUS) {
        ML_EVENTCHKINF_SET_RAW(EVENTCHKINF_USED_DODONGOS_CAVERN_BLUE_WARP, true);
    }
    sEnOssanInit(thisx, play);
    ML_EVENTCHKINF_SET_RAW(EVENTCHKINF_USED_DODONGOS_CAVERN_BLUE_WARP, dcWarp);

    if (ML_BMORI1_EVENT_DONE()) {
        Audio_QueueSeqCmd(NA_BGM_SHOP);
    } else {
        Actor_Kill(thisx);
    }
}

static void MlEnOssan_Update(Actor* thisx, PlayState* play) {
    EnOssan* this = (EnOssan*)thisx;
    Player* player = GET_PLAYER(play);
    s32 wasIdle = ML_IS_HOST_FN(this->actionFunc, EnOssan_MainActionFunc) && (this->stateFlag == OSSAN_STATE_IDLE);
    s32 wasHidden = player->stateFlags2 & PLAYER_STATE2_DISABLE_DRAW;

    sEnOssanUpdate(thisx, play);
    if (!ML_IS_HOST_FN(this->actionFunc, EnOssan_MainActionFunc)) {
        return;
    }
    if (wasIdle && !wasHidden && (this->stateFlag != OSSAN_STATE_IDLE)) {
        player->stateFlags2 &= ~PLAYER_STATE2_DISABLE_DRAW;
    }
    this->cursorColorG = 255;
    this->cursorColorB = 255;
}

/*
 * En_Ssh: ML's cursed men (ledger row 74). ML recompiled En_Ssh with its face reactions and Gold Skulltula token lines
 * gone: in Hollow Root Keep each man says his own line, text 0x22 + params, and when it closes gives a small key once,
 * recorded in the chest flag his params name. ML waits only for the stun timer before offering to talk, and for the
 * player to leave cutscene mode before the key. Vanilla's Idle runs first, then ML's line and offer replace its own.
 */

static void MlEnSsh_Talk(EnSsh* this, PlayState* play) {
    EnSsh_Bob(this, play);
    if (Actor_TextboxIsClosing(&this->actor, play) && !Player_InCsMode(play)) {
        ML_SET_HOST_FN(this->actionFunc, EnSsh_Idle);
        if (!Flags_GetTreasure(play, this->actor.params)) {
            Flags_SetTreasure(play, this->actor.params);
            Actor_OfferGetItem(&this->actor, play, GI_KEY_SMALL, 960.0f, 960.0f);
        }
    }
}

static ActorFunc sEnSshUpdate;

static void MlEnSsh_Update(Actor* thisx, PlayState* play) {
    EnSsh* this = (EnSsh*)thisx;
    s32 wasIdle = ML_IS_HOST_FN(this->actionFunc, EnSsh_Idle);

    if (ML_IS_HOST_FN(this->actionFunc, EnSsh_Talk)) {
        this->actionFunc = MlEnSsh_Talk;
    }
    sEnSshUpdate(thisx, play);
    if (wasIdle && ML_IS_HOST_FN(this->actionFunc, EnSsh_Idle) && (this->unkTimer == 0)) {
        this->actor.textId = Flags_GetTreasure(play, this->actor.params) ? 0x22 : 0x22 + this->actor.params;
        Actor_OfferTalk(&this->actor, play, 100.0f);
    }
}

/*
 * En_Wonder_Talk: ML's readable notes (ledger row 36). ML's Init keeps only the switch (params & 0x3F; 0x3F is not
 * "none") and never removes the actor: once the switch is set it simply stops offering its text. Its one rule is
 * text = params + 0x300 (the Kokiri house note 0xB00, the kakariko3 readables 0x306-0x30C), with a talk state that
 * never matches, so the text is shown and nothing follows it. ML forces flags 0x31 (attention, no update or draw
 * culling) around the action.
 */

#define ML_WONDERTALK_FLAGS \
    (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED)

static void MlEnWonderTalk_SetText(EnWonderTalk* this, PlayState* play) {
    this->actor.textId = this->actor.params + 0x300;
    this->unk_156 = -1;
    ML_SET_HOST_FN(this->actionFunc, func_80B3943C);
}

static void MlEnWonderTalk_Init(Actor* thisx, PlayState* play) {
    EnWonderTalk* this = (EnWonderTalk*)thisx;

    this->switchFlag = this->actor.params & 0x3F;
    this->actionFunc = MlEnWonderTalk_SetText;
    this->unk_15C = 40.0f;
}

static ActorFunc sEnWonderTalkUpdate;

static void MlEnWonderTalk_Update(Actor* thisx, PlayState* play) {
    EnWonderTalk* this = (EnWonderTalk*)thisx;

    thisx->flags |= ML_WONDERTALK_FLAGS;
    sEnWonderTalkUpdate(thisx, play);
    thisx->flags |= ML_WONDERTALK_FLAGS;
    // Vanilla moves on to its talk-state action once the talk starts; ML leaves the action alone
    if (ML_IS_HOST_FN(this->actionFunc, func_80B395F0)) {
        ML_SET_HOST_FN(this->actionFunc, func_80B3943C);
    }
}

void MlVanillaNpcs_Register(const SOHModApi* api) {
    ActorDBEntry* entry;

    entry = MlPatch_Actor(ACTOR_EN_DAIKU);
    sEnDaikuUpdate = entry->update;
    entry->update = MlEnDaiku_Update;

    entry = MlPatch_Actor(ACTOR_EN_HS);
    sEnHsUpdate = entry->update;
    entry->update = MlEnHs_Update;

    entry = MlPatch_Actor(ACTOR_EN_HY);
    sEnHyUpdate = entry->update;
    entry->update = MlEnHy_Update;

    entry = MlPatch_Actor(ACTOR_EN_KO);
    sEnKoUpdate = entry->update;
    entry->update = MlEnKo_Update;

    entry = MlPatch_Actor(ACTOR_EN_MS);
    sEnMsInit = entry->init;
    sEnMsUpdate = entry->update;
    entry->init = MlEnMs_Init;
    entry->update = MlEnMs_Update;

    entry = MlPatch_Actor(ACTOR_EN_NIW_LADY);
    sEnNiwLadyUpdate = entry->update;
    entry->update = MlEnNiwLady_Update;

    entry = MlPatch_Actor(ACTOR_EN_OSSAN);
    sEnOssanInit = entry->init;
    sEnOssanUpdate = entry->update;
    entry->init = MlEnOssan_Init;
    entry->update = MlEnOssan_Update;

    entry = MlPatch_Actor(ACTOR_EN_SSH);
    sEnSshUpdate = entry->update;
    entry->update = MlEnSsh_Update;

    entry = MlPatch_Actor(ACTOR_EN_WONDER_TALK);
    sEnWonderTalkUpdate = entry->update;
    entry->init = MlEnWonderTalk_Init;
    entry->update = MlEnWonderTalk_Update;

    api->Log("[MissingLink] ML's edits to En_Daiku, En_Hs, En_Hy, En_Ko, En_Ms, En_Niw_Lady, En_Ossan, En_Ssh and "
             "En_Wonder_Talk are on");
}
