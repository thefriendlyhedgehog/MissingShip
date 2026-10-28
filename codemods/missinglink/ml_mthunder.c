/*
 * The Missing Link's Blade Beam (ledger row 17), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_MThunder), registered through the mod
 * API. The player spawns actor 0x57 by number while charging a spin, so, as the branch does while ML is mounted, this
 * takes En_M_Thunder's slot itself (its ActorDB entry) rather than a name, and the beam spawner runs from SoH's
 * OnPlayerUpdate hook.
 *
 * ML replaced vanilla En_M_Thunder (the spin-attack magic) wholesale: ML dma 195, text 0x9A0, vram
 * 0x80800000, init at +0x9E0. Its Init dispatches on params:
 *   0xDEAD  the Blade Beam: flies 35 units/frame along its yaw for up to 20 frames, player-aligned AT
 *           collider, drawn with vanilla's never-used gUnusedBeamBladeDL;
 *   0xBEEF  Saria's soul, the figure that speaks from the sword (ML's object_link_child +0x345D0 and +0x344D0,
 *           named gMlChildSoulFigureDL and gMlChildSoulFigureXluDL in ML's layer), at a fixed point (Kakariko's
 *           slot, Monktown Valley: (-1957, -81, 2767); elsewhere, Deku Grove: (710, 259, 2080)), its colours
 *           pulsing and its eyes blinking. ML's custom code (0x806032A0) spawns it as Link starts drawing the
 *           Soul-Charged sword, and it removes itself once his animation is no longer normal2fighter_free; it only
 *           starts to grow after 16 frames. The two arrival cutscenes hold that pose (Link cue 26,
 *           PLAYER_CSACTION_30) while she speaks; in play the draw lasts 20 frames, so she barely shows;
 *   other   what the player spawns while charging a spin: once the Soul-Charged sword is owned, a
 *           sparkle (Eff_Dust type 3) for as long as the charge is held. Vanilla's spin glow, magic
 *           spin and spin-attack collider are gone in ML.
 *
 * The beam is spawned from ML's custom code blob (dma 507, loaded at 0x80600000), ported below as
 * MlMThunder_PlayerUpdate, run from SoH's OnPlayerUpdate hook. Everything is gated on
 * slingshot ammo == 40: ML repurposed the Bullet Bag (40) GetItem as the Soul-Charged Kokiri Sword, and that ammo
 * count is its only trace in the save. Ported from MissingShip's SoH fork (ovl_Ml_MThunder, ledger row 17).
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "align_asset_macro.h"
#include "soh/ActorDB.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/unbound/SceneDB.h"

#include <stdbool.h>
#include <string.h>

#include "missinglink.h"

// params values ML gives actor 0x57. Anything else is the spin-attack charge spawned by the player.
#define ML_MTHUNDER_BLADE_BEAM 0xDEAD
#define ML_MTHUNDER_SOUL 0xBEEF
// MissingShip's stand-in for ML's gutted En_Bom clone (its actor 0x0E) that trips crystal switches.
#define ML_MTHUNDER_SWITCH_HIT 0xB0B0

typedef struct MlMThunder {
    Actor actor;
    ColliderCylinder collider;
    Actor* switchActor; // Obj_Switch within reach of the beam
    Actor* child;       // Eff_Dust sparkle (charge mode)
    s8 childSpawned;
    s32 timer;
    s8 sfxPlayed;
    u8 alpha;
    s8 switchHit;
    // Saria's soul (ML +0x1AC-0x1C4)
    const char* eyeTex;
    s16 blinkTimer;
    f32 soulScale;
    u32 soulFrames;
    u8 soulColors[6]; // the white parts' RGB, then the green parts'
    u32 pulseTimer;
} MlMThunder;

static void MlMThunder_UpdateBeam(Actor* thisx, PlayState* play);
static void MlMThunder_DrawBeam(Actor* thisx, PlayState* play);
static void MlMThunder_UpdateSwitchHit(Actor* thisx, PlayState* play);
static void MlMThunder_UpdateCharge(Actor* thisx, PlayState* play);
static void MlMThunder_UpdateSoul(Actor* thisx, PlayState* play);
static void MlMThunder_DrawSoul(Actor* thisx, PlayState* play);

// ML's beam collider (overlay data +0x9AC): player-aligned AT, damage flag 1 << 17, 1 damage.
static ColliderCylinderInit sBeamCylinderInit = {
    {
        COLTYPE_NONE,
        AT_ON | AT_TYPE_PLAYER,
        AC_NONE,
        OC1_NONE,
        OC2_TYPE_PLAYER,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00020000, 0x00, 0x01 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NONE,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { 32, 32, 0, { 0, 0, 0 } },
};

// The explosion ML's En_Bom clone produces (vanilla En_Bom's explosion element: explosive, 8 damage).
static ColliderCylinderInit sSwitchHitCylinderInit = {
    {
        COLTYPE_HIT0,
        AT_ON | AT_TYPE_ALL,
        AC_NONE,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000008, 0x00, 0x08 },
        { 0x00000000, 0x00, 0x00 },
        TOUCH_ON | TOUCH_SFX_NONE,
        BUMP_NONE,
        OCELEM_NONE,
    },
    { 0, 0, 0, { 0, 0, 0 } },
};

static void MlMThunder_Init(Actor* thisx, PlayState* play) {
    MlMThunder* this = (MlMThunder*)thisx;

    thisx->room = -1;

    switch ((u16)thisx->params) {
        case ML_MTHUNDER_BLADE_BEAM:
            Actor_SetScale(thisx, 0.1f);
            Collider_InitCylinder(play, &this->collider);
            Collider_SetCylinder(play, &this->collider, thisx, &sBeamCylinderInit);
            Collider_UpdateCylinder(thisx, &this->collider);
            this->alpha = 255;
            thisx->update = MlMThunder_UpdateBeam;
            thisx->draw = MlMThunder_DrawBeam;
            break;
        case ML_MTHUNDER_SWITCH_HIT:
            Collider_InitCylinder(play, &this->collider);
            Collider_SetCylinder(play, &this->collider, thisx, &sSwitchHitCylinderInit);
            this->timer = 10;
            thisx->update = MlMThunder_UpdateSwitchHit;
            break;
        case ML_MTHUNDER_SOUL: {
            static const u8 sSoulColors[6] = { 255, 255, 255, 74, 255, 185 };

            Actor_SetScale(thisx, 0.0f);
            memcpy(this->soulColors, sSoulColors, sizeof(sSoulColors));
            thisx->update = MlMThunder_UpdateSoul;
            thisx->draw = MlMThunder_DrawSoul;
            break;
        }
        default:
            thisx->update = MlMThunder_UpdateCharge;
            break;
    }
}

static void MlMThunder_Destroy(Actor* thisx, PlayState* play) {
    MlMThunder* this = (MlMThunder*)thisx;

    if ((u16)thisx->params == ML_MTHUNDER_BLADE_BEAM || (u16)thisx->params == ML_MTHUNDER_SWITCH_HIT) {
        Collider_DestroyCylinder(play, &this->collider);
    }
}

// ML +0x000: the spin-charge companion.
static void MlMThunder_UpdateCharge(Actor* thisx, PlayState* play) {
    MlMThunder* this = (MlMThunder*)thisx;
    Player* player = GET_PLAYER(play);

    if (gSaveContext.inventory.ammo[SLOT_SLINGSHOT] == 40 && !this->childSpawned) {
        this->child = Actor_SpawnAsChild(&play->actorCtx, thisx, play, ACTOR_EFF_DUST, thisx->world.pos.x,
                                         thisx->world.pos.y, thisx->world.pos.z, thisx->shape.rot.x,
                                         thisx->shape.rot.y, thisx->shape.rot.z, 3);
        this->childSpawned = true;
    }

    if (!(player->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK)) {
        if (this->child != NULL) {
            Actor_Kill(this->child);
        }
        Actor_Kill(thisx);
    }
}

// ML +0x544.
static void MlMThunder_UpdateBeam(Actor* thisx, PlayState* play) {
    MlMThunder* this = (MlMThunder*)thisx;
    Player* player = GET_PLAYER(play);
    f32 pitchScale;

    if (!this->sfxPlayed) {
        Sfx_PlaySfxCentered(NA_SE_IT_ROLLING_CUT_LV1);
        this->sfxPlayed = true;
    }

    this->alpha -= 12;

    // ML kills the beam here but, like the original, still finishes the frame.
    if ((this->timer > 20) || (thisx->bgCheckFlags & (1 | 8)) /* ground | wall */ ||
        (this->collider.base.atFlags & AT_HIT) || (this->collider.base.acFlags & AC_HIT)) {
        Actor_Kill(thisx);
    }
    this->timer++;

    Actor_UpdateBgCheckInfo(play, thisx, 0.0f, 10.0f, 0.0f, 5);
    Collider_UpdateCylinder(thisx, &this->collider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    thisx->flags |= ACTOR_FLAG_SFX_FOR_PLAYER_BODY_HIT;

    pitchScale = Math_CosS(thisx->world.rot.x) * -35.0f;
    thisx->world.pos.x += pitchScale * Math_SinS(thisx->shape.rot.y);
    thisx->world.pos.z += pitchScale * Math_CosS(thisx->shape.rot.y);

    // Crystal switches: ML drops an invisible, silent explosion on any crystal switch the beam passes.
    if (this->switchActor == NULL) {
        this->switchActor = Actor_FindNearby(play, thisx, ACTOR_OBJ_SWITCH, ACTORCAT_SWITCH, 35.0f);
    } else if (((this->switchActor->params & 0xF) >= 3) && !this->switchHit &&
               (this->switchActor->world.pos.y - 10.0f < player->actor.world.pos.y)) {
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_M_THUNDER, this->switchActor->world.pos.x,
                    this->switchActor->world.pos.y, this->switchActor->world.pos.z, 0x3FFF, 0, 0,
                    ML_MTHUNDER_SWITCH_HIT);
        this->switchHit = true;
    }
}

