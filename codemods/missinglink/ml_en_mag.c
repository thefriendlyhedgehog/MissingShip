/*
 * The Missing Link's title screen (En_Mag, ledger rows 90 and 125), as a code mod: ML's logo, flame, credits and red
 * PRESS START on object_ml_mag, a faster copyright fade, and the title cutscene letterboxed with the HUD hidden.
 *
 * MissingShip's Unbound branch patches these into En_Mag's code while ML is mounted. As the Business Scrubs do, this
 * takes En_Mag's slot (its ActorDB entry): ML's step after vanilla's Init, the letterbox before vanilla's Update, and
 * ML's DrawInner (ML dma 286, +0xE74) in place of vanilla's. ML draws a dark brown bar (gMlTitleBarTex, tinted
 * 30,20,0) under the credits and another behind PRESS START; they are left out at the owner's request, as in the
 * branch: the text keeps its own drop shadow over the scene. Without ML's title textures, vanilla's title draws.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "align_asset_macro.h"
#include "soh/ActorDB.h"
#include "soh/ResourceManagerHelpers.h"
#include "overlays/actors/ovl_En_Mag/z_en_mag.h"

#include "missinglink.h"

#define dgMlTitleEffectMask00Tex "__OTR__objects/object_ml_mag/gMlTitleEffectMask00Tex"
static const ALIGN_ASSET(2) char gMlTitleEffectMask00Tex[] = dgMlTitleEffectMask00Tex;
#define dgMlTitleEffectMask01Tex "__OTR__objects/object_ml_mag/gMlTitleEffectMask01Tex"
static const ALIGN_ASSET(2) char gMlTitleEffectMask01Tex[] = dgMlTitleEffectMask01Tex;
#define dgMlTitleEffectMask02Tex "__OTR__objects/object_ml_mag/gMlTitleEffectMask02Tex"
static const ALIGN_ASSET(2) char gMlTitleEffectMask02Tex[] = dgMlTitleEffectMask02Tex;
#define dgMlTitleEffectMask10Tex "__OTR__objects/object_ml_mag/gMlTitleEffectMask10Tex"
static const ALIGN_ASSET(2) char gMlTitleEffectMask10Tex[] = dgMlTitleEffectMask10Tex;
#define dgMlTitleEffectMask11Tex "__OTR__objects/object_ml_mag/gMlTitleEffectMask11Tex"
static const ALIGN_ASSET(2) char gMlTitleEffectMask11Tex[] = dgMlTitleEffectMask11Tex;
#define dgMlTitleEffectMask12Tex "__OTR__objects/object_ml_mag/gMlTitleEffectMask12Tex"
static const ALIGN_ASSET(2) char gMlTitleEffectMask12Tex[] = dgMlTitleEffectMask12Tex;
#define dgMlTitleEffectMask20Tex "__OTR__objects/object_ml_mag/gMlTitleEffectMask20Tex"
static const ALIGN_ASSET(2) char gMlTitleEffectMask20Tex[] = dgMlTitleEffectMask20Tex;
#define dgMlTitleEffectMask21Tex "__OTR__objects/object_ml_mag/gMlTitleEffectMask21Tex"
static const ALIGN_ASSET(2) char gMlTitleEffectMask21Tex[] = dgMlTitleEffectMask21Tex;
#define dgMlTitleEffectMask22Tex "__OTR__objects/object_ml_mag/gMlTitleEffectMask22Tex"
static const ALIGN_ASSET(2) char gMlTitleEffectMask22Tex[] = dgMlTitleEffectMask22Tex;
#define dgMlTitleFlameEffectTex "__OTR__objects/object_ml_mag/gMlTitleFlameEffectTex"
static const ALIGN_ASSET(2) char gMlTitleFlameEffectTex[] = dgMlTitleFlameEffectTex;
#define dgMlTitleLogoTex "__OTR__objects/object_ml_mag/gMlTitleLogoTex"
static const ALIGN_ASSET(2) char gMlTitleLogoTex[] = dgMlTitleLogoTex;
#define dgMlTitlePressStartTex "__OTR__objects/object_ml_mag/gMlTitlePressStartTex"
static const ALIGN_ASSET(2) char gMlTitlePressStartTex[] = dgMlTitlePressStartTex;
#define dgMlTitleText1Tex "__OTR__objects/object_ml_mag/gMlTitleText1Tex"
static const ALIGN_ASSET(2) char gMlTitleText1Tex[] = dgMlTitleText1Tex;
#define dgMlTitleText2Tex "__OTR__objects/object_ml_mag/gMlTitleText2Tex"
static const ALIGN_ASSET(2) char gMlTitleText2Tex[] = dgMlTitleText2Tex;

// vanilla's, from z_en_mag.c
void EnMag_DrawEffectTextures(Gfx** gfxP, void* maskTex, void* effectTex, s16 maskWidth, s16 maskHeight,
                              s16 effectWidth, s16 effectHeight, s16 rectLeft, s16 rectTop, s16 rectWidth,
                              s16 rectHeight, u16 dsdx, u16 dtdy, u16 shifts, u16 shiftt, u16 flag, EnMag* this);
void EnMag_DrawImageRGBA32(Gfx** gfxp, s16 centerX, s16 centerY, const char* source, u32 width, u32 height);

static void MlEnMag_DrawInner(Actor* thisx, PlayState* play, Gfx** gfxP) {
    static void* effectMaskTextures[] = {
        (void*)gMlTitleEffectMask00Tex, (void*)gMlTitleEffectMask01Tex, (void*)gMlTitleEffectMask02Tex,
        (void*)gMlTitleEffectMask10Tex, (void*)gMlTitleEffectMask11Tex, (void*)gMlTitleEffectMask12Tex,
        (void*)gMlTitleEffectMask20Tex, (void*)gMlTitleEffectMask21Tex, (void*)gMlTitleEffectMask22Tex,
    };
    static f32 sPulse = 0.5f;
    static u8 sPulseFalling = false;
    EnMag* this = (EnMag*)thisx;
    Gfx* gfx = *gfxP;
    u16 i, j, k;
    u16 rectLeft;
    u16 rectTop;
    s16 subAlpha;
    s16 copyrightAlpha;
    u8 r, g;

    gSPSegment(gfx++, 0x06, play->objectCtx.status[this->actor.objBankIndex].segment);

    Gfx_SetupDL_39Ptr(&gfx);

    this->effectScroll -= 2;

    gDPSetCycleType(gfx++, G_CYC_2CYCLE);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetRenderMode(gfx++, G_RM_PASS, G_RM_CLD_SURF2);
    gDPSetCombineLERP(gfx++, TEXEL1, PRIMITIVE, PRIM_LOD_FRAC, TEXEL0, TEXEL1, 1, PRIM_LOD_FRAC, TEXEL0, PRIMITIVE,
                      ENVIRONMENT, COMBINED, ENVIRONMENT, COMBINED, 0, PRIMITIVE, 0);

    gDPSetPrimColor(gfx++, 0, (s16)this->effectPrimLodFrac, (s16)this->effectPrimColor[0],
                    (s16)this->effectPrimColor[1], (s16)this->effectPrimColor[2], (s16)this->effectAlpha);
    gDPSetEnvColor(gfx++, (s16)this->effectEnvColor[0], (s16)this->effectEnvColor[1], (s16)this->effectEnvColor[2],
                   255);

    if ((s16)this->effectPrimLodFrac != 0) {
        for (k = 0, i = 0, rectTop = 0; i < 3; i++, rectTop += 64) {
            for (j = 0, rectLeft = 64; j < 3; j++, k++, rectLeft += 64) {
                EnMag_DrawEffectTextures(&gfx, effectMaskTextures[k], (void*)gMlTitleFlameEffectTex, 64, 64, 32, 32,
                                         rectLeft, rectTop, 64, 64, 1024, 1024, 1, 1, k, this);
            }
        }
    }

    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, (s16)this->mainAlpha);

    if ((s16)this->mainAlpha != 0) {
        EnMag_DrawImageRGBA32(&gfx, 160, 100, gMlTitleLogoTex, 160, 160);
    }

    subAlpha = (s16)this->subAlpha;
    if (subAlpha != 0) {
        copyrightAlpha = (s16)this->copyrightAlpha;

        gDPSetAlphaCompare(gfx++, G_AC_NONE);
        gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);

        // The two credit lines
        gDPPipeSync(gfx++);
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, subAlpha);
        gDPLoadTextureBlock(gfx++, gMlTitleText1Tex, G_IM_FMT_IA, G_IM_SIZ_8b, 112, 13, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                            G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPTextureRectangle(gfx++, 195 << 2, 179 << 2, 307 << 2, 192 << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        gDPLoadTextureBlock(gfx++, gMlTitleText2Tex, G_IM_FMT_IA, G_IM_SIZ_8b, 112, 12, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                            G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPTextureRectangle(gfx++, 195 << 2, 192 << 2, 307 << 2, 204 << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);

        if (copyrightAlpha != 0) {
            // PRESS START pulses between dark red (127, 15, 15) and red (255, 30, 30), 0.02 a frame
            if (!sPulseFalling) {
                sPulse += 0.02f;
                if (sPulse >= 1.0f) {
                    sPulse = 1.0f;
                    sPulseFalling = true;
                }
            } else {
                sPulse -= 0.02f;
                if (sPulse <= 0.5f) {
                    sPulse = 0.5f;
                    sPulseFalling = false;
                }
            }
            r = (s32)(sPulse * 255.0f);
            g = (s32)(sPulse * 30.0f);
            gDPPipeSync(gfx++);
            gDPSetPrimColor(gfx++, 0, 0, r, g, g, copyrightAlpha);
            gDPLoadTextureBlock(gfx++, gMlTitlePressStartTex, G_IM_FMT_IA, G_IM_SIZ_8b, 72, 11, 0,
                                G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);
            gSPTextureRectangle(gfx++, 231 << 2, 162 << 2, 303 << 2, 173 << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        }
    }

    *gfxP = gfx;
}

static ActorFunc sEnMagInit;
static ActorFunc sEnMagUpdate;
static ActorFunc sEnMagDraw;
static s32 sMlTitle = false;

static void MlEnMag_Init(Actor* thisx, PlayState* play) {
    sEnMagInit(thisx, play);
    sMlTitle = ResourceMgr_FileExists("objects/object_ml_mag/gMlTitleLogoTex");
    if (sMlTitle) {
        ((EnMag*)thisx)->copyrightAlphaStep = 10;
    }
}

static void MlEnMag_Update(Actor* thisx, PlayState* play) {
    // ML letterboxes the title cutscene and hides the HUD while it runs
    if (sMlTitle && (play->csCtx.state != CS_STATE_IDLE)) {
        Interface_ChangeHudVisibilityMode(1);
        Letterbox_SetSizeTarget(32);
        ShrinkWindow_SetCurrentVal(32);
    }
    sEnMagUpdate(thisx, play);
}

// Vanilla's EnMag_Draw around ML's DrawInner
static void MlEnMag_Draw(Actor* thisx, PlayState* play) {
    Gfx* gfx;
    Gfx* gfxRef;

    if (!sMlTitle) {
        sEnMagDraw(thisx, play);
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    gfxRef = POLY_OPA_DISP;
    gfx = Graph_GfxPlusOne(gfxRef);
    gSPDisplayList(OVERLAY_DISP++, gfx);

    MlEnMag_DrawInner(thisx, play, &gfx);

    gSPEndDisplayList(gfx++);
    Graph_BranchDlist(gfxRef, gfx);
    POLY_OPA_DISP = gfx;

    CLOSE_DISPS(play->state.gfxCtx);
}

void MlEnMag_Register(const SOHModApi* api) {
    ActorDBEntry* entry = ActorDB_Retrieve(ACTOR_EN_MAG);

    sEnMagInit = entry->init;
    sEnMagUpdate = entry->update;
    sEnMagDraw = entry->draw;
    entry->init = MlEnMag_Init;
    entry->update = MlEnMag_Update;
    entry->draw = MlEnMag_Draw;
    api->Log("[MissingLink] En_Mag's slot draws ML's title screen");
}
