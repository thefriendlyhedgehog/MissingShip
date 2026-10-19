/*
 * The Missing Link's Shadow Temple boss, as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_BossVa), registered through the mod API.
 *
 * Description: The Missing Link's Shadow Temple boss, its rewrite of Boss_Va (slot 0xBA)
 *
 * Source overlay: ML dma 225, text 0x2C00, instance 0x1154, on ML's own object 0x8C (dma 26, shipped as
 * object_ml_bv). Placed once, in HAKAdan_bs. Nothing of vanilla's Barinade remains.
 *
 * A 34-limb creature with three heads (limbs 4-6) and four Kokiri-type ML_Elf fairies circling limb 7. It waits
 * until Link drops into the arena (below y -2783, within 500 units), then plays the scene's main cutscene and its
 * intro animation. Phase 0: it turns toward Link, then either lunges (and backs off) or spits an ML_BossVaShot.
 * Only explosives hurt its body; the spikes (every sixth sphere) hurt Link. Health 30. At 0 it plays one of two
 * scene cutscenes, stands back up at full health (the fairies scatter) and enters phase 1, where three Beamos
 * (En_Vm) ride its heads and the heads aim at Link. At 0 again it dies: 34 ML_Light flames spread over its body,
 * it fades out and leaves a blue warp. Damage is applied only once 40 frames have passed since the last hit.
 *
 * Deliberate differences, each commented where it happens:
 *  - ML calls Math_Atan2S without a prototype, so it receives two doubles; with integer offsets the float halves
 *    it reads are always 0. SoH passes 0 directly.
 *  - ML writes through its fairy, Beamos and flame pointers after those actors may have been deleted (a Beamos can
 *    be blown up; the flames are killed while the loop still moves them). SoH checks the actor is still live.
 *
 * Ported from MissingShip's SoH fork (ovl_Ml_BossVa); its fairies, shot and flames are spawned by name.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ActorDB.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "overlays/actors/ovl_En_Elf/z_en_elf.h"
#include "overlays/actors/ovl_Door_Warp1/z_door_warp1.h"
#include "align_asset_macro.h"

#include <math.h>
#include <stdio.h>

#include "missinglink.h"

#define ML_BOSSVA_LIMB_MAX 35 // limbs are 1-based in the draw callbacks (34 limbs); index 0 is never written
#define ML_BOSSVA_LIGHT_COUNT 34
#define ML_BOSSVA_COLLIDER_COUNT 51

typedef enum {
    /*  0 */ ML_BOSSVA_ANIM_RISE,       // phase 0 -> 1: stands back up
    /*  1 */ ML_BOSSVA_ANIM_LUNGE_HOLD,
    /*  2 */ ML_BOSSVA_ANIM_SHOOT,
    /*  3 */ ML_BOSSVA_ANIM_RETREAT,
    /*  4 */ ML_BOSSVA_ANIM_HURT,
    /*  5 */ ML_BOSSVA_ANIM_DEATH,
    /*  6 */ ML_BOSSVA_ANIM_LUNGE,
    /*  7 */ ML_BOSSVA_ANIM_IDLE,
    /*  8 */ ML_BOSSVA_ANIM_INTRO,
    /*  9 */ ML_BOSSVA_ANIM_LUNGE_HOLD_2,
    /* 10 */ ML_BOSSVA_ANIM_TURN_LEFT,
    /* 11 */ ML_BOSSVA_ANIM_TURN_RIGHT
} MlBossVaAnim;

typedef enum {
    /* 0 */ ML_BOSSVA_WAIT,     // until Link is close and below y -2783
    /* 1 */ ML_BOSSVA_INTRO,
    /* 2 */ ML_BOSSVA_NONE_2,
    /* 3 */ ML_BOSSVA_START,    // unused: goes straight to 4
    /* 4 */ ML_BOSSVA_TURN,
    /* 5 */ ML_BOSSVA_LUNGE,    // phase 0 attacks
    /* 6 */ ML_BOSSVA_SHOOT,
    /* 7 */ ML_BOSSVA_HURT,
    /* 8 */ ML_BOSSVA_NONE_8,
    /* 9 */ ML_BOSSVA_DOWN,     // phase 0: regroup into phase 1; phase 1: death
    /* 10 */ ML_BOSSVA_LUNGE_2, // phase 1 attacks
    /* 11 */ ML_BOSSVA_SHOOT_2
} MlBossVaAction;

// One of the three heads (limbs 4-6): a random wobble while the boss is in phase 0.
typedef struct {
    /* 0x0 */ Vec3s target;
    /* 0x6 */ Vec3s cur;
    /* 0xC */ u8 timer;
} MlBossVaWobble;

// ML's layout (size 0x1154), for reference against its overlay; SoH's own offsets differ.
typedef struct MlBossVa {
    /* 0x0000 */ Actor actor;
    /* 0x014C */ SkelAnime skelAnime;
    /* 0x0194 */ ColliderJntSph collider;
    /* 0x01B4 */ ColliderJntSphElement colliderElements[ML_BOSSVA_COLLIDER_COUNT];
    /* 0x0E74 */ Player* player;
    /* 0x0E78 */ u8 turning;    // set once Link is more than 0x2400 off the boss's heading
    /* 0x0E7A */ s16 action;
    /* 0x0E7C */ s16 phase;     // 0, then 1 after the first knockdown
    /* 0x0E7E */ s16 animIndex;
    /* 0x0E80 */ s32 timer;
    /* 0x0E84 */ s8 animating;  // SkelAnime_Update runs only once the intro reaches frame 120
    /* 0x0E88 */ f32 playSpeed; // for the next animation change; reset to 1 every frame
    /* 0x0E8C */ f32 turnFrameAccum;
    /* 0x0E90 */ u16 rewindTimer;
    /* 0x0E92 */ u16 damageTaken; // health lost since the current action began
    /* 0x0E94 */ s16 lastYaw;
    /* 0x0E96 */ s16 targetYaw;
    /* 0x0E98 */ MlBossVaWobble wobble[3];
    /* 0x0EC4 */ Actor* fairies[4];
    /* 0x0ED4 */ Vec3f fairyOffsets[4];
    /* 0x0F04 */ u16 fairyAngle;
    /* 0x0F06 */ u8 lastHealth;
    /* 0x0F07 */ u8 hurtFrame;
    /* 0x0F08 */ u8 invincibleTimer; // damage is applied only while this is above 40
    /* 0x0F0A */ u16 deathTimer;
    /* 0x0F0C */ s16 alpha;
    /* 0x0F10 */ Actor* beamos[3];
    /* 0x0F1C */ Actor* lights[ML_BOSSVA_LIGHT_COUNT];
    /* 0x0FA8 */ Vec3f limbPos[ML_BOSSVA_LIMB_MAX];
    /* 0x114C */ f32 lightScale;
    /* 0x1150 */ u16 shotCount; // phase 1 shots; lengthens the rewind timer
} MlBossVa;

