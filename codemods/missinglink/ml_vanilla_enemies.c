/*
 * The Missing Link's small edits to vanilla enemies, as a code mod: the Bombchu rail (En_Bom_Chu), Deku Babas' nuts
 * (En_Dekubaba), the Deku Scrub clamp (En_Dekunuts), the Baby Dodongo's dust (En_Dodojr), the giant Floormaster
 * (En_Floormas), Deku nuts and the Hylian Shield (En_Nutsball) and the Peahat's music (En_Peehat).
 *
 * MissingShip's Unbound branch patches these into each actor's code while ML is mounted (ledger rows 22, 52, 56, 65,
 * 73, 75, 93 and the code patches; MissingShip docs/UNBOUND_MAP.md). As the Business Scrubs do, this takes each one's
 * slot (its ActorDB entry) and goes around vanilla's code: ML's rules run before or after vanilla's Init and Update,
 * and where ML changed one action function, Update swaps it for ML's copy before vanilla's runs.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ActorDB.h"
#include "overlays/actors/ovl_En_Bom_Chu/z_en_bom_chu.h"
#include "overlays/actors/ovl_En_Dekubaba/z_en_dekubaba.h"
#include "overlays/actors/ovl_En_Dekunuts/z_en_dekunuts.h"
#include "overlays/actors/ovl_En_Dodojr/z_en_dodojr.h"
#include "overlays/actors/ovl_En_Floormas/z_en_floormas.h"
#include "overlays/actors/ovl_En_Nutsball/z_en_nutsball.h"
#include "overlays/effects/ovl_Effect_Ss_Hahen/z_eff_ss_hahen.h"
#include "objects/gameplay_keep/gameplay_keep.h"

#include <libultraship/bridge/consolevariablebridge.h>
#include <math.h>
#include <string.h>

#include "missinglink.h"

ML_HOST_FN_DECL(EnBomChu_Move);
ML_HOST_FN_DECL(EnDekubaba_ShrinkDie);
ML_HOST_FN_DECL(EnDodojr_CrawlTowardsTarget);
ML_HOST_FN_DECL(EnDodojr_Despawn);
ML_HOST_FN_DECL(EnDodojr_EmergeFromGround);
ML_HOST_FN_DECL(EnFloormas_Freeze);
ML_HOST_FN_DECL(EnFloormas_SmWait);
ML_HOST_FN_DECL(EnFloormas_TakeDamage);
ML_HOST_FN_DECL(func_80ABBBA8);

// vanilla's
void EnBomChu_Move(EnBomChu* this, PlayState* play);
void EnDekubaba_ShrinkDie(EnDekubaba* this, PlayState* play);
void EnDodojr_CrawlTowardsTarget(EnDodojr* this, PlayState* play);
void EnDodojr_EmergeFromGround(EnDodojr* this, PlayState* play);
void EnDodojr_Despawn(EnDodojr* this, PlayState* play);
void EnFloormas_SmWait(EnFloormas* this, PlayState* play);
void EnFloormas_SetupBigDecideAction(EnFloormas* this);
void EnFloormas_SetupSplit(EnFloormas* this);
void EnFloormas_SetupRecover(EnFloormas* this);
void EnFloormas_SetupStand(EnFloormas* this);
void EnFloormas_Die(EnFloormas* this, PlayState* play);
void EnFloormas_TakeDamage(EnFloormas* this, PlayState* play);
void EnFloormas_Freeze(EnFloormas* this, PlayState* play);
void func_80ABBBA8(EnNutsball* this, PlayState* play);

/*
 * En_Bom_Chu: ML's Bombchu rail in Hollow Root Keep (ledger row 93). ML calls its own routine (0x80607A00) at the end
 * of En_Bom_Chu's Update. In the Shadow Temple's scene, a Bombchu that reaches either end of the gap at z 1920-2192,
 * y 692 and up is put on the rail at y 692, z 2080 and turned to cross it: from x -342 and up it heads -x (from x -71
 * at most, moved 20 on), below x -588 it heads +x (from x -888 at least, moved 20 on).
 */

static ActorFunc sEnBomChuUpdate;