// ML +0x77C.
static void MlMThunder_DrawBeam(Actor* thisx, PlayState* play) {
    MlMThunder* this = (MlMThunder*)thisx;
    u32 frames = play->gameplayFrames;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 16, 64, 1, 0, frames * 5, 32, 128));
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 255, 255, 170, this->alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 100, 0, 128);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gUnusedBeamBladeDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

// Stand-in for ML's En_Bom clone: vanilla's explosion collider (radius +8 per frame for 10 frames), no
// model, no sound, no effects -- ML nopped all of those.
static void MlMThunder_UpdateSwitchHit(Actor* thisx, PlayState* play) {
    MlMThunder* this = (MlMThunder*)thisx;
    s16 radius = (11 - this->timer) * 8;

    this->collider.dim.radius = radius;
    this->collider.dim.height = radius * 2;
    this->collider.dim.yShift = -radius;
    Collider_UpdateCylinder(thisx, &this->collider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);

    if (--this->timer <= 0) {
        Actor_Kill(thisx);
    }
}

// ---- Saria's soul (ML +0xE4 update, +0x494 draw) ----

#define dgMlChildSoulFigureDL "__OTR__objects/object_link_child/gMlChildSoulFigureDL"
static const ALIGN_ASSET(2) char gMlChildSoulFigureDL[] = dgMlChildSoulFigureDL;
#define dgMlChildSoulFigureXluDL "__OTR__objects/object_link_child/gMlChildSoulFigureXluDL"
static const ALIGN_ASSET(2) char gMlChildSoulFigureXluDL[] = dgMlChildSoulFigureXluDL;
#define dgMlChildSoulFigureGreenDL "__OTR__objects/object_link_child/gMlChildSoulFigureGreenDL"
static const ALIGN_ASSET(2) char gMlChildSoulFigureGreenDL[] = dgMlChildSoulFigureGreenDL;
#define dgMlChildSoulFigureWhiteDL "__OTR__objects/object_link_child/gMlChildSoulFigureWhiteDL"
static const ALIGN_ASSET(2) char gMlChildSoulFigureWhiteDL[] = dgMlChildSoulFigureWhiteDL;
#define dgMlChildSoulFigureEyesOpenTex "__OTR__objects/object_link_child/gMlChildSoulFigureEyesOpenTex"
static const ALIGN_ASSET(2) char gMlChildSoulFigureEyesOpenTex[] = dgMlChildSoulFigureEyesOpenTex;
#define dgMlChildSoulFigureEyesHalfTex "__OTR__objects/object_link_child/gMlChildSoulFigureEyesHalfTex"
static const ALIGN_ASSET(2) char gMlChildSoulFigureEyesHalfTex[] = dgMlChildSoulFigureEyesHalfTex;
#define dgMlChildSoulFigureEyesClosedTex "__OTR__objects/object_link_child/gMlChildSoulFigureEyesClosedTex"
static const ALIGN_ASSET(2) char gMlChildSoulFigureEyesClosedTex[] = dgMlChildSoulFigureEyesClosedTex;