#define dgMlBossVaDeathAnim "__OTR__objects/object_ml_bv/gMlBossVaDeathAnim"
static const ALIGN_ASSET(2) char gMlBossVaDeathAnim[] = dgMlBossVaDeathAnim;
#define dgMlBossVaHurtAnim "__OTR__objects/object_ml_bv/gMlBossVaHurtAnim"
static const ALIGN_ASSET(2) char gMlBossVaHurtAnim[] = dgMlBossVaHurtAnim;
#define dgMlBossVaIdleAnim "__OTR__objects/object_ml_bv/gMlBossVaIdleAnim"
static const ALIGN_ASSET(2) char gMlBossVaIdleAnim[] = dgMlBossVaIdleAnim;
#define dgMlBossVaIntroAnim "__OTR__objects/object_ml_bv/gMlBossVaIntroAnim"
static const ALIGN_ASSET(2) char gMlBossVaIntroAnim[] = dgMlBossVaIntroAnim;
#define dgMlBossVaLungeAnim "__OTR__objects/object_ml_bv/gMlBossVaLungeAnim"
static const ALIGN_ASSET(2) char gMlBossVaLungeAnim[] = dgMlBossVaLungeAnim;
#define dgMlBossVaLungeHold2Anim "__OTR__objects/object_ml_bv/gMlBossVaLungeHold2Anim"
static const ALIGN_ASSET(2) char gMlBossVaLungeHold2Anim[] = dgMlBossVaLungeHold2Anim;
#define dgMlBossVaLungeHoldAnim "__OTR__objects/object_ml_bv/gMlBossVaLungeHoldAnim"
static const ALIGN_ASSET(2) char gMlBossVaLungeHoldAnim[] = dgMlBossVaLungeHoldAnim;
#define dgMlBossVaRetreatAnim "__OTR__objects/object_ml_bv/gMlBossVaRetreatAnim"
static const ALIGN_ASSET(2) char gMlBossVaRetreatAnim[] = dgMlBossVaRetreatAnim;
#define dgMlBossVaRiseAnim "__OTR__objects/object_ml_bv/gMlBossVaRiseAnim"
static const ALIGN_ASSET(2) char gMlBossVaRiseAnim[] = dgMlBossVaRiseAnim;
#define dgMlBossVaShootAnim "__OTR__objects/object_ml_bv/gMlBossVaShootAnim"
static const ALIGN_ASSET(2) char gMlBossVaShootAnim[] = dgMlBossVaShootAnim;
#define dgMlBossVaSkel "__OTR__objects/object_ml_bv/gMlBossVaSkel"
static const ALIGN_ASSET(2) char gMlBossVaSkel[] = dgMlBossVaSkel;
#define dgMlBossVaTitleCardTex "__OTR__objects/object_ml_bv/gMlBossVaTitleCardTex"
static const ALIGN_ASSET(2) char gMlBossVaTitleCardTex[] = dgMlBossVaTitleCardTex;
#define dgMlBossVaTurnLeftAnim "__OTR__objects/object_ml_bv/gMlBossVaTurnLeftAnim"
static const ALIGN_ASSET(2) char gMlBossVaTurnLeftAnim[] = dgMlBossVaTurnLeftAnim;
#define dgMlBossVaTurnRightAnim "__OTR__objects/object_ml_bv/gMlBossVaTurnRightAnim"
static const ALIGN_ASSET(2) char gMlBossVaTurnRightAnim[] = dgMlBossVaTurnRightAnim;
#define dgMlHakadanBsDeathCs "__OTR__scenes/shared/HAKAdan_bs_scene/gMlHakadanBsDeathCs"
static const ALIGN_ASSET(2) char gMlHakadanBsDeathCs[] = dgMlHakadanBsDeathCs;
#define dgMlHakadanBsIntroCs "__OTR__scenes/shared/HAKAdan_bs_scene/gMlHakadanBsIntroCs"
static const ALIGN_ASSET(2) char gMlHakadanBsIntroCs[] = dgMlHakadanBsIntroCs;
#define dgMlHakadanBsKnockdownCs "__OTR__scenes/shared/HAKAdan_bs_scene/gMlHakadanBsKnockdownCs"
static const ALIGN_ASSET(2) char gMlHakadanBsKnockdownCs[] = dgMlHakadanBsKnockdownCs;