/*
 * SoH's Controllable Bombchus (RemoteBombchu.cpp) would steer a Bombchu against the rail, so our branch has it let go
 * of one on the rail's walkways or in their gap (VB_CONTROL_BOMBCHU): the camera goes back to Link, Link can move,
 * and that Bombchu is never taken again. Stock has no such check. Its steering runs at OnActorUpdate, right after this
 * Update, and lets go when A is pressed; so the first time a moving Bombchu is there, this presses A for that hook,
 * with the stick centred and B released so that the frame neither steers nor explodes it. The input is put back
 * before the next actor updates (or at the end of the frame), so nothing else sees the press.
 */
#define ML_CHU_RELEASED_MAX 8

static Actor* sMlChuReleased[ML_CHU_RELEASED_MAX];
static u8 sMlChuInputPending;
static u16 sMlChuSavedPress;
static s8 sMlChuSavedStickX;

static s32 MlEnBomChu_OnRail(Actor* thisx, PlayState* play) {
    return (play->sceneNum == SCENE_SHADOW_TEMPLE) && (thisx->world.pos.z >= 1920.0f) &&
           (thisx->world.pos.z < 2192.0f) && (thisx->world.pos.y >= 680.0f) && (thisx->world.pos.x >= -900.0f) &&
           (thisx->world.pos.x < -60.0f);
}

static void MlEnBomChu_LetGo(Actor* thisx, PlayState* play) {
    Input* input = &play->state.input[0];
    s32 free = -1;

    if (!CVarGetInteger("gEnhancements.RemoteBombchu", 0) || (thisx->update == NULL) ||
        !ML_IS_HOST_FN(((EnBomChu*)thisx)->actionFunc, EnBomChu_Move) || !MlEnBomChu_OnRail(thisx, play)) {
        return;
    }
    for (s32 i = 0; i < ML_CHU_RELEASED_MAX; i++) {
        if (sMlChuReleased[i] == thisx) {
            return;
        }
        if ((free < 0) && (sMlChuReleased[i] == NULL)) {
            free = i;
        }
    }
    if (free >= 0) {
        sMlChuReleased[free] = thisx;
    }
    if (!sMlChuInputPending) {
        sMlChuSavedPress = input->press.button;
        sMlChuSavedStickX = input->cur.stick_x;
        sMlChuInputPending = true;
    }
    input->press.button = (input->press.button | BTN_A) & ~BTN_B;
    input->cur.stick_x = 0;
}

static void MlEnBomChu_RestoreInput(void) {
    if (sMlChuInputPending && (gPlayState != NULL)) {
        gPlayState->state.input[0].press.button = sMlChuSavedPress;
        gPlayState->state.input[0].cur.stick_x = sMlChuSavedStickX;
    }
    sMlChuInputPending = false;
}

static void MlEnBomChu_ShouldActorUpdate(void* actor, bool* result) {
    MlEnBomChu_RestoreInput();
}

static void MlEnBomChu_OnGameFrameUpdate(void) {
    MlEnBomChu_RestoreInput();
}

static void MlEnBomChu_OnActorDestroy(void* actor) {
    for (s32 i = 0; i < ML_CHU_RELEASED_MAX; i++) {
        if (sMlChuReleased[i] == actor) {
            sMlChuReleased[i] = NULL;
        }
    }
}

static void MlEnBomChu_OnPlayDestroy(void) {
    memset(sMlChuReleased, 0, sizeof(sMlChuReleased));
}

static void MlEnBomChu_Update(Actor* thisx, PlayState* play) {
    sEnBomChuUpdate(thisx, play);
    if ((thisx->update != NULL) && (play->sceneNum == SCENE_SHADOW_TEMPLE) && (thisx->world.pos.z >= 1920.0f) &&
        (thisx->world.pos.z < 2192.0f) && (thisx->world.pos.y >= 692.0f)) {
        if (thisx->world.pos.x >= -342.0f) {
            thisx->world.pos.x = ((thisx->world.pos.x < -71.0f) ? thisx->world.pos.x : -71.0f) - 20.0f;
            thisx->world.pos.y = 692.0f;
            thisx->world.pos.z = 2080.0f;
            thisx->world.rot.x = thisx->world.rot.z = 0;
            thisx->world.rot.y = -0x4000;
        }
        if (thisx->world.pos.x < -588.0f) {
            thisx->world.pos.x = ((thisx->world.pos.x > -888.0f) ? thisx->world.pos.x : -888.0f) + 20.0f;
            thisx->world.pos.y = 692.0f;
            thisx->world.pos.z = 2080.0f;
            thisx->world.rot.x = thisx->world.rot.z = 0;
            thisx->world.rot.y = 0x4000;
        }
    }
    MlEnBomChu_LetGo(thisx, play);
}

