/*
 * The Missing Link's Deep Woods fog wall (ledger rows 26 and 105), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_FogWall), registered through the mod API.
 *
 * Description: The Missing Link's Deep Woods fog walls and looping corridor (ML's actor 0x174)
 *
 * ML put a copy of En_Holl (the room-transition plane) in Demo_Gt's slot and changed three things, all marked "ML:"
 * below: it always draws the plane, it fades it by the player's distance, and, for walls placed off the ground
 * (home y != 0), it loops the corridor: walking past z = 1400 sends Link back, mirrored and turned around, with the
 * camera shifted by the same amount (ML custom code 0x80602080). Placed 9x in Deep Woods room 5, as room actors, so
 * TRANSITION_ACTOR_INDEX falls back to params >> 10 (entry 0), as ML reads it. Everything else is Unbound's En_Holl,
 * renamed. Ported from MissingShip's SoH fork (ovl_Ml_FogWall, ledger rows 26 and 105).
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include <string.h>

#include <math.h>
#include <stdio.h>

#include "missinglink.h"

struct MlFogWall;

typedef void (*MlFogWallActionFunc)(struct MlFogWall*, PlayState*);

typedef struct MlFogWall {
    /* 0x0000 */ Actor actor;
    /* 0x014C */ s16 planeAlpha;
    /* 0x014E */ u8 side;
    /* 0x014F */ u8 unk_14F;
    /* 0x0150 */ MlFogWallActionFunc actionFunc;
    /* 0x0154 */ u8 fogOnly; // MissingShip (MS-147): the transition entry doesn't touch this wall's room
} MlFogWall;

// Horizontal Plane parameters

#define PLANE_Y_MIN -50.0f
#define PLANE_Y_MAX 200.0f

#define PLANE_HALFWIDTH 100.0f
#define PLANE_HALFWIDTH_2 200.0f

static void MlFogWall_NextAction(MlFogWall* this, PlayState* play);
static void MlFogWall_func_80A58DD4(MlFogWall* this, PlayState* play);
static void MlFogWall_func_80A59014(MlFogWall* this, PlayState* play);
static void MlFogWall_func_80A591C0(MlFogWall* this, PlayState* play);
static void MlFogWall_func_80A593A4(MlFogWall* this, PlayState* play);
static void MlFogWall_func_80A59520(MlFogWall* this, PlayState* play);
static void MlFogWall_func_80A59618(MlFogWall* this, PlayState* play);

static MlFogWallActionFunc sActionFuncs[] = {
    MlFogWall_func_80A58DD4, MlFogWall_func_80A591C0, MlFogWall_func_80A59520, MlFogWall_func_80A59618,
    MlFogWall_func_80A59014, MlFogWall_func_80A593A4, MlFogWall_func_80A59014,
};

static InitChainEntry sInitChain[] = {
    ICHAIN_F32(uncullZoneForward, 4000, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneScale, 400, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneDownward, 400, ICHAIN_STOP),
};

/**
 * These are all absolute distances in the relative z direction. That is, moving
 *   towards or away from the "face" of the loading plane regardless of orientation.
 * Moving within these distances of the load plane have the following effects:
 * [0] : Load the room on this side of the loading plane if not already loaded
 * [1] : Load the room on the other side of the loading plane
 * [2] : Fade Region (opaque -> transparent if approaching, transparent -> opaque if receding)
 * [3] : Transparent Region
 *
 * When traversing a loading plane of this kind, it attempts to:
 *   Load Current Room (fails as it is already loaded)
 *   Load Next Room
 *   Load Previous Room
 *   Load Next Room
 *
 *  @bug The striped nature of loading planes can cause some actors to unload due to
 *      conflicting Object Lists between the two rooms
 *
 *  @bug If you can get around to the other side of the loading plane without triggering it,
 *      you can load the room on the other side multiple times
 */
static f32 sHorizTriggerDists[2][4] = {
    { 200.0f, 150.0f, 100.0f, 50.0f },
    { 100.0f, 75.0f, 50.0f, 25.0f },
};

#include "overlays/ovl_En_Holl/ovl_En_Holl.h"