// ML numbers its spheres 1-51 and maps limbs to them in a switch (0x80800400); each sphere's limb below is the limb
// that switch sends it to, so the stock Collider_UpdateSpheres does the same work.
static ColliderJntSphElementInit sJntSphElementsInit[] = {
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 24, { { -17, -34, -8 }, 294 }, 10 } }, // ML element id 1
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 23, { { -201, 2, 181 }, 272 }, 10 } }, // ML element id 2
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 23, { { -503, 10, 516 }, 272 }, 10 } }, // ML element id 3
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 24, { { -363, -11, 376 }, 294 }, 10 } }, // ML element id 4
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 24, { { -660, 7, 709 }, 294 }, 10 } }, // ML element id 5
    { { 0x02, { 0xFFCFFFFF, 0x00, 0x04 }, { 0x00000008, 0x00, 0x40 }, 0x09, 0x05, 0x01 },
      { 25, { { -259, -59, 288 }, 400 }, 10 } }, // ML element id 6
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 27, { { 22, -18, 8 }, 296 }, 10 } }, // ML element id 7
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 26, { { -232, 18, 78 }, 273 }, 10 } }, // ML element id 8
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 26, { { -666, 26, 201 }, 273 }, 10 } }, // ML element id 9
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 27, { { -475, 5, 150 }, 296 }, 10 } }, // ML element id 10
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 27, { { -904, 23, 273 }, 296 }, 10 } }, // ML element id 11
    { { 0x02, { 0xFFCFFFFF, 0x00, 0x04 }, { 0x00000008, 0x00, 0x40 }, 0x09, 0x05, 0x01 },
      { 28, { { -343, -43, 122 }, 402 }, 10 } }, // ML element id 12
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 30, { { 17, 30, -12 }, 296 }, 10 } }, // ML element id 13
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 29, { { -235, 66, -93 }, 274 }, 10 } }, // ML element id 14
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 29, { { -675, 74, -192 }, 274 }, 10 } }, // ML element id 15
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 30, { { -488, 53, -125 }, 296 }, 10 } }, // ML element id 16
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 30, { { -923, 71, -221 }, 296 }, 10 } }, // ML element id 17
    { { 0x02, { 0xFFCFFFFF, 0x00, 0x04 }, { 0x00000008, 0x00, 0x40 }, 0x09, 0x05, 0x01 },
      { 31, { { -363, 5, -64 }, 403 }, 10 } }, // ML element id 18
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 33, { { 22, 20, 9 }, 295 }, 10 } }, // ML element id 19
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 32, { { -163, 56, -179 }, 273 }, 10 } }, // ML element id 20
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 32, { { -492, 64, -488 }, 273 }, 10 } }, // ML element id 21
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 33, { { -356, 43, -344 }, 295 }, 10 } }, // ML element id 22
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 33, { { -682, 61, -648 }, 295 }, 10 } }, // ML element id 23
    { { 0x02, { 0xFFCFFFFF, 0x00, 0x04 }, { 0x00000008, 0x00, 0x40 }, 0x09, 0x05, 0x01 },
      { 34, { { -270, -5, -238 }, 401 }, 10 } }, // ML element id 24
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 12, { { 8, -92, -18 }, 294 }, 10 } }, // ML element id 25
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 11, { { 191, -56, 171 }, 272 }, 10 } }, // ML element id 26
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 11, { { 494, -48, 506 }, 272 }, 10 } }, // ML element id 27
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 12, { { 353, -69, 366 }, 294 }, 10 } }, // ML element id 28
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 12, { { 651, -51, 699 }, 294 }, 10 } }, // ML element id 29
    { { 0x02, { 0xFFCFFFFF, 0x00, 0x04 }, { 0x00000008, 0x00, 0x40 }, 0x09, 0x05, 0x01 },
      { 13, { { 250, -118, 278 }, 400 }, 10 } }, // ML element id 30
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 15, { { -32, -76, -2 }, 296 }, 10 } }, // ML element id 31
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 14, { { 222, -40, 68 }, 273 }, 10 } }, // ML element id 32
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 14, { { 657, -32, 191 }, 273 }, 10 } }, // ML element id 33
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 15, { { 465, -53, 139 }, 296 }, 10 } }, // ML element id 34
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 15, { { 894, -35, 263 }, 296 }, 10 } }, // ML element id 35
    { { 0x02, { 0xFFCFFFFF, 0x00, 0x04 }, { 0x00000008, 0x00, 0x40 }, 0x09, 0x05, 0x01 },
      { 16, { { 333, -102, 112 }, 402 }, 10 } }, // ML element id 36
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 18, { { -26, -28, -22 }, 296 }, 10 } }, // ML element id 37
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 17, { { 225, 8, -103 }, 274 }, 10 } }, // ML element id 38
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 17, { { 665, 16, -203 }, 274 }, 10 } }, // ML element id 39
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 18, { { 478, -5, -135 }, 296 }, 10 } }, // ML element id 40
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 18, { { 914, 13, -231 }, 296 }, 10 } }, // ML element id 41
    { { 0x02, { 0xFFCFFFFF, 0x00, 0x04 }, { 0x00000008, 0x00, 0x40 }, 0x09, 0x05, 0x01 },
      { 19, { { 353, -54, -74 }, 403 }, 10 } }, // ML element id 42
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 21, { { -31, -38, -2 }, 295 }, 10 } }, // ML element id 43
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 20, { { 153, -2, -189 }, 273 }, 10 } }, // ML element id 44
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 20, { { 482, 6, -498 }, 273 }, 10 } }, // ML element id 45
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 21, { { 346, -15, -355 }, 295 }, 10 } }, // ML element id 46
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0x00000008, 0x00, 0x40 }, 0x00, 0x01, 0x01 },
      { 21, { { 673, 3, -658 }, 295 }, 10 } }, // ML element id 47
    { { 0x02, { 0xFFCFFFFF, 0x00, 0x04 }, { 0x00000008, 0x00, 0x40 }, 0x09, 0x05, 0x01 },
      { 22, { { 261, -63, -249 }, 401 }, 10 } }, // ML element id 48
    { { 0x02, { 0xFFCFFFFF, 0x04, 0x08 }, { 0x00000008, 0x00, 0x40 }, 0x09, 0x05, 0x01 },
      { 2, { { 0, 555, -1 }, 929 }, 10 } }, // ML element id 49
    { { 0x02, { 0xFFCFFFFF, 0x04, 0x08 }, { 0x00000008, 0x00, 0x40 }, 0x09, 0x05, 0x01 },
      { 8, { { 0, -125, 244 }, 566 }, 10 } }, // ML element id 50
    { { 0x02, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, 0x00, 0x01, 0x01 },
      { 2, { { 0, 272, -756 }, 566 }, 10 } }, // ML element id 51
};

static ColliderJntSphInit sJntSphInit = {
    {
        COLTYPE_HIT6,
        AT_ON | AT_TYPE_ENEMY,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_JNTSPH,
    },
    ARRAY_COUNT(sJntSphElementsInit),
    sJntSphElementsInit,
};

static CollisionCheckInfoInit sColChkInfoInit = { 1, 15, 30, 30 };

static DamageTable sDamageTable = { {
    0xD0, 0xF2, 0xF1, 0xF0, 0xD0, 0xF2, 0xF2, 0xC0, 0xF1, 0xF2, 0xF4, 0xE2, 0xF1, 0xE4, 0xE2, 0xE2,
    0xEF, 0x60, 0x60, 0x60, 0x00, 0x00, 0xF1, 0xF4, 0xF2, 0xF2, 0xF8, 0xF4, 0xF4, 0x00, 0xF4, 0x00,
} };

// (scale factor, y offset) per flame, indexed like limbPos
static f32 sLightParams[34][2] = {
    { 0.02f, -80.0f },
    { 0.015f, 20.0f },
    { 0.025f, 20.0f },
    { 0.015f, 20.0f },
    { 0.01f, -20.0f },
    { 0.01f, -20.0f },
    { 0.01f, -50.0f },
    { 0.015f, -120.0f },
    { 0.005f, -20.0f },
    { 0.005f, -20.0f },
    { 0.005f, -20.0f },
    { 0.005f, -20.0f },
    { 0.005f, -20.0f },
    { 0.01f, -40.0f },
    { 0.005f, -20.0f },
    { 0.005f, -20.0f },
    { 0.01f, -40.0f },
    { 0.005f, -20.0f },
    { 0.005f, -20.0f },
    { 0.01f, -40.0f },
    { 0.005f, -20.0f },
    { 0.005f, -20.0f },
    { 0.01f, -40.0f },
    { 0.005f, -20.0f },
    { 0.005f, -20.0f },
    { 0.005f, -20.0f },
    { 0.005f, -20.0f },
    { 0.005f, -20.0f },
    { 0.01f, -40.0f },
    { 0.005f, -20.0f },
    { 0.005f, -20.0f },
    { 0.01f, -40.0f },
    { 0.005f, -20.0f },
    { 0.005f, -20.0f },
};

