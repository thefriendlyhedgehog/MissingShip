/*
 * The Missing Link's ending card (ledger row 92), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_Ending), registered through the mod API.
 *
 * Description: The Missing Link's ending card in spot10, Deep Woods (ML's rewrite of En_Dodongo, slot 0x012)
 * Ported from MissingShip's SoH fork (ovl_Ml_Ending, ledger row 92).
 *
 * Source overlay: ML dma 159, text 0x840, on object_sa. Saria plays her ocarina, eyes closed, translucent (alpha 200).
 * After 250 frames she fades out at 5 a frame; from frame 349 "The End" and "The Legend of Zelda: - The Missing
 * Link -" fade in, tinted (170, 130, 95), and the timer stops at 400. The three text images are IA8 pictures in the
 * overlay's own data (shipped as ml_ovl_ending).
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/Enhancements/gameplaystats.h"
#include "objects/object_sa/object_sa.h"
#include "align_asset_macro.h"

#include <stdio.h>

#include "missinglink.h"

#define ML_ENDING_LIMB_MAX 17

// ML's layout (size 0x270), for reference against its overlay; SoH's own offsets differ.
typedef struct MlEnding {
    /* 0x000 */ Actor actor;
    /* 0x14C */ SkelAnime skelAnime;
    /* 0x18C */ Vec3s jointTable[ML_ENDING_LIMB_MAX];
    /* 0x1F2 */ Vec3s morphTable[ML_ENDING_LIMB_MAX];
    /* 0x258 */ s16 alpha;      // Saria's, 200 then fading to 0
    /* 0x25C */ u32 textColor;  // RGBA; the text fades in through the low byte
    /* 0x260 */ u32 timer;
} MlEnding;

#define dgMlEndingTheEndTex "__OTR__objects/ml_ovl_ending/gMlEndingTheEndTex"
static const ALIGN_ASSET(2) char gMlEndingTheEndTex[] = dgMlEndingTheEndTex;
#define dgMlEndingTitleLeftTex "__OTR__objects/ml_ovl_ending/gMlEndingTitleLeftTex"
static const ALIGN_ASSET(2) char gMlEndingTitleLeftTex[] = dgMlEndingTitleLeftTex;
#define dgMlEndingTitleRightTex "__OTR__objects/ml_ovl_ending/gMlEndingTitleRightTex"
static const ALIGN_ASSET(2) char gMlEndingTitleRightTex[] = dgMlEndingTitleRightTex;

typedef struct {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ const char* texture;
    /* 0x0C */ s16 width;
    /* 0x0E */ s16 height;
} MlEndingText;

// ML draws them in this order; the title's two halves sit side by side at y 188.
static MlEndingText sTexts[] = {
    { 88.0f, 188.0f, gMlEndingTitleLeftTex, 72, 32 },
    { 160.0f, 188.0f, gMlEndingTitleRightTex, 72, 32 },
    { 120.0f, 108.0f, gMlEndingTheEndTex, 80, 24 },
};

static void MlEnding_Init(Actor* thisx, PlayState* play) {
    MlEnding* this = (MlEnding*)thisx;

    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)gSariaSkel, (AnimationHeader*)gSariaPlayingOcarinaAnim,
                       this->jointTable, this->morphTable, ML_ENDING_LIMB_MAX);
    this->alpha = 200;
    this->textColor = 0xAA825F00;

    // MissingShip (MS-145): the ending card is ML's end of the game, so stop Gameplay Stats' timer here ("Game
    // Completed", SoH's Ganon timestamp). It is only placed in Deep Woods' ending cutscene header.
    if ((play->sceneNum == SCENE_LOST_WOODS) && !gSaveContext.ship.stats.gameComplete) {
        gSaveContext.ship.stats.itemTimestamp[TIMESTAMP_DEFEAT_GANON] = GAMEPLAYSTAT_TOTAL_TIME;
        gSaveContext.ship.stats.gameComplete = true;
    }
}

static void MlEnding_Destroy(Actor* thisx, PlayState* play) {
}

static void MlEnding_Update(Actor* thisx, PlayState* play) {
    MlEnding* this = (MlEnding*)thisx;

    SkelAnime_Update(&this->skelAnime);
    if (this->timer < 250) {
        this->timer++;
        return;
    }
    if (this->alpha != 0) {
        Math_StepToS(&this->alpha, 0, 5);
    }
    if (this->timer < 349) {
        this->timer++;
        return;
    }
    if ((this->textColor & 0xFF) != 0xFF) {
        this->textColor += 5;
    }
    if (this->timer != 400) {
        this->timer++;
    }
}

// ML's sprite helper (+0xFC), at the scale ML uses (1:1, top-left anchored).
static Gfx* MlEnding_DrawText(Gfx* gfx, MlEndingText* text, u32 color) {
    s32 x = (s32)text->x;
    s32 y = (s32)text->y;

    Gfx_SetupDL_39Ptr(&gfx);
    gDPSetCombineLERP(gfx++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
    gDPSetPrimColor(gfx++, 0, 0, (color >> 24) & 0xFF, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
    gDPSetEnvColor(gfx++, 0, 0, 0, 255);
    gDPPipeSync(gfx++);
    gDPLoadTextureBlock(gfx++, text->texture, G_IM_FMT_IA, G_IM_SIZ_8b, text->width, text->height, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    gSPTextureRectangle(gfx++, x << 2, y << 2, (x + text->width) << 2, (y + text->height) << 2, G_TX_RENDERTILE, 0, 0,
                        1 << 10, 1 << 10);
    return gfx;
}

static void MlEnding_Draw(Actor* thisx, PlayState* play) {
    MlEnding* this = (MlEnding*)thisx;
    s32 i;

    OPEN_DISPS(play->state.gfxCtx);

    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gSariaEyeClosedTex));
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(gSariaEyeClosedTex));
    gSPSegment(POLY_OPA_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(gSariaMouthClosed2Tex));
    gSPSegment(POLY_XLU_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(gSariaEyeClosedTex));
    gSPSegment(POLY_XLU_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(gSariaEyeClosedTex));
    gSPSegment(POLY_XLU_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(gSariaMouthClosed2Tex));

    if (this->alpha == 255) {
        func_80034BA0(play, &this->skelAnime, NULL, NULL, &this->actor, this->alpha);
    } else if (this->alpha != 0) {
        func_80034CC4(play, &this->skelAnime, NULL, NULL, &this->actor, this->alpha);
    }

    if ((this->textColor & 0xFF) != 0) {
        for (i = 0; i < ARRAY_COUNT(sTexts); i++) {
            OVERLAY_DISP = MlEnding_DrawText(OVERLAY_DISP, &sTexts[i], this->textColor);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void MlEnding_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_Ending";
    definition.description = "The Missing Link: ending card";
    definition.category = ACTORCAT_NPC;
    definition.actorFlags = (ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED);
    definition.objectId = OBJECT_SA;
    definition.instanceSize = sizeof(MlEnding);
    definition.init = MlEnding_Init;
    definition.destroy = MlEnding_Destroy;
    definition.update = MlEnding_Update;
    definition.draw = MlEnding_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_Ending is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
