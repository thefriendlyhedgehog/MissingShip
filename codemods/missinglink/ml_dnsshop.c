/*
 * The Missing Link's grotto scrub shop (ledger rows 27 and 127), as a code mod.
 *
 * ML's Demo_Shd slot (0x119) holds a copy of En_Dns: the grotto scrub in the Desert Colossus's scene slot. MissingShip's
 * Unbound branch registers it as ML_DnsShop on En_Dns's own functions, which tell it apart by its id
 * (soh/soh/missinglink/MlEnDns.c). It keeps vanilla's sale table but for a 99-rupee Piece of Heart; any sale sets switch
 * 9, and once it is set the Piece of Heart row sells a recovery heart, with MissingShip's line 0xF0A2 (shipped in ML's
 * layer) saying so. Here the vanilla functions run unchanged and the shop's differences go around them: its entries
 * after Init, its line after each Update (the shop updates before the player reads the line), and switch 9 inside each
 * entry's payment.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Dns/z_en_dns.h"

#include <stdio.h>

#include "missinglink.h"

#define ML_SHOP_SWITCH 9
#define ML_SHOP_HEART_TEXT 0xF0A2
#define ML_DNS_TYPE_COUNT (DNS_TYPE_DEKU_NUT_UPGRADE + 1)

// vanilla's, from z_en_dns.c
void EnDns_Init(Actor* thisx, PlayState* play);
void EnDns_Destroy(Actor* thisx, PlayState* play);
void EnDns_Update(Actor* thisx, PlayState* play);
void EnDns_Draw(Actor* thisx, PlayState* play);
u32 EnDns_CanBuyPrice(EnDns* this);
void EnDns_PayForHeartPiece(EnDns* this);

static void MlDnsShop_PayForHeartPiece(EnDns* this);

static DnsItemEntry sMlHeartPiece = { 99, 1, GI_HEART_PIECE, EnDns_CanBuyPrice, MlDnsShop_PayForHeartPiece };
static DnsItemEntry sMlShopHeart = { 99, 1, GI_HEART, EnDns_CanBuyPrice, MlDnsShop_PayForHeartPiece };

// Vanilla's entries for the other rows, each paying through MlDnsShop_Pay, which records the sale and then pays as
// vanilla's entry does
static DnsItemEntry sShopEntries[ML_DNS_TYPE_COUNT];
static EnDnsSetRupeesAndFlags sVanillaPay[ML_DNS_TYPE_COUNT];

static void MlDnsShop_PayForHeartPiece(EnDns* this) {
    Flags_SetSwitch(gPlayState, ML_SHOP_SWITCH);
    EnDns_PayForHeartPiece(this);
}

static void MlDnsShop_Pay(EnDns* this) {
    Flags_SetSwitch(gPlayState, ML_SHOP_SWITCH);
    sVanillaPay[this->actor.params](this);
}

static void MlDnsShop_Init(Actor* thisx, PlayState* play) {
    EnDns* this = (EnDns*)thisx;
    s16 params;

    EnDns_Init(thisx, play);
    params = this->actor.params;
    if ((params < 0) || (params >= ML_DNS_TYPE_COUNT) || (this->dnsItemEntry == NULL)) {
        return; // killed by vanilla's Init
    }
    if (params == DNS_TYPE_HEART_PIECE) {
        if (Flags_GetSwitch(play, ML_SHOP_SWITCH)) {
            this->dnsItemEntry = &sMlShopHeart;
            this->actor.textId = ML_SHOP_HEART_TEXT;
        } else {
            this->dnsItemEntry = &sMlHeartPiece;
        }
    } else {
        if (sVanillaPay[params] == NULL) {
            sShopEntries[params] = *this->dnsItemEntry;
            sVanillaPay[params] = this->dnsItemEntry->setRupeesAndFlags;
            sShopEntries[params].setRupeesAndFlags = MlDnsShop_Pay;
        }
        this->dnsItemEntry = &sShopEntries[params];
    }
}

static void MlDnsShop_Update(Actor* thisx, PlayState* play) {
    EnDns* this = (EnDns*)thisx;

    EnDns_Update(thisx, play);
    if ((this->actor.params == DNS_TYPE_HEART_PIECE) && Flags_GetSwitch(play, ML_SHOP_SWITCH)) {
        this->actor.textId = ML_SHOP_HEART_TEXT;
    }
}

void MlDnsShop_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_DnsShop";
    definition.description = "The Missing Link: grotto scrub shop";
    definition.category = ACTORCAT_BG;
    definition.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY;
    definition.objectId = OBJECT_SHOPNUTS;
    definition.instanceSize = sizeof(EnDns);
    definition.init = MlDnsShop_Init;
    definition.destroy = EnDns_Destroy;
    definition.update = MlDnsShop_Update;
    definition.draw = EnDns_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_DnsShop is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