static InitChainEntry sInitChain[] = {
    ICHAIN_VEC3F_DIV1000(scale, 100, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneForward, 3000, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneScale, 500, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneDownward, 1000, ICHAIN_CONTINUE),
    ICHAIN_U8(naviEnemyId, 0x14, ICHAIN_STOP),
};

static const char* sAnims[] = {
    gMlBossVaRiseAnim,      gMlBossVaLungeHoldAnim,  gMlBossVaShootAnim,    gMlBossVaRetreatAnim,
    gMlBossVaHurtAnim,      gMlBossVaDeathAnim,      gMlBossVaLungeAnim,    gMlBossVaIdleAnim,
    gMlBossVaIntroAnim,     gMlBossVaLungeHold2Anim, gMlBossVaTurnLeftAnim, gMlBossVaTurnRightAnim,
};

static Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

// SoH difference: ML keeps writing through these pointers after the actor may have been deleted.
static s32 MlBossVa_IsLive(PlayState* play, Actor* actor) {
    s32 i;
    Actor* it;

    if (actor == NULL) {
        return false;
    }
    for (i = 0; i < ACTORCAT_MAX; i++) {
        for (it = play->actorCtx.actorLists[i].head; it != NULL; it = it->next) {
            if (it == actor) {
                return true;
            }
        }
    }
    return false;
}

static void MlBossVa_SetAction(MlBossVa* this, s16 action) {
    this->action = action;
    this->timer = 0;
    this->damageTaken = 0;
}

// ML 0x80801014 / 0x80801094: hold an animation at one frame. ML passes playSpeed 0 and the last frame as a raw
// integer where Animation_Change expects a float; the endFrame it stores is only read during the intro.
static void MlBossVa_SetFrame(MlBossVa* this, s32 frame, f32 morphFrames) {
    Animation_Change(&this->skelAnime, (AnimationHeader*)sAnims[this->animIndex], 0.0f, (f32)frame, 0.0f, ANIMMODE_LOOP, morphFrames);
}

// ML 0x808006A0: a direction from limb 7's rotation, turned by the boss's yaw. ML's y term is
// sqrt|1 - (x^2 - z^2)|, kept as written.
static void MlBossVa_LimbDir(Vec3s* rot, MlBossVa* this, Vec3f* out) {
    f32 fx = (rot->x * 2) * (1.0f / 16384.0f);
    f32 fz = (rot->z * 2) * (1.0f / 16384.0f);
    f32 s = Math_SinS(this->actor.shape.rot.y);
    f32 c = Math_CosS(this->actor.shape.rot.y);

    out->x = fx * s + fz * c;
    out->z = fz * s + fx * c;
    out->y = sqrtf(fabsf(1.0f - (SQ(out->x) - SQ(out->z))));
}

// ML 0x80800804: turn a fairy's offset around the y axis; returns the fairy's new heading.
static s16 MlBossVa_RotateOffset(Vec3f* out, Vec3f* in, s16 angle) {
    f32 r = sqrtf(SQ(in->x) + SQ(in->z));
    // ML calls Math_Atan2S without a prototype and passes (double)z, (double)x; the callee reads the low words,
    // which are 0 for these integer offsets.
    s16 yaw = Math_Atan2S(0.0f, 0.0f) + angle;

    out->x = Math_SinS(yaw) * r;
    out->z = Math_CosS(yaw) * r;
    out->y = in->y;
    return yaw + 0x4000;
}

static void MlBossVa_Init(Actor* thisx, PlayState* play) {
    static Vec3f sFairyOffsets[] = {
        { 8.0f, 5.0f, -6.0f },
        { 5.0f, -10.0f, 9.0f },
        { -6.0f, -15.0f, 2.0f },
        { -5.0f, 8.0f, -10.0f },
    };
    MlBossVa* this = (MlBossVa*)thisx;
    s32 i;

    this->turning = false;
    this->player = GET_PLAYER(play);
    this->alpha = 255;
    this->phase = 0;
    this->animIndex = ML_BOSSVA_ANIM_INTRO;
    this->lightScale = 0.0f;
    this->deathTimer = 0;
    this->action = ML_BOSSVA_WAIT;
    Actor_ProcessInitChain(&this->actor, sInitChain);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)gMlBossVaSkel,
                       (AnimationHeader*)sAnims[this->animIndex], NULL, NULL, 0);

    for (i = 0; i < 4; i++) {
        this->fairyOffsets[i] = sFairyOffsets[i];
    }
    this->playSpeed = 1.0f;

    for (i = 0; i < 4; i++) {
        this->fairies[i] = Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ActorDB_RetrieveId("ML_Elf"),
                                              this->actor.world.pos.x, this->actor.world.pos.y + 50.0f,
                                              this->actor.world.pos.z, 0, 0, 0, FAIRY_KOKIRI);
    }
    // Rand_S16Offset(0, -1) is always 0. ML copies it into home/world yaw for the first two fairies only.
    for (i = 0; i < 4; i++) {
        if (this->fairies[i] != NULL) {
            this->fairies[i]->shape.rot.y = Rand_S16Offset(0, -1);
            if (i < 2) {
                this->fairies[i]->home.rot.y = this->fairies[i]->world.rot.y = this->fairies[i]->shape.rot.y;
            }
        }
    }

    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, &this->actor, &sJntSphInit, this->colliderElements);
    CollisionCheck_SetInfo(&this->actor.colChkInfo, &sDamageTable, &sColChkInfoInit);
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    this->actor.colChkInfo.health = 30;
    this->beamos[0] = this->beamos[1] = this->beamos[2] = NULL;
}

static void MlBossVa_Destroy(Actor* thisx, PlayState* play) {
    MlBossVa* this = (MlBossVa*)thisx;

    Collider_DestroyJntSph(play, &this->collider);
}

// ML 0x80801114: turn toward targetYaw. Returns true once facing it (and `force`, i.e. the timeout, is set, or
// Link is not near), which lets the caller pick an attack. While turning it plays the turn animations one frame at
// a time, paced by the turn speed.
static s32 MlBossVa_Turn(MlBossVa* this, PlayState* play, s16 targetYaw, s32 force) {
    s16 diff = this->actor.shape.rot.y - targetYaw;
    s16 step = diff >> 6;
    s16 limit = 0x800;
    s32 frame;

    if (step < 0) {
        step = -step;
    }
    if (force) {
        limit = 0x100;
        step = 0x200;
    }
    if (this->actor.xzDistToPlayer < 180.0f) {
        this->timer--;
        limit = 0x100;
        step = 0x200;
    }
    if ((u16)(diff + 0x2400) > 0x4800) {
        this->turning = true;
    }

    if (((diff < limit) && (-limit < diff)) || (this->rewindTimer != 0) || !this->turning) {
        if (this->rewindTimer != 0) {
            this->timer = 0;
        }
        this->turning = false;
        this->animIndex = ML_BOSSVA_ANIM_IDLE;
        this->turnFrameAccum = 0.0f;
        return force;
    }

    if (diff > 0x80) {
        this->animIndex = ML_BOSSVA_ANIM_TURN_RIGHT;
    } else if (diff < -0x80) {
        this->animIndex = ML_BOSSVA_ANIM_TURN_LEFT;
    }
    Math_ScaledStepToS(&this->actor.shape.rot.y, targetYaw, step);
    this->playSpeed = step * (1.0f / 256.0f);
    this->turnFrameAccum += this->playSpeed;
    if (this->turnFrameAccum > 1.0f) {
        frame = (u8)((s32)this->skelAnime.curFrame + 1);
        if (frame >= Animation_GetLastFrame((void*)sAnims[this->animIndex])) {
            frame = 0;
        }
        this->turnFrameAccum -= 1.0f;
        MlBossVa_SetFrame(this, frame, 5.0f);
    }
    return false;
}

