/*
 * Young Link holds the Hylian Shield like any shield, as in The Missing Link, and nothing else of ML.
 *
 * Vanilla's Player_IsChildWithHylianShield keeps a child's Hylian Shield on his back: he ducks under it and can't
 * guard with it raised. This mod makes that rule false (VB_BE_CHILD_WITH_HYLIAN_SHIELD), so the child guards and
 * Z-targets with it as with the Deku Shield, and draws it in his right hand.
 *
 * The hand has no vanilla model, so it is built here from the player's own assets: vanilla's adult hand holding the
 * Hylian Shield, without the hand's and gauntlet's triangles, scaled by 0.8 and moved into the child's fist, then the
 * child's closed right hand. ML's own hand is the same shield under the same transform (its handle's vertices are
 * the adult's x 0.8 + (-10.8, -54.8, -7.9), within a unit). The adult list is copied as it is in memory, with every
 * state command kept, so textures, culling and combiners are vanilla's. If the list isn't vanilla's shape (a model
 * pack replaced it), nothing changes and the rule stays vanilla's.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ResourceManagerHelpers.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "objects/object_link_child/object_link_child.h"

#include <string.h>

extern HOST_DATA Gfx* sPlayerRightHandShieldDLs[];

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;

// the adult list's eight vertex loads, in order; the 3rd to 5th and 7th to 8th are the hand and gauntlet
static const u8 sAdultVtxCounts[] = { 10, 12, 27, 6, 3, 6, 3, 11 };
static const bool sAdultVtxIsHand[] = { false, false, true, true, true, false, true, true };

#define SHIELD_GFX_MAX 256

static Gfx sHandDL[SHIELD_GFX_MAX];
static Mtx sShieldMtx;
static bool sReady;

static u8 Opcode(const Gfx* g) {
    return (g->words.w0 >> 24) & 0xFF;
}

// Copies the adult hand-and-shield list into dest without the hand's vertex loads and triangles; returns the number
// of commands written, or -1 if the list isn't vanilla's shape
static s32 CopyShield(const Gfx* src, Gfx* dest, s32 room) {
    s32 out = 0;
    s32 vtx = -1;
    bool dropping = false;

    for (s32 i = 0; i < 1024; i++) {
        u8 op = Opcode(&src[i]);
        s32 words = 1;

        switch (op) {
            case G_ENDDL:
                return (vtx == ARRAY_COUNT(sAdultVtxCounts) - 1) ? out : -1;
            case G_MARKER:
                i++;
                continue;
            case G_SETTIMG_OTR_HASH:
                words = 2;
                break;
            case G_VTX_OTR_HASH:
                vtx++;
                if ((vtx >= ARRAY_COUNT(sAdultVtxCounts)) ||
                    (((src[i].words.w0 >> 12) & 0xFF) != sAdultVtxCounts[vtx])) {
                    return -1;
                }
                dropping = sAdultVtxIsHand[vtx];
                words = 2;
                break;
            case G_TRI1:
            case G_TRI2:
                if (vtx < 0) {
                    return -1;
                }
                break;
            case G_DL:
            case G_RDPPIPESYNC:
            case G_RDPLOADSYNC:
            case G_RDPTILESYNC:
            case G_SETOTHERMODE_H:
            case G_SETOTHERMODE_L:
            case G_TEXTURE:
            case G_SETTILE:
            case G_LOADBLOCK:
            case G_LOADTLUT:
            case G_SETTILESIZE:
            case G_SETCOMBINE:
            case G_GEOMETRYMODE:
            case G_SETPRIMCOLOR:
                dropping = false;
                break;
            default:
                return -1;
        }
        if (!dropping) {
            if (out + words > room) {
                return -1;
            }
            for (s32 w = 0; w < words; w++) {
                dest[out++] = src[i + w];
            }
        }
        i += words - 1;
    }
    return -1;
}

static void Build(void) {
    Gfx* adult = ResourceMgr_LoadGfxByName(gLinkAdultRightHandHoldingHylianShieldNearDL);
    Gfx* g = sHandDL;
    MtxF mf;
    s32 n;

    if (adult == NULL) {
        return;
    }
    memset(&mf, 0, sizeof(mf));
    mf.xx = mf.yy = mf.zz = 0.8f;
    mf.ww = 1.0f;
    mf.xw = -10.8f;
    mf.yw = -54.8f;
    mf.zw = -7.9f;
    Matrix_MtxFToMtx(&mf, &sShieldMtx);

    gSPMatrix(g++, &sShieldMtx, G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    n = CopyShield(adult, g, SHIELD_GFX_MAX - 4);
    if (n < 0) {
        sApi->Log("[childshield] the adult Hylian Shield hand isn't vanilla's; the child's shield stays on his back");
        return;
    }
    g += n;
    gSPPopMatrix(g++, G_MTX_MODELVIEW);
    gSPDisplayList(g++, (Gfx*)gLinkChildRightHandClosedNearDL);
    gSPEndDisplayList(g++);

    // one list for both LODs, as ML's
    sPlayerRightHandShieldDLs[PLAYER_SHIELD_HYLIAN * 4 + 1] = sHandDL;
    sPlayerRightHandShieldDLs[PLAYER_SHIELD_HYLIAN * 4 + 3] = sHandDL;
    sReady = true;
    sApi->Log("[childshield] young Link holds the Hylian Shield");
}

// in case the assets weren't there at ModInit
static void ChildShield_OnLoadGame(int32_t fileNum) {
    if (!sReady) {
        Build();
    }
}

static void ChildShield_ChildWithHylianShield(bool* should, va_list args) {
    if (sReady) {
        *should = false;
    }
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    // built before anything draws the adult hand, so the copied vertex loads still hold offsets, not cached pointers
    Build();
    SOH_REGISTER_HOOK(sApi, OnLoadGame, ChildShield_OnLoadGame);
    sApi->RegisterVB(VB_BE_CHILD_WITH_HYLIAN_SHIELD, ChildShield_ChildWithHylianShield);
}