static const char* sSoulEyeTextures[] = { gMlChildSoulFigureEyesOpenTex, gMlChildSoulFigureEyesHalfTex,
                                          gMlChildSoulFigureEyesClosedTex };

static s32 MlMThunder_IsUnsheathing(Player* player) {
    char* anim = (char*)player->skelAnime.animation;

    return (anim != NULL) && ResourceMgr_OTRSigCheck(anim) &&
           (strcmp(anim, dgPlayerAnim_link_normal_normal2fighter_free) == 0);
}

// ML writes the colour straight into object_link_child's prim colours each frame; SoH's DLs are shared resources, so
// each prim colour of the DL is patched instead (each keeps its own alpha).
static void MlMThunder_SetSoulColor(const char* dl, const char* const* patchNames, s32 count, const u8* rgb) {
    Gfx* gfx = ResourceMgr_LoadGfxByName(dl);
    s32 n = 0;

    if (gfx == NULL) {
        return;
    }
    for (s32 i = 0; (n < count) && ((gfx[i].words.w0 >> 24) != G_ENDDL); i++) {
        u32 op = gfx[i].words.w0 >> 24;

        if (op == G_SETPRIMCOLOR) {
            ResourceMgr_PatchCustomGfxByName(dl, patchNames[n++], i,
                                             (Gfx)gsDPSetPrimColor((gfx[i].words.w0 >> 8) & 0xFF,
                                                                   gfx[i].words.w0 & 0xFF, rgb[0], rgb[1], rgb[2],
                                                                   gfx[i].words.w1 & 0xFF));
        } else if ((op == 0x20) || (op == 0x31) || (op == 0x32) || (op == 0x33) || (op == 0x35) || (op == 0x36) ||
                   (op == 0x42)) {
            i++; // LUS's two-word commands
        }
    }
}