// Action 0.
static void MlBossVa_Wait(MlBossVa* this, PlayState* play) {
    if ((this->actor.xzDistToPlayer < 500.0f) && (this->player->actor.world.pos.y < -2783.0f)) {
        Cutscene_SetSegment(play, (void*)gMlHakadanBsIntroCs);
        gSaveContext.cutsceneTrigger = 1;
        this->actor.targetMode = 4;
        this->action = ML_BOSSVA_INTRO;
    }
}

// Action 1.
static void MlBossVa_Intro(MlBossVa* this, PlayState* play) {
    this->timer++;
    if (this->timer == 120) {
        this->animating = true;
    } else if (this->timer == 145) {
        TitleCard_InitBossName(play, &play->actorCtx.titleCtx, (void*)gMlBossVaTitleCardTex, 160, 180, 128, 40,
                               false);
    }
    if ((this->skelAnime.curFrame == this->skelAnime.endFrame - 2.0f) && (this->animIndex == ML_BOSSVA_ANIM_INTRO)) {
        this->timer = 0;
        this->action = ML_BOSSVA_TURN;
    }
}

// Action 4, phase 0.
static void MlBossVa_Turn0(MlBossVa* this, PlayState* play) {
    s32 ready = MlBossVa_Turn(this, play, this->actor.yawTowardsPlayer, this->timer >= 120);

    this->timer++;
    if (ready) {
        MlBossVa_SetAction(this, (Rand_ZeroFloat(2.0f) < 1.0f) ? ML_BOSSVA_LUNGE : ML_BOSSVA_SHOOT);
        Audio_PlayActorSound2(&this->actor, 0x39E1);
    }
}

// Action 4, phase 1: as phase 0, but it aims 0x1200 past Link on the side it was already turning toward.
static void MlBossVa_Turn1(MlBossVa* this, PlayState* play) {
    s16 target;
    s16 diff;
    s32 ready;

    if (this->timer == 0) {
        this->lastYaw = this->actor.shape.rot.y;
    }
    target = this->targetYaw = this->actor.yawTowardsPlayer;
    diff = this->actor.shape.rot.y - this->actor.yawTowardsPlayer;
    if ((s16)ABS(diff) < 0x6A00) {
        diff = this->actor.shape.rot.y - this->lastYaw;
        if (diff < 0) {
            target = this->targetYaw = this->actor.yawTowardsPlayer - 0x1200;
        } else if (diff > 0) {
            target = this->targetYaw = this->actor.yawTowardsPlayer + 0x1200;
        }
    }
    this->lastYaw = this->actor.shape.rot.y;

    ready = MlBossVa_Turn(this, play, target, this->timer >= 200);
    this->timer++;
    if (ready) {
        MlBossVa_SetAction(this, (Rand_ZeroFloat(2.0f) < 0.67f) ? ML_BOSSVA_LUNGE_2 : ML_BOSSVA_SHOOT_2);
        Audio_PlayActorSound2(&this->actor, 0x39E1);
    }
}

// Actions 5 and 10: lunge forward, hold, back off.
static void MlBossVa_Lunge(MlBossVa* this, s16 holdAnim) {
    s16 lungeLen = Animation_GetLastFrame((void*)sAnims[ML_BOSSVA_ANIM_LUNGE]);
    s16 holdLen = Animation_GetLastFrame((void*)sAnims[holdAnim]);
    s16 retreatLen = Animation_GetLastFrame((void*)sAnims[ML_BOSSVA_ANIM_RETREAT]);

    this->timer++;
    if (this->timer < (u16)lungeLen) {
        this->animIndex = ML_BOSSVA_ANIM_LUNGE;
        this->actor.speedXZ = 100.0f / (u16)lungeLen;
    } else if (this->timer < (u16)holdLen + (u16)lungeLen) {
        this->animIndex = holdAnim;
        this->actor.speedXZ = 0.0f;
    } else if (this->timer < (u16)retreatLen + (u16)holdLen + (u16)lungeLen) {
        this->animIndex = ML_BOSSVA_ANIM_RETREAT;
        this->actor.speedXZ = -100.0f / (u16)retreatLen;
    } else {
        MlBossVa_SetAction(this, ML_BOSSVA_TURN);
        this->actor.speedXZ = 0.0f;
    }
}

// ML 0x808014BC: an ML_BossVaShot from 160 units ahead, turned up to 0x280 toward Link.
static void MlBossVa_SpawnShot(MlBossVa* this, PlayState* play) {
    s16 yaw = this->actor.shape.rot.y;

    Math_ScaledStepToS(&yaw, this->actor.yawTowardsPlayer, 0x280);
    Actor_Spawn(&play->actorCtx, play, ActorDB_RetrieveId("ML_BossVaShot"),
                this->actor.world.pos.x + Math_SinS(this->actor.shape.rot.y) * 160.0f,
                this->actor.world.pos.y + 22.0f,
                this->actor.world.pos.z + Math_CosS(this->actor.shape.rot.y) * 160.0f, 0, yaw, 0, 0xFFFF);
}

// Actions 6 and 11.
static void MlBossVa_Shoot(MlBossVa* this, PlayState* play) {
    s16 len = Animation_GetLastFrame((void*)sAnims[ML_BOSSVA_ANIM_SHOOT]);

    this->timer++;
    if (this->timer < (u16)len) {
        this->animIndex = ML_BOSSVA_ANIM_SHOOT;
    } else if (this->timer == (u16)len) {
        MlBossVa_SetAction(this, ML_BOSSVA_TURN);
        return;
    }
    if (this->timer == 16) {
        MlBossVa_SpawnShot(this, play);
        if (this->action == ML_BOSSVA_SHOOT_2) {
            this->shotCount++;
        }
    }
}