static void MlFogWall_SetupAction(MlFogWall* this, MlFogWallActionFunc func) {
    this->actionFunc = func;
}

static s32 MlFogWall_IsKokiriSetup8(void) {
    return gSaveContext.entranceIndex == ENTR_KOKIRI_FOREST_0 && gSaveContext.sceneLayer == 8;
}

static void MlFogWall_ChooseAction(MlFogWall* this) {
    s32 action;

    action = (this->actor.params >> 6) & 7;
    MlFogWall_SetupAction(this, sActionFuncs[action]);
    // ML: the plane is never hidden and always starts opaque.
    this->planeAlpha = 255;
}

static void MlFogWall_Init(Actor* thisx, PlayState* play) {
    MlFogWall* this = (MlFogWall*)thisx;

    Actor_ProcessInitChain(&this->actor, sInitChain);
    MlFogWall_ChooseAction(this);
    this->unk_14F = 0;

    // MissingShip (MS-147): ML places these as room actors with params 0, so they all use transition entry 0 (Deep
    // Woods rooms 2 <-> 1) although they stand in room 5. Crossing one then loads room 2, swaps the current and
    // previous room every frame while Link is in range, and unloads whichever is "previous" when he leaves: half
    // the time that is room 5, leaving Link in a black void. A wall whose own room is on neither side of its entry
    // is a fog barrier only; it still draws, fades and loops the corridor, but loads no rooms. So is one whose entry
    // the scene does not have (a room actor reads entry params >> 10), which then never reads the list.
    if (TRANSITION_ACTOR_INDEX(thisx) >= play->transiActorCtx.numActors) {
        this->fogOnly = true;
    } else {
        TransitionActorEntry* entry = &play->transiActorCtx.list[TRANSITION_ACTOR_INDEX(thisx)];

        this->fogOnly = (entry->sides[0].room != thisx->room) && (entry->sides[1].room != thisx->room);
    }
}

static void MlFogWall_Destroy(Actor* thisx, PlayState* play) {
    s32 transitionActorIdx = TRANSITION_ACTOR_INDEX(thisx);
    TransitionActorEntry* transitionEntry;

    if (transitionActorIdx >= play->transiActorCtx.numActors) {
        return;
    }
    transitionEntry = &play->transiActorCtx.list[transitionActorIdx];
    transitionEntry->id = -transitionEntry->id;
}

static void MlFogWall_SwapRooms(PlayState* play) {
    Room tempRoom;
    RoomContext* roomCtx = &play->roomCtx;

    tempRoom = roomCtx->curRoom;
    roomCtx->curRoom = roomCtx->prevRoom;
    roomCtx->prevRoom = tempRoom;
    play->roomCtx.activeBufPage ^= 1;
}

// Horizontal Planes
static void MlFogWall_func_80A58DD4(MlFogWall* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s32 phi_t0 = ((play->sceneNum == SCENE_SPIRIT_TEMPLE) ? 1 : 0) & 0xFFFFFFFF;
    Vec3f vec;
    f32 absZ;
    s32 transitionActorIdx;

    Actor_WorldToActorCoords(&this->actor, &vec, &player->actor.world.pos);
    this->side = (vec.z < 0.0f) ? 0 : 1;
    absZ = fabsf(vec.z);
    if (vec.y > PLANE_Y_MIN && vec.y < PLANE_Y_MAX && fabsf(vec.x) < PLANE_HALFWIDTH &&
        absZ < sHorizTriggerDists[phi_t0][0]) {
        transitionActorIdx = TRANSITION_ACTOR_INDEX(&this->actor);
        if (absZ > sHorizTriggerDists[phi_t0][1]) {
            if (play->roomCtx.prevRoom.num >= 0 && play->roomCtx.status == 0) {
                this->actor.room = play->transiActorCtx.list[transitionActorIdx].sides[this->side].room;
                MlFogWall_SwapRooms(play);
                Room_FinishRoomChange(play, &play->roomCtx);
            }
        } else {
            this->actor.room = play->transiActorCtx.list[transitionActorIdx].sides[this->side ^ 1].room;
            if (play->roomCtx.prevRoom.num < 0) {
                Room_RequestNewRoom(play, &play->roomCtx, this->actor.room);
            } else {
                this->planeAlpha = (255.0f / (sHorizTriggerDists[phi_t0][2] - sHorizTriggerDists[phi_t0][3])) *
                                   (absZ - sHorizTriggerDists[phi_t0][3]);
                this->planeAlpha = CLAMP(this->planeAlpha, 0, 255);
                if (play->roomCtx.curRoom.num != this->actor.room) {
                    MlFogWall_SwapRooms(play);
                }
            }
        }
    }
}