static void MlMThunder_UpdateSoul(Actor* thisx, PlayState* play) {
    static const char* sWhiteNames[] = { "mlSoulW0", "mlSoulW1", "mlSoulW2", "mlSoulW3", "mlSoulW4",
                                         "mlSoulW5", "mlSoulW6", "mlSoulW7", "mlSoulW8" };
    static const char* sGreenNames[] = { "mlSoulG0", "mlSoulG1" };
    static const char* sXluNames[] = { "mlSoulX0" };
    MlMThunder* this = (MlMThunder*)thisx;
    Player* player = GET_PLAYER(play);
    s32 i;

    if (play->sceneNum == SCENE_KAKARIKO_VILLAGE) { // ML's Monktown Valley
        thisx->world.pos.x = -1957.0f;
        thisx->world.pos.y = -81.0f;
        thisx->world.pos.z = 2767.0f;
        thisx->shape.rot.y = 0x4000;
    } else { // ML's Deku Grove
        thisx->world.pos.x = 710.0f;
        thisx->world.pos.y = (this->soulFrames < 900) ? 259.0f : 249.0f;
        thisx->world.pos.z = 2080.0f;
        thisx->shape.rot.y = 0x6E39;
    }

    // ML kills it here but, like the original, still finishes the frame.
    if (!MlMThunder_IsUnsheathing(player) || ((play->sceneNum == SCENE_KAKARIKO_VILLAGE) && (this->soulFrames >= 701))) {
        Actor_Kill(thisx);
    }
    if (this->soulFrames >= 16) {
        if (this->soulScale != 0.01f) {
            Math_StepToF(&this->soulScale, 0.01f, 0.0009f);
        }
        Actor_SetScale(thisx, this->soulScale);
    }

    // Every colour dims by 2 a frame for 10 frames, then brightens for 10.
    for (i = 0; i < ARRAY_COUNT(this->soulColors); i++) {
        this->soulColors[i] += (this->pulseTimer < 10) ? -2 : 2;
    }
    if (++this->pulseTimer == 20) {
        this->pulseTimer = 0;
    }
    MlMThunder_SetSoulColor(gMlChildSoulFigureWhiteDL, sWhiteNames, ARRAY_COUNT(sWhiteNames), &this->soulColors[0]);
    MlMThunder_SetSoulColor(gMlChildSoulFigureGreenDL, sGreenNames, ARRAY_COUNT(sGreenNames), &this->soulColors[3]);
    MlMThunder_SetSoulColor(gMlChildSoulFigureXluDL, sXluNames, ARRAY_COUNT(sXluNames), &this->soulColors[3]);

    // Blinks: open, then half and closed for a frame each, every 30-59 frames.
    if (this->blinkTimer == 0) {
        this->blinkTimer = Rand_S16Offset(30, 30);
    }
    this->blinkTimer--;
    this->eyeTex = sSoulEyeTextures[(this->blinkTimer < 2) ? (2 - this->blinkTimer) : 0];
    this->soulFrames++;
}