// Action 7: drift home, flash, play the hurt animation frame by frame; at 0 health, start the knockdown or death.
static void MlBossVa_Hurt(MlBossVa* this, PlayState* play) {
    this->actor.world.pos.x += (this->actor.home.pos.x - this->actor.world.pos.x) * 0.05f;
    this->actor.world.pos.z += (this->actor.home.pos.z - this->actor.world.pos.z) * 0.05f;
    this->timer++;
    this->actor.speedXZ = 0.0f;
    this->shotCount = 0;
    this->rewindTimer = 0;
    this->animIndex = ML_BOSSVA_ANIM_HURT;
    this->hurtFrame++;
    if (this->timer <= 32) {
        this->hurtFrame = this->timer;
    }
    if ((this->timer < 100) && (this->damageTaken < 15)) {
        if (this->actor.colChkInfo.health != 0) {
            this->actor.colorFilterParams = 0x5FF1;
            this->hurtFrame++;
            this->actor.colorFilterTimer = 3;
            Player_PlaySfx(&this->actor, 0x3956);
        }
        if ((this->hurtFrame == 0x22) || (this->hurtFrame >= 0x2C)) {
            this->hurtFrame = 0x21;
        }
    }
    MlBossVa_SetFrame(this, this->hurtFrame, 1.0f);

    if (this->hurtFrame < 0x2D) {
        if (this->damageTaken >= 20) {
            this->invincibleTimer = 0;
        }
    } else {
        this->invincibleTimer = 0;
        if (this->hurtFrame >= 0x50) {
            MlBossVa_SetAction(this, ML_BOSSVA_TURN);
        }
    }

    if (this->actor.colChkInfo.health == 0) {
        MlBossVa_SetAction(this, ML_BOSSVA_DOWN);
        this->invincibleTimer = 0;
        Cutscene_SetSegment(play, (this->phase != 0) ? (void*)gMlHakadanBsDeathCs : (void*)gMlHakadanBsKnockdownCs);
        gSaveContext.cutsceneTrigger = 1;
        this->actor.shape.rot = this->actor.home.rot;
        this->actor.world.pos = this->actor.home.pos;
    }
}

// Action 9, phase 0: full health again and the rise animation; when it ends, phase 1.
static void MlBossVa_Regroup(MlBossVa* this, PlayState* play) {
    if ((this->animIndex == ML_BOSSVA_ANIM_RISE) &&
        (Animation_GetLastFrame((void*)sAnims[ML_BOSSVA_ANIM_RISE]) == this->skelAnime.curFrame)) {
        this->phase = 1;
        this->action = ML_BOSSVA_TURN;
        this->damageTaken = 0;
        this->timer = 1;
    } else {
        this->timer++;
    }
    this->invincibleTimer = 0;
    this->actor.colChkInfo.health = 30;
    this->actor.speedXZ = 0.0f;
    this->animIndex = ML_BOSSVA_ANIM_RISE;
}

// Action 9, phase 1: flames spread over the body, it fades, and a blue warp appears.
static void MlBossVa_Die(MlBossVa* this, PlayState* play) {
    f32 scale;
    s32 i;

    if ((this->animIndex == ML_BOSSVA_ANIM_DEATH) &&
        (Animation_GetLastFrame((void*)sAnims[ML_BOSSVA_ANIM_DEATH]) == this->skelAnime.curFrame)) {
        if (this->alpha == 255) {
            for (i = 0; i < ML_BOSSVA_LIGHT_COUNT; i++) {
                if (MlBossVa_IsLive(play, this->lights[i])) {
                    Actor_Kill(this->lights[i]);
                }
                this->lights[i] = NULL; // SoH: ML keeps moving the killed flames below
            }
            Actor_Spawn(&play->actorCtx, play, ACTOR_DOOR_WARP1, this->actor.home.pos.x, this->actor.home.pos.y,
                        this->actor.home.pos.z, 0, 0, 0, WARP_DUNGEON_ADULT);
            GameInteractor_ExecuteOnBossDefeat(&this->actor); // MissingShip (MS-145): SoH's boss hooks (stats)
        }
        this->skelAnime.curFrame = Animation_GetLastFrame((void*)sAnims[ML_BOSSVA_ANIM_DEATH]) - 1.0f;
        this->alpha -= 15;
        if (this->alpha == 0) {
            Actor_Kill(&this->actor);
        }
    }

    if (this->deathTimer == 0) {
        for (i = 0; i < ML_BOSSVA_LIGHT_COUNT; i++) {
            this->lights[i] = Actor_Spawn(&play->actorCtx, play, ActorDB_RetrieveId("ML_Light"), this->actor.home.pos.x,
                                          this->actor.home.pos.y, this->actor.home.pos.z, 0, 0, 0, 0);
        }
        this->lightScale = 0.0f;
        this->alpha = 255;
    }
    if (this->deathTimer < 80) {
        this->lightScale += 0.025f;
    } else if (this->deathTimer > 200) {
        this->lightScale -= 0.04f;
    }
    // Flame i sits on limbPos[i]; index 0 is never written (limbs are 1-based), so the first flame stays at the
    // origin, as in ML.
    for (i = 0; i < ML_BOSSVA_LIGHT_COUNT; i++) {
        if (!MlBossVa_IsLive(play, this->lights[i])) {
            continue;
        }
        scale = sLightParams[i][0] * this->lightScale;
        this->lights[i]->scale.x = this->lights[i]->scale.y = this->lights[i]->scale.z = scale;
        this->lights[i]->world.pos.x = this->limbPos[i].x;
        this->lights[i]->world.pos.y = this->limbPos[i].y + sLightParams[i][1];
        this->lights[i]->world.pos.z = this->limbPos[i].z;
    }
    this->deathTimer = MIN(this->deathTimer + 2, 255);
    this->timer++;
    this->animIndex = ML_BOSSVA_ANIM_DEATH;
}

// ML 0x80801E4C.
static void MlBossVa_PlayRattle(MlBossVa* this) {
    func_800F436C(&this->actor.projectedPos, 0x28AB, 1.0f + Rand_CenteredFloat(0.3f));
}

