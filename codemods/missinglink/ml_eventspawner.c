/*
 * The Missing Link's event spawner and young Epona (ledger rows 26 and 54), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_EventSpawner), registered through the mod API.
 *
 * Scripted-event spawner and young Epona (ML's rewrite of En_Fz, slot 0x121)
 *
 * ML turned the Freezard slot (ML dma 259, text 0xBA0) into an event helper on object_horse_link_child. Its params
 * pick a mode (ML function offsets in brackets):
 *   0, 8+  young Epona standing still, blinking (spot20)                                            [+0xB10]
 *   1      only when Link arrives by ML entrance 0x138 or 0x058: the horse chase (spot09). A short cutscene, then a
 *          40-second timer while young Epona gallops along path 0; once she stops, the timer running out sends
 *          Link to ML entrance 0x13C                                                                  [+0x424]
 *   2, 4, 5, 6  only on the same two entrances: spawn Bg_Umajump / Ml_Malon / a Deku Baba / a Tektite in place,
 *          then leave                                                                                 [+0x958..]
 *   3      young Epona walking forward with Link riding her                                           [+0x88C]
 *          ML_Fishpond spawns it on ML entrance 0x420 (ML's layer's ml/fishing_pond_event), after the pond intro.
 *   7      stop the running timer                                                                     [+0xAE4]
 * ML's entrance numbers are the vanilla ones in the slots ML builds in: ML's Deku Road is vanilla's Gerudo Valley
 * (spot09) and its Cluck Cluck Ranch vanilla's Lon Lon Ranch (spot20), so 0x138 is ENTR_GERUDO_VALLEY_OUTSIDE_TENT,
 * 0x058 ENTR_LON_LON_RANCH_ENTRANCE and 0x13C ENTR_LON_LON_RANCH_OUTSIDE_TALONS_HOUSE (MissingShip's
 * ml_entrance_map.c). Ported from MissingShip's SoH fork (ovl_Ml_EventSpawner, ledger rows 26 and 54). Placed by
 * name where ML's rooms place 0x121; vanilla's Freezard stays vanilla elsewhere.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "objects/object_horse_link_child/object_horse_link_child.h"
#include "soh/ActorDB.h"

#include <stdio.h>

#include "missinglink.h"

struct MlEventSpawner;

typedef void (*MlEventSpawnerActionFunc)(struct MlEventSpawner*, PlayState*);

// ML's layout (size 0x240), for reference against its overlay; SoH's own offsets differ.
typedef struct MlEventSpawner {
    /* 0x000 */ Actor actor;
    /* 0x14C */ Skin skin;                  // young Epona (object_horse_link_child)
    /* 0x1DC */ ColliderCylinder collider;
    /* 0x228 */ MlEventSpawnerActionFunc actionFunc;
    /* 0x22C */ s16 blinkTimer;
    /* 0x230 */ void* eyeTex;
    /* 0x234 */ s16 colliderInit;
    /* 0x236 */ s16 timer;
    /* 0x238 */ Path* path;
    /* 0x23C */ s16 waypoint;
    /* 0x23E */ s16 dustTimer;
} MlEventSpawner;

static void MlEventSpawner_Stand(MlEventSpawner* this, PlayState* play);
static void MlEventSpawner_ChaseIntro(MlEventSpawner* this, PlayState* play);
static void MlEventSpawner_Gallop(MlEventSpawner* this, PlayState* play);
static void MlEventSpawner_WaitForTimer(MlEventSpawner* this, PlayState* play);
static void MlEventSpawner_Ride(MlEventSpawner* this, PlayState* play);

// ML's init bytes are laid out for Collider_SetCylinderType1 ({ 0x0A, 0x11, 0x09, 0x20, 0x01 }) but go to
// Collider_SetCylinder, which reads a sixth byte: OC is off and the shape reads as a JntSph whose element count is
// the toucher's damage flags (negative), so the collider never touches anything. Its dimensions still feed
// Actor_UpdateBgCheckInfo, so it is kept for those and never registered with AT or OC.
static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_ENEMY, AC_ON | AC_TYPE_PLAYER, OC1_TYPE_2, OC2_HIT_PLAYER, COLSHAPE_JNTSPH },
    { ELEMTYPE_UNK0, { 0xFFCFFFFF, 0x04, 0x05 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_ON, BUMP_ON, OCELEM_ON },
    { 32, 128, 0, { 0, 0, 0 } },
};