// Horizontal Planes
static void MlFogWall_func_80A59014(MlFogWall* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s32 useViewEye = gDbgCamEnabled || play->csCtx.state != CS_STATE_IDLE;
    Vec3f vec;
    s32 temp;
    f32 planeHalfWidth;
    f32 absZ;

    Actor_WorldToActorCoords(&this->actor, &vec, (useViewEye) ? &play->view.eye : &player->actor.world.pos);
    planeHalfWidth = (((this->actor.params >> 6) & 7) == 6) ? PLANE_HALFWIDTH : PLANE_HALFWIDTH_2;

    temp = MlFogWall_IsKokiriSetup8();
    if (temp || (PLANE_Y_MIN < vec.y && vec.y < PLANE_Y_MAX && fabsf(vec.x) < planeHalfWidth &&
                 (absZ = fabsf(vec.z), 100.0f > absZ && absZ > 50.0f))) {
        s32 transitionActorIdx = TRANSITION_ACTOR_INDEX(&this->actor);
        s32 side = (vec.z < 0.0f) ? 0 : 1;
        TransitionActorEntry* transitionEntry = &play->transiActorCtx.list[transitionActorIdx];
        s32 room = transitionEntry->sides[side].room;

        this->actor.room = room;
        if (temp) {}
        if (this->actor.room != play->roomCtx.curRoom.num) {
            if (room) {}
            if (Room_RequestNewRoom(play, &play->roomCtx, this->actor.room)) {
                MlFogWall_SetupAction(this, MlFogWall_NextAction);
            }
        }
    }
}

// Vertical Planes
static void MlFogWall_func_80A591C0(MlFogWall* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 absY = fabsf(this->actor.yDistToPlayer);
    s32 transitionActorIdx;

    if (this->actor.xzDistToPlayer < 500.0f && absY < 700.0f) {
        transitionActorIdx = TRANSITION_ACTOR_INDEX(&this->actor);
        if (absY < 95.0f) {
            play->bgCoverAlpha = 0xFF;
        } else if (absY > 605.0f) {
            play->bgCoverAlpha = 0;
        } else {
            play->bgCoverAlpha = (s16)(605.0f - absY) * 0.5f;
        }
        if (absY < 95.0f) {
            this->actor.room = play->transiActorCtx.list[transitionActorIdx].sides[1].room;
            Math_SmoothStepToF(&player->actor.world.pos.x, this->actor.world.pos.x, 1.0f, 50.0f, 10.0f);
            Math_SmoothStepToF(&player->actor.world.pos.z, this->actor.world.pos.z, 1.0f, 50.0f, 10.0f);
            if (this->actor.room != play->roomCtx.curRoom.num &&
                Room_RequestNewRoom(play, &play->roomCtx, this->actor.room) != 0) {
                MlFogWall_SetupAction(this, MlFogWall_NextAction);
                this->unk_14F = 1;
                player->actor.speedXZ = 0.0f;
            }
        }
    } else if (this->unk_14F != 0) {
        play->bgCoverAlpha = 0;
        this->unk_14F = 0;
    }
}

// Vertical Planes
static void MlFogWall_func_80A593A4(MlFogWall* this, PlayState* play) {
    f32 absY;
    s32 side;
    s32 transitionActorIdx;

    if ((this->actor.xzDistToPlayer < 120.0f) && (absY = fabsf(this->actor.yDistToPlayer), absY < 200.0f)) {
        if (absY < 50.0f) {
            play->bgCoverAlpha = 0xFF;
        } else {
            play->bgCoverAlpha = (200.0f - absY) * 1.7f;
        }
        if (absY > 50.0f) {
            transitionActorIdx = TRANSITION_ACTOR_INDEX(&this->actor);
            side = (0.0f < this->actor.yDistToPlayer) ? 0 : 1;
            this->actor.room = play->transiActorCtx.list[transitionActorIdx].sides[side].room;
            if (this->actor.room != play->roomCtx.curRoom.num &&
                Room_RequestNewRoom(play, &play->roomCtx, this->actor.room) != 0) {
                MlFogWall_SetupAction(this, MlFogWall_NextAction);
                this->unk_14F = 1;
            }
        }
    } else if (this->unk_14F != 0) {
        this->unk_14F = 0;
        play->bgCoverAlpha = 0;
    }
}

