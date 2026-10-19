/*
 * The Missing Link's fishing pond scenery (ledger row 10), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_Fishpond),
 * registered through the mod API.
 *
 * ML's own overlay, in vanilla's unused slot 0x003 (ML dma 104, 496 bytes), ported as MissingShip's SoH fork has it
 * (ovl_Ml_Fishpond, ledger row 10). Placed 13x in ML's fishing pond (turibori room 0), params selecting one of four
 * display lists, which ML keeps in its replacement for object slot 0x40 (OBJECT_JJ in vanilla) and its layer ships as
 * objects/object_ml_fishpond.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "align_asset_macro.h"
#include "soh/ActorDB.h"
#include "soh/unbound/SceneDB.h"
#include "overlays/actors/ovl_En_Elf/z_en_elf.h"

#include <stdio.h>

#include "missinglink.h"

typedef struct MlFishpond {
    Actor actor;
    u8 dlIndex;
} MlFishpond; // ML instanceSize = 0x150

static const SOHModApi* sApi;

#define dgMlFishpond0DL "__OTR__objects/object_ml_fishpond/gMlFishpond0DL"
static const ALIGN_ASSET(2) char gMlFishpond0DL[] = dgMlFishpond0DL;
#define dgMlFishpond1DL "__OTR__objects/object_ml_fishpond/gMlFishpond1DL"
static const ALIGN_ASSET(2) char gMlFishpond1DL[] = dgMlFishpond1DL;
#define dgMlFishpond2DL "__OTR__objects/object_ml_fishpond/gMlFishpond2DL"
static const ALIGN_ASSET(2) char gMlFishpond2DL[] = dgMlFishpond2DL;
#define dgMlFishpond3DL "__OTR__objects/object_ml_fishpond/gMlFishpond3DL"
static const ALIGN_ASSET(2) char gMlFishpond3DL[] = dgMlFishpond3DL;

// ML stores these as four segment-6 pointers in the overlay's .data section:
// 0x06000978, 0x06002878, 0x06003068, 0x06003728.
static const char* sDLists[] = {
    gMlFishpond0DL,
    gMlFishpond1DL,
    gMlFishpond2DL,
    gMlFishpond3DL,
};

// ML's companions are ML actors of their own, spawned by the names they are registered under (MlActors.cpp). One not
// registered yet is skipped with an error, as a spawn of an unknown actor is.
static void MlFishpond_SpawnByName(PlayState* play, const char* name, f32 x, f32 y, f32 z, s16 rotY, s16 params) {
    s32 id = ActorDB_RetrieveId(name);

    if (id < 0) {
        char message[96];
        snprintf(message, sizeof(message), "[MissingLink] ML_Fishpond: %s is not registered; its spawn is skipped", name);
        sApi->Log(message);
        return;
    }
    Actor_Spawn(&play->actorCtx, play, id, x, y, z, 0, rotY, 0, params);
}

static void MlFishpond_Init(Actor* thisx, PlayState* play) {
    MlFishpond* this = (MlFishpond*)thisx;

    thisx->uncullZoneForward = 6000.0f;
    thisx->uncullZoneScale = 2000.0f;
    thisx->uncullZoneDownward = 4000.0f;
    Actor_SetScale(thisx, 0.1f);

    this->dlIndex = (u8)thisx->params;
    // ML indexes a four-entry table with the raw params byte and does not range check it. Every placement in
    // turibori_r0 uses 0-3, but clamp rather than inherit an out-of-bounds read.
    if (this->dlIndex >= ARRAY_COUNT(sDLists)) {
        this->dlIndex = 0;
    }

    // The params==0 instance spawns a companion chosen by the entrance Link arrived from: after a blue warp (ML 0x41C,
    // vanilla's ENTR_FISHING_POND_0) a Navi-type Ml_Elf at the origin; after the pond intro cutscene (ML 0x420, via
    // ML's patched destination 56: ML's layer's entrance ml/fishing_pond_event) Ml_EventSpawner mode 3, Epona walking
    // off with Link.
    if (thisx->params == 0) {
        if (gSaveContext.entranceIndex == ENTR_FISHING_POND_0) {
            MlFishpond_SpawnByName(play, "ML_Elf", 0.0f, 0.0f, 0.0f, 0, FAIRY_NAVI);
        } else if (gSaveContext.entranceIndex == MlEntrance_Index("SCENE_FISHING_POND/ml/fishing_pond_event")) {
            // on a stock Unbound, the vanilla rows ml_cutscenes.c gives ML's entrance
            MlFishpond_SpawnByName(play, "ML_EventSpawner", 691.0f, 2000.0f, -558.0f, -8640, 3);
        }
    }
}

static void MlFishpond_Destroy(Actor* thisx, PlayState* play) {
}

static void MlFishpond_Update(Actor* thisx, PlayState* play) {
}

static void MlFishpond_Draw(Actor* thisx, PlayState* play) {
    MlFishpond* this = (MlFishpond*)thisx;

    Gfx_DrawDListOpa(play, (Gfx*)sDLists[this->dlIndex]);
}

void MlFishpond_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    sApi = api;
    definition.structSize = sizeof(definition);
    definition.key = "ML_Fishpond";
    definition.description = "The Missing Link: fishing pond scenery";
    definition.category = ACTORCAT_BG;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = OBJECT_JJ;
    definition.instanceSize = sizeof(MlFishpond);
    definition.init = MlFishpond_Init;
    definition.destroy = MlFishpond_Destroy;
    definition.update = MlFishpond_Update;
    definition.draw = MlFishpond_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_Fishpond is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