static void* sEyeTextures[] = { (void*)gChildEponaEyeOpenTex, (void*)gChildEponaEyeHalfTex, (void*)gChildEponaEyeCloseTex };

// ML keeps a flag in the overlay's data that lets the chase start once per overlay load; the overlay unloads when
// its last instance goes. SoH's data lives for the session, so count instances and reset the flag with the last one.
static s32 sChaseAllowed = true;
static s32 sInstances = 0;

static void MlEventSpawner_SpawnHere(Actor* thisx, PlayState* play, s16 actorId, s16 params) {
    if (actorId < 0) {
        return;
    }
    Actor_Spawn(&play->actorCtx, play, actorId, thisx->world.pos.x, thisx->world.pos.y, thisx->world.pos.z,
                thisx->shape.rot.x, thisx->shape.rot.y, thisx->shape.rot.z, params);
}

static void MlEventSpawner_InitSkin(MlEventSpawner* this, PlayState* play) {
    Skin_Init(play, &this->skin, (SkeletonHeader*)gChildEponaSkel, (AnimationHeader*)gChildEponaIdleAnim);
    Animation_PlayOnceSetSpeed(&this->skin.skelAnime, (AnimationHeader*)gChildEponaIdleAnim, 0.0f);
    Animation_MorphToLoop(&this->skin.skelAnime, (AnimationHeader*)gChildEponaIdleAnim, -5.0f);
    // An actor is drawn once before its first Update, so the eyes need a texture now; a NULL segment 8 crashes
    // the renderer (MS-144).
    this->eyeTex = sEyeTextures[0];
}

static void MlEventSpawner_StartChase(MlEventSpawner* this, PlayState* play) {
    gSaveContext.entranceIndex = ENTR_LON_LON_RANCH_ENTRANCE; // ML 0x058
    if (!sChaseAllowed) {
        Actor_Kill(&this->actor);
        return;
    }
    MlEventSpawner_InitSkin(this, play);
    Audio_QueueSeqCmd(NA_BGM_HORSE);
    func_80064520(play, &play->csCtx); // start a manual cutscene
    sChaseAllowed = false;
    this->path = Path_GetByIndex(play, 0, 0xFF);
    this->actor.room = -1;
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    this->waypoint = 0;
    this->actor.gravity = -1.0f;
    this->actionFunc = MlEventSpawner_ChaseIntro;
}

static void MlEventSpawner_Init(Actor* thisx, PlayState* play) {
    MlEventSpawner* this = (MlEventSpawner*)thisx;
    s32 eventEntrance = (gSaveContext.entranceIndex == ENTR_GERUDO_VALLEY_OUTSIDE_TENT) || // ML 0x138
                        (gSaveContext.entranceIndex == ENTR_LON_LON_RANCH_ENTRANCE);      // ML 0x058

    sInstances++;
    switch ((u16)thisx->params) {
        case 1:
            if (!eventEntrance) {
                Actor_Kill(thisx);
                return;
            }
            MlEventSpawner_StartChase(this, play);
            break;
        case 2:
        case 4:
        case 5:
        case 6:
            if (eventEntrance) {
                // ML's En_Ma1 slot is its own young Malon, ML_Malon, spawned by name: until it is registered, the
                // spawn is skipped with an error. The other three are vanilla's.
                s16 malon = ActorDB_RetrieveId("ML_Malon");
                const s16 sSpawns[][2] = {
                    { ACTOR_BG_UMAJUMP, 0 }, { 0, 0 }, { malon, -1 }, { ACTOR_EN_DEKUBABA, 1 }, { ACTOR_EN_TITE, -1 },
                };

                if (thisx->params == 4 && malon < 0) {
                    LUSLOG_ERROR("[MissingLink] ML_EventSpawner: %s is not registered; its spawn is skipped", "ML_Malon");
                }
                MlEventSpawner_SpawnHere(thisx, play, sSpawns[thisx->params - 2][0], sSpawns[thisx->params - 2][1]);
            }
            Actor_Kill(thisx);
            return;
        case 3:
            MlEventSpawner_InitSkin(this, play);
            Animation_MorphToLoop(&this->skin.skelAnime, (AnimationHeader*)gChildEponaGallopingAnim, -5.0f);
            this->skin.skelAnime.playSpeed = 1.4f;
            this->actionFunc = MlEventSpawner_Ride;
            break;
        case 7:
            gSaveContext.timerState = TIMER_STATE_STOP;
            Actor_Kill(thisx);
            return;
        default: // 0 and anything past 7
            MlEventSpawner_InitSkin(this, play);
            this->actionFunc = MlEventSpawner_Stand;
            break;
    }
    Actor_SetScale(thisx, 0.0065f);
    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawHorse, 20.0f);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sCylinderInit);
    this->colliderInit = true;
}