// Vertical Planes
static void MlFogWall_func_80A59520(MlFogWall* this, PlayState* play) {
    f32 absY;
    s8 side;
    s32 transitionActorIdx;

    if (this->actor.xzDistToPlayer < 120.0f) {
        absY = fabsf(this->actor.yDistToPlayer);
        if (absY < 200.0f && absY > 50.0f) {
            transitionActorIdx = TRANSITION_ACTOR_INDEX(&this->actor);
            side = (0.0f < this->actor.yDistToPlayer) ? 0 : 1;
            this->actor.room = play->transiActorCtx.list[transitionActorIdx].sides[side].room;
            if (this->actor.room != play->roomCtx.curRoom.num &&
                Room_RequestNewRoom(play, &play->roomCtx, this->actor.room) != 0) {
                MlFogWall_SetupAction(this, MlFogWall_NextAction);
            }
        }
    }
}

// Horizontal Planes
static void MlFogWall_func_80A59618(MlFogWall* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Vec3f vec;
    f32 absZ;
    s32 side;
    s32 transitionActorIdx;

    if (!Flags_GetSwitch(play, this->actor.params & 0x3F)) {
        if (this->unk_14F != 0) {
            play->bgCoverAlpha = 0;
            this->unk_14F = 0;
        }
    } else {
        Actor_WorldToActorCoords(&this->actor, &vec, &player->actor.world.pos);
        absZ = fabsf(vec.z);
        if (PLANE_Y_MIN < vec.y && vec.y < PLANE_Y_MAX && fabsf(vec.x) < PLANE_HALFWIDTH_2 && absZ < 100.0f) {
            this->unk_14F = 1;
            transitionActorIdx = TRANSITION_ACTOR_INDEX(&this->actor);
            play->bgCoverAlpha = 0xFF - (s32)((absZ - 50.0f) * 5.9f);
            if (play->bgCoverAlpha >= 0x100) {
                play->bgCoverAlpha = 0xFF;
            } else if (play->bgCoverAlpha < 0) {
                play->bgCoverAlpha = 0;
            }
            if (absZ < 50.0f) {
                side = (vec.z < 0.0f) ? 0 : 1;
                this->actor.room = play->transiActorCtx.list[transitionActorIdx].sides[side].room;
                if (this->actor.room != play->roomCtx.curRoom.num &&
                    Room_RequestNewRoom(play, &play->roomCtx, this->actor.room) != 0) {
                    MlFogWall_SetupAction(this, MlFogWall_NextAction);
                }
            }
        } else if (this->unk_14F != 0) {
            play->bgCoverAlpha = 0;
            this->unk_14F = 0;
        }
    }
}

static void MlFogWall_NextAction(MlFogWall* this, PlayState* play) {
    if (!MlFogWall_IsKokiriSetup8() && play->roomCtx.status == 0) {
        Room_FinishRoomChange(play, &play->roomCtx);
        if (play->bgCoverAlpha == 0) {
            this->unk_14F = 0;
        }
        MlFogWall_ChooseAction(this);
    }
}

