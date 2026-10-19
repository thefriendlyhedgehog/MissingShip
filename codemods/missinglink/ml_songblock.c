/*
 * The Missing Link's block sealing the way to Saria's house (ledger row 21), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_SongBlock), registered through the mod
 * API. ML rewrote Obj_Timeblock wholesale: no Song of Time logic. It draws and collides from ML's object_ml_songblock
 * and, on Init, places a vanilla En_Okarina_Tag (params 0x1C0C: switch 0x0C) at fixed Kokiri Forest coordinates.
 * Playing the song there sets switch 0x0C; the block then shows a Magic_Wind sparkle and, after 30 frames, sinks 2
 * units per frame for 150 frames under a short cutscene before removing itself.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "align_asset_macro.h"

#include <stdio.h>

#include "missinglink.h"

typedef struct MlSongBlock {
    DynaPolyActor dyna;
    Actor* sparkle; // Magic_Wind shown while it sinks
    s32 timer;
    s8 state;
    s8 killedAtInit;
} MlSongBlock;

#define dgMlSongBlockCol "__OTR__objects/object_ml_songblock/gMlSongBlockCol"
static const ALIGN_ASSET(2) char gMlSongBlockCol[] = dgMlSongBlockCol;
#define dgMlSongBlockDL "__OTR__objects/object_ml_songblock/gMlSongBlockDL"
static const ALIGN_ASSET(2) char gMlSongBlockDL[] = dgMlSongBlockDL;

#define ML_SONGBLOCK_SWITCH 0x0C

static void MlSongBlock_Init(Actor* thisx, PlayState* play) {
    MlSongBlock* this = (MlSongBlock*)thisx;
    CollisionHeader* colHeader = NULL;

    if (Flags_GetSwitch(play, ML_SONGBLOCK_SWITCH)) {
        this->killedAtInit = true;
        Actor_Kill(thisx);
        return;
    }

    Actor_SetScale(thisx, 1.0f);
    thisx->uncullZoneForward = 2000.0f;
    DynaPolyActor_Init(&this->dyna, DPM_PLAYER);
    CollisionHeader_GetVirtual((void*)gMlSongBlockCol, &colHeader);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, colHeader);

    // ML hardcodes where the song must be played.
    Actor_Spawn(&play->actorCtx, play, ACTOR_EN_OKARINA_TAG, 1128.0f, 268.0f, -1370.0f, 0, thisx->world.rot.y, 0,
                0x1C0C);
}

static void MlSongBlock_Destroy(Actor* thisx, PlayState* play) {
    MlSongBlock* this = (MlSongBlock*)thisx;

    if (!this->killedAtInit) {
        DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
    }
}

static void MlSongBlock_Update(Actor* thisx, PlayState* play) {
    MlSongBlock* this = (MlSongBlock*)thisx;

    if (this->state == 1) {
        this->sparkle = Actor_Spawn(&play->actorCtx, play, ACTOR_MAGIC_WIND, thisx->world.pos.x, thisx->world.pos.y,
                                    thisx->world.pos.z, 0, 0, 0, 0);
        this->state = 2;
    } else if (this->state == 2) {
        Camera_ChangeDataIdx(&play->mainCamera, 3);
        func_80064520(play, &play->csCtx);
        this->timer++;
        if (this->timer >= 31) {
            thisx->world.pos.y -= 2.0f;
            Player_PlaySfx(thisx, 0x208F);
            if (this->timer >= 181) {
                func_80064534(play, &play->csCtx);
                if (this->sparkle != NULL) {
                    Actor_Kill(this->sparkle);
                }
                Actor_Kill(thisx);
            }
        }
    } else if (Flags_GetSwitch(play, ML_SONGBLOCK_SWITCH)) {
        this->state = 1;
    }
}

static void MlSongBlock_Draw(Actor* thisx, PlayState* play) {
    Gfx_DrawDListOpa(play, (Gfx*)gMlSongBlockDL);
}

void MlSongBlock_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_SongBlock";
    definition.description = "The Missing Link: song block";
    definition.category = ACTORCAT_BG;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    definition.objectId = OBJECT_OUKE_HAKA;     // the slot ML's ROM refills with its block
    definition.instanceSize = sizeof(MlSongBlock);
    definition.init = MlSongBlock_Init;
    definition.destroy = MlSongBlock_Destroy;
    definition.update = MlSongBlock_Update;
    definition.draw = MlSongBlock_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_SongBlock is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