// ML 0x80801E90: sounds keyed to animation frames.
static void MlBossVa_PlayAnimSfx(MlBossVa* this) {
    s32 frame = (s32)this->skelAnime.curFrame;

    switch (this->animIndex) {
        case ML_BOSSVA_ANIM_RISE:
            if (frame == 30) {
                Audio_PlayActorSound2(&this->actor, 0x381A);
            } else if (frame == 55) {
                Audio_PlayActorSound2(&this->actor, 0x3813);
            } else if (frame == 13) {
                Audio_PlayActorSound2(&this->actor, 0x390D);
            }
            break;
        case ML_BOSSVA_ANIM_LUNGE_HOLD:
            if (frame == 4) {
                Audio_PlayActorSound2(&this->actor, 0x39E0);
            } else if (frame == 14) {
                Audio_PlayActorSound2(&this->actor, 0x3952);
            }
            break;
        case ML_BOSSVA_ANIM_SHOOT:
            if (frame == 8) {
                Audio_PlayActorSound2(&this->actor, 0x3994);
            }
            break;
        case ML_BOSSVA_ANIM_RETREAT:
            if ((frame == 1) || (frame == 4) || (frame == 13) || (frame == 16)) {
                MlBossVa_PlayRattle(this);
            }
            break;
        case ML_BOSSVA_ANIM_LUNGE:
            if ((frame == 1) || (frame == 7) || (frame == 12)) {
                MlBossVa_PlayRattle(this);
            }
            break;
        case ML_BOSSVA_ANIM_INTRO:
            if (frame == 64) {
                Audio_PlayActorSound2(&this->actor, 0x398F);
            } else if (frame == 102) {
                Audio_PlayActorSound2(&this->actor, 0x39E1);
            } else if (frame == 32) {
                Audio_PlayActorSound2(&this->actor, 0x288E);
            }
            break;
        case ML_BOSSVA_ANIM_LUNGE_HOLD_2:
            if ((frame == 5) || (frame == 14)) {
                Audio_PlayActorSound2(&this->actor, 0x39E0);
            }
            break;
        case ML_BOSSVA_ANIM_TURN_LEFT:
            if ((frame == 1) || (frame == 5) || (frame == 17) || (frame == 21)) {
                MlBossVa_PlayRattle(this);
            }
            break;
        case ML_BOSSVA_ANIM_TURN_RIGHT:
            if ((frame == 1) || (frame == 4) || (frame == 16) || (frame == 20)) {
                MlBossVa_PlayRattle(this);
            }
            break;
    }
}

// Phase 1, action 4: three Beamos ride the heads (limbs 4-6), 80 units below them.
static void MlBossVa_UpdateBeamos(MlBossVa* this, PlayState* play) {
    s32 i;

    if (this->beamos[0] == NULL) {
        for (i = 0; i < 3; i++) {
            this->beamos[i] =
                Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_EN_VM, this->actor.world.pos.x,
                                   this->actor.world.pos.y + 50.0f, this->actor.world.pos.z, 0, 0, 0, 0x7F00);
        }
    }
    for (i = 0; i < 3; i++) {
        if (MlBossVa_IsLive(play, this->beamos[i])) {
            this->beamos[i]->world.pos.x = this->limbPos[4 + i].x;
            this->beamos[i]->world.pos.y = this->limbPos[4 + i].y - 80.0f;
            this->beamos[i]->world.pos.z = this->limbPos[4 + i].z;
        }
    }
}

static void MlBossVa_KillBeamos(MlBossVa* this, PlayState* play) {
    s32 i;

    if (this->beamos[0] != NULL) {
        for (i = 0; i < 3; i++) {
            if (MlBossVa_IsLive(play, this->beamos[i])) {
                Actor_Kill(this->beamos[i]);
            }
            this->beamos[i] = NULL;
        }
    }
}

static void MlBossVa_Update(Actor* thisx, PlayState* play) {
    MlBossVa* this = (MlBossVa*)thisx;
    s32 anyPhaseRewind = false;
    u8 health;

    if (this->animating) {
        SkelAnime_Update(&this->skelAnime);
    }
    if (this->skelAnime.animation != sAnims[this->animIndex]) {
        Animation_Change(&this->skelAnime, (AnimationHeader*)sAnims[this->animIndex], this->playSpeed, 0.0f, 0.0f, ANIMMODE_LOOP, 5.0f);
    }
    this->playSpeed = 1.0f;

    if (this->phase == 0) {
        switch (this->action) {
            case ML_BOSSVA_WAIT:
                MlBossVa_Wait(this, play);
                anyPhaseRewind = (this->action == ML_BOSSVA_WAIT);
                break;
            case ML_BOSSVA_INTRO:
                MlBossVa_Intro(this, play);
                break;
            case ML_BOSSVA_START:
                this->timer = 0;
                this->action = ML_BOSSVA_TURN;
                anyPhaseRewind = true;
                break;
            case ML_BOSSVA_TURN:
                MlBossVa_Turn0(this, play);
                break;
            case ML_BOSSVA_LUNGE:
                MlBossVa_Lunge(this, ML_BOSSVA_ANIM_LUNGE_HOLD);
                break;
            case ML_BOSSVA_SHOOT:
                MlBossVa_Shoot(this, play);
                break;
            case ML_BOSSVA_HURT:
                MlBossVa_Hurt(this, play);
                break;
            case ML_BOSSVA_DOWN:
                MlBossVa_Regroup(this, play);
                break;
        }
    } else if (this->phase == 1) {
        if ((this->action == ML_BOSSVA_TURN) && (this->rewindTimer == 0)) {
            MlBossVa_UpdateBeamos(this, play);
        } else if (this->action != ML_BOSSVA_TURN) {
            MlBossVa_KillBeamos(this, play);
        }
        switch (this->action) {
            case ML_BOSSVA_TURN:
                MlBossVa_Turn1(this, play);
                break;
            case ML_BOSSVA_HURT:
                MlBossVa_Hurt(this, play);
                break;
            case ML_BOSSVA_DOWN:
                MlBossVa_Die(this, play);
                break;
            case ML_BOSSVA_LUNGE_2:
                MlBossVa_Lunge(this, ML_BOSSVA_ANIM_LUNGE_HOLD_2);
                break;
            case ML_BOSSVA_SHOOT_2:
                MlBossVa_Shoot(this, play);
                break;
        }
    }

    // While a Magic Hourglass rewind plays the boss holds still for a while (the turn resets instead of attacking).
    // ML skips the phase test when coming from the wait and start actions.
    if (MlHourglass_RewindStep() != 0) {
        if (anyPhaseRewind || (this->phase == 0)) {
            this->rewindTimer = 60;
        } else if (this->phase == 1) {
            this->rewindTimer = this->shotCount + 0x26;
        }
    }
    if (this->rewindTimer != 0) {
        this->rewindTimer--;
    }

    this->actor.world.rot.y = this->actor.shape.rot.y;
    Actor_MoveXZGravity(&this->actor);

    this->invincibleTimer++;
    if (this->invincibleTimer > 250) {
        this->invincibleTimer = 250;
        Actor_ApplyDamage(&this->actor);
    } else if (this->invincibleTimer > 40) {
        Actor_ApplyDamage(&this->actor);
    }
    health = this->actor.colChkInfo.health;
    if ((health < this->lastHealth) && (this->action != ML_BOSSVA_HURT)) {
        MlBossVa_SetAction(this, ML_BOSSVA_HURT);
    }
    this->damageTaken += this->lastHealth - health;
    this->lastHealth = health;

    MlBossVa_PlayAnimSfx(this);

    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    if (this->action != ML_BOSSVA_HURT) {
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
}

// ML 0x808025E0: a random wobble target, each axis in [-910, 910).
static void MlBossVa_RandomWobble(Vec3s* target) {
    target->x = Rand_ZeroFloat(1820.0f) - 910.0f;
    target->y = Rand_ZeroFloat(1820.0f) - 910.0f;
    target->z = Rand_ZeroFloat(1820.0f) - 910.0f;
}

// Heads (limbs 4-6): in phase 0 they wobble around a fixed pose; in phase 1 they aim at Link.
static s32 MlBossVa_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* thisx,
                                     Gfx** gfx) {
    static const Vec3s sPose[] = {
        { 0x0C16, -0x216A, 0x5DD8 },
        { 0x02D8, -0x105A, -0x016C },
        { 0x093E, -0x2D80, -0x7054 },
    };
    static const f32 sAimDist[] = { 197.0f, 62.0f, 30.0f };
    static const s16 sAimPitch[] = { 0x100, 0x1000, 0x1000 };
    static const s16 sAimYaw[] = { 0, 0x1800, 0 };
    MlBossVa* this = (MlBossVa*)thisx;
    MlBossVaWobble* w;
    s16 x;
    s16 y;
    s16 z;

    if ((u16)this->action < 2) {
        return false;
    }
    if ((limbIndex < 4) || (limbIndex > 6)) {
        return false;
    }
    w = &this->wobble[limbIndex - 4];
    if (w->timer == 0) {
        w->timer = (u8)(Rand_ZeroFloat(5.0f) + 5.0f);
        MlBossVa_RandomWobble(&w->target);
    }
    // cur = (cur + target) * 7/8, with C's truncating division.
    x = w->cur.x + w->target.x;
    y = w->cur.y + w->target.y;
    z = w->cur.z + w->target.z;
    w->cur.x = x - x / 8;
    w->cur.y = y - y / 8;
    w->cur.z = z - z / 8;
    w->timer--;

    if (this->phase == 1) {
        rot->y = (s16)(this->actor.yawTowardsPlayer + sAimYaw[limbIndex - 4] - this->actor.shape.rot.y);
        rot->z = 0;
        rot->x =
            (s16)((s32)(sAimDist[limbIndex - 4] / this->actor.xzDistToPlayer * 2048.0f) + sAimPitch[limbIndex - 4]);
    } else {
        rot->x = w->cur.x + sPose[limbIndex - 4].x;
        rot->y = w->cur.y + sPose[limbIndex - 4].y;
        rot->z = w->cur.z + sPose[limbIndex - 4].z;
    }
    return false;
}

