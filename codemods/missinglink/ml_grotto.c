/*
 * The Missing Link's grotto hole (ledger row 25), as a code mod.
 *
 * MissingShip's Unbound branch makes it a type of Door_Ana, `ml/grotto`, that draws ML's hole from its own gameplay_keep
 * so it shows in any scene, and reads ML's destinations from unbound/grottos.json. Stock Unbound reads neither, so this
 * is Door_Ana's code with both: ML's model by path (objects/object_ml_gkeep in ML's layer), and ML's copy of the
 * destination table (MissingShip's fork, z_door_ana.c sGrottoEntrances). ML's rooms place it by name (the builder
 * renames ML's Door_Ana placements to `ml/grotto`), so vanilla's grottos are unchanged.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "align_asset_macro.h"
#include "overlays/actors/ovl_Door_Ana/z_door_ana.h"

#include <stdio.h>

#include "missinglink.h"

#define dgMlGrottoDL "__OTR__objects/object_ml_gkeep/gMlGrottoDL"
static const ALIGN_ASSET(2) char gMlGrottoDL[] = dgMlGrottoDL;

static void MlGrotto_WaitClosed(DoorAna* this, PlayState* play);
static void MlGrotto_WaitOpen(DoorAna* this, PlayState* play);
static void MlGrotto_GrabPlayer(DoorAna* this, PlayState* play);

static ColliderCylinderInit sCylinderInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_NONE,
        OC2_NONE,
        COLSHAPE_CYLINDER,
    },
    {
        ELEMTYPE_UNK2,
        { 0x00000000, 0x00, 0x00 },
        { 0x00000048, 0x00, 0x00 },
        TOUCH_NONE,
        BUMP_ON,
        OCELEM_NONE,
    },
    { 50, 10, 0, { 0 } },
};

// ML's table. Slot 2 is ML's own entrance into the Deku Tree (its spawn 2), which a stock Unbound does not have: the
// MissingShip builder's --stock layer puts it on vanilla rows (missinglink/entrances, ml_cutscenes.c), and the grotto
// takes it from there.
static s16 sGrottoEntrances[] = {
    ENTR_FAIRYS_FOUNTAIN_0,           ENTR_KAKARIKO_VILLAGE_FRONT_GATE,    ENTR_DEKU_TREE_ENTRANCE,
    ENTR_DEKU_TREE_ENTRANCE,          ENTR_DEKU_TREE_BOSS_DOOR,            ENTR_TESTROOM_0,
    ENTR_DESERT_COLOSSUS_EAST_EXIT,   ENTR_DESERT_COLOSSUS_OUTSIDE_TEMPLE, ENTR_DESERT_COLOSSUS_2,
    ENTR_GROTTOS_8,                   ENTR_GROTTOS_9,                      ENTR_GROTTOS_10,
    ENTR_GROTTOS_11,                  ENTR_GROTTOS_12,                     ENTR_GROTTOS_13,
};

static void MlGrotto_SetupAction(DoorAna* this, DoorAnaActionFunc actionFunc) {
    this->actionFunc = actionFunc;
}

static void MlGrotto_Init(Actor* thisx, PlayState* play) {
    DoorAna* this = (DoorAna*)thisx;

    this->actor.shape.rot.z = 0;
    this->actor.shape.rot.y = this->actor.shape.rot.z;
    // hidden grottos open with explosives or a hammer (0x200) or the Song of Storms (0x100)
    if ((this->actor.params & 0x300) != 0) {
        if ((this->actor.params & 0x200) != 0) {
            Collider_InitCylinder(play, &this->collider);
            Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
        } else {
            this->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
        }
        Actor_SetScale(&this->actor, 0);
        MlGrotto_SetupAction(this, MlGrotto_WaitClosed);
    } else {
        MlGrotto_SetupAction(this, MlGrotto_WaitOpen);
    }
    this->actor.targetMode = 0;
}

static void MlGrotto_Destroy(Actor* thisx, PlayState* play) {
    DoorAna* this = (DoorAna*)thisx;

    if ((this->actor.params & 0x200) != 0) {
        Collider_DestroyCylinder(play, &this->collider);
    }
}

static void MlGrotto_WaitClosed(DoorAna* this, PlayState* play) {
    u32 openGrotto = false;

    if (!(this->actor.params & 0x200)) {
        if (this->actor.xyzDistToPlayerSq < 40000.0f && Flags_GetEnv(play, 5)) {
            openGrotto = true;
            this->actor.flags &= ~ACTOR_FLAG_UPDATE_CULLING_DISABLED;
        }
    } else {
        if (this->collider.base.acFlags & AC_HIT) {
            openGrotto = true;
            Collider_DestroyCylinder(play, &this->collider);
        } else {
            Collider_UpdateCylinder(&this->actor, &this->collider);
            CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        }
    }
    if (openGrotto) {
        this->actor.params &= ~0x0300;
        MlGrotto_SetupAction(this, MlGrotto_WaitOpen);
        Audio_PlaySfxGeneral(NA_SE_SY_CORRECT_CHIME, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }
    Actor_SetClosestSecretDistance(&this->actor, play);
}

static void MlGrotto_WaitOpen(DoorAna* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s32 destinationIdx;

    if (Math_StepToF(&this->actor.scale.x, 0.01f, 0.001f)) {
        if ((this->actor.targetMode != 0) && (play->transitionTrigger == TRANS_TRIGGER_OFF) &&
            (player->stateFlags1 & PLAYER_STATE1_FLOOR_DISABLED) && (player->av1.actionVar1 == 0)) {
            destinationIdx = ((this->actor.params >> 0xC) & 7) - 1;
            // ML leaves out Play_SetupRespawnPoint: its grottos lead to ordinary scenes, left through their own exits
            gSaveContext.respawn[RESPAWN_MODE_RETURN].pos.y = this->actor.world.pos.y;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].yaw = this->actor.home.rot.y;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].data = this->actor.params & 0xFFFF;
            if (destinationIdx < 0) {
                destinationIdx = this->actor.home.rot.z + 1;
            }
            play->nextEntranceIndex = sGrottoEntrances[destinationIdx];
            if ((destinationIdx == 2) && (MlEntrance_Index("SCENE_DEKU_TREE/ml/deku_tree_2") >= 0)) {
                play->nextEntranceIndex = MlEntrance_Index("SCENE_DEKU_TREE/ml/deku_tree_2");
            }
            MlGrotto_SetupAction(this, MlGrotto_GrabPlayer);
        } else {
            if (!Player_InCsMode(play) && !(player->stateFlags1 & (PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_IN_WATER)) &&
                this->actor.xzDistToPlayer <= 15.0f && -50.0f <= this->actor.yDistToPlayer &&
                this->actor.yDistToPlayer <= 15.0f) {
                player->stateFlags1 |= PLAYER_STATE1_FLOOR_DISABLED;
                this->actor.targetMode = 1;
            } else {
                this->actor.targetMode = 0;
            }
        }
    }
    Actor_SetScale(&this->actor, this->actor.scale.x);
}

static void MlGrotto_GrabPlayer(DoorAna* this, PlayState* play) {
    Player* player;

    if (this->actor.yDistToPlayer <= 0.0f && 15.0f < this->actor.xzDistToPlayer) {
        player = GET_PLAYER(play);
        player->actor.world.pos.x = Math_SinS(this->actor.yawTowardsPlayer) * 15.0f + this->actor.world.pos.x;
        player->actor.world.pos.z = Math_CosS(this->actor.yawTowardsPlayer) * 15.0f + this->actor.world.pos.z;
    }
}

static void MlGrotto_Update(Actor* thisx, PlayState* play) {
    DoorAna* this = (DoorAna*)thisx;

    this->actionFunc(this, play);
    // faces the camera, as vanilla's does unless its enhancement is off (the SDK has no CVar prefixes: the full name)
    if (!CVarGetInteger("gEnhancements.DisableGrottoRotation", 0)) {
        this->actor.shape.rot.y = Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) + 0x8000;
    }
}

static void MlGrotto_Draw(Actor* thisx, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gMlGrottoDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

void MlGrotto_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ml/grotto";
    definition.description = "The Missing Link: grotto";
    definition.category = ACTORCAT_ITEMACTION;
    definition.actorFlags = ACTOR_FLAG_UPDATE_DURING_OCARINA;
    definition.objectId = OBJECT_GAMEPLAY_KEEP;     // vanilla's is OBJECT_GAMEPLAY_FIELD_KEEP, which dungeons lack
    definition.instanceSize = sizeof(DoorAna);
    definition.init = MlGrotto_Init;
    definition.destroy = MlGrotto_Destroy;
    definition.update = MlGrotto_Update;
    definition.draw = MlGrotto_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ml/grotto is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