/* En_Dekubaba: ML skips the drop-flag test, so a Deku Baba always drops nuts, never a random drop (row 75) */

static ActorFunc sEnDekubabaUpdate;

static void MlEnDekubaba_Update(Actor* thisx, PlayState* play) {
    if (ML_IS_HOST_FN(((EnDekubaba*)thisx)->actionFunc, EnDekubaba_ShrinkDie)) {
        thisx->dropFlag = 0;
    }
    sEnDekubabaUpdate(thisx, play);
}

/*
 * En_Dekunuts: ML keeps room-0 scrubs within 30 units of home (its hook on Update's return, row 22): Deep Woods'
 * room-0 scrubs sit at the water's edge, and a Blade Beam would knock them into the water, where they never die.
 */

#define ML_DEKUNUTS_FLOWER 10

static ActorFunc sEnDekunutsUpdate;

static void MlEnDekunuts_Update(Actor* thisx, PlayState* play) {
    f32 dist;
    f32 scale;

    sEnDekunutsUpdate(thisx, play);
    if ((thisx->update == NULL) || (thisx->params == ML_DEKUNUTS_FLOWER) || (thisx->room != 0)) {
        return;
    }
    dist = Actor_WorldDistXZToPoint(thisx, &thisx->home.pos);
    if (dist > 30.0f) {
        scale = 30.0f / dist;
        thisx->world.pos.x = thisx->home.pos.x + (thisx->world.pos.x - thisx->home.pos.x) * scale;
        thisx->world.pos.z = thisx->home.pos.z + (thisx->world.pos.z - thisx->home.pos.z) * scale;
    }
}

/*
 * En_Dodojr: ML raises each puff of the Baby Dodongo's large dust to its own height, not dustPos.y (row 56). The
 * puffs only differ where dustPos.y is the floor: while it rises out of the ground and sinks back, so those two
 * action functions run ML's copies.
 */

static void MlEnDodojr_SpawnLargeDust(EnDodojr* this, PlayState* play, s32 count) {
    Color_RGBA8 prim = { 170, 130, 90, 255 };
    Color_RGBA8 env = { 100, 60, 20, 0 };
    Vec3f velocity = { 0.0f, 0.0f, 0.0f };
    Vec3f accel = { 0.0f, 0.3f, 0.0f };
    Vec3f pos;
    s16 angle = ((Rand_ZeroOne() - 0.5f) * 65536.0f);
    s32 i;

    pos.y = this->actor.world.pos.y;

    for (i = count; i >= 0; i--, angle += (s16)(0x10000 / count)) {
        accel.x = (Rand_ZeroOne() - 0.5f) * 4.0f;
        accel.z = (Rand_ZeroOne() - 0.5f) * 4.0f;

        pos.x = (Math_SinS(angle) * 22.0f) + this->dustPos.x;
        pos.z = (Math_CosS(angle) * 22.0f) + this->dustPos.z;

        func_8002836C(play, &pos, &velocity, &accel, &prim, &env, 120, 40, 10);
    }
}

// Vanilla's EnDodojr_EmergeFromGround (rising out of the ground)
static void MlEnDodojr_Rise(EnDodojr* this, PlayState* play) {
    f32 t;

    Math_SmoothStepToS(&this->actor.shape.rot.x, 0, 4, 0x3E8, 0x64);
    t = this->actor.shape.rot.x;
    t /= 16384.0f;
    this->actor.world.pos.y = this->actor.home.pos.y + (60.0f * t);
    MlEnDodojr_SpawnLargeDust(this, play, 3);

    if (t == 0.0f) {
        this->actor.shape.shadowDraw = ActorShadow_DrawCircle;
        this->actor.world.rot.x = this->actor.shape.rot.x;
        this->actor.speedXZ = 2.6f;
        ML_SET_HOST_FN(this->actionFunc, EnDodojr_CrawlTowardsTarget);
    }
}