static void MlEventSpawner_Destroy(Actor* thisx, PlayState* play) {
    MlEventSpawner* this = (MlEventSpawner*)thisx;

    if (this->colliderInit) {
        Collider_DestroyCylinder(play, &this->collider);
    }
    if (--sInstances == 0) {
        sChaseAllowed = true;
    }
}

static void MlEventSpawner_Stand(MlEventSpawner* this, PlayState* play) {
    Collider_UpdateCylinder(&this->actor, &this->collider);
}

// Once she has stopped, the chase is lost when the timer runs out.
static void MlEventSpawner_WaitForTimer(MlEventSpawner* this, PlayState* play) {
    Collider_UpdateCylinder(&this->actor, &this->collider);
    if (gSaveContext.timerSeconds == 0) {
        gSaveContext.timerState = TIMER_STATE_STOP;
        play->nextEntranceIndex = ENTR_LON_LON_RANCH_OUTSIDE_TALONS_HOUSE; // ML 0x13C
        play->transitionTrigger = TRANS_TRIGGER_START;
    }
}

// Mode 3: walk forward carrying Link. ML plays sfx 0x2004, the EV-bank horse run without the 0x800 flag.
static void MlEventSpawner_Ride(MlEventSpawner* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    Player_PlaySfx(&this->actor, 0x2004);
    this->actor.speedXZ = 4.5f;
    Actor_MoveXZGravity(&this->actor);
    player->actor.world.pos.x = this->actor.world.pos.x;
    player->actor.world.pos.z = this->actor.world.pos.z;
    player->actor.world.pos.y = this->actor.world.pos.y + 5.0f;
    player->actor.gravity = 0.0f;
    player->actor.shape.rot.y = this->actor.shape.rot.y;
}

static void MlEventSpawner_StartGallop(MlEventSpawner* this, PlayState* play) {
    func_80064534(play, &play->csCtx); // end the manual cutscene
    Interface_SetTimer(40);
    Animation_MorphToLoop(&this->skin.skelAnime, (AnimationHeader*)gChildEponaGallopingAnim, -5.0f);
    Sfx_PlaySfxCentered(NA_SE_SY_START_SHOT);
    this->timer = 0;
    this->actionFunc = MlEventSpawner_Gallop;
}

static void MlEventSpawner_ChaseIntro(MlEventSpawner* this, PlayState* play) {
    if (this->timer > 40) {
        MlEventSpawner_StartGallop(this, play);
    } else {
        Camera_ChangeDataIdx(&play->mainCamera, 4);
        this->timer++;
    }
    if (this->timer == 25) {
        Player_PlaySfx(&this->actor, NA_SE_EV_KID_HORSE_NEIGH);
    }
}

static void MlEventSpawner_StopGallop(MlEventSpawner* this, PlayState* play) {
    Animation_MorphToLoop(&this->skin.skelAnime, (AnimationHeader*)gChildEponaIdleAnim, -5.0f);
    this->actor.speedXZ = 0.0f;
    this->skin.skelAnime.playSpeed = 1.0f;
    Player_PlaySfx(&this->actor, NA_SE_EV_HORSE_SLIP);
    this->actionFunc = MlEventSpawner_WaitForTimer;
}

