/*
 * The Missing Link's item rules, shop prices, GetItem models and new save, as a code mod.
 *
 * MissingShip's Unbound branch reads these from ML's layer (unbound/items.json, shops.json, getitem.json, save.json;
 * Unbound SPEC §7.13, §7.12, §7.11, §7.6) and applies them inside the game's code: ItemRegistry.cpp,
 * ShopRegistry.cpp, GetItemDrawRegistry.cpp and NewSaveTemplate.cpp, called from z_parameter.c, z_player_lib.c,
 * z_en_girla.c, z_draw.c, z_sram.c and SkipIntro.cpp. A stock Unbound reads none of them, so this reads the same
 * documents (the layer's missinglink/<name> copies, ml_json.h) and gets the same results another way:
 * - items: the item slot table, the upgrades' capacities and masks are the game's own tables, patched as the branch
 *   does. The strength the hidden bracelet still gives, the Bombchu cap and its green count go through SoH's
 *   VB_USE_STRENGTH_UPGRADE, VB_CHECK_BOMBCHU_CAPACITY, VB_COLOR_AMMO_GREEN and VB_CAN_BUY_BOMBCHUS. The four
 *   empty-bottle slots have no hook: the actors that ask for an empty bottle see ML's slots copied into vanilla's for
 *   the length of their update.
 * - shops: the shop table is static, so a shelf item's price is set after each of its updates (the item resets it to
 *   the table's whenever it is restocked).
 * - getitem: the GetItem draw table is the game's own, patched as the branch does.
 * - save: the branch applies the template as Sram_InitNewSave makes a new file, before the file select writes it and
 *   shows its preview. No hook runs there; OnSaveFile runs right after that first write, so the template is applied
 *   then and the file saved once more, which also gives the file select's preview ML's items. A file that was never
 *   saved in play (cutscene 0xFFF1) gets it again when loaded, for files made before. Skip Intro's destination is set
 *   right after SoH's own handler.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ActorDB.h"
#include "overlays/actors/ovl_En_GirlA/z_en_girla.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/ResourceManagerHelpers.h"
#include "soh/SaveManager.h"

#include <stdlib.h>
#include <string.h>

#include "missinglink.h"
#include "ml_json.h"

// --- items ---

#define ML_ITEMS_WITH_SLOTS 56 // gItemSlots: ITEM_STICK to ITEM_CLAIM_CHECK
#define ML_UPGRADES 8          // UPG_QUIVER to UPG_NUTS
#define ML_VANILLA_BOMBCHUS 50

static u8 sMlVanillaSlots[ML_ITEMS_WITH_SLOTS];
static u32 sMlVanillaMasks[ML_UPGRADES];
static u16 sMlVanillaCapacities[ML_UPGRADES][4];
static s32 sMlBottleSlots[4] = { SLOT_BOTTLE_1, SLOT_BOTTLE_2, SLOT_BOTTLE_3, SLOT_BOTTLE_4 };
static s32 sMlBombchus = ML_VANILLA_BOMBCHUS;
static u8 sMlItemsLoaded;

static void MlItems_Load(void) {
    const MlJson* doc = MlJson_Load("items");
    const MlJson* bottles;

    memcpy(sMlVanillaSlots, gItemSlots, sizeof(sMlVanillaSlots));
    memcpy(sMlVanillaMasks, gUpgradeMasks, sizeof(sMlVanillaMasks));
    memcpy(sMlVanillaCapacities, gUpgradeCapacities, sizeof(sMlVanillaCapacities));
    sMlItemsLoaded = true;
    if (doc == NULL) {
        return;
    }
    ML_JSON_FOR_EACH(slot, MlJson_Get(doc, "slots")) {
        s32 item = atoi(slot->key);
        s32 to = MlJson_Int(slot, -1);

        if ((item >= 0) && (item < ML_ITEMS_WITH_SLOTS) && (to >= 0) && (to <= SLOT_TRADE_CHILD)) {
            gItemSlots[item] = to;
        }
    }
    ML_JSON_FOR_EACH(row, MlJson_Get(doc, "capacities")) {
        s32 upg = atoi(row->key);

        if ((upg >= 0) && (upg < ML_UPGRADES) && (upg != UPG_STRENGTH) && (upg != UPG_SCALE) && (row->count == 4)) {
            for (s32 i = 0; i < 4; i++) {
                gUpgradeCapacities[upg][i] = MlJson_Int(MlJson_At(row, i), gUpgradeCapacities[upg][i]);
            }
        }
    }
    ML_JSON_FOR_EACH(hidden, MlJson_Get(doc, "hiddenUpgrades")) {
        s32 upg = MlJson_Int(hidden, -1);

        if ((upg >= 0) && (upg < ML_UPGRADES)) {
            gUpgradeMasks[upg] = 0;
        }
    }
    bottles = MlJson_Get(doc, "emptyBottleSlots");
    if ((bottles != NULL) && (bottles->count == 4)) {
        for (s32 i = 0; i < 4; i++) {
            sMlBottleSlots[i] = MlJson_Int(MlJson_At(bottles, i), sMlBottleSlots[i]);
        }
    }
    sMlBombchus = MlJson_Int(MlJson_Get(doc, "bombchuCapacity"), ML_VANILLA_BOMBCHUS);
    LUSLOG_INFO("[MissingLink] items: empty bottles in slots %d %d %d %d, %d Bombchus", sMlBottleSlots[0],
                sMlBottleSlots[1], sMlBottleSlots[2], sMlBottleSlots[3], sMlBombchus);
}

// Player_GetStrength: a hidden bracelet (its mask cleared, so the pause menu and NPCs see none) still lifts, read
// through vanilla's mask as the branch's Unbound_UpgradeLevel does.
static void MlItems_Strength(bool* should, va_list args) {
    va_list copy;
    s32* strength;
    s32 level;

    if (!*should || (gUpgradeMasks[UPG_STRENGTH] == sMlVanillaMasks[UPG_STRENGTH])) {
        return;
    }
    va_copy(copy, args);
    strength = va_arg(copy, s32*);
    va_end(copy);
    level = (s32)(gSaveContext.inventory.upgrades & sMlVanillaMasks[UPG_STRENGTH]) >> gUpgradeShifts[UPG_STRENGTH];
    if (CVarGetInteger("gCheats.TimelessEquipment", 0) || LINK_IS_ADULT) {
        *strength = level;
    } else {
        *strength = (level != 0) ? PLAYER_STR_BRACELET : PLAYER_STR_NONE;
    }
    *should = false;
}

// Item_Give and Inventory_ChangeAmmo cap Bombchus at 50 after this asks; ML's cap comes first.
static void MlItems_BombchuCapacity(bool* should, va_list args) {
    if (*should && (AMMO(ITEM_BOMBCHU) > sMlBombchus)) {
        AMMO(ITEM_BOMBCHU) = sMlBombchus;
    }
}

// The HUD's count turns green when full
static void MlItems_AmmoGreen(bool* should, va_list args) {
    va_list copy;
    s32 item;

    va_copy(copy, args);
    item = va_arg(copy, s32); // an s16, promoted
    va_end(copy);
    if ((item == ITEM_BOMBCHU) && (sMlBombchus != ML_VANILLA_BOMBCHUS) && (AMMO(ITEM_BOMBCHU) == sMlBombchus)) {
        *should = true;
    }
}

// A shop's Bombchus can't be bought once the bag is full
static void MlItems_CanBuyBombchus(bool* should, va_list args) {
    va_list copy;
    s32* canBuy;

    if (*should || (AMMO(ITEM_BOMBCHU) < sMlBombchus)) {
        return;
    }
    va_copy(copy, args);
    canBuy = va_arg(copy, s32*);
    va_end(copy);
    *canBuy = CANBUY_RESULT_CANT_GET_NOW;
    *should = true;
}

// Inventory_HasEmptyBottle looks in vanilla's four bottle slots. While an actor that asks it updates, those slots hold
// the items in ML's four, and get their own back after (unless the update put something there).
static u8 sMlBottleSaved[4];
static u8 sMlBottleShown[4];

static void MlItems_ShowBottles(void) {
    u8* items = gSaveContext.inventory.items;
    u8 shown[4];

    for (s32 i = 0; i < 4; i++) {
        shown[i] = items[sMlBottleSlots[i]];
    }
    for (s32 i = 0; i < 4; i++) {
        sMlBottleSaved[i] = items[SLOT_BOTTLE_1 + i];
        sMlBottleShown[i] = shown[i];
        items[SLOT_BOTTLE_1 + i] = shown[i];
    }
}

static void MlItems_HideBottles(void) {
    u8* items = gSaveContext.inventory.items;

    for (s32 i = 0; i < 4; i++) {
        if (items[SLOT_BOTTLE_1 + i] == sMlBottleShown[i]) {
            items[SLOT_BOTTLE_1 + i] = sMlBottleSaved[i];
        }
    }
}

static s32 MlItems_BottlesMoved(void) {
    for (s32 i = 0; i < 4; i++) {
        if (sMlBottleSlots[i] != SLOT_BOTTLE_1 + i) {
            return true;
        }
    }
    return false;
}

// The actors that ask for an empty bottle (En_Ossan runs the shop shelves' En_GirlA checks)
static const s16 sMlBottleActors[] = { ACTOR_EN_POH, ACTOR_EN_DNS, ACTOR_EN_COW,      ACTOR_EN_DS,
                                       ACTOR_EN_TA,  ACTOR_EN_OSSAN, ACTOR_EN_PO_FIELD };
static ActorFunc sMlBottleActorUpdates[ARRAY_COUNT(sMlBottleActors)];

#define ML_BOTTLE_UPDATE(n)                                    \
    static void MlItems_BottleUpdate##n(Actor* thisx, PlayState* play) { \
        MlItems_ShowBottles();                                 \
        sMlBottleActorUpdates[n](thisx, play);                 \
        MlItems_HideBottles();                                 \
    }
ML_BOTTLE_UPDATE(0)
ML_BOTTLE_UPDATE(1)
ML_BOTTLE_UPDATE(2)
ML_BOTTLE_UPDATE(3)
ML_BOTTLE_UPDATE(4)
ML_BOTTLE_UPDATE(5)
ML_BOTTLE_UPDATE(6)
static const ActorFunc sMlBottleUpdates[] = {
    MlItems_BottleUpdate0, MlItems_BottleUpdate1, MlItems_BottleUpdate2, MlItems_BottleUpdate3,
    MlItems_BottleUpdate4, MlItems_BottleUpdate5, MlItems_BottleUpdate6,
};

static void MlItems_WrapBottleActors(void) {
    for (u32 i = 0; i < ARRAY_COUNT(sMlBottleActors); i++) {
        ActorDBEntry* entry = ActorDB_Retrieve(sMlBottleActors[i]);

        sMlBottleActorUpdates[i] = entry->update;
        entry->update = sMlBottleUpdates[i];
    }
}

// --- shops ---

static s16 sMlShopPrices[SI_MAX];

static void MlShops_Load(void) {
    const MlJson* doc = MlJson_Load("shops");

    for (s32 i = 0; i < SI_MAX; i++) {
        sMlShopPrices[i] = -1;
    }
    ML_JSON_FOR_EACH(item, MlJson_Get(doc, "items")) {
        s32 si = atoi(item->key);
        s32 price = MlJson_Int(MlJson_Get(item, "price"), -1);

        if ((si >= 0) && (si < SI_MAX) && (price >= 0) && (price <= 999)) {
            sMlShopPrices[si] = price;
        }
    }
}

static ActorFunc sMlEnGirlAUpdate;

// EnGirlA_WaitForObject sets the table's price whenever the item is (re)stocked; ML's replaces it. Better Bombchu
// Shopping's (and the randomizer's) 99-rupee 10-pack keeps its price, as in the branch.
static void MlShops_EnGirlAUpdate(Actor* thisx, PlayState* play) {
    EnGirlA* this = (EnGirlA*)thisx;
    s16 params = thisx->params;

    sMlEnGirlAUpdate(thisx, play);
    if (!this->isInitialized || (params < 0) || (params >= SI_RANDOMIZED_ITEM) || (sMlShopPrices[params] < 0)) {
        return;
    }
    if ((IS_RANDO || CVarGetInteger("gEnhancements.BetterBombchuShopping", 0)) && (this->getItemId == GI_BOMBCHUS_10)) {
        return;
    }
    this->basePrice = sMlShopPrices[params];
}

// --- GetItem models ---

typedef struct {
    void (*drawFunc)(PlayState*, s16);
    Gfx* dlists[8];
} MlDrawItemTableEntry; // z_draw.c's DrawItemTableEntry

extern HOST_DATA MlDrawItemTableEntry sDrawItemTable[GID_MAXIMUM];

static void MlGetItem_Load(void) {
    const MlJson* doc = MlJson_Load("getitem");
    s32 changed = 0;

    ML_JSON_FOR_EACH(row, MlJson_Get(doc, "draws")) {
        s32 gid = atoi(row->key);
        s32 drawAs = MlJson_Int(MlJson_Get(row, "drawAs"), gid);
        const MlJson* dlists = MlJson_Get(row, "dlists");
        char* paths[8];
        s32 count = (dlists != NULL) ? dlists->count : 0;
        s32 valid = (gid >= 0) && (gid < GID_MAXIMUM) && (drawAs >= 0) && (drawAs < GID_MAXIMUM) && (count >= 1) &&
                    (count <= 8);

        for (s32 i = 0; valid && (i < count); i++) {
            const char* path = MlJson_Str(MlJson_At(dlists, i), "");

            valid = ResourceMgr_FileExists(path);
            if (!valid) {
                LUSLOG_ERROR("[MissingLink] getitem: %s is in no mounted archive; GetItem model %d keeps vanilla's",
                             path, gid);
            }
        }
        if (!valid) {
            continue;
        }
        for (s32 i = 0; i < count; i++) {
            const char* path = MlJson_Str(MlJson_At(dlists, i), "");

            // the game's asset strings: "__OTR__" + the path; malloc's alignment is ALIGN_ASSET's or better
            paths[i] = malloc(strlen(path) + 8);
            strcpy(paths[i], "__OTR__");
            strcat(paths[i], path);
        }
        sDrawItemTable[gid].drawFunc = sDrawItemTable[drawAs].drawFunc;
        for (s32 i = 0; i < 8; i++) {
            sDrawItemTable[gid].dlists[i] = (i < count) ? (Gfx*)paths[i] : NULL;
        }
        changed++;
    }
    LUSLOG_INFO("[MissingLink] getitem: %d GetItem model(s) changed", changed);
}

// --- the new save ---

static const MlJson* sMlSave;
static u8 sMlFreshFile; // the file loaded was never saved in play; cleared once a scene starts

// Sram_InitNewSave's end in the branch (Unbound_ApplyNewSaveTemplate)
static void MlSave_Apply(void) {
    const MlJson* value;

    ML_JSON_FOR_EACH(item, MlJson_Get(sMlSave, "items")) {
        s32 slot = atoi(item->key);

        if ((slot >= 0) && (slot < ARRAY_COUNT(gSaveContext.inventory.items))) {
            gSaveContext.inventory.items[slot] = MlJson_Int(item, ITEM_NONE);
        }
    }
    if ((value = MlJson_Get(sMlSave, "equipment")) != NULL) {
        gSaveContext.inventory.equipment = MlJson_Int(value, 0);
    }
    if ((value = MlJson_Get(sMlSave, "equipped")) != NULL) {
        gSaveContext.equips.equipment = MlJson_Int(value, 0);
    }
    if ((value = MlJson_Get(sMlSave, "upgrades")) != NULL) {
        gSaveContext.inventory.upgrades = (u32)value->num;
    }
    if ((value = MlJson_Get(sMlSave, "questItems")) != NULL) {
        gSaveContext.inventory.questItems = (u32)value->num;
    }
    if ((value = MlJson_Get(sMlSave, "magic")) != NULL) {
        gSaveContext.magic = MlJson_Int(value, 0);
    }
    ML_JSON_FOR_EACH(button, MlJson_Get(sMlSave, "buttonItems")) {
        s32 index = atoi(button->key);

        if ((index >= 0) && (index < ARRAY_COUNT(gSaveContext.equips.buttonItems))) {
            gSaveContext.equips.buttonItems[index] = MlJson_Int(button, ITEM_NONE);
        }
    }
    ML_JSON_FOR_EACH(button, MlJson_Get(sMlSave, "cButtonSlots")) {
        s32 index = atoi(button->key);

        if ((index >= 0) && (index < ARRAY_COUNT(gSaveContext.equips.cButtonSlots))) {
            gSaveContext.equips.cButtonSlots[index] = MlJson_Int(button, SLOT_NONE);
        }
    }
}

// SaveManager::LoadFile, before the file-select screen moves magic into its fill target. A new file keeps the opening
// cutscene's 0xFFF1 until its first save in play, so applying the template again until then changes nothing.
static void MlSave_OnLoadFile(int32_t fileNum) {
    sMlFreshFile = (gSaveContext.cutsceneIndex == 0xFFF1);
    if (sMlFreshFile && (sMlSave != NULL)) {
        MlSave_Apply();
    }
}

// SaveManager::SaveFileThreaded, on the save thread, after Sram_InitSave wrote a new file and refreshed its preview
// from gSaveContext; the file select waits for the save thread meanwhile. The template goes on gSaveContext and the
// file is saved again, which refreshes the preview too. A file made with the debug save starts at cutscene 0 and is
// left alone, as Sram_InitDebugSave leaves it on the branch.
static u8 sMlResaving;

static void MlSave_OnSaveFile(int32_t fileNum, int32_t sectionID) {
    if (sMlResaving) {
        sMlResaving = false;
        return;
    }
    if ((sMlSave == NULL) || (gPlayState != NULL) || (sectionID != SECTION_ID_BASE) || (fileNum < 0) ||
        (fileNum > 2) || (fileNum != gSaveContext.fileNum) || (gSaveContext.cutsceneIndex != 0xFFF1)) {
        return;
    }
    MlSave_Apply();
    sMlResaving = true;
    Save_SaveFile(); // queued behind this save on the save thread, which the file select waits for
    LUSLOG_INFO("[MissingLink] new file %d made with ML's new save", fileNum);
}

static void MlSave_OnSceneInit(int16_t sceneNum) {
    sMlFreshFile = false;
}

// Skip Intro: SoH's handler (which runs first) has skipped the opening in Link's house; ML's opening ends at its
// `skipIntro` entrance and time instead (the branch's SkipIntro.cpp).
static void MlSave_TransitionCs(bool* should, va_list args) {
    const MlJson* skip = MlJson_Get(sMlSave, "skipIntro");
    s32 entrance = MlJson_Int(MlJson_Get(skip, "entrance"), -1);
    s32 time = MlJson_Int(MlJson_Get(skip, "time"), -1);

    if (*should || !sMlFreshFile || IS_RANDO || (entrance < 0) ||
        !CVarGetInteger("gEnhancements.TimeSavers.SkipCutscene.Intro", 0) ||
        (gSaveContext.entranceIndex != ENTR_LINKS_HOUSE_CHILD_SPAWN) || (gSaveContext.cutsceneIndex != 0)) {
        return;
    }
    gSaveContext.entranceIndex = entrance;
    if ((time >= 0) && (time <= 0xFFFF)) {
        gSaveContext.dayTime = gSaveContext.skyboxTime = time;
    }
}

void MlItems_Register(const SOHModApi* api) {
    MlItems_Load();
    MlShops_Load();
    MlGetItem_Load();
    sMlSave = MlJson_Load("save");

    api->RegisterVB(VB_USE_STRENGTH_UPGRADE, MlItems_Strength);
    api->RegisterVB(VB_CHECK_BOMBCHU_CAPACITY, MlItems_BombchuCapacity);
    api->RegisterVB(VB_COLOR_AMMO_GREEN, MlItems_AmmoGreen);
    api->RegisterVB(VB_CAN_BUY_BOMBCHUS, MlItems_CanBuyBombchus);
    if (MlItems_BottlesMoved()) {
        MlItems_WrapBottleActors();
    }
    {
        ActorDBEntry* entry = ActorDB_Retrieve(ACTOR_EN_GIRLA);

        sMlEnGirlAUpdate = entry->update;
        entry->update = MlShops_EnGirlAUpdate;
    }
    SOH_REGISTER_HOOK(api, OnLoadFile, MlSave_OnLoadFile);
    SOH_REGISTER_HOOK(api, OnSaveFile, MlSave_OnSaveFile);
    SOH_REGISTER_HOOK(api, OnSceneInit, MlSave_OnSceneInit);
    api->RegisterVB(VB_PLAY_TRANSITION_CS, MlSave_TransitionCs);
    api->Log("[MissingLink] ML's item rules, shop prices, GetItem models and new save");
}
