/*
 * The Missing Link's HUD and title cards, as a code mod.
 *
 * ML restyles vanilla's HUD from its code patches (MissingShip ledger rows 60-64): a black drop shadow under the B, C
 * and Start buttons and under the rupee and key counts, a lighter red Start button, a green A note on the ocarina
 * staff, the Soul-Charged sword's own B icon, and its own place-name card: a ribbon across the screen, tinted per
 * scene, in only the scene slots ML gives a title. MissingShip's Unbound branch puts these inside Interface_Draw,
 * Message_ResetOcarinaNoteState and the title card code (z_parameter.c, z_message_PAL.c, z_actor.c, z_demo.c and
 * soh/soh/missinglink/MlTitleCard.c). A mod cannot hook inside those, so:
 *
 * - The shadows are drawn at OnPlayDrawBegin, first in the overlay list and so under the buttons, at the places SoH's
 *   own code computes for the buttons and counters (the HUD editor's positions included). Pausing skips
 *   OnPlayDrawEnd, so that hook would lose them on the pause screen, where ML shows the Start button's shadow. On the
 *   one frame the pause screen copies the screen for its background they are drawn at OnPlayDrawEnd instead, after
 *   the copy, so they stay out of that picture.
 * - The A note's colour is set at OnPlayDrawBegin, which comes before Message_Draw draws the ocarina staff on every
 *   frame, also those Interface_Draw skips.
 * - The Start colour is changed in place: after Interface_Draw, the Start button's colour command is found in the
 *   overlay list and given ML's colour. A Start colour set in SoH's cosmetics editor is kept.
 * - The title card: vanilla's is hidden for the frame (its alpha zeroed while the world draws) and ML's drawn after
 *   the actors, where vanilla draws it. A place card outside ML's slots is cut off when it would appear.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "macros.h"
#include "variables.h"
#include "align_asset_macro.h"
#include "textures/parameter_static/parameter_static.h"
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/cosmetics/cosmeticsTypes.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#include <stdio.h>
#include <string.h>

#include "missinglink.h"

// vanilla's, from z_parameter.c
Gfx* Gfx_TextureIA8(Gfx* displayListHead, void* texture, s16 textureWidth, s16 textureHeight, s16 rectLeft, s16 rectTop,
                    s16 rectWidth, s16 rectHeight, u16 dsdx, u16 dtdy);
Gfx* Gfx_TextureI8(Gfx* displayListHead, void* texture, s16 textureWidth, s16 textureHeight, s16 rectLeft, s16 rectTop,
                   s16 rectWidth, s16 rectHeight, u16 dsdx, u16 dtdy);
extern HOST_DATA Color_RGB8 sOcarinaNoteABtnPrim; // z_message_PAL.c

// ML's ribbon (custom code +0x3BE0), 160x8 IA16, which ML's layer ships under this path
#define dgMlTitleCardBannerTex "__OTR__textures/ml_titlecard/gMlTitleCardBannerTex"
static const ALIGN_ASSET(2) char gMlTitleCardBannerTex[] = dgMlTitleCardBannerTex;

static const char* sMlDigitTextures[] = { gCounterDigit0Tex, gCounterDigit1Tex, gCounterDigit2Tex, gCounterDigit3Tex,
                                          gCounterDigit4Tex, gCounterDigit5Tex, gCounterDigit6Tex, gCounterDigit7Tex,
                                          gCounterDigit8Tex, gCounterDigit9Tex };

static s32 MlHud_CVar(const char* element, const char* field, s32 defaultValue) {
    char name[96];

    snprintf(name, sizeof(name), "gCosmetics.HUD.%s.%s", element, field);
    return CVarGetInteger(name, defaultValue);
}

static f32 MlHud_CVarFloat(const char* element, const char* field, f32 defaultValue) {
    char name[96];

    snprintf(name, sizeof(name), "gCosmetics.HUD.%s.%s", element, field);
    return CVarGetFloat(name, defaultValue);
}

// Where SoH draws a HUD element (Interface_DrawItemButtons, Interface_Draw): at (x, y), measured from the right edge
// for the buttons and from the left for the counters, unless it was moved in SoH's HUD editor. The buttons take the
// top margin, the counters the bottom one.
static void MlHud_Place(const char* element, s32 fromRight, s16 x, s16 y, s16* outX, s16* outY) {
    s32 posType = MlHud_CVar(element, "PosType", 0);
    s32 useMargins = MlHud_CVar(element, "UseMargins", 0) != 0;
    s16 marginL = CVarGetInteger("gCosmetics.HUD.Margin.L", 0);
    s16 marginR = CVarGetInteger("gCosmetics.HUD.Margin.R", 0);
    s16 marginY = 0;
    s16 marginX = 0;

    if (useMargins) {
        marginY = fromRight ? -CVarGetInteger("gCosmetics.HUD.Margin.T", 0)
                            : CVarGetInteger("gCosmetics.HUD.Margin.B", 0);
        if (posType == ORIGINAL_LOCATION) {
            marginX = fromRight ? marginR : marginL;
        }
    }
    if (posType == ORIGINAL_LOCATION) {
        *outX = fromRight ? OTRGetRectDimensionFromRightEdge(x + marginX) : OTRGetRectDimensionFromLeftEdge(x + marginX);
        *outY = y + marginY;
        return;
    }
    *outY = MlHud_CVar(element, "PosY", 0) + marginY;
    switch (posType) {
        case ANCHOR_LEFT:
            *outX = OTRGetDimensionFromLeftEdge(MlHud_CVar(element, "PosX", 0) + (useMargins ? marginL : 0));
            break;
        case ANCHOR_RIGHT:
            *outX = OTRGetDimensionFromRightEdge(MlHud_CVar(element, "PosX", 0) + (useMargins ? marginR : 0));
            break;
        case ANCHOR_NONE:
            *outX = MlHud_CVar(element, "PosX", 0);
            break;
        default:
            *outX = -9999;
            break;
    }
}

typedef struct {
    s16 x;
    s16 y;
    s32 width;
    s32 height;
    s32 widthFactor;
    s32 heightFactor;
} MlHudStart;

// The Start button's place and size (Interface_DrawItemButtons)
static void MlHud_PlaceStart(MlHudStart* start) {
    static const s16 sStartButtonLeftPos[] = { 132, 130, 130, 132 };
    s32 posType = MlHud_CVar("StartButton", "PosType", 0);
    f32 scale = (posType != ORIGINAL_LOCATION) ? MlHud_CVarFloat("StartButton", "Scale", 0.75f) : 0.75f;

    start->width = 32 * scale;
    start->height = 32 * scale;
    start->widthFactor = (1 << 10) * 32 / start->width;
    start->heightFactor = (1 << 10) * 32 / start->height;
    if (posType != ORIGINAL_LOCATION) {
        // the HUD editor's position, less 13 px at the button's scale
        s32 useMargins = MlHud_CVar("StartButton", "UseMargins", 0) != 0;
        f32 posX = MlHud_CVar("StartButton", "PosX", 0) - (scale * 13);

        start->y = MlHud_CVar("StartButton", "PosY", 0) - (scale * 13) +
                   (useMargins ? -CVarGetInteger("gCosmetics.HUD.Margin.T", 0) : 0);
        switch (posType) {
            case ANCHOR_LEFT:
                start->x = OTRGetDimensionFromLeftEdge(
                    posX + (useMargins ? CVarGetInteger("gCosmetics.HUD.Margin.L", 0) : 0));
                break;
            case ANCHOR_RIGHT:
                start->x = OTRGetDimensionFromRightEdge(
                    posX + (useMargins ? CVarGetInteger("gCosmetics.HUD.Margin.R", 0) : 0));
                break;
            case ANCHOR_NONE:
                start->x = MlHud_CVar("StartButton", "PosX", 0);
                break;
            default:
                start->x = -9999;
                break;
        }
        return;
    }
    MlHud_Place("StartButton", true, sStartButtonLeftPos[gSaveContext.language], 16, &start->x, &start->y);
}

// Whether Interface_Draw draws the item buttons and counters this frame
static s32 MlHud_FullUi(PlayState* play) {
    return !CVarGetInteger("gEnhancements.MinimalUI", 0) || !R_MINIMAP_DISABLED || (play->pauseCtx.state != 0);
}

static s32 MlHud_DrawsInterface(PlayState* play) {
    return (gSaveContext.gameMode == GAMEMODE_NORMAL) && !GameInteractor_NoUIActive() &&
           (play->pauseCtx.debugState == 0);
}

// ML's hook over Interface_DrawItemButtons' Graph_OpenDisps: a black drop shadow under each button back, 2 px down and
// right (the Start button's 1 px, and only while the pause menu is open), its alpha capped at 125.
static void MlHud_DrawButtonShadows(PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    PauseContext* pauseCtx = &play->pauseCtx;
    s32 bScaled = 32 * 0.95f;
    s32 bFactor;
    s16 x;
    s16 y;

    OPEN_DISPS(play->state.gfxCtx);

    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 255);

    // B, which also loads the button texture the others reuse
    if (MlHud_CVar("BButton", "PosType", 0) != ORIGINAL_LOCATION) {
        bScaled = 32 * MlHud_CVarFloat("BButton", "Scale", 0.95f);
    }
    bFactor = (1 << 10) * 32 / bScaled;
    MlHud_Place("BButton", true, R_ITEM_BTN_X(0), R_ITEM_BTN_Y(0), &x, &y);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 0, 0, 0, MIN(interfaceCtx->bAlpha, 125));
    OVERLAY_DISP =
        Gfx_TextureIA8(OVERLAY_DISP, (void*)gButtonBackgroundTex, 32, 32, x + 2, y + 2, bScaled, bScaled, bFactor, bFactor);

    // C-Left, C-Down, C-Right
    {
        static const char* sNames[] = { "CLeftButton", "CDownButton", "CRightButton" };
        static const s16 sPos[][2] = { { C_LEFT_BUTTON_X, C_LEFT_BUTTON_Y },
                                       { C_DOWN_BUTTON_X, C_DOWN_BUTTON_Y },
                                       { C_RIGHT_BUTTON_X, C_RIGHT_BUTTON_Y } };
        u16 alphas[] = { interfaceCtx->cLeftAlpha, interfaceCtx->cDownAlpha, interfaceCtx->cRightAlpha };

        for (s32 i = 0; i < 3; i++) {
            s32 button = i + 1;

            MlHud_Place(sNames[i], true, sPos[i][0], sPos[i][1], &x, &y);
            gDPPipeSync(OVERLAY_DISP++);
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 0, 0, 0, MIN(alphas[i], 125));
            gSPWideTextureRectangle(OVERLAY_DISP++, (x + 2) << 2, (y + 2) << 2, (x + 2 + R_ITEM_BTN_WIDTH(button)) << 2,
                                    (y + 2 + R_ITEM_BTN_WIDTH(button)) << 2, G_TX_RENDERTILE, 0, 0,
                                    R_ITEM_BTN_DD(button) << 1, R_ITEM_BTN_DD(button) << 1);
        }
    }

    if ((pauseCtx->state == 6) || (pauseCtx->state == 7)) {
        MlHudStart start;

        MlHud_PlaceStart(&start);
        gDPPipeSync(OVERLAY_DISP++);
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 0, 0, 0, MIN(interfaceCtx->startAlpha, 125));
        gSPWideTextureRectangle(OVERLAY_DISP++, (start.x + 1) << 2, (start.y + 1) << 2, (start.x + 1 + start.width) << 2,
                                (start.y + 1 + start.height) << 2, G_TX_RENDERTILE, 0, 0, start.widthFactor,
                                start.heightFactor);
    }

    gDPPipeSync(OVERLAY_DISP++);

    CLOSE_DISPS(play->state.gfxCtx);
}

// ML's 1 px black shadow under the rupee count, and under the key count in its Shadow Temple slot only (its hook over
// Interface_Draw's Gfx_SetupDL_39Overlay)
static void MlHud_DrawCounterShadows(PlayState* play) {
    static const s16 sRupeeDigitsFirst[] = { 1, 0, 0, 0 };
    static const s16 sRupeeDigitsCount[] = { 2, 3, 3, 3 };
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s16 x;
    s16 y;

    OPEN_DISPS(play->state.gfxCtx);

    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCombineLERP(OVERLAY_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, TEXEL0, 0,
                      PRIMITIVE, 0);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 0, 0, 0, interfaceCtx->magicAlpha);

    if ((play->sceneNum == SCENE_SHADOW_TEMPLE) && (gSaveContext.inventory.dungeonKeys[gSaveContext.mapIndex] >= 0) &&
        GameInteractor_Should(VB_RENDER_KEY_COUNTER, true)) {
        s32 keys = gSaveContext.inventory.dungeonKeys[gSaveContext.mapIndex];
        s32 ones = 16;

        MlHud_Place("SmallKey", false, 26, 190, &x, &y);
        if (keys >= 10) {
            OVERLAY_DISP = Gfx_TextureI8(OVERLAY_DISP, (void*)sMlDigitTextures[(keys / 10) % 10], 8, 16, x + 17, y + 1,
                                         8, 16, 1 << 10, 1 << 10);
            ones = 24;
        }
        OVERLAY_DISP = Gfx_TextureI8(OVERLAY_DISP, (void*)sMlDigitTextures[keys % 10], 8, 16, x + ones + 1, y + 1, 8,
                                     16, 1 << 10, 1 << 10);
    }

    if (GameInteractor_Should(VB_RENDER_RUPEE_COUNTER, true)) {
        s16 digits[3];
        s16 rupees = gSaveContext.rupees;
        s32 wallet = CUR_UPG_VALUE(UPG_WALLET);

        if ((rupees > 9999) || (rupees < 0)) {
            rupees &= 0xDDD;
        }
        digits[0] = MIN(rupees / 100, 9);
        digits[1] = (rupees % 100) / 10;
        digits[2] = rupees % 10;
        MlHud_Place("Rupees", false, 26, 206, &x, &y);
        for (s32 i = 0; i < sRupeeDigitsCount[wallet]; i++) {
            OVERLAY_DISP = Gfx_TextureI8(OVERLAY_DISP, (void*)sMlDigitTextures[digits[sRupeeDigitsFirst[wallet] + i]],
                                         8, 16, x + 16 + 8 * i + 1, y + 1, 8, 16, 1 << 10, 1 << 10);
        }
    }

    gDPPipeSync(OVERLAY_DISP++);

    CLOSE_DISPS(play->state.gfxCtx);
}

// ML's Start button is (223, 73, 73), whatever SoH's colour scheme. Interface_DrawItemButtons draws it as a colour
// command followed by the button's rectangle; find that pair in the overlay list and change the colour.
static void MlHud_RecolourStart(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    PauseContext* pauseCtx = &play->pauseCtx;
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    Color_RGB8 stock = { 200, 0, 0 };
    MlHudStart start;
    Gfx expected[4];
    Gfx* p = expected;

    if (CVarGetInteger("gCosmetics.HUD.StartButton.Changed", 0) || !MlHud_FullUi(play) ||
        !((pauseCtx->state < 8) || (pauseCtx->state >= 18)) || ((pauseCtx->state == 0) && (pauseCtx->debugState == 0))) {
        return;
    }
    if (CVarGetInteger("gCosmetics.DefaultColorScheme", COLORSCHEME_N64) == COLORSCHEME_GAMECUBE) {
        stock = (Color_RGB8){ 120, 120, 120 };
    }
    MlHud_PlaceStart(&start);
    memset(expected, 0, sizeof(expected));
    gDPSetPrimColor(p++, 0, 0, stock.r, stock.g, stock.b, interfaceCtx->startAlpha);
    gSPWideTextureRectangle(p++, start.x << 2, start.y << 2, (start.x + start.width) << 2,
                            (start.y + start.height) << 2, G_TX_RENDERTILE, 0, 0, start.widthFactor,
                            start.heightFactor);

    for (Gfx* gfx = gfxCtx->overlay.bufp; gfx + ARRAY_COUNT(expected) <= gfxCtx->overlay.p; gfx++) {
        if (memcmp(gfx, expected, sizeof(expected)) == 0) {
            gDPSetPrimColor(gfx, 0, 0, 223, 73, 73, interfaceCtx->startAlpha);
            return;
        }
    }
}

// --- the title card (MlTitleCard.c on the branch) ---

static s16 sMlTitleAlpha;
static u8 sMlTitleHidden;
static u8 sMlShadowsAfterCopy; // the pause screen copies the screen this frame: the shadows wait until after the copy

// ML shows a place-name card only in the slots its scene table gives a title (vanilla's titleFile size check, which
// SoH dropped). Anything else, e.g. Deep Woods' grottos, would get ML's ribbon over an empty title (MS-105).
static s32 MlHud_ShowsPlaceName(s16 sceneNum) {
    switch (sceneNum) {
        case SCENE_FOREST_TEMPLE:      // Bmori1
        case SCENE_SHADOW_TEMPLE:      // HAKAdan
        case SCENE_BACK_ALLEY_HOUSE:   // kakariko3
        case SCENE_BOMBCHU_SHOP:       // night_shop
        case SCENE_KAKARIKO_VILLAGE:   // spot01
        case SCENE_KOKIRI_FOREST:      // spot04
        case SCENE_GERUDO_VALLEY:      // spot09
        case SCENE_LOST_WOODS:         // spot10
        case SCENE_GERUDOS_FORTRESS:   // spot12
        case SCENE_LON_LON_RANCH:      // spot20
            return true;
        default:
            return false;
    }
}

// A card is presented once its delay runs out. A place name outside ML's slots ends there, before it fades in. ML's
// first-visit cutscene card (z_demo.c, 0x80064B44) and the player's are at y 80, vanilla's 120; the y only shows in
// slot 0x18, where vanilla's draw is kept.
static void MlHud_OnPresentTitleCard(void) {
    TitleCardContext* titleCtx;

    if (gPlayState == NULL) {
        return;
    }
    titleCtx = &gPlayState->actorCtx.titleCtx;
    if (titleCtx->isBossCard) {
        return;
    }
    if (!MlHud_ShowsPlaceName(gPlayState->sceneNum)) {
        titleCtx->durationTimer = 0;
        titleCtx->alpha = 0;
        return;
    }
    if ((titleCtx->x == 160) && (titleCtx->y == 120) && (titleCtx->width == 144) && (titleCtx->height == 24)) {
        titleCtx->y = 80;
    }
}

// ML draws the card itself (TitleCard_Draw's call in Actor_DrawAll hooked to 0x806037F0) in every scene but slot
// 0x18: its ribbon stretched across the screen at y 66-86 and tinted per scene, under the white title at (88, 68).
// Both fade with the card's alpha. It ignores the card's position, so SoH's title card position settings do not move
// it. ML's layer pads its 144x16 titles into the 144x24 texture 4 rows down, so the texture starts at y 64.
static void MlHud_DrawTitleCard(PlayState* play, TitleCardContext* titleCtx, s16 alpha) {
    u8 r = 0;
    u8 g = 255;
    u8 b = 138;
    s32 x = 88;
    s32 y = 68 - 4;

    switch (play->sceneNum) {
        case SCENE_KOKIRI_FOREST:
        case SCENE_LOST_WOODS:
            r = 255;
            g = 252;
            b = 0;
            break;
        case SCENE_FOREST_TEMPLE:
        case SCENE_BACK_ALLEY_HOUSE:
        case SCENE_BOMBCHU_SHOP:
        case SCENE_KAKARIKO_VILLAGE:
            r = 255;
            g = 0;
            b = 0;
            break;
    }

    OPEN_DISPS(play->state.gfxCtx);

    OVERLAY_DISP = Gfx_SetupDL_52NoCD(OVERLAY_DISP);
    gDPSetCombineLERP(OVERLAY_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, r, g, b, (u8)alpha);
    gDPSetEnvColor(OVERLAY_DISP++, 0, 0, 0, 255);
    gDPLoadTextureBlock(OVERLAY_DISP++, gMlTitleCardBannerTex, G_IM_FMT_IA, G_IM_SIZ_16b, 160, 8, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    gSPTextureRectangle(OVERLAY_DISP++, 0 << 2, 66 << 2, 320 << 2, 86 << 2, G_TX_RENDERTILE, 0, 0, 1 << 9, 0x199);

    gDPPipeSync(OVERLAY_DISP++);
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, (u8)alpha);
    gDPLoadTextureBlock(OVERLAY_DISP++, (uintptr_t)titleCtx->texture, G_IM_FMT_IA, G_IM_SIZ_8b, titleCtx->width,
                        titleCtx->height, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPTextureRectangle(OVERLAY_DISP++, x << 2, y << 2, (x + titleCtx->width) << 2, (y + titleCtx->height) << 2,
                        G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);

    CLOSE_DISPS(play->state.gfxCtx);
}

// --- hooks ---

static void MlHud_DrawShadows(PlayState* play) {
    Gfx_SetupDL_39Overlay(play->state.gfxCtx);
    MlHud_DrawCounterShadows(play);
    MlHud_DrawButtonShadows(play);
}

static void MlHud_OnPlayDrawBegin(void) {
    PlayState* play = gPlayState;
    TitleCardContext* titleCtx;

    if (play == NULL) {
        return;
    }

    // ML's A note is green (0x80106A18). Message_ResetOcarinaNoteState sets it at each ocarina prompt; set here, before
    // Message_Draw draws the staff on every frame (also those Interface_Draw skips). A colour set in SoH's cosmetics
    // editor still applies.
    if (!CVarGetInteger("gCosmetics.HUD.AButton.Changed", 0)) {
        sOcarinaNoteABtnPrim = (Color_RGB8){ 80, 255, 150 };
    }

    // Vanilla's card is hidden while the world draws; OnPlayDrawEnd draws ML's.
    titleCtx = &play->actorCtx.titleCtx;
    sMlTitleHidden = false;
    if ((titleCtx->alpha != 0) && (play->sceneNum != SCENE_SHADOW_TEMPLE_BOSS)) {
        sMlTitleAlpha = titleCtx->alpha;
        sMlTitleHidden = true;
        titleCtx->alpha = 0;
    }

    // The frame the pause screen (or a transition) copies the screen for its background: anything drawn now would stay
    // in that picture, under the live buttons. Play_Draw adds the copy to the overlay list just before OnPlayDrawEnd,
    // and Interface_Draw draws after it, so the pause screen's frame gets its shadows there. A transition's copy skips
    // the rest of the frame, Interface_Draw included.
    sMlShadowsAfterCopy = false;
    if (!MlHud_DrawsInterface(play) || (gTrnsnUnkState == 1) || !MlHud_FullUi(play)) {
        return;
    }
    if (R_PAUSE_MENU_MODE == 1) {
        sMlShadowsAfterCopy = true;
        return;
    }
    MlHud_DrawShadows(play);
}

static void MlHud_OnPlayDrawEnd(void) {
    if ((gPlayState != NULL) && sMlTitleHidden) {
        MlHud_DrawTitleCard(gPlayState, &gPlayState->actorCtx.titleCtx, sMlTitleAlpha);
    }
    if ((gPlayState != NULL) && sMlShadowsAfterCopy) {
        MlHud_DrawShadows(gPlayState);
    }
    sMlShadowsAfterCopy = false;
}

static void MlHud_OnInterfaceDrawEnd(PlayState* play) {
    MlHud_RecolourStart(play);
}

static void MlHud_OnGameFrameUpdate(void) {
    if (sMlTitleHidden) {
        if (gPlayState != NULL) {
            gPlayState->actorCtx.titleCtx.alpha = sMlTitleAlpha;
        }
        sMlTitleHidden = false;
    }
}

// With the Soul-Charged sword (slingshot ammo 40), ML swaps the Kokiri Sword's B icon for its own, which ML's layer
// keeps in the Bullet Bag 40 icon slot
static void MlHud_OnInterfaceResolveButtonIcon(PlayState* play, uint8_t button, uint16_t item, const char** iconPath) {
    if ((button == 0) && (item == ITEM_SWORD_KOKIRI) && (AMMO(ITEM_SLINGSHOT) == 40)) {
        *iconPath = (const char*)gItemIcons[ITEM_BULLET_BAG_40];
    }
}

void MlHud_Register(const SOHModApi* api) {
    SOH_REGISTER_HOOK(api, OnPlayDrawBegin, MlHud_OnPlayDrawBegin);
    SOH_REGISTER_HOOK(api, OnPlayDrawEnd, MlHud_OnPlayDrawEnd);
    SOH_REGISTER_HOOK(api, OnInterfaceDrawEnd, MlHud_OnInterfaceDrawEnd);
    SOH_REGISTER_HOOK(api, OnGameFrameUpdate, MlHud_OnGameFrameUpdate);
    SOH_REGISTER_HOOK(api, OnPresentTitleCard, MlHud_OnPresentTitleCard);
    SOH_REGISTER_HOOK(api, OnInterfaceResolveButtonIcon, MlHud_OnInterfaceResolveButtonIcon);
    api->Log("[MissingLink] ML's HUD and title cards");
}
