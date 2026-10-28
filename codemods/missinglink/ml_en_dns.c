/*
 * The Missing Link's Business Scrubs (ledger row 27), as a code mod.
 *
 * ML recompiled En_Dns. Its sale table (data +0x90): the Piece of Heart costs 99, the Deku Shield 1, the Green Potion
 * row sells Bugs, and the nut-upgrade row costs 20 because it is the Deku Baba garden's entry fee: that scrub opens the
 * garden gate (switch 0x27) instead of handing over an item, and leaves once the gate is open. The stick-upgrade row
 * sells ML's "bottle of Deku Grove air", an empty bottle, and turns into the Bugs sale once the Magic Bean slot is
 * filled.
 *
 * MissingShip's Unbound branch patches these into En_Dns's code while ML is mounted (soh/soh/missinglink/MlEnDns.c).
 * The game spawns the scrubs by number, so, as the Blade Beam does, this takes En_Dns's slot (its ActorDB entry) and
 * goes around vanilla's code: ML's Init rules before vanilla's Init and ML's sale table after it; and since vanilla's
 * sale helper is called from two action functions, Update swaps those two for ML's copies, which offer ML's bottle,
 * take the garden fee and end that sale at once. ML_DnsShop (ml_dnsshop.c) runs En_Dns's functions under its own
 * entry, so none of this reaches it.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ActorDB.h"
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "overlays/actors/ovl_En_Dns/z_en_dns.h"

#include "missinglink.h"

ML_HOST_FN_DECL(EnDns_Sale);
ML_HOST_FN_DECL(EnDns_SetupBurrow);
ML_HOST_FN_DECL(EnDns_SetupSale);

#define ML_GARDEN_SWITCH 0x27

// vanilla's, from z_en_dns.c
void EnDns_Init(Actor* thisx, PlayState* play);
void EnDns_Update(Actor* thisx, PlayState* play);
void EnDns_OfferSaleItem(EnDns* this, PlayState* play);
void EnDns_SetupSale(EnDns* this, PlayState* play);
void EnDns_Sale(EnDns* this, PlayState* play);
void EnDns_SetupBurrow(EnDns* this, PlayState* play);
u32 EnDns_CanBuyPrice(EnDns* this);
u32 EnDns_CanBuyDekuShield(EnDns* this);
u32 EnDns_CanBuyBottle(EnDns* this);
void EnDns_PayPrice(EnDns* this);
void EnDns_PayForHeartPiece(EnDns* this);
void EnDns_PayForDekuNutUpgrade(EnDns* this);

static DnsItemEntry sMlHeartPiece = { 99, 1, GI_HEART_PIECE, EnDns_CanBuyPrice, EnDns_PayForHeartPiece };
static DnsItemEntry sMlDekuShield = { 1, 1, GI_SHIELD_DEKU, EnDns_CanBuyDekuShield, EnDns_PayPrice };
static DnsItemEntry sMlBugs = { 40, 1, GI_BUGS, EnDns_CanBuyBottle, EnDns_PayPrice };
static DnsItemEntry sMlGardenFee = { 20, 1, GI_NUT_UPGRADE_30, EnDns_CanBuyPrice, EnDns_PayForDekuNutUpgrade };

static void MlEnDns_Init(Actor* thisx, PlayState* play) {
    EnDns* this = (EnDns*)thisx;

    // The garden scrub leaves once its gate is open; the air-bottle scrub sells Bugs once the Magic Bean slot is filled
    if ((this->actor.params == DNS_TYPE_DEKU_NUT_UPGRADE) && Flags_GetSwitch(play, ML_GARDEN_SWITCH)) {
        Actor_Kill(&this->actor);
        return;
    }
    if ((this->actor.params == DNS_TYPE_DEKU_STICK_UPGRADE) && (gSaveContext.inventory.items[SLOT_BEAN] != ITEM_NONE)) {
        this->actor.params = DNS_TYPE_GREEN_POTION;
    }
    EnDns_Init(thisx, play);
    if (this->dnsItemEntry == NULL) {
        return; // killed by vanilla's Init
    }
    switch (this->actor.params) {
        case DNS_TYPE_HEART_PIECE:
            this->dnsItemEntry = &sMlHeartPiece;
            break;
        case DNS_TYPE_DEKU_SHIELD:
            this->dnsItemEntry = &sMlDekuShield;
            break;
        case DNS_TYPE_GREEN_POTION:
            this->dnsItemEntry = &sMlBugs;
            break;
        case DNS_TYPE_DEKU_NUT_UPGRADE:
            this->dnsItemEntry = &sMlGardenFee;
            break;
    }
}

// Vanilla's EnDns_OfferSaleItem with ML's two rows in front of it
static void MlEnDns_OfferSaleItem(EnDns* this, PlayState* play) {
    GetItemEntry itemEntry;

    if ((this->actor.params == DNS_TYPE_DEKU_STICK_UPGRADE) && (CUR_UPG_VALUE(UPG_STICKS) < 2)) {
        itemEntry = ItemTable_Retrieve(GI_BOTTLE);
        gSaveContext.ship.pendingSale = itemEntry.itemId;
        gSaveContext.ship.pendingSaleMod = itemEntry.modIndex;
        Actor_OfferGetItem(&this->actor, play, GI_BOTTLE, 130.0f, 100.0f);
        return;
    }
    if ((this->actor.params == DNS_TYPE_DEKU_NUT_UPGRADE) && (CUR_UPG_VALUE(UPG_NUTS) < 2)) {
        // ML clears the pending rupee change and offers nothing; the fee is paid as the scrub burrows
        if (!Flags_GetSwitch(play, ML_GARDEN_SWITCH)) {
            Flags_SetSwitch(play, ML_GARDEN_SWITCH);
            gSaveContext.rupeeAccumulator = 0;
        }
        return;
    }
    EnDns_OfferSaleItem(this, play);
}

static void MlEnDns_Sale(EnDns* this, PlayState* play);

// Vanilla's EnDns_SetupSale, offering through ML's helper
static void MlEnDns_SetupSale(EnDns* this, PlayState* play) {
    if ((Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && Message_ShouldAdvance(play)) {
        Message_CloseTextbox(play);
        if (GameInteractor_Should(VB_GIVE_ITEM_FROM_BUSINESS_SCRUB, true, this)) {
            MlEnDns_OfferSaleItem(this, play);
        }
        this->actionFunc = MlEnDns_Sale;
    }
}

// Vanilla's EnDns_Sale, offering through ML's helper; the garden fee hands over nothing, so its sale is over at once
static void MlEnDns_Sale(EnDns* this, PlayState* play) {
    if (Actor_HasParent(&this->actor, play) || !GameInteractor_Should(VB_GIVE_ITEM_FROM_BUSINESS_SCRUB, true, this) ||
        (this->actor.params == DNS_TYPE_DEKU_NUT_UPGRADE)) {
        this->actor.parent = NULL;
        ML_SET_HOST_FN(this->actionFunc, EnDns_SetupBurrow);
    } else {
        MlEnDns_OfferSaleItem(this, play);
    }
}

static void MlEnDns_Update(Actor* thisx, PlayState* play) {
    EnDns* this = (EnDns*)thisx;

    if (ML_IS_HOST_FN(this->actionFunc, EnDns_SetupSale)) {
        this->actionFunc = MlEnDns_SetupSale;
    } else if (ML_IS_HOST_FN(this->actionFunc, EnDns_Sale)) {
        this->actionFunc = MlEnDns_Sale;
    }
    EnDns_Update(thisx, play);
}

void MlEnDns_Register(const SOHModApi* api) {
    ActorDBEntry* entry = MlPatch_Actor(ACTOR_EN_DNS);

    entry->init = MlEnDns_Init;
    entry->update = MlEnDns_Update;
    api->Log("[MissingLink] En_Dns's slot runs ML's Business Scrubs");
}