// Vanilla's EnDodojr_Despawn (sinking back)
static void MlEnDodojr_Sink(EnDodojr* this, PlayState* play) {
    f32 t;

    Math_SmoothStepToS(&this->actor.shape.rot.x, 0x4000, 4, 1000, 100);

    if (DECR(this->timer3) != 0) {
        t = (30 - this->timer3) / 30.0f;
        this->actor.world.pos.y = this->actor.home.pos.y - (60.0f * t);
    } else {
        Actor_Kill(&this->actor);
    }

    MlEnDodojr_SpawnLargeDust(this, play, 3);
}

static ActorFunc sEnDodojrUpdate;

static void MlEnDodojr_Update(Actor* thisx, PlayState* play) {
    EnDodojr* this = (EnDodojr*)thisx;

    if (ML_IS_HOST_FN(this->actionFunc, EnDodojr_EmergeFromGround)) {
        this->actionFunc = MlEnDodojr_Rise;
    } else if (ML_IS_HOST_FN(this->actionFunc, EnDodojr_Despawn)) {
        this->actionFunc = MlEnDodojr_Sink;
    }
    sEnDodojrUpdate(thisx, play);
}

/*
 * En_Floormas: ML's giant Floormaster (row 73). ML recompiled En_Floormas with a branch for params 0x20, placed once in
 * Hollow Root Keep's room 20: 1.6 times the size, three times the collider, the mini-boss music, and no small partners
 * until it splits (vanilla's Init spawns them; here they are removed straight away). When it dies it splits into three
 * full-size Floormasters (scale 0.01, params 0): the two new ones spawn their own small partners in their Init, and
 * the giant gets a fresh pair. Vanilla calls its death from two action functions, which run ML's copies for the giant.
 */

#define ML_SPAWN_GIANT 0x20
#define SPAWN_SMALL 0x10

static ActorFunc sEnFloormasInit;
static ActorFunc sEnFloormasUpdate;

static void MlEnFloormas_Init(Actor* thisx, PlayState* play) {
    EnFloormas* this = (EnFloormas*)thisx;

    sEnFloormasInit(thisx, play);
    if ((thisx->params != ML_SPAWN_GIANT) || (thisx->update == NULL)) {
        return;
    }
    if (thisx->parent != NULL) {
        Actor_Kill(thisx->parent);
    }
    if (thisx->child != NULL) {
        Actor_Kill(thisx->child);
    }
    thisx->parent = thisx->child = NULL;
    Actor_SetScale(thisx, 0.016f);
    this->collider.dim.radius *= 3;
    this->collider.dim.height *= 3;
    func_800F5ACC(NA_BGM_MINI_BOSS);
}

// Like EnFloormas_SetupSplit, but the piece is a full-size Floormaster
static void MlEnFloormas_SetupGiantSplit(EnFloormas* this) {
    EnFloormas_SetupSplit(this);
    Actor_SetScale(&this->actor, 0.01f);
    this->actor.params = 0;
}

static Actor* MlEnFloormas_SpawnPiece(EnFloormas* this, PlayState* play, s16 params) {
    return Actor_Spawn(&play->actorCtx, play, ACTOR_EN_FLOORMAS, this->actor.world.pos.x, this->actor.world.pos.y,
                       this->actor.world.pos.z, 0, 0, 0, params);
}

static void MlEnFloormas_HidePiece(Actor* piece) {
    piece->flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
    piece->draw = NULL;
    ML_SET_HOST_FN(((EnFloormas*)piece)->actionFunc, EnFloormas_SmWait);
}