static void MlFogWall_Update(Actor* thisx, PlayState* play) {
    MlFogWall* this = (MlFogWall*)thisx;
    Player* player = GET_PLAYER(play);

    // ML: its own update (overlay +0xC68). The wall is scaled thin, and within 60 units of it (along z) it
    // fades with the player's x distance: 2 alpha per unit, opaque from 128 units away.
    this->actor.scale.x = 0.0049743652f; // ML loads only the upper half of 0.005 (0x3BA30000)
    if (fabsf(player->actor.world.pos.z - this->actor.world.pos.z) < 60.0f) {
        s32 alpha = (s32)(fabsf(player->actor.world.pos.x - this->actor.world.pos.x) * 2.0f);

        this->planeAlpha = (alpha < 255) ? alpha : 255;
    }
    if (this->fogOnly) {
        Vec3f vec;

        // Only the side, which Draw uses to face the plane toward Link.
        Actor_WorldToActorCoords(&this->actor, &vec, &player->actor.world.pos);
        this->side = (vec.z < 0.0f) ? 0 : 1;
        return;
    }
    this->actionFunc(this, play);
}

// ML: its custom code 0x80602080, run while the plane is translucent (the player is close to it).
static void MlFogWall_LoopCorridor(MlFogWall* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    Camera* camera = &play->mainCamera;
    u32 homeYBits;
    f32 oldX;
    f32 oldZ;
    f32 dx;
    f32 dz;

    // ML tests the upper halfword of home.pos.y: walls standing on y = 0 never loop.
    memcpy(&homeYBits, &this->actor.home.pos.y, sizeof(homeYBits));
    if ((homeYBits >> 16) == 0 || player->actor.world.pos.z < 1400.0f) {
        return;
    }

    oldX = player->actor.world.pos.x;
    oldZ = player->actor.world.pos.z;
    player->actor.world.pos.y = -11.0f;
    player->actor.world.pos.x = (this->actor.home.pos.x - oldX) + -317.0f;
    player->actor.world.pos.z = 1832.0f - (oldZ - this->actor.home.pos.z);
    dx = player->actor.world.pos.x - oldX;
    dz = player->actor.world.pos.z - oldZ;

    camera->at.x += dx;
    camera->at.z += dz;
    camera->playerPosRot.pos.x += dx;
    camera->playerPosRot.pos.z += dz;

    player->actor.shape.rot.y += 0x8000;
    player->yaw = player->actor.shape.rot.y;
    player->actor.world.rot.y = player->actor.shape.rot.y;
    // ML 0x80602188 skips the next two wall push-outs so Link is not shoved back out of the loop. The push comes from
    // the player's wall sweep, which runs from where Link stood last frame (Player keeps it in home.pos and copies it to
    // prevPos) across the corridor's walls to where he is now. Moving those along with him makes the sweep start here,
    // so there is nothing to push him back across.
    player->actor.home.pos = player->actor.world.pos;
    player->actor.prevPos = player->actor.world.pos;
    this->planeAlpha = 255;
}

static void MlFogWall_Draw(Actor* thisx, PlayState* play) {
    MlFogWall* this = (MlFogWall*)thisx;
    Gfx* gfxP;
    u32 setupDlIdx;

    // Only draw the plane if not invisible
    if (this->planeAlpha != 0) {
        OPEN_DISPS(play->state.gfxCtx);

        if (this->planeAlpha == 255) {
            gfxP = POLY_OPA_DISP;
            setupDlIdx = 37;
        } else {
            gfxP = POLY_XLU_DISP;
            setupDlIdx = 0;
        }
        gfxP = Gfx_SetupDL(gfxP, setupDlIdx);
        if (this->side == 0) {
            Matrix_RotateY(M_PI, MTXMODE_APPLY);
        }

        gSPMatrix(gfxP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gDPSetPrimColor(gfxP++, 0, 0, 0, 0, 0, (u8)this->planeAlpha);
        gSPDisplayList(gfxP++, (Gfx*)sPlaneDL);

        if (this->planeAlpha == 255) {
            POLY_OPA_DISP = gfxP;
        } else {
            POLY_XLU_DISP = gfxP;
            MlFogWall_LoopCorridor(this, play); // ML
        }

        CLOSE_DISPS(play->state.gfxCtx);
    }
}

void MlFogWall_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_FogWall";
    definition.description = "The Missing Link: Deep Woods fog wall";
    definition.category = ACTORCAT_DOOR;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = OBJECT_GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(MlFogWall);
    definition.init = MlFogWall_Init;
    definition.destroy = MlFogWall_Destroy;
    definition.update = MlFogWall_Update;
    definition.draw = MlFogWall_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_FogWall is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