static void MlMThunder_DrawSoul(Actor* thisx, PlayState* play) {
    MlMThunder* this = (MlMThunder*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(this->eyeTex));
    CLOSE_DISPS(play->state.gfxCtx);

    Gfx_DrawDListOpa(play, (Gfx*)gMlChildSoulFigureDL);
    Gfx_DrawDListXlu(play, (Gfx*)gMlChildSoulFigureXluDL);
}

/*
 * Blade Beam spawner, from ML's custom code (0x80602DE0, the part that concerns the beam). ML calls it
 * once per frame while drawing Link; SoH may draw several times per update when interpolating, so it
 * runs after the player's update instead. The static state mirrors ML's globals at 0x80603600-0x80603618.
 */
static u8 sSpinBeamState;  // 0x80603604: 0 idle, 1 charging, 2 fired / waiting for the glow to end
static u32 sSpinChargeFrames; // 0x80603608
static u8 sSwingBeamState; // 0x80603603: 0 idle, 1 armed, 2 fired
static u8 sGlowActive;     // 0x80603610 (the blade's colour pulse is ml_player.c's)
static u8 sSoulSpawned;    // 0x80603618

static void MlMThunder_SpawnBeam(Player* player, PlayState* play) {
    Actor_Spawn(&play->actorCtx, play, ACTOR_EN_M_THUNDER, player->bodyPartsPos[0].x, player->bodyPartsPos[0].y,
                player->bodyPartsPos[0].z, 0, player->actor.shape.rot.y - 0x8000, 0, ML_MTHUNDER_BLADE_BEAM);
}

