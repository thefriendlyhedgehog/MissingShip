/*
 * The Missing Link's hole-hopping Deku Scrub (ledger row 33), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_HoleScrub),
 * registered through the mod API.
 *
 * ML dma 298: vanilla En_Hintnuts rebuilt with changes, placed in its Deku Tree-slot scene (room 2) on one of three
 * Deku flowers (En_Dekunuts params 10) beside two push blocks. It never throws nuts or walks: it faces you, turns
 * away and talks when you come within 30 units (text = params & 0x7FFF; +2 with bit 15 once switch 5 is set;
 * 0x1023 once room 0 is cleared), and when hit it burrows and pops up on a random flower that no push block
 * (Obj_Oshihiki) stands within 30 units of. With fewer than two open flowers it is caught: switch 15 is set and it is
 * gone for good. While loaded it also sinks every other enemy-category actor to y -960, hiding the Deku scrub bodies
 * so only their flowers remain.
 *
 * Vanilla paths ML's changes make unreachable (nut throwing, the enemy-only freeze puzzle) are left out.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/object_hintnuts/object_hintnuts.h"
#include "soh/ResourceManagerHelpers.h"

#include <math.h>
#include <stdio.h>

#include "missinglink.h"

struct MlHoleScrub;

typedef void (*MlHoleScrubActionFunc)(struct MlHoleScrub*, PlayState*);

typedef struct MlHoleScrub {
    Actor actor;
    SkelAnime skelAnime;
    MlHoleScrubActionFunc actionFunc;
    s16 animFlagAndTimer;
    s16 runYaw;
    u16 textIdCopy;
    Vec3s jointTable[10];
    Vec3s morphTable[10];
    ColliderCylinder collider;
} MlHoleScrub;

#define ML_HOLESCRUB_CAUGHT_SWITCH 15

static void MlHoleScrub_Wait(MlHoleScrub* this, PlayState* play);
static void MlHoleScrub_LookAround(MlHoleScrub* this, PlayState* play);
static void MlHoleScrub_Stand(MlHoleScrub* this, PlayState* play);
static void MlHoleScrub_Burrow(MlHoleScrub* this, PlayState* play);
static void MlHoleScrub_BeginRun(MlHoleScrub* this, PlayState* play);
static void MlHoleScrub_Run(MlHoleScrub* this, PlayState* play);
static void MlHoleScrub_Talk(MlHoleScrub* this, PlayState* play);
static void MlHoleScrub_Leave(MlHoleScrub* this, PlayState* play);

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_HIT6,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_1,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK0,
        { 0x00000000, 0x00, 0x00 },
        { 0xFFCFFFFF, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON,
        OCELEM_ON,
    },
    { 18, 32, 0, { 0, 0, 0 } },
};

static CollisionCheckInfoInit sColChkInfoInit = { 1, 18, 32, MASS_HEAVY };

static InitChainEntry sInitChain[] = {
    ICHAIN_F32(gravity, -1, ICHAIN_CONTINUE),
    ICHAIN_S8(naviEnemyId, 0x0A, ICHAIN_CONTINUE),
    ICHAIN_F32(targetArrowOffset, 2600, ICHAIN_STOP),
};

static void MlHoleScrub_SetupWait(MlHoleScrub* this) {
    Animation_PlayOnceSetSpeed(&this->skelAnime, (AnimationHeader*)gHintNutsUpAnim, 0.0f);
    this->animFlagAndTimer = Rand_S16Offset(100, 50);
    this->collider.dim.height = 5;
    this->actor.world.pos.y = this->actor.home.pos.y; // ML keeps x/z: the scrub has moved to another flower
    this->collider.base.acFlags &= ~AC_ON;
    this->actionFunc = MlHoleScrub_Wait;
}

static void MlHoleScrub_Init(Actor* thisx, PlayState* play) {
    MlHoleScrub* this = (MlHoleScrub*)thisx;

    Actor_ProcessInitChain(&this->actor, sInitChain);
    if (Flags_GetSwitch(play, ML_HOLESCRUB_CAUGHT_SWITCH)) {
        Actor_Kill(&this->actor);
        return;
    }
    ActorShape_Init(&this->actor.shape, 0x0, ActorShadow_DrawCircle, 35.0f);
    SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)gHintNutsSkel, (AnimationHeader*)gHintNutsStandAnim, this->jointTable, this->morphTable, 10);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    CollisionCheck_SetInfo(&this->actor.colChkInfo, NULL, &sColChkInfoInit);
    this->textIdCopy = this->actor.textId;
    MlHoleScrub_SetupWait(this);
}

static void MlHoleScrub_Destroy(Actor* thisx, PlayState* play) {
    MlHoleScrub* this = (MlHoleScrub*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
    ResourceMgr_UnregisterSkeleton(&this->skelAnime);
}

static void MlHoleScrub_SetupStand(MlHoleScrub* this) {
    Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)gHintNutsStandAnim, -3.0f);
    this->animFlagAndTimer = 1;
    this->actionFunc = MlHoleScrub_Stand;
}

static void MlHoleScrub_SetupBurrow(MlHoleScrub* this) {
    Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)gHintNutsBurrowAnim, -5.0f);
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_NUTS_DOWN);
    this->actionFunc = MlHoleScrub_Burrow;
}

static void MlHoleScrub_SetupRun(MlHoleScrub* this) {
    this->animFlagAndTimer = 5; // ML drops the run animation and never moves the scrub
    this->actionFunc = MlHoleScrub_Run;
}

static void MlHoleScrub_SetupLeave(MlHoleScrub* this) {
    // ML keeps only the bookkeeping: no run animation, no speed, no recovery heart.
    this->collider.base.ocFlags1 &= ~OC1_ON;
    this->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    this->animFlagAndTimer = 100;
    this->actor.world.rot.y = this->actor.shape.rot.y;
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_NUTS_DAMAGE);
    this->actionFunc = MlHoleScrub_Leave;
}

static void MlHoleScrub_Wait(MlHoleScrub* this, PlayState* play) {
    s32 hasSlowPlaybackSpeed = this->skelAnime.playSpeed < 0.5f;

    if (hasSlowPlaybackSpeed && (this->animFlagAndTimer != 0)) {
        this->animFlagAndTimer--;
    }
    if (Animation_OnFrame(&this->skelAnime, 9.0f)) {
        this->collider.base.acFlags |= AC_ON;
    } else if (Animation_OnFrame(&this->skelAnime, 8.0f)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_NUTS_UP);
    }
    this->collider.dim.height = 5.0f + ((CLAMP(this->skelAnime.curFrame, 9.0f, 12.0f) - 9.0f) * 9.0f);
    // ML removed both "player within 120 -> burrow" reactions: a nearby player just keeps it up.
    if (SkelAnime_Update(&this->skelAnime) && !(this->actor.xzDistToPlayer < 120.0f)) {
        if ((this->animFlagAndTimer == 0) && (this->actor.xzDistToPlayer > 320.0f)) {
            Animation_PlayLoop(&this->skelAnime, (AnimationHeader*)gHintNutsLookAroundAnim);
            this->animFlagAndTimer = 2;
            this->actionFunc = MlHoleScrub_LookAround;
        } else {
            MlHoleScrub_SetupStand(this);
        }
    }
    if (hasSlowPlaybackSpeed && 160.0f < this->actor.xzDistToPlayer && fabsf(this->actor.yDistToPlayer) < 120.0f &&
        ((this->animFlagAndTimer == 0) || (this->actor.xzDistToPlayer < 480.0f))) {
        this->skelAnime.playSpeed = 1.0f;
    }
}

static void MlHoleScrub_LookAround(MlHoleScrub* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (Animation_OnFrame(&this->skelAnime, 0.0f) && this->animFlagAndTimer != 0) {
        this->animFlagAndTimer--;
    }
    if ((this->actor.xzDistToPlayer < 120.0f) || (this->animFlagAndTimer == 0)) {
        this->actionFunc = MlHoleScrub_Stand; // ML: straight to standing, no burrow
    }
}

static void MlHoleScrub_Stand(MlHoleScrub* this, PlayState* play) {
    this->collider.base.ocFlags1 |= OC1_ON; // ML: restores the push-out SetupLeave cleared
    SkelAnime_Update(&this->skelAnime);
    if (Animation_OnFrame(&this->skelAnime, 0.0f) && this->animFlagAndTimer != 0) {
        this->animFlagAndTimer--;
    }
    if (!(this->animFlagAndTimer & 0x1000)) {
        Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 2, 0xE38);
    }
    if (this->actor.xzDistToPlayer < 30.0f) {
        MlHoleScrub_SetupRun(this);
    }
}

// ML's addition at Burrow frame 4: hop to a random open flower, or be caught.
static void MlHoleScrub_Hop(MlHoleScrub* this, PlayState* play) {
    Actor* holes[4] = { NULL, NULL, NULL, NULL };
    s32 count = 0;
    Actor* flower;
    Actor* prop;
    s32 tries;

    for (flower = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; flower != NULL; flower = flower->next) {
        s32 blocked = false;

        if (flower->id != ACTOR_EN_DEKUNUTS || flower->params != 10) {
            continue;
        }
        for (prop = play->actorCtx.actorLists[ACTORCAT_PROP].head; prop != NULL; prop = prop->next) {
            if (prop->id == ACTOR_OBJ_OSHIHIKI && Actor_WorldDistXZToActor(prop, flower) < 30.0f) {
                blocked = true;
                break;
            }
        }
        if (!blocked && count < ARRAY_COUNT(holes)) {
            holes[count++] = flower;
        }
    }

    if (holes[1] == NULL) {
        Flags_SetSwitch(play, ML_HOLESCRUB_CAUGHT_SWITCH);
        Audio_PlayActorSound2(&this->actor, NA_SE_EN_NUTS_DAMAGE);
        Actor_Kill(&this->actor);
        return;
    }
    // ML draws among the first three and retries without a bound; cap it here.
    for (tries = 0; tries < 64; tries++) {
        Actor* hole = holes[Rand_S16Offset(0, 3)];

        if (hole != NULL && !(Actor_WorldDistXZToActor(hole, &this->actor) < 30.0f)) {
            this->actor.world.pos.x = hole->world.pos.x;
            this->actor.world.pos.z = hole->world.pos.z;
            return;
        }
    }
}

static void MlHoleScrub_Burrow(MlHoleScrub* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        MlHoleScrub_SetupWait(this);
    } else {
        this->collider.dim.height = 5.0f + ((3.0f - CLAMP(this->skelAnime.curFrame, 1.0f, 3.0f)) * 12.0f);
    }
    if (Animation_OnFrame(&this->skelAnime, 4.0f)) {
        this->collider.base.acFlags &= ~AC_ON;
        MlHoleScrub_Hop(this, play);
    }
}

static void MlHoleScrub_BeginRun(MlHoleScrub* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        this->runYaw = this->actor.yawTowardsPlayer + 0x8000;
        MlHoleScrub_SetupRun(this);
    }
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 2, 0xE38);
}

static void MlHoleScrub_CheckProximity(MlHoleScrub* this, PlayState* play) {
    u16 textId;

    if ((this->collider.base.ocFlags1 & OC1_HIT) || this->actor.isTargeted) {
        this->actor.flags |= ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
    } else {
        this->actor.flags &= ~ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED;
    }
    if (this->actor.xzDistToPlayer < 130.0f) {
        textId = this->actor.params & 0x7FFF;
        if (this->actor.params & 0x8000) {
            if (Flags_GetSwitch(play, 5)) {
                textId += 2;
            }
        } else if (Flags_GetClear(play, 0)) {
            textId = 0x1023;
        }
        this->actor.textId = textId;
        Actor_OfferTalkNearColChkInfoCylinder(&this->actor, play);
    }
}

static void MlHoleScrub_Run(MlHoleScrub* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (this->animFlagAndTimer != 0) { // ML counts every frame (its Animation_OnFrame test is gone)
        this->animFlagAndTimer--;
    }
    Math_StepToF(&this->actor.speedXZ, 7.5f, 1.0f);
    if (Math_SmoothStepToS(&this->actor.world.rot.y, this->runYaw, 1, 0xE38, 0xB6) == 0) {
        if (this->actor.bgCheckFlags & 0x20) {
            this->runYaw = Actor_WorldYawTowardPoint(&this->actor, &this->actor.home.pos);
        } else if (this->actor.bgCheckFlags & 8) {
            this->runYaw = this->actor.wallYaw;
        } else if (this->animFlagAndTimer == 0) {
            s16 toHome = Actor_WorldYawTowardPoint(&this->actor, &this->actor.home.pos);
            s16 diff = toHome - this->actor.yawTowardsPlayer;

            if (ABS(diff) >= 0x2001) {
                this->runYaw = toHome;
            } else {
                this->runYaw = (s16)((((0.0f <= (f32)diff) ? 1.0f : -1.0f) * -8192.0f) +
                                     (f32)this->actor.yawTowardsPlayer);
            }
        } else {
            this->runYaw = (s16)(this->actor.yawTowardsPlayer + 0x8000);
        }
    }
    this->actor.shape.rot.y = this->actor.world.rot.y + 0x8000;
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)gHintNutsTalkAnim, -5.0f);
        this->actor.speedXZ = 0.0f;
        this->actionFunc = MlHoleScrub_Talk;
    } else {
        // ML compares the distance home with -20, so the vanilla "back home, burrow" branch never runs.
        MlHoleScrub_CheckProximity(this, play);
    }
}

static void MlHoleScrub_Talk(MlHoleScrub* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.yawTowardsPlayer, 0x3, 0x400, 0x100);
    if (Message_GetState(&play->msgCtx) == TEXT_STATE_NONE) { // ML: once the text has closed
        MlHoleScrub_SetupLeave(this);
    }
}

static void MlHoleScrub_Leave(MlHoleScrub* this, PlayState* play) {
    s16 yaw;

    SkelAnime_Update(&this->skelAnime);
    if (this->animFlagAndTimer != 0) {
        this->animFlagAndTimer--;
    }
    if (this->actor.bgCheckFlags & 8) {
        yaw = this->actor.wallYaw;
    } else {
        yaw = this->actor.yawTowardsPlayer - Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) - 0x8000;
        if (ABS(yaw) >= 0x4001) {
            yaw = Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) + 0x8000;
        } else {
            yaw = Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) - (yaw >> 1) + 0x8000;
        }
    }
    Math_ScaledStepToS(&this->actor.shape.rot.y, yaw, 0x800);
    this->actor.world.rot.y = this->actor.shape.rot.y;
    // ML: back to standing at once instead of running off and despawning.
    this->actionFunc = MlHoleScrub_Stand;
    Message_CloseTextbox(play);
    if (this->actor.child != NULL) {
        Actor_ChangeCategory(play, &play->actorCtx, this->actor.child, ACTORCAT_PROP);
    }
}

static void MlHoleScrub_ColliderCheck(MlHoleScrub* this, PlayState* play) {
    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        Actor_SetDropFlag(&this->actor, &this->collider.info, 1);
        if (this->collider.base.ac->id != ACTOR_EN_NUTSBALL) {
            MlHoleScrub_SetupBurrow(this);
            return;
        }
    } else if (play->actorCtx.unk_02 == 0) {
        return;
    }
    // Hit by a Deku nut or an explosion: vanilla's HitByScrubProjectile, minus the enemy-only puzzle.
    Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)gHintNutsUnburrowAnim, -3.0f);
    this->collider.dim.height = 37;
    Audio_PlayActorSound2(&this->actor, NA_SE_EN_NUTS_DAMAGE);
    this->collider.base.acFlags &= ~AC_ON;
    this->actionFunc = MlHoleScrub_BeginRun;
}

static void MlHoleScrub_Update(Actor* thisx, PlayState* play) {
    MlHoleScrub* this = (MlHoleScrub*)thisx;
    Actor* enemy;

    MlHoleScrub_ColliderCheck(this, play);
    this->actionFunc(this, play);
    // ML drops Actor_MoveXZGravity: the scrub only ever teleports.
    Actor_UpdateBgCheckInfo(play, &this->actor, 20.0f, this->collider.dim.radius, this->collider.dim.height, 0x1D);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base); // ML: unconditionally
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    if (this->actionFunc == MlHoleScrub_Wait) {
        Actor_SetFocus(&this->actor, this->skelAnime.curFrame);
    } else if (this->actionFunc == MlHoleScrub_Burrow) {
        Actor_SetFocus(&this->actor,
                       20.0f - ((this->skelAnime.curFrame * 20.0f) / Animation_GetLastFrame((AnimationHeader*)gHintNutsBurrowAnim)));
    } else {
        Actor_SetFocus(&this->actor, 20.0f);
    }
    for (enemy = play->actorCtx.actorLists[ACTORCAT_ENEMY].head; enemy != NULL; enemy = enemy->next) {
        if (enemy->params != 10) {
            enemy->world.pos.y = -960.0f;
        }
    }
}

static void MlHoleScrub_Draw(Actor* thisx, PlayState* play) {
    MlHoleScrub* this = (MlHoleScrub*)thisx;

    SkelAnime_DrawSkeletonOpa(play, &this->skelAnime, NULL, NULL, this);
}

void MlHoleScrub_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_HoleScrub";
    definition.description = "The Missing Link: hole-hopping scrub";
    definition.category = ACTORCAT_NPC;
    definition.actorFlags = ACTOR_FLAG_HOSTILE;
    definition.objectId = OBJECT_HINTNUTS;
    definition.instanceSize = sizeof(MlHoleScrub);
    definition.init = MlHoleScrub_Init;
    definition.destroy = MlHoleScrub_Destroy;
    definition.update = MlHoleScrub_Update;
    definition.draw = MlHoleScrub_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_HoleScrub is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