static void MlEnFloormas_GiantSplit(EnFloormas* this, PlayState* play) {
    Actor* piece;

    piece = MlEnFloormas_SpawnPiece(this, play, 0);
    this->actor.parent = piece;
    if (piece == NULL) {
        Actor_Kill(&this->actor);
        return;
    }
    MlEnFloormas_HidePiece(piece);
    piece = MlEnFloormas_SpawnPiece(this, play, 0);
    this->actor.child = piece;
    if (piece == NULL) {
        Actor_Kill(this->actor.parent);
        Actor_Kill(&this->actor);
        return;
    }
    MlEnFloormas_HidePiece(piece);

    this->actor.shape.rot.y = this->actor.yawTowardsPlayer + 0x8000;
    MlEnFloormas_SetupGiantSplit((EnFloormas*)this->actor.child);
    MlEnFloormas_SetupGiantSplit((EnFloormas*)this->actor.parent);
    MlEnFloormas_SetupGiantSplit(this);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_FLOORMASTER_SPLIT);

    // The giant's own pair of small partners, linked as vanilla's Init links them
    this->actor.parent = MlEnFloormas_SpawnPiece(this, play, SPAWN_SMALL);
    if (this->actor.parent == NULL) {
        Actor_Kill(&this->actor);
        return;
    }
    this->actor.child = MlEnFloormas_SpawnPiece(this, play, SPAWN_SMALL);
    if (this->actor.child == NULL) {
        Actor_Kill(this->actor.parent);
        Actor_Kill(&this->actor);
        return;
    }
    this->actor.parent->child = &this->actor;
    this->actor.parent->parent = this->actor.child;
    this->actor.child->parent = &this->actor;
    this->actor.child->child = this->actor.parent;
    EnFloormas_SetupBigDecideAction(this);
}

static void MlEnFloormas_Die(EnFloormas* this, PlayState* play) {
    if (this->actor.scale.x > 0.01f) {
        MlEnFloormas_GiantSplit(this, play);
    } else {
        EnFloormas_Die(this, play);
    }
}

// Vanilla's EnFloormas_TakeDamage, dying through ML's split
static void MlEnFloormas_TakeDamage(EnFloormas* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime) != 0) {
        if (this->actor.colChkInfo.health == 0) {
            MlEnFloormas_Die(this, play);
        } else {
            EnFloormas_SetupRecover(this);
        }
    }
    if (Animation_OnFrame(&this->skelAnime, 13.0f)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_DODO_M_GND);
    }
    Math_StepToF(&this->actor.speedXZ, 0.0f, 0.2f);
}

// Vanilla's EnFloormas_Freeze, dying through ML's split
static void MlEnFloormas_Freeze(EnFloormas* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->actionTimer != 0) {
        this->actionTimer--;
    }
    if (this->actionTimer == 0) {
        if (this->actor.colChkInfo.health == 0) {
            MlEnFloormas_Die(this, play);
            return;
        }
        EnFloormas_SetupStand(this);
    }
}

static void MlEnFloormas_Update(Actor* thisx, PlayState* play) {
    EnFloormas* this = (EnFloormas*)thisx;

    if (thisx->scale.x > 0.01f) {
        if (ML_IS_HOST_FN(this->actionFunc, EnFloormas_TakeDamage)) {
            this->actionFunc = MlEnFloormas_TakeDamage;
        } else if (ML_IS_HOST_FN(this->actionFunc, EnFloormas_Freeze)) {
            this->actionFunc = MlEnFloormas_Freeze;
        }
    }
    sEnFloormasUpdate(thisx, play);
}

/*
 * En_Nutsball: ML reflects nuts off the Hylian Shield only, at any age; the Deku Shield no longer does (code patches).
 * ML also shatters the nut while Link plays gameplay_keep's swim_15step_up animation (it checks after the hit test, so
 * a nut that just broke bursts twice, as in ML). Animation pointers are path strings here.
 */