static void MlBossVa_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx, Gfx** gfx) {
    MlBossVa* this = (MlBossVa*)thisx;
    Vec3f dir = { 0.0f, 0.0f, 0.0f };
    Vec3f offset;
    Vec3f target;
    f32 remaining;
    f32 div;
    f32 s;
    f32 c;
    s32 i;

    Matrix_MultVec3f(&sZeroVec, &this->limbPos[limbIndex]);

    if (limbIndex == 8) {
        Matrix_MultVec3f(&sZeroVec, &this->actor.focus.pos);
    } else if ((limbIndex == 7) && (this->fairies[0] != NULL)) {
        if (this->animIndex != ML_BOSSVA_ANIM_RISE) {
            // The fairies circle limb 7.
            MlBossVa_LimbDir(rot, this, &dir);
            this->fairyAngle += 0x200;
            for (i = 0; i < 4; i++) {
                Actor* fairy = this->fairies[i];

                if (!MlBossVa_IsLive(play, fairy)) {
                    continue;
                }
                Matrix_MultVec3f(&sZeroVec, &fairy->world.pos);
                fairy->world.pos.x += dir.x * -55.0f;
                fairy->world.pos.y += dir.y * -55.0f;
                fairy->world.pos.z += dir.z * -55.0f;
                fairy->home.rot.y = MlBossVa_RotateOffset(&offset, &this->fairyOffsets[i], this->fairyAngle);
                fairy->world.pos.x += offset.x;
                fairy->world.pos.y += offset.y;
                fairy->world.pos.z += offset.z;
            }
        } else if (this->timer >= 14) {
            // Standing back up: the fairies fly 160 units ahead and vanish when the animation nears its end.
            s = Math_SinS(this->actor.shape.rot.y);
            c = Math_CosS(this->actor.shape.rot.y);
            remaining = Animation_GetLastFrame((void*)sAnims[ML_BOSSVA_ANIM_RISE]) - this->skelAnime.curFrame - 13.0f - 30.0f;
            div = sqrtf(fabsf(remaining)) * remaining;
            if (div > 0.0f) {
                target.x = this->actor.world.pos.x + s * 160.0f;
                target.y = this->actor.world.pos.y + 50.0f;
                target.z = this->actor.world.pos.z + c * 160.0f;
                for (i = 0; i < 4; i++) {
                    Actor* fairy = this->fairies[i];

                    if (!MlBossVa_IsLive(play, fairy)) {
                        continue;
                    }
                    fairy->world.pos.x += (target.x - fairy->world.pos.x) / div;
                    fairy->world.pos.y += (target.y - fairy->world.pos.y) / div;
                    fairy->world.pos.z += (target.z - fairy->world.pos.z) / div;
                }
            } else {
                for (i = 0; i < 4; i++) {
                    if (MlBossVa_IsLive(play, this->fairies[i])) {
                        Actor_Kill(this->fairies[i]);
                    }
                    this->fairies[i] = NULL;
                }
            }
        }
    }

    Collider_UpdateSpheres(limbIndex, &this->collider);
}

static void MlBossVa_Draw(Actor* thisx, PlayState* play) {
    MlBossVa* this = (MlBossVa*)thisx;
    u8 c = 255 - this->deathTimer;

    OPEN_DISPS(play->state.gfxCtx);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, c, c, c, this->alpha);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, c, c, c, this->alpha);
    gDPSetEnvColor(POLY_OPA_DISP++, c, c, c, this->alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, c, c, c, this->alpha);
    CLOSE_DISPS(play->state.gfxCtx);

    if ((this->alpha < 255) && (this->alpha != 0)) {
        func_80034CC4(play, &this->skelAnime, NULL, NULL, &this->actor, this->alpha);
    } else {
        func_80034BA0(play, &this->skelAnime, MlBossVa_OverrideLimbDraw, MlBossVa_PostLimbDraw, &this->actor,
                      this->alpha);
    }
}

void MlBossVa_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_BossVa";
    definition.description = "The Missing Link: Shadow Temple boss";
    definition.category = ACTORCAT_BOSS;
    definition.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = OBJECT_BV;
    definition.instanceSize = sizeof(MlBossVa);
    definition.init = MlBossVa_Init;
    definition.destroy = MlBossVa_Destroy;
    definition.update = MlBossVa_Update;
    definition.draw = MlBossVa_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_BossVa is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