static s32 MlEventSpawner_FollowPath(MlEventSpawner* this) {
    s16 yaw;
    f32 distSq = Path_OrientAndGetDistSq(&this->actor, this->path, this->waypoint, &yaw);

    Math_SmoothStepToS(&this->actor.world.rot.y, yaw, 10, 1000, 1);
    if ((distSq > 0.0f) && (distSq < 1000.0f)) {
        this->waypoint++;
        return true;
    }
    return false;
}

static void MlEventSpawner_Gallop(MlEventSpawner* this, PlayState* play) {
    f32 dustScale = Rand_ZeroFloat(13.0f);
    f32 dustRadius = Rand_ZeroFloat(10.0f);
    // ML passes an uninitialised stack halfword as the dust count, next to a Rand_S16Offset(.., 9) call whose result
    // it discards; the random 0-8 that call looks meant to produce stands in for it.
    s16 dustCount = Rand_S16Offset(0, 9);
    f32* playSpeed = &this->skin.skelAnime.playSpeed;
    s16 waypoint = this->waypoint;

    Collider_UpdateCylinder(&this->actor, &this->collider);

    // her gait by stretch of the path
    if (waypoint < 3) {
        Math_StepToF(playSpeed, 1.8f, 0.1f);
    } else if ((waypoint < 6) || ((waypoint >= 15) && (waypoint < 19))) {
        Math_StepToF(playSpeed, 1.3f, 0.2f);
    } else if (waypoint < 9) {
        Math_StepToF(playSpeed, 1.8f, 0.2f);
    } else if (waypoint < 11) {
        Math_StepToF(playSpeed, 1.6f, 0.1f);
    } else if (waypoint < 15) {
        Math_StepToF(playSpeed, 1.65f, 0.1f);
    } else {
        Math_StepToF(playSpeed, 1.5f, 0.1f);
    }

    MlEventSpawner_FollowPath(this);
    if (this->waypoint >= this->path->count) {
        MlEventSpawner_StopGallop(this, play);
    } else {
        this->actor.speedXZ = *playSpeed * 5.75f;
        this->actor.shape.rot.y = this->actor.world.rot.y;
        Actor_MoveXZGravity(&this->actor);
    }

    if (this->dustTimer == 4) {
        func_80033480(play, &this->actor.world.pos, dustRadius + 40.0f, dustCount,
                      (s16)(dustScale + 50.0f), (s16)(dustScale + 50.0f), 1);
        this->dustTimer = 0;
    } else {
        this->dustTimer++;
    }
    Player_PlaySfx(&this->actor, 0x2004);
}

static void MlEventSpawner_Update(Actor* thisx, PlayState* play) {
    MlEventSpawner* this = (MlEventSpawner*)thisx;

    this->actionFunc(this, play);
    Actor_UpdateBgCheckInfo(play, thisx, 20.0f, this->collider.dim.radius, this->collider.dim.height,
                            5); // floor and walls, as ML passes
    SkelAnime_Update(&this->skin.skelAnime);

    if (this->blinkTimer == 0) {
        this->blinkTimer = Rand_S16Offset(30, 30);
    }
    this->blinkTimer--;
    this->eyeTex = sEyeTextures[(this->blinkTimer < 2) ? 2 - this->blinkTimer : 0];
}

static s32 MlEventSpawner_OverrideLimbDraw(Actor* thisx, PlayState* play, s32 limbIndex, Skin* skin) {
    MlEventSpawner* this = (MlEventSpawner*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(this->eyeTex));
    gSPSegment(POLY_XLU_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(this->eyeTex));
    CLOSE_DISPS(play->state.gfxCtx);
    return true;
}

static void MlEventSpawner_Draw(Actor* thisx, PlayState* play) {
    MlEventSpawner* this = (MlEventSpawner*)thisx;

    func_800A6360(thisx, play, &this->skin, NULL, MlEventSpawner_OverrideLimbDraw, true);
}

void MlEventSpawner_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_EventSpawner";
    definition.description = "The Missing Link: event spawner and young Epona";
    definition.category = ACTORCAT_BG;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = OBJECT_HORSE_LINK_CHILD;
    definition.instanceSize = sizeof(MlEventSpawner);
    definition.init = MlEventSpawner_Init;
    definition.destroy = MlEventSpawner_Destroy;
    definition.update = MlEventSpawner_Update;
    definition.draw = MlEventSpawner_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_EventSpawner is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