// unsheathing: the player's animation is gPlayerAnim_link_normal_normal2fighter_free (compared by path in
// MlActors.cpp: SoH asset symbols are per-translation-unit copies, so the pointer only matches in the file that set
// it).
static void MlMThunder_PlayerUpdate(Player* player, PlayState* play, u8 unsheathing) {
    static s32 sEventEntrance = -1;
    // ML clears the sword marker while gSaveContext.entranceIndex == 0x400 in ML's own numbering (custom code
    // 0x80602F94). ML's patched cutscene destination 61 sets it: the Cluck Cluck Ranch cutscene after the Hollow Root
    // Keep boss continues on Deku Road's cutscene layer, and the sword loses its charge there. SoH reaches that
    // layer through its own entrance group, so this can't fire on ordinary ranch visits (MS-148, MS-151). On Unbound
    // that group is ML's layer's entrance ml/gerudo_valley_event (unbound/scenes.json), found by name; on a stock
    // Unbound, the vanilla rows ml_cutscenes.c gives it.
    if (sEventEntrance < 0) {
        sEventEntrance = MlEntrance_Index("SCENE_GERUDO_VALLEY/ml/gerudo_valley_event");
    }
    if (sEventEntrance >= 0 && gSaveContext.entranceIndex == sEventEntrance) {
        gSaveContext.inventory.ammo[SLOT_SLINGSHOT] = 0;
    }

    if (gSaveContext.inventory.ammo[SLOT_SLINGSHOT] != 40) {
        return;
    }

    bool charging = (player->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK) != 0;
    // ML reads the halfword at 0x844, so bit 0 is unk_845: odd on the first (and third) swing of a combo.
    bool comboSwing = (player->unk_845 & 1) != 0;
    bool fullHealth = gSaveContext.health == gSaveContext.healthCapacity;

    // Spin attack: charge for at least 8 frames, release, and a beam fires.
    if (sSpinBeamState == 0) {
        if (charging) {
            sSpinBeamState = 1;
        }
    } else if (sSpinBeamState == 1) {
        if (charging) {
            sSpinChargeFrames++;
        } else {
            if (sSpinChargeFrames >= 8) {
                MlMThunder_SpawnBeam(player, play);
            }
            sSpinBeamState = 2;
            sSpinChargeFrames = 0;
        }
    } else if (sSpinBeamState == 2 && !sGlowActive) {
        sSpinBeamState = 0;
    }

    // Full health: one beam at the start of each sword combo, Kokiri Sword in hand.
    if (comboSwing) {
        if (fullHealth && sSwingBeamState == 0) {
            if (player->heldItemAction == PLAYER_IA_SWORD_KOKIRI) {
                sSwingBeamState = 1;
            }
        } else if (sSwingBeamState == 1) {
            MlMThunder_SpawnBeam(player, play);
            sSwingBeamState = 2;
        }
    } else if (sSwingBeamState == 1) {
        MlMThunder_SpawnBeam(player, play);
        sSwingBeamState = 2;
    } else if (player->unk_844 == 0 && player->unk_845 == 0 && sSwingBeamState == 2) {
        sSwingBeamState = 0;
    }

    // Saria's soul (0x80603280): once each time Link starts drawing the sword.
    if (unsheathing) {
        if (!sSoulSpawned) {
            Actor_Spawn(&play->actorCtx, play, ACTOR_EN_M_THUNDER, 0.0f, 0.0f, 0.0f, 0, 0, 0, ML_MTHUNDER_SOUL);
            sSoulSpawned = true;
        }
    } else {
        sSoulSpawned = false;
    }

    sGlowActive = charging || (comboSwing && fullHealth) || unsheathing;
}

// The branch's MlActors.cpp OnPlayerUpdate, for the beam: the player's unsheathing animation, compared by path (SoH
// asset symbols are per-translation-unit copies, so the pointer only matches in the file that set it).
static void MlMThunder_OnPlayerUpdate(void) {
    Player* player = GET_PLAYER(gPlayState);

    MlMThunder_PlayerUpdate(player, gPlayState, MlMThunder_IsUnsheathing(player));
}

// SoH's Remove Spin Attack Darkness swaps vanilla's update for its own after Init (RemoveSpinAttackDarkness.cpp), which
// reads vanilla's fields. Our branch skips that while ML is mounted; here ML's update is put back before each update.
// ML's never changes after Init.
static void MlMThunder_ShouldUpdate(void* actor, bool* result) {
    Actor* thisx = (Actor*)actor;

    switch ((u16)thisx->params) {
        case ML_MTHUNDER_BLADE_BEAM:
            thisx->update = MlMThunder_UpdateBeam;
            break;
        case ML_MTHUNDER_SWITCH_HIT:
            thisx->update = MlMThunder_UpdateSwitchHit;
            break;
        case ML_MTHUNDER_SOUL:
            thisx->update = MlMThunder_UpdateSoul;
            break;
        default:
            thisx->update = MlMThunder_UpdateCharge;
            break;
    }
}

void MlMThunder_Register(const SOHModApi* api) {
    ActorDBEntry* entry = MlPatch_Actor(ACTOR_EN_M_THUNDER);

    entry->desc = "The Missing Link: Blade Beam and spin charge";
    entry->category = ACTORCAT_ITEMACTION;
    entry->flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    entry->objectId = OBJECT_GAMEPLAY_KEEP; // ML lists object_link_child: Saria's soul draws from it, by name here
    entry->instanceSize = sizeof(MlMThunder);
    entry->init = MlMThunder_Init;
    entry->destroy = MlMThunder_Destroy;
    entry->update = MlMThunder_UpdateCharge;
    entry->draw = NULL;
    entry->reset = NULL;
    SOH_REGISTER_HOOK(api, OnPlayerUpdate, MlMThunder_OnPlayerUpdate);
    SOH_REGISTER_HOOK_FOR_ID(api, ShouldActorUpdate, ACTOR_EN_M_THUNDER, MlMThunder_ShouldUpdate);
    api->Log("[MissingLink] En_M_Thunder's slot is ML's Blade Beam");
}