static void MlEnNutsball_Fly(EnNutsball* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3s rot;
    Vec3f pos;

    this->timer--;

    if (this->timer == 0) {
        this->actor.gravity = -1.0f;
    }

    this->actor.home.rot.z += 0x2AA8;

    if ((this->actor.bgCheckFlags & 8) || (this->actor.bgCheckFlags & 1) || (this->collider.base.atFlags & AT_HIT) ||
        (this->collider.base.acFlags & AC_HIT) || (this->collider.base.ocFlags1 & OC1_HIT)) {
        if (player->currentShield == PLAYER_SHIELD_HYLIAN) {
            if ((this->collider.base.atFlags & AT_HIT) && (this->collider.base.atFlags & AT_TYPE_ENEMY) &&
                (this->collider.base.atFlags & AT_BOUNCED)) {
                this->collider.base.atFlags &= ~AT_TYPE_ENEMY & ~AT_BOUNCED & ~AT_HIT;
                this->collider.base.atFlags |= AT_TYPE_PLAYER;

                this->collider.info.toucher.dmgFlags = 2;
                Matrix_MtxFToYXZRotS(&player->shieldMf, &rot, 0);
                this->actor.world.rot.y = rot.y + 0x8000;
                this->timer = 30;
                return;
            }
        }

        pos.x = this->actor.world.pos.x;
        pos.y = this->actor.world.pos.y + 4;
        pos.z = this->actor.world.pos.z;

        EffectSsHahen_SpawnBurst(play, &pos, 6.0f, 0, 7, 3, 15, HAHEN_OBJECT_DEFAULT, 10, NULL);
        SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EN_OCTAROCK_ROCK);
        Actor_Kill(&this->actor);
    } else {
        if (this->timer == -300) {
            Actor_Kill(&this->actor);
        }
    }

    if ((player->skelAnime.animation != NULL) &&
        (strcmp((const char*)player->skelAnime.animation, gPlayerAnim_link_swimer_swim_15step_up) == 0)) {
        pos.x = this->actor.world.pos.x;
        pos.y = this->actor.world.pos.y + 4;
        pos.z = this->actor.world.pos.z;

        EffectSsHahen_SpawnBurst(play, &pos, 6.0f, 0, 7, 3, 15, HAHEN_OBJECT_DEFAULT, 10, NULL);
        SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EN_OCTAROCK_ROCK);
        Actor_Kill(&this->actor);
    }
}

static ActorFunc sEnNutsballUpdate;

static void MlEnNutsball_Update(Actor* thisx, PlayState* play) {
    EnNutsball* this = (EnNutsball*)thisx;

    if (ML_IS_HOST_FN(this->actionFunc, func_80ABBBA8)) {
        this->actionFunc = MlEnNutsball_Fly;
    }
    sEnNutsballUpdate(thisx, play);
}

/* En_Peehat: ML's Init ends by starting the mini-boss music, for every Peahat, larvae included (row 89) */

static ActorFunc sEnPeehatInit;

static void MlEnPeehat_Init(Actor* thisx, PlayState* play) {
    sEnPeehatInit(thisx, play);
    func_800F5ACC(NA_BGM_MINI_BOSS);
}

void MlVanillaEnemies_Register(const SOHModApi* api) {
    ActorDBEntry* entry;

    entry = ActorDB_Retrieve(ACTOR_EN_BOM_CHU);
    sEnBomChuUpdate = entry->update;
    entry->update = MlEnBomChu_Update;
    SOH_REGISTER_HOOK(api, ShouldActorUpdate, MlEnBomChu_ShouldActorUpdate);
    SOH_REGISTER_HOOK(api, OnGameFrameUpdate, MlEnBomChu_OnGameFrameUpdate);
    SOH_REGISTER_HOOK_FOR_ID(api, OnActorDestroy, ACTOR_EN_BOM_CHU, MlEnBomChu_OnActorDestroy);
    SOH_REGISTER_HOOK(api, OnPlayDestroy, MlEnBomChu_OnPlayDestroy);

    entry = ActorDB_Retrieve(ACTOR_EN_DEKUBABA);
    sEnDekubabaUpdate = entry->update;
    entry->update = MlEnDekubaba_Update;

    entry = ActorDB_Retrieve(ACTOR_EN_DEKUNUTS);
    sEnDekunutsUpdate = entry->update;
    entry->update = MlEnDekunuts_Update;

    entry = ActorDB_Retrieve(ACTOR_EN_DODOJR);
    sEnDodojrUpdate = entry->update;
    entry->update = MlEnDodojr_Update;

    entry = ActorDB_Retrieve(ACTOR_EN_FLOORMAS);
    sEnFloormasInit = entry->init;
    sEnFloormasUpdate = entry->update;
    entry->init = MlEnFloormas_Init;
    entry->update = MlEnFloormas_Update;

    entry = ActorDB_Retrieve(ACTOR_EN_NUTSBALL);
    sEnNutsballUpdate = entry->update;
    entry->update = MlEnNutsball_Update;

    entry = ActorDB_Retrieve(ACTOR_EN_PEEHAT);
    sEnPeehatInit = entry->init;
    entry->init = MlEnPeehat_Init;

    api->Log("[MissingLink] ML's edits to En_Bom_Chu, En_Dekubaba, En_Dekunuts, En_Dodojr, En_Floormas, En_Nutsball "
             "and En_Peehat are on");
}
