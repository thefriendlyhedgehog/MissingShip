/*
 * The Missing Link's pause menu, as a code mod: its age rules, slot and world-map positions, cursor rules, ammo counts,
 * map points and arrow colours.
 *
 * MissingShip's Unbound branch reads these from unbound/pause.json (PauseRegistry.cpp, Unbound SPEC §7.9) and applies
 * them inside the kaleido code. A stock Unbound reads none of it, and most of the tables involved are static, so this
 * reads the same document (the layer's missinglink/pause, ml_json.h) and gets the same result another way:
 * - age rules: the three age tables are the game's own and are patched. Play_Init and the item page's trade-item
 *   cycling rewrite some slots' rules, so they are put back each frame of the pause menu.
 * - positions: the pause menu builds its vertices every frame it draws, and its display lists only point at them, so
 *   after the menu is drawn the slots, equipment cells, quest icons and world-map pieces ML moves are moved by the
 *   difference between ML's position and vanilla's (vanilla's own tables below), along with the cursor on them.
 * - ammo: vanilla's count loop is switched off (gAmmoItems) and ML's counts are drawn after the item icons: the
 *   items in ML's ammo slots, each with its vanilla count, placed where ML places it.
 * - cursor rules: the equipment page's vertical steps and the quest page's redirects are applied to where vanilla's
 *   cursor went this frame, from where it was before.
 * - map points: written where the branch writes them, after the menu works them out.
 * - arrow colours: the pulsing colour of the L and R arrows is moved onto ML's colours in the frame's display list.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
// z_kaleido_scope.h declares these without HOST_DATA, which Windows needs for data the game exports (below)
#define gAmmoItems MlUndeclaredAmmoItems
#define gEquipAgeReqs MlUndeclaredEquipAgeReqs
#define gSlotAgeReqs MlUndeclaredSlotAgeReqs
#define gItemAgeReqs MlUndeclaredItemAgeReqs
#include "overlays/misc/ovl_kaleido_scope/z_kaleido_scope.h"
#undef gAmmoItems
#undef gEquipAgeReqs
#undef gSlotAgeReqs
#undef gItemAgeReqs
#include "soh/ModApi/CustomItemRegistry/CustomItemRegistry.h"
#include "soh/ModApi/CustomEquipRegistry/CustomEquipRegistry.h"
#include <libultraship/bridge/consolevariablebridge.h>

#include <stdlib.h>
#include <string.h>

#include "missinglink.h"
#include "ml_json.h"

#define ML_AGE_ANY 9 // vanilla's "no age requirement"
#define ML_QUEST_SLOTS 47
#define ML_MAP_PIECES 32
#define ML_MAP_POINTS 12
#define ML_AMMO_GROUPS 7

extern HOST_DATA u8 gAmmoItems[16];
extern HOST_DATA u8 gEquipAgeReqs[][4];
extern HOST_DATA u8 gSlotAgeReqs[];
extern HOST_DATA u8 gItemAgeReqs[];
void KaleidoScope_DrawAmmoCount(PauseContext* pauseCtx, GraphicsContext* gfxCtx, s16 item, int slot);

// Vanilla's positions (z_kaleido_scope_PAL.c): the equipment columns, the quest icons, the world-map pieces
static const s16 sMlVanillaEquipX[4] = { -114, 12, 44, 76 };
static const s16 sMlVanillaQuestX[ML_QUEST_SLOTS] = {
    74,  74,  46,  18,  18,  46,   -108, -90,  -72, -54, -36, -18, -108, -90, -72, -54,
    -36, -18, 20,  46,  72,  -110, -86,  -110, -54, -98, -86, -74, -62,  -50, -38, -26,
    -14, -98, -86, -74, -62, -50,  -38,  -26,  -14, -88, -81, -72, -90,  -83, -74,
};
static const s16 sMlVanillaQuestY[ML_QUEST_SLOTS] = {
    38, 6,   -12, 6,   38,  56,  -20, -20, -20, -20, -20, -20, 2,   2,   2,   2,   2,   2,  -46, -46, -46, 58, 58, 34,
    58, -52, -52, -52, -52, -52, -52, -52, -52, -52, -52, -52, -52, -52, -52, -52, -52, 34, 34,  34,  36,  36, 36,
};
static const s16 sMlVanillaPieceX[ML_MAP_PIECES] = {
    47, -49, -17, -15, -9, 24, 43, 14, 9, 38, 82, 71, -76, -87, -108, -54,
    -93, -67, -56, -33, -10, 1, 14, 24, 35, 58, 74, 89, 0, -58, 19, 28,
};
static const s16 sMlVanillaPieceY[ML_MAP_PIECES] = {
    15, 40, 11, 45, 52, 37, 36, 57, 54, 33, 31, 45, 32, 42, 49, -10,
    31, 27, 15, -49, 8, 38, 7, 47, 30, 1, -9, 25, 0, 1, -32, -26,
};
// The equipment page's cells by the item each shows: a row per equipment type, column 0 its upgrade
static const s16 sMlEquipCells[4][4] = {
    { ITEM_QUIVER_30, ITEM_SWORD_KOKIRI, ITEM_SWORD_MASTER, ITEM_SWORD_BGS },
    { ITEM_BOMB_BAG_20, ITEM_SHIELD_DEKU, ITEM_SHIELD_HYLIAN, ITEM_SHIELD_MIRROR },
    { ITEM_BRACELET, ITEM_TUNIC_KOKIRI, ITEM_TUNIC_GORON, ITEM_TUNIC_ZORA },
    { ITEM_SCALE_SILVER, ITEM_BOOTS_KOKIRI, ITEM_BOOTS_IRON, ITEM_BOOTS_HOVER },
};
// The seven ammo counts vanilla places, by the item they count, and the slot each count's digits belong to
static const s16 sMlAmmoItems[ML_AMMO_GROUPS] = { ITEM_STICK,     ITEM_NUT,     ITEM_BOMB, ITEM_BOW,
                                                  ITEM_SLINGSHOT, ITEM_BOMBCHU, ITEM_BEAN };
static const s16 sMlAmmoSlots[ML_AMMO_GROUPS] = { 0, 1, 2, 3, 6, 8, 14 };
static const s16 sMlVanillaPulse[2][3] = { { 180, 210, 255 }, { 100, 100, 150 } };

typedef struct {
    s16 to;
    s16 except;
    s16 slot; // -1: an edge instead
    s16 edge;
} MlQuestRedirect;

static struct {
    s16 slotAges[24]; // -1: vanilla's
    s16 itemAges[ITEM_NONE];
    s16 equipAges[4][4];
    s16 gridDX; // the item grid's X, ML's less vanilla's
    u8 ammoSlots[24];
    s32 ammoSlotCount; // -1: vanilla's counts
    s16 ammoDigitSlot[ML_AMMO_GROUPS];
    s16 equipDX[4];
    s16 equipVertical[16]; // the point a vertical step lands on, by the point it would land on
    u8 equipVerticalSet;
    u8 equipAnySlot;
    s16 questDX[ML_QUEST_SLOTS];
    s16 questDY[ML_QUEST_SLOTS];
    s16 questFromRight; // -1: vanilla's
    MlQuestRedirect redirects[16];
    s32 redirectCount;
    s16 pieceDX[ML_MAP_PIECES];
    s16 pieceDY[ML_MAP_PIECES];
    s16 points[ML_MAP_POINTS]; // -2: vanilla's, -1: never shown, else the event flag that shows it
    s16 pulse[2][3];
    u8 pulseSet;
} sMlPause;

static s16 MlPause_Age(const MlJson* value) {
    const char* age = MlJson_Str(value, "");

    return (strcmp(age, "any") == 0)     ? ML_AGE_ANY
           : (strcmp(age, "child") == 0) ? LINK_AGE_CHILD
           : (strcmp(age, "adult") == 0) ? LINK_AGE_ADULT
                                         : -1;
}

static void MlPause_Load(void) {
    const MlJson* doc = MlJson_Load("pause");
    const MlJson* ages = MlJson_Get(doc, "ageRequirements");
    const MlJson* items = MlJson_Get(doc, "items");
    const MlJson* ammo = MlJson_Get(items, "ammo");
    const MlJson* equipment = MlJson_Get(doc, "equipment");
    const MlJson* quest = MlJson_Get(doc, "quest");
    const MlJson* worldMap = MlJson_Get(doc, "worldMap");
    const MlJson* pulse = MlJson_Get(MlJson_Get(doc, "colors"), "infoPanel");
    const MlJson* value;

    memset(&sMlPause, 0, sizeof(sMlPause));
    for (s32 i = 0; i < 24; i++) {
        sMlPause.slotAges[i] = -1;
    }
    for (s32 i = 0; i < ITEM_NONE; i++) {
        sMlPause.itemAges[i] = -1;
    }
    for (s32 i = 0; i < 16; i++) {
        sMlPause.equipAges[i / 4][i % 4] = -1;
        sMlPause.equipVertical[i] = i;
    }
    for (s32 i = 0; i < ML_MAP_POINTS; i++) {
        sMlPause.points[i] = -2;
    }
    for (s32 i = 0; i < ML_AMMO_GROUPS; i++) {
        sMlPause.ammoDigitSlot[i] = sMlAmmoSlots[i];
    }
    sMlPause.ammoSlotCount = -1;
    sMlPause.questFromRight = -1;

    ML_JSON_FOR_EACH(slot, MlJson_Get(ages, "slots")) {
        s32 s = atoi(slot->key);

        if ((s >= 0) && (s < 24)) {
            sMlPause.slotAges[s] = MlPause_Age(slot);
        }
    }
    ML_JSON_FOR_EACH(item, MlJson_Get(ages, "items")) {
        s32 i = atoi(item->key);

        if ((i >= 0) && (i < ITEM_NONE)) {
            sMlPause.itemAges[i] = MlPause_Age(item);
        }
    }
    ML_JSON_FOR_EACH(cell, MlJson_Get(ages, "equipment")) {
        s32 item = atoi(cell->key);

        for (s32 i = 0; i < 16; i++) {
            if (sMlEquipCells[i / 4][i % 4] == item) {
                sMlPause.equipAges[i / 4][i % 4] = MlPause_Age(cell);
            }
        }
    }

    sMlPause.gridDX = MlJson_Int(MlJson_Get(items, "gridX"), -96) + 96;
    if ((value = MlJson_Get(ammo, "slots")) != NULL) {
        sMlPause.ammoSlotCount = 0;
        ML_JSON_FOR_EACH(slot, value) {
            s32 s = MlJson_Int(slot, -1);

            if ((s >= 0) && (s < 24) && (sMlPause.ammoSlotCount < 24)) {
                sMlPause.ammoSlots[sMlPause.ammoSlotCount++] = s;
            }
        }
    }
    ML_JSON_FOR_EACH(digits, MlJson_Get(ammo, "digits")) {
        s32 item = atoi(digits->key);
        s32 slot = MlJson_Int(digits, -1);

        for (s32 g = 0; g < ML_AMMO_GROUPS; g++) {
            if ((sMlAmmoItems[g] == item) && (slot >= 0) && (slot < 24)) {
                sMlPause.ammoDigitSlot[g] = slot;
            }
        }
    }
    // `ammo.full` (ML's 20 Bombchus) is the Bombchu cap's green count, in ml_items.c

    value = MlJson_Get(equipment, "columnsX");
    for (s32 col = 0; col < 4; col++) {
        sMlPause.equipDX[col] = MlJson_Int(MlJson_At(value, col), sMlVanillaEquipX[col]) - sMlVanillaEquipX[col];
    }
    ML_JSON_FOR_EACH(step, MlJson_Get(MlJson_Get(equipment, "cursor"), "vertical")) {
        s32 from = atoi(step->key);
        s32 to = MlJson_Int(step, -1);

        if ((from >= 0) && (from < 16) && (to >= 0) && (to < 16)) {
            sMlPause.equipVertical[from] = to;
            sMlPause.equipVerticalSet = true;
        }
    }
    sMlPause.equipAnySlot = MlJson_IsTrue(MlJson_Get(MlJson_Get(equipment, "cursor"), "anySlot"));

    ML_JSON_FOR_EACH(slot, MlJson_Get(quest, "slots")) {
        s32 s = atoi(slot->key);

        if ((s >= 0) && (s < ML_QUEST_SLOTS) && (slot->count == 2)) {
            sMlPause.questDX[s] = MlJson_Int(MlJson_At(slot, 0), sMlVanillaQuestX[s]) - sMlVanillaQuestX[s];
            sMlPause.questDY[s] = MlJson_Int(MlJson_At(slot, 1), sMlVanillaQuestY[s]) - sMlVanillaQuestY[s];
        }
    }
    sMlPause.questFromRight = MlJson_Int(MlJson_Get(MlJson_Get(quest, "cursor"), "fromRight"), -1);
    ML_JSON_FOR_EACH(redirect, MlJson_Get(MlJson_Get(quest, "cursor"), "redirects")) {
        MlQuestRedirect* r;
        const char* edge = MlJson_Str(MlJson_Get(redirect, "edge"), "");

        if (sMlPause.redirectCount >= ARRAY_COUNT(sMlPause.redirects)) {
            break;
        }
        r = &sMlPause.redirects[sMlPause.redirectCount];
        r->to = MlJson_Int(MlJson_Get(redirect, "to"), -1);
        r->except = MlJson_Int(MlJson_Get(redirect, "except"), -1);
        r->slot = MlJson_Int(MlJson_Get(redirect, "slot"), -1);
        r->edge = (strcmp(edge, "left") == 0)    ? PAUSE_CURSOR_PAGE_LEFT
                  : (strcmp(edge, "right") == 0) ? PAUSE_CURSOR_PAGE_RIGHT
                                                 : 0;
        if ((r->to >= 0) && ((r->slot >= 0) || (r->edge != 0))) {
            sMlPause.redirectCount++;
        }
    }

    ML_JSON_FOR_EACH(piece, MlJson_Get(worldMap, "pieces")) {
        s32 p = atoi(piece->key);

        if ((p >= 0) && (p < ML_MAP_PIECES) && (piece->count == 2)) {
            sMlPause.pieceDX[p] = MlJson_Int(MlJson_At(piece, 0), sMlVanillaPieceX[p]) - sMlVanillaPieceX[p];
            sMlPause.pieceDY[p] = MlJson_Int(MlJson_At(piece, 1), sMlVanillaPieceY[p]) - sMlVanillaPieceY[p];
        }
    }
    ML_JSON_FOR_EACH(point, MlJson_Get(worldMap, "points")) {
        s32 p = atoi(point->key);

        if ((p >= 0) && (p < ML_MAP_POINTS)) {
            sMlPause.points[p] = (point->type == ML_JSON_OBJECT) ? MlJson_Int(MlJson_Get(point, "flag"), -1) : -1;
        }
    }

    if ((pulse != NULL) && (pulse->count == 2)) {
        for (s32 i = 0; i < 2; i++) {
            for (s32 c = 0; c < 3; c++) {
                sMlPause.pulse[i][c] = MlJson_Int(MlJson_At(MlJson_At(pulse, i), c), sMlVanillaPulse[i][c]);
            }
        }
        sMlPause.pulseSet = true;
    }
}

// --- age rules ---

static void MlPause_ApplySlotAges(void) {
    for (s32 s = 0; s < 24; s++) {
        if (sMlPause.slotAges[s] >= 0) {
            gSlotAgeReqs[s] = sMlPause.slotAges[s];
        }
    }
}

static void MlPause_ApplyAges(void) {
    MlPause_ApplySlotAges();
    for (s32 i = 0; i < ITEM_NONE; i++) {
        if (sMlPause.itemAges[i] >= 0) {
            gItemAgeReqs[i] = sMlPause.itemAges[i];
        }
    }
    for (s32 i = 0; i < 16; i++) {
        if (sMlPause.equipAges[i / 4][i % 4] >= 0) {
            gEquipAgeReqs[i / 4][i % 4] = sMlPause.equipAges[i / 4][i % 4];
        }
    }
}

// --- the frame's menu ---

static u8 sMlInDungeon;
static struct {
    s16 pageIndex;
    u16 specialPos;
    u16 questPoint;
    u16 equipPoint;
    u16 equipY;
} sMlBefore;

// KaleidoScopeCall_Draw's condition, and KaleidoScope_Draw's for the pages: the vertices are this frame's
static s32 MlPause_Drawn(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;

    return (R_PAUSE_MENU_MODE >= 3) && (pauseCtx->debugState == 0) &&
           (((pauseCtx->state >= 4) && (pauseCtx->state <= 7)) || ((pauseCtx->state >= 11) && (pauseCtx->state <= 18)));
}

// KaleidoScope_Update, before the menu is drawn: ML's map points (the branch's Unbound_PauseWorldMapPoints), its age
// rules, and where the cursors are
static void MlPause_OnKaleidoscopeUpdate(int16_t inDungeonScene) {
    PauseContext* pauseCtx;

    sMlInDungeon = inDungeonScene;
    MlPause_ApplySlotAges();
    if (gPlayState == NULL) {
        return;
    }
    pauseCtx = &gPlayState->pauseCtx;
    for (s32 p = 0; p < ML_MAP_POINTS; p++) {
        s16 flag = sMlPause.points[p];

        if (flag != -2) {
            pauseCtx->worldMapPoints[p] =
                (flag >= 0) && (gSaveContext.eventChkInf[flag >> 4] & (1 << (flag & 0xF))) ? 1 : 0;
        }
    }
    sMlBefore.pageIndex = pauseCtx->pageIndex;
    sMlBefore.specialPos = pauseCtx->cursorSpecialPos;
    sMlBefore.questPoint = pauseCtx->cursorPoint[PAUSE_QUEST];
    sMlBefore.equipPoint = pauseCtx->cursorPoint[PAUSE_EQUIP];
    sMlBefore.equipY = pauseCtx->cursorY[PAUSE_EQUIP];
}

// After each item slot is drawn: the trade-item cycling rewrote a slot's rule before the loop. After the last slot,
// ML's ammo counts, in vanilla's colours and digits.
static void MlPause_OnKaleidoItemDraw(PlayState* play, int32_t slot, int32_t item, Vtx* vertices) {
    PauseContext* pauseCtx = &play->pauseCtx;

    MlPause_ApplySlotAges();
    if ((slot != 23) || (sMlPause.ammoSlotCount < 0) || CVarGetInteger("gEnhancements.BetterAmmoRendering", 0) ||
        !KaleidoItemManager_IsVanillaPageShown()) {
        return;
    }
    // each count's two digits, moved from the slot vanilla puts them on to ML's
    for (s32 g = 0; g < ML_AMMO_GROUPS; g++) {
        s16 from = sMlAmmoSlots[g];
        s16 to = sMlPause.ammoDigitSlot[g];
        s16 dx = 32 * ((to % 6) - (from % 6));
        s16 dy = -32 * ((to / 6) - (from / 6));

        for (s32 v = 0; (v < 8) && ((dx != 0) || (dy != 0)); v++) {
            pauseCtx->itemVtx[(31 + 2 * g) * 4 + v].v.ob[0] += dx;
            pauseCtx->itemVtx[(31 + 2 * g) * 4 + v].v.ob[1] += dy;
        }
    }
    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCombineLERP(POLY_OPA_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
    CLOSE_DISPS(play->state.gfxCtx);
    for (s32 i = 0; i < sMlPause.ammoSlotCount; i++) {
        u8 slotItem = gSaveContext.inventory.items[sMlPause.ammoSlots[i]];

        for (s32 g = 0; g < ML_AMMO_GROUPS; g++) {
            if (sMlAmmoItems[g] == slotItem) {
                KaleidoScope_DrawAmmoCount(pauseCtx, play->state.gfxCtx, slotItem, sMlAmmoSlots[g]);
            }
        }
    }
}

static void MlPause_MoveQuad(Vtx* quad, s16 dx, s16 dy) {
    for (s32 v = 0; v < 4; v++) {
        quad[v].v.ob[0] += dx;
        quad[v].v.ob[1] += dy;
    }
}

static void MlPause_MoveVertices(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    s16 cursorDX = 0;
    s16 cursorDY = 0;
    s32 digits = 2 * (CVarGetInteger("gEnhancements.BetterAmmoRendering", 0) ? 24 : ML_AMMO_GROUPS);

    // every slot, equipped-item frame and ammo digit of the item page is placed from the item grid
    if ((sMlPause.gridDX != 0) && KaleidoItemManager_IsVanillaPageShown()) {
        for (s32 q = 0; q < 24 + 7 + digits; q++) {
            MlPause_MoveQuad(&pauseCtx->itemVtx[q * 4], sMlPause.gridDX, 0);
        }
        if (pauseCtx->pageIndex == PAUSE_ITEM) {
            cursorDX = sMlPause.gridDX;
        }
    }
    if (KaleidoEquipManager_IsVanillaPageShown()) {
        for (s32 cell = 0; cell < 16; cell++) {
            MlPause_MoveQuad(&pauseCtx->equipVtx[cell * 4], sMlPause.equipDX[cell % 4], 0);
        }
        // the frames on the equipped items (16 + type), placed from the cell of the type's value
        for (s32 type = 0; type < 4; type++) {
            s32 value = CUR_EQUIP_VALUE(type);

            if ((value > 0) && (value < 4)) {
                MlPause_MoveQuad(&pauseCtx->equipVtx[(16 + type) * 4], sMlPause.equipDX[value], 0);
            }
        }
        if (pauseCtx->pageIndex == PAUSE_EQUIP) {
            cursorDX = sMlPause.equipDX[pauseCtx->cursorX[PAUSE_EQUIP] & 3];
        }
    }
    if (KaleidoQuestManager_IsVanillaPageShown()) {
        for (s32 q = 0; q < ML_QUEST_SLOTS; q++) {
            MlPause_MoveQuad(&pauseCtx->questVtx[q * 4], sMlPause.questDX[q], sMlPause.questDY[q]);
        }
        if ((pauseCtx->pageIndex == PAUSE_QUEST) && (pauseCtx->cursorPoint[PAUSE_QUEST] < ML_QUEST_SLOTS)) {
            cursorDX = sMlPause.questDX[pauseCtx->cursorPoint[PAUSE_QUEST]];
            cursorDY = sMlPause.questDY[pauseCtx->cursorPoint[PAUSE_QUEST]];
        }
    }
    // the world map (a dungeon's map page has other vertices): its pieces follow the page's 15 quads, and a point's
    // marker (the cursor's spot) is piece 16 + point
    if (!sMlInDungeon) {
        for (s32 p = 0; p < ML_MAP_PIECES; p++) {
            MlPause_MoveQuad(&pauseCtx->mapPageVtx[60 + p * 4], sMlPause.pieceDX[p], sMlPause.pieceDY[p]);
        }
        if (pauseCtx->pageIndex == PAUSE_MAP) {
            s32 piece = 16 + pauseCtx->cursorPoint[PAUSE_WORLD_MAP];

            if ((piece >= 16) && (piece < ML_MAP_PIECES)) {
                cursorDX = sMlPause.pieceDX[piece];
                cursorDY = sMlPause.pieceDY[piece];
            }
        }
    }
    if ((pauseCtx->cursorSpecialPos == 0) && ((cursorDX != 0) || (cursorDY != 0))) {
        pauseCtx->cursorVtx[0].v.ob[0] += cursorDX;
        pauseCtx->cursorVtx[0].v.ob[1] += cursorDY;
        KaleidoScope_UpdateCursorSize(pauseCtx);
    }
}

// --- cursor rules ---

// z_kaleido_equipment.c's ownership tests; ML's `anySlot` stops on any equipment cell
static s32 MlPause_EquipStop(s32 point, s32 up) {
    s32 row = point / 4;

    if ((point % 4) == 0) {
        return up ? (CUR_UPG_VALUE((row == 0) ? UPG_BULLET_BAG : row) != 0) : (CUR_UPG_VALUE(row) != 0);
    }
    return sMlPause.equipAnySlot || ((gBitFlags[point - 1] & gSaveContext.inventory.equipment) != 0);
}

// The equipment page's vertical move from where the cursor was, with ML's steps
static void MlPause_EquipVertical(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = &play->state.input[0];
    s32 dpad = CVarGetInteger("gSettings.DPadOnPause", 0);
    s32 up = (pauseCtx->stickRelY > 30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DUP));
    s32 down = (pauseCtx->stickRelY < -30) || (dpad && CHECK_BTN_ALL(input->press.button, BTN_DDOWN));
    s32 sideways = (pauseCtx->stickRelX > 30) || (pauseCtx->stickRelX < -30) ||
                   (dpad && CHECK_BTN_ANY(input->press.button, BTN_DLEFT | BTN_DRIGHT));
    s32 point = sMlBefore.equipPoint;
    s32 row = sMlBefore.equipY;

    if (!sMlPause.equipVerticalSet || (pauseCtx->pageIndex != PAUSE_EQUIP) || (sMlBefore.pageIndex != PAUSE_EQUIP) ||
        (pauseCtx->state != 6) || (sMlBefore.specialPos != 0) || (pauseCtx->cursorSpecialPos != 0) ||
        !(up || down) || sideways || (point >= 16) || !KaleidoEquipManager_IsVanillaPageShown()) {
        return;
    }
    for (;;) {
        if ((up && (row == 0)) || (down && (row >= 3))) {
            point = sMlBefore.equipPoint; // nowhere to stop
            break;
        }
        point = sMlPause.equipVertical[point + (up ? -4 : 4)];
        row = point / 4;
        if (MlPause_EquipStop(point, up)) {
            break;
        }
    }
    if (point == pauseCtx->cursorPoint[PAUSE_EQUIP]) {
        return;
    }
    if (pauseCtx->cursorPoint[PAUSE_EQUIP] == sMlBefore.equipPoint) { // vanilla's cursor stayed, so made no sound
        Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }
    pauseCtx->cursorPoint[PAUSE_EQUIP] = point;
    pauseCtx->cursorX[PAUSE_EQUIP] = point % 4;
    pauseCtx->cursorY[PAUSE_EQUIP] = point / 4;
    KaleidoScope_SetCursorVtx(pauseCtx, point * 4, pauseCtx->equipVtx);
}

// A quest-page point and the item it shows (z_kaleido_collect.c)
static void MlPause_QuestPoint(PauseContext* pauseCtx, s16 point) {
    s16 item = PAUSE_ITEM_NONE;

    if (point == 0x18) {
        if ((gSaveContext.inventory.questItems & 0xF0000000) != 0) {
            item = 0x72;
        }
    } else if (CHECK_QUEST_ITEM(point)) {
        item = (point < 6) ? (point + 0x66) : (point < 0x12) ? (point + 0x54) : (point + 0x5A);
    }
    pauseCtx->cursorPoint[PAUSE_QUEST] = point;
    pauseCtx->cursorItem[PAUSE_QUEST] = item;
    pauseCtx->cursorSlot[PAUSE_QUEST] = point;
    KaleidoScope_SetCursorVtx(pauseCtx, point * 4, pauseCtx->questVtx);
}

// The quest page's move this frame, redirected as ML's are: its first step onto `to` from anywhere but `except` goes
// to `slot` or the page's edge; vanilla's cursor always takes the first step it looks at
static void MlPause_QuestCursor(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    s16 point = pauseCtx->cursorPoint[PAUSE_QUEST];

    if ((pauseCtx->pageIndex != PAUSE_QUEST) || (sMlBefore.pageIndex != PAUSE_QUEST) ||
        !KaleidoQuestManager_IsVanillaPageShown() || (pauseCtx->cursorSpecialPos != 0)) {
        return;
    }
    // in from the right arrow, vanilla lands on point 0
    if ((sMlBefore.specialPos == PAUSE_CURSOR_PAGE_RIGHT) && (point == 0) && (sMlPause.questFromRight >= 0)) {
        MlPause_QuestPoint(pauseCtx, sMlPause.questFromRight);
        return;
    }
    if ((sMlBefore.specialPos != 0) || (point == sMlBefore.questPoint)) {
        return;
    }
    for (s32 i = 0; i < sMlPause.redirectCount; i++) {
        MlQuestRedirect* r = &sMlPause.redirects[i];

        if ((point != r->to) || (sMlBefore.questPoint == r->except)) {
            continue;
        }
        if (r->edge != 0) {
            pauseCtx->cursorPoint[PAUSE_QUEST] = sMlBefore.questPoint;
            pauseCtx->cursorSpecialPos = r->edge;
        } else {
            MlPause_QuestPoint(pauseCtx, r->slot);
        }
        return;
    }
}

// --- the arrows' colour ---

// KaleidoScope_DrawInfoPanel pulses the arrow the cursor is on between two colours (function statics); the same point
// between ML's two
static void MlPause_RecolourArrows(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    PauseContext* pauseCtx = &play->pauseCtx;
    const s16(*v)[3] = sMlVanillaPulse;

    if (!sMlPause.pulseSet || (pauseCtx->unk_1E4 != 0) ||
        ((pauseCtx->cursorSpecialPos != PAUSE_CURSOR_PAGE_LEFT) &&
         (pauseCtx->cursorSpecialPos != PAUSE_CURSOR_PAGE_RIGHT))) {
        return;
    }
    for (Gfx* gfx = gfxCtx->polyOpa.p - 2; gfx >= gfxCtx->polyOpa.bufp; gfx--) {
        u32 w0 = (u32)gfx->words.w0;
        u32 next = ((u32)gfx[1].words.w0 >> 24) & 0xFF;
        u32 w1 = (u32)gfx->words.w1;
        s32 r = (w1 >> 24) & 0xFF;
        s32 g = (w1 >> 16) & 0xFF;
        s32 b = (w1 >> 8) & 0xFF;
        s32 a = w1 & 0xFF;
        f32 t;

        // the arrow's colour, then its display list
        if ((((w0 >> 24) & 0xFF) != G_SETPRIMCOLOR) || (a != 220) ||
            ((next != G_DL) && (next != G_DL_OTR_HASH) && (next != G_DL_OTR_FILEPATH) && (next != G_DL_INDEX) &&
             (next != G_MARKER))) {
            continue;
        }
        // on the line between vanilla's two colours
        t = (f32)(r - v[0][0]) / (v[1][0] - v[0][0]);
        if ((t < 0.0f) || (t > 1.0f) || (ABS(g - (v[0][1] + t * (v[1][1] - v[0][1]))) > 2.0f) ||
            (ABS(b - (v[0][2] + t * (v[1][2] - v[0][2]))) > 2.0f)) {
            continue;
        }
        gDPSetPrimColor(gfx, 0, 0, (u8)(sMlPause.pulse[0][0] + t * (sMlPause.pulse[1][0] - sMlPause.pulse[0][0])),
                        (u8)(sMlPause.pulse[0][1] + t * (sMlPause.pulse[1][1] - sMlPause.pulse[0][1])),
                        (u8)(sMlPause.pulse[0][2] + t * (sMlPause.pulse[1][2] - sMlPause.pulse[0][2])), a);
        return;
    }
}

// After the frame is drawn and before its display lists run
static void MlPause_OnGameFrameUpdate(void) {
    PlayState* play = gPlayState;

    if ((play == NULL) || !MlPause_Drawn(play)) {
        return;
    }
    MlPause_EquipVertical(play);
    MlPause_QuestCursor(play);
    MlPause_MoveVertices(play);
    MlPause_RecolourArrows(play);
}

void MlPause_Register(const SOHModApi* api) {
    MlPause_Load();
    MlPause_ApplyAges();
    if (sMlPause.ammoSlotCount >= 0) {
        // vanilla's count loop draws nothing; ML's counts are drawn after the icons
        memset(gAmmoItems, ITEM_NONE, sizeof(gAmmoItems));
    }
    SOH_REGISTER_HOOK(api, OnKaleidoscopeUpdate, MlPause_OnKaleidoscopeUpdate);
    SOH_REGISTER_HOOK(api, OnKaleidoItemDraw, MlPause_OnKaleidoItemDraw);
    SOH_REGISTER_HOOK(api, OnGameFrameUpdate, MlPause_OnGameFrameUpdate);
    api->Log("[MissingLink] ML's pause menu: age rules, positions, cursor rules, ammo counts, map points, colours");
}
