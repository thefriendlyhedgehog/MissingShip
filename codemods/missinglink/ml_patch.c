/*
 * ML's changes to the game's own tables, swapped with the game's as ML's world comes and goes (ml_patch.h).
 *
 * Each watched place keeps two copies: the game's, taken before ML's first write, and ML's, taken when ML's world
 * goes. The world changes only on the file select or at boot (ModQuests on the host), while no actor is loaded, so
 * a slot's functions are never swapped under a live actor.
 */
#include "ml_patch.h"
#include "missinglink.h"

#include "soh/ActorDB.h"
#include "soh/unbound/SceneDB.h"
#include "variables.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    ML_PATCH_FIXED,
    ML_PATCH_ACTOR,
    ML_PATCH_ENTRANCE,
} MlPatchKind;

typedef struct {
    MlPatchKind kind;
    void* p;   // ML_PATCH_FIXED
    s32 index; // an actor's id or an entrance's row
    size_t size;
    u8* game;
    u8* ml;
} MlPatch;

static const SOHModApi* sApi;
static MlPatch* sPatches;
static s32 sCount;
static s32 sCapacity;
static bool sStarted;           // the host took ML's quest
static bool sInMlWorld = true; // ML registers its quest, and writes, while its world covers the game's

// The slot's fields ML changes: category to reset (not its name, id or the count of loaded actors)
#define ML_ACTOR_FROM offsetof(ActorDBEntry, category)
#define ML_ACTOR_SIZE (offsetof(ActorDBEntry, numLoaded) - ML_ACTOR_FROM)

static u8* MlPatch_Where(const MlPatch* patch) {
    switch (patch->kind) {
        case ML_PATCH_ACTOR:
            return (u8*)ActorDB_Retrieve(patch->index) + ML_ACTOR_FROM;
        case ML_PATCH_ENTRANCE:
            return (u8*)&gEntranceTable[patch->index];
        default:
            return patch->p;
    }
}

static void MlPatch_Add(MlPatchKind kind, void* p, s32 index, size_t size) {
    MlPatch* patch;

    for (s32 i = 0; i < sCount; i++) {
        if ((sPatches[i].kind == kind) && (sPatches[i].p == p) && (sPatches[i].index == index)) {
            return;
        }
    }
    if (sCount == sCapacity) {
        s32 capacity = (sCapacity == 0) ? 64 : sCapacity * 2;
        MlPatch* grown = realloc(sPatches, capacity * sizeof(MlPatch));

        if (grown == NULL) {
            return;
        }
        sPatches = grown;
        sCapacity = capacity;
    }
    patch = &sPatches[sCount];
    patch->kind = kind;
    patch->p = p;
    patch->index = index;
    patch->size = size;
    patch->game = malloc(size);
    patch->ml = NULL;
    if (patch->game == NULL) {
        return;
    }
    memcpy(patch->game, MlPatch_Where(patch), size);
    sCount++;
}

void MlPatch_Watch(void* p, size_t size) {
    MlPatch_Add(ML_PATCH_FIXED, p, 0, size);
}

void* MlPatch_Actor(int id) {
    ActorDBEntry* entry = ActorDB_Retrieve(id);

    if ((entry != NULL) && entry->valid) {
        MlPatch_Add(ML_PATCH_ACTOR, NULL, id, ML_ACTOR_SIZE);
    }
    return entry;
}

void MlPatch_Entrance(int index) {
    if ((index >= 0) && (index < EntranceDB_GetEntryCount())) {
        MlPatch_Add(ML_PATCH_ENTRANCE, NULL, index, sizeof(EntranceInfo));
    }
}

void MlVanillaNpcs_LeaveWorld(void); // ml_vanilla_npcs.c

static void MlPatch_OnQuestWorldChange(const char* world) {
    bool inMlWorld = (world != NULL) && (strcmp(world, ML_QUEST_KEY) == 0);
    s32 swapped = 0;
    char line[96];

    if (!sStarted || (inMlWorld == sInMlWorld)) {
        return;
    }
    sInMlWorld = inMlWorld;
    if (!inMlWorld) {
        MlVanillaNpcs_LeaveWorld();
    }
    for (s32 i = 0; i < sCount; i++) {
        MlPatch* patch = &sPatches[i];
        u8* at = MlPatch_Where(patch);

        if (!inMlWorld) {
            if ((patch->ml == NULL) && ((patch->ml = malloc(patch->size)) == NULL)) {
                continue;
            }
            memcpy(patch->ml, at, patch->size);
            memcpy(at, patch->game, patch->size);
            swapped++;
        } else if (patch->ml != NULL) {
            memcpy(at, patch->ml, patch->size);
            swapped++;
        }
    }
    snprintf(line, sizeof(line), "[MissingLink] %s: %d of ML's changes to the game's tables %s", world ? world : "the game's world",
             swapped, inMlWorld ? "back on" : "put back");
    sApi->Log(line);
}

void MlPatch_Start(void) {
    sStarted = true;
}

void MlPatch_Register(const SOHModApi* api) {
    sApi = api;
    SOH_REGISTER_HOOK(api, OnQuestWorldChange, MlPatch_OnQuestWorldChange);
}
