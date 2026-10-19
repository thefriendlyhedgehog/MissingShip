/*
 * The Missing Link's illustrated prologue ("But one day...", ledger row 29), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_Prologue), registered through the mod
 * API. Placed once in ML's intro stub scene (the link_home slot). It shows five 320x192 CI4 illustrations from ML's
 * object_ml_prologue, switching at frames 445, 695, 945 and 1195 while the prologue cutscene's text plays, drawn
 * screen-space as 24 strips of 320x8 from line 24 to 216 (as ML does, each strip fits TMEM).
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "align_asset_macro.h"

#include <stdio.h>

#include "missinglink.h"

typedef struct MlPrologue {
    Actor actor;
    s16 picture; // 0-4
    s32 timer;
} MlPrologue;

static const ALIGN_ASSET(2) char sTlut0[] = "__OTR__objects/object_ml_prologue/gMlPrologue0TLUT";
static const ALIGN_ASSET(2) char sStrip0_00[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip00Tex";
static const ALIGN_ASSET(2) char sStrip0_01[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip01Tex";
static const ALIGN_ASSET(2) char sStrip0_02[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip02Tex";
static const ALIGN_ASSET(2) char sStrip0_03[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip03Tex";
static const ALIGN_ASSET(2) char sStrip0_04[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip04Tex";
static const ALIGN_ASSET(2) char sStrip0_05[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip05Tex";
static const ALIGN_ASSET(2) char sStrip0_06[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip06Tex";
static const ALIGN_ASSET(2) char sStrip0_07[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip07Tex";
static const ALIGN_ASSET(2) char sStrip0_08[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip08Tex";
static const ALIGN_ASSET(2) char sStrip0_09[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip09Tex";
static const ALIGN_ASSET(2) char sStrip0_10[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip10Tex";
static const ALIGN_ASSET(2) char sStrip0_11[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip11Tex";
static const ALIGN_ASSET(2) char sStrip0_12[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip12Tex";
static const ALIGN_ASSET(2) char sStrip0_13[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip13Tex";
static const ALIGN_ASSET(2) char sStrip0_14[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip14Tex";
static const ALIGN_ASSET(2) char sStrip0_15[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip15Tex";
static const ALIGN_ASSET(2) char sStrip0_16[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip16Tex";
static const ALIGN_ASSET(2) char sStrip0_17[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip17Tex";
static const ALIGN_ASSET(2) char sStrip0_18[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip18Tex";
static const ALIGN_ASSET(2) char sStrip0_19[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip19Tex";
static const ALIGN_ASSET(2) char sStrip0_20[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip20Tex";
static const ALIGN_ASSET(2) char sStrip0_21[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip21Tex";
static const ALIGN_ASSET(2) char sStrip0_22[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip22Tex";
static const ALIGN_ASSET(2) char sStrip0_23[] = "__OTR__objects/object_ml_prologue/gMlPrologue0Strip23Tex";
static const ALIGN_ASSET(2) char sTlut1[] = "__OTR__objects/object_ml_prologue/gMlPrologue1TLUT";
static const ALIGN_ASSET(2) char sStrip1_00[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip00Tex";
static const ALIGN_ASSET(2) char sStrip1_01[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip01Tex";
static const ALIGN_ASSET(2) char sStrip1_02[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip02Tex";
static const ALIGN_ASSET(2) char sStrip1_03[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip03Tex";
static const ALIGN_ASSET(2) char sStrip1_04[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip04Tex";
static const ALIGN_ASSET(2) char sStrip1_05[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip05Tex";
static const ALIGN_ASSET(2) char sStrip1_06[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip06Tex";
static const ALIGN_ASSET(2) char sStrip1_07[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip07Tex";
static const ALIGN_ASSET(2) char sStrip1_08[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip08Tex";
static const ALIGN_ASSET(2) char sStrip1_09[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip09Tex";
static const ALIGN_ASSET(2) char sStrip1_10[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip10Tex";
static const ALIGN_ASSET(2) char sStrip1_11[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip11Tex";
static const ALIGN_ASSET(2) char sStrip1_12[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip12Tex";
static const ALIGN_ASSET(2) char sStrip1_13[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip13Tex";
static const ALIGN_ASSET(2) char sStrip1_14[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip14Tex";
static const ALIGN_ASSET(2) char sStrip1_15[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip15Tex";
static const ALIGN_ASSET(2) char sStrip1_16[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip16Tex";
static const ALIGN_ASSET(2) char sStrip1_17[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip17Tex";
static const ALIGN_ASSET(2) char sStrip1_18[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip18Tex";
static const ALIGN_ASSET(2) char sStrip1_19[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip19Tex";
static const ALIGN_ASSET(2) char sStrip1_20[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip20Tex";
static const ALIGN_ASSET(2) char sStrip1_21[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip21Tex";
static const ALIGN_ASSET(2) char sStrip1_22[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip22Tex";
static const ALIGN_ASSET(2) char sStrip1_23[] = "__OTR__objects/object_ml_prologue/gMlPrologue1Strip23Tex";
static const ALIGN_ASSET(2) char sTlut2[] = "__OTR__objects/object_ml_prologue/gMlPrologue2TLUT";
static const ALIGN_ASSET(2) char sStrip2_00[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip00Tex";
static const ALIGN_ASSET(2) char sStrip2_01[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip01Tex";
static const ALIGN_ASSET(2) char sStrip2_02[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip02Tex";
static const ALIGN_ASSET(2) char sStrip2_03[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip03Tex";
static const ALIGN_ASSET(2) char sStrip2_04[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip04Tex";
static const ALIGN_ASSET(2) char sStrip2_05[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip05Tex";
static const ALIGN_ASSET(2) char sStrip2_06[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip06Tex";
static const ALIGN_ASSET(2) char sStrip2_07[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip07Tex";
static const ALIGN_ASSET(2) char sStrip2_08[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip08Tex";
static const ALIGN_ASSET(2) char sStrip2_09[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip09Tex";
static const ALIGN_ASSET(2) char sStrip2_10[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip10Tex";
static const ALIGN_ASSET(2) char sStrip2_11[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip11Tex";
static const ALIGN_ASSET(2) char sStrip2_12[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip12Tex";
static const ALIGN_ASSET(2) char sStrip2_13[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip13Tex";
static const ALIGN_ASSET(2) char sStrip2_14[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip14Tex";
static const ALIGN_ASSET(2) char sStrip2_15[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip15Tex";
static const ALIGN_ASSET(2) char sStrip2_16[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip16Tex";
static const ALIGN_ASSET(2) char sStrip2_17[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip17Tex";
static const ALIGN_ASSET(2) char sStrip2_18[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip18Tex";
static const ALIGN_ASSET(2) char sStrip2_19[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip19Tex";
static const ALIGN_ASSET(2) char sStrip2_20[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip20Tex";
static const ALIGN_ASSET(2) char sStrip2_21[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip21Tex";
static const ALIGN_ASSET(2) char sStrip2_22[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip22Tex";
static const ALIGN_ASSET(2) char sStrip2_23[] = "__OTR__objects/object_ml_prologue/gMlPrologue2Strip23Tex";
static const ALIGN_ASSET(2) char sTlut3[] = "__OTR__objects/object_ml_prologue/gMlPrologue3TLUT";
static const ALIGN_ASSET(2) char sStrip3_00[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip00Tex";
static const ALIGN_ASSET(2) char sStrip3_01[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip01Tex";
static const ALIGN_ASSET(2) char sStrip3_02[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip02Tex";
static const ALIGN_ASSET(2) char sStrip3_03[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip03Tex";
static const ALIGN_ASSET(2) char sStrip3_04[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip04Tex";
static const ALIGN_ASSET(2) char sStrip3_05[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip05Tex";
static const ALIGN_ASSET(2) char sStrip3_06[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip06Tex";
static const ALIGN_ASSET(2) char sStrip3_07[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip07Tex";
static const ALIGN_ASSET(2) char sStrip3_08[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip08Tex";
static const ALIGN_ASSET(2) char sStrip3_09[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip09Tex";
static const ALIGN_ASSET(2) char sStrip3_10[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip10Tex";
static const ALIGN_ASSET(2) char sStrip3_11[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip11Tex";
static const ALIGN_ASSET(2) char sStrip3_12[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip12Tex";
static const ALIGN_ASSET(2) char sStrip3_13[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip13Tex";
static const ALIGN_ASSET(2) char sStrip3_14[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip14Tex";
static const ALIGN_ASSET(2) char sStrip3_15[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip15Tex";
static const ALIGN_ASSET(2) char sStrip3_16[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip16Tex";
static const ALIGN_ASSET(2) char sStrip3_17[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip17Tex";
static const ALIGN_ASSET(2) char sStrip3_18[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip18Tex";
static const ALIGN_ASSET(2) char sStrip3_19[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip19Tex";
static const ALIGN_ASSET(2) char sStrip3_20[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip20Tex";
static const ALIGN_ASSET(2) char sStrip3_21[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip21Tex";
static const ALIGN_ASSET(2) char sStrip3_22[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip22Tex";
static const ALIGN_ASSET(2) char sStrip3_23[] = "__OTR__objects/object_ml_prologue/gMlPrologue3Strip23Tex";
static const ALIGN_ASSET(2) char sTlut4[] = "__OTR__objects/object_ml_prologue/gMlPrologue4TLUT";
static const ALIGN_ASSET(2) char sStrip4_00[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip00Tex";
static const ALIGN_ASSET(2) char sStrip4_01[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip01Tex";
static const ALIGN_ASSET(2) char sStrip4_02[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip02Tex";
static const ALIGN_ASSET(2) char sStrip4_03[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip03Tex";
static const ALIGN_ASSET(2) char sStrip4_04[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip04Tex";
static const ALIGN_ASSET(2) char sStrip4_05[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip05Tex";
static const ALIGN_ASSET(2) char sStrip4_06[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip06Tex";
static const ALIGN_ASSET(2) char sStrip4_07[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip07Tex";
static const ALIGN_ASSET(2) char sStrip4_08[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip08Tex";
static const ALIGN_ASSET(2) char sStrip4_09[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip09Tex";
static const ALIGN_ASSET(2) char sStrip4_10[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip10Tex";
static const ALIGN_ASSET(2) char sStrip4_11[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip11Tex";
static const ALIGN_ASSET(2) char sStrip4_12[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip12Tex";
static const ALIGN_ASSET(2) char sStrip4_13[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip13Tex";
static const ALIGN_ASSET(2) char sStrip4_14[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip14Tex";
static const ALIGN_ASSET(2) char sStrip4_15[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip15Tex";
static const ALIGN_ASSET(2) char sStrip4_16[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip16Tex";
static const ALIGN_ASSET(2) char sStrip4_17[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip17Tex";
static const ALIGN_ASSET(2) char sStrip4_18[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip18Tex";
static const ALIGN_ASSET(2) char sStrip4_19[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip19Tex";
static const ALIGN_ASSET(2) char sStrip4_20[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip20Tex";
static const ALIGN_ASSET(2) char sStrip4_21[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip21Tex";
static const ALIGN_ASSET(2) char sStrip4_22[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip22Tex";
static const ALIGN_ASSET(2) char sStrip4_23[] = "__OTR__objects/object_ml_prologue/gMlPrologue4Strip23Tex";

static const char* sTluts[5] = { sTlut0, sTlut1, sTlut2, sTlut3, sTlut4 };
static const char* sStrips[5][24] = {
    { sStrip0_00, sStrip0_01, sStrip0_02, sStrip0_03, sStrip0_04, sStrip0_05, sStrip0_06, sStrip0_07, sStrip0_08, sStrip0_09, sStrip0_10, sStrip0_11, sStrip0_12, sStrip0_13, sStrip0_14, sStrip0_15, sStrip0_16, sStrip0_17, sStrip0_18, sStrip0_19, sStrip0_20, sStrip0_21, sStrip0_22, sStrip0_23 },
    { sStrip1_00, sStrip1_01, sStrip1_02, sStrip1_03, sStrip1_04, sStrip1_05, sStrip1_06, sStrip1_07, sStrip1_08, sStrip1_09, sStrip1_10, sStrip1_11, sStrip1_12, sStrip1_13, sStrip1_14, sStrip1_15, sStrip1_16, sStrip1_17, sStrip1_18, sStrip1_19, sStrip1_20, sStrip1_21, sStrip1_22, sStrip1_23 },
    { sStrip2_00, sStrip2_01, sStrip2_02, sStrip2_03, sStrip2_04, sStrip2_05, sStrip2_06, sStrip2_07, sStrip2_08, sStrip2_09, sStrip2_10, sStrip2_11, sStrip2_12, sStrip2_13, sStrip2_14, sStrip2_15, sStrip2_16, sStrip2_17, sStrip2_18, sStrip2_19, sStrip2_20, sStrip2_21, sStrip2_22, sStrip2_23 },
    { sStrip3_00, sStrip3_01, sStrip3_02, sStrip3_03, sStrip3_04, sStrip3_05, sStrip3_06, sStrip3_07, sStrip3_08, sStrip3_09, sStrip3_10, sStrip3_11, sStrip3_12, sStrip3_13, sStrip3_14, sStrip3_15, sStrip3_16, sStrip3_17, sStrip3_18, sStrip3_19, sStrip3_20, sStrip3_21, sStrip3_22, sStrip3_23 },
    { sStrip4_00, sStrip4_01, sStrip4_02, sStrip4_03, sStrip4_04, sStrip4_05, sStrip4_06, sStrip4_07, sStrip4_08, sStrip4_09, sStrip4_10, sStrip4_11, sStrip4_12, sStrip4_13, sStrip4_14, sStrip4_15, sStrip4_16, sStrip4_17, sStrip4_18, sStrip4_19, sStrip4_20, sStrip4_21, sStrip4_22, sStrip4_23 },
};

static void MlPrologue_Init(Actor* thisx, PlayState* play) {
}

static void MlPrologue_Destroy(Actor* thisx, PlayState* play) {
}

// ML +0x008: advance the picture on fixed frames; stop counting on the last one.
static void MlPrologue_Update(Actor* thisx, PlayState* play) {
    MlPrologue* this = (MlPrologue*)thisx;

    switch (this->timer) {
        case 445:
            this->picture = 1;
            break;
        case 695:
            this->picture = 2;
            break;
        case 945:
            this->picture = 3;
            break;
        case 1195:
            this->picture = 4;
            return;
        default:
            if (this->picture == 4) {
                return;
            }
            break;
    }
    this->timer++;
}

// ML +0x090: (PRIM - ENV) * TEXEL0 + ENV with white prim / black env, i.e. the picture as-is.
static void MlPrologue_Draw(Actor* thisx, PlayState* play) {
    MlPrologue* this = (MlPrologue*)thisx;
    s32 i;

    OPEN_DISPS(play->state.gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetCycleType(POLY_OPA_DISP++, G_CYC_1CYCLE);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetTexturePersp(POLY_OPA_DISP++, G_TP_NONE);
    // ML uses bilerp; at SoH resolutions that blends each 8-line strip against its clamped edge and shows seams.
    gDPSetTextureFilter(POLY_OPA_DISP++, G_TF_POINT);
    gDPSetCombineLERP(POLY_OPA_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
    gDPSetTextureLUT(POLY_OPA_DISP++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(POLY_OPA_DISP++, 0, sTluts[this->picture]);

    for (i = 0; i < 24; i++) {
        s32 y = 24 + i * 8;

        gDPLoadTextureBlock_4b(POLY_OPA_DISP++, sStrips[this->picture][i], G_IM_FMT_CI, 320, 8, 0,
                               G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                               G_TX_NOLOD, G_TX_NOLOD);
        gSPTextureRectangle(POLY_OPA_DISP++, 0 << 2, y << 2, 320 << 2, (y + 8) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10,
                            1 << 10);
    }
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetTextureLUT(POLY_OPA_DISP++, G_TT_NONE);

    CLOSE_DISPS(play->state.gfxCtx);
}

void MlPrologue_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_Prologue";
    definition.description = "The Missing Link: illustrated prologue";
    definition.category = ACTORCAT_BG;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    definition.objectId = OBJECT_GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(MlPrologue);
    definition.init = MlPrologue_Init;
    definition.destroy = MlPrologue_Destroy;
    definition.update = MlPrologue_Update;
    definition.draw = MlPrologue_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_Prologue is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
