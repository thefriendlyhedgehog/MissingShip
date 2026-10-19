/*
 * The Missing Link's code patches around the game, as a code mod: its sound mute, no dispersed sparkles, its world-map
 * areas, its title screen's setup, and the refusal of vanilla's scenes it does not have.
 *
 * MissingShip's Unbound branch makes these inside Audio_PlaySfxGeneral (code_800F7260.c),
 * EffectSsKiraKira_SpawnDispersed (z_effect_soft_sprite_old_init.c), the misc-settings scene command
 * (z_scene_otr.cpp), Opening_SetupTitleScreen (z_opening.c) and Play_Init (z_play.c). A mod cannot hook inside most of
 * those, so each is undone or redone just after: the muted sounds are taken out of the request queue before the audio
 * update reads it, the sparkles are deleted before they are drawn, the area is remapped after the scene's commands,
 * and the title setup is applied when the next game state is set, before Play_Init reads it. The scene refusal is in
 * Play_Init itself: it asks VB_PLAY_TRANSITION_CS before it picks the scene's layer and spawns it, whichever way the
 * game got there (a transition, a file load, a warp, the debug map select).
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/unbound/SceneDB.h"

#include "missinglink.h"

ML_HOST_FN_DECL(func_809AABF0);

// --- sounds ---

typedef struct {
    /* 0x00 */ u16 sfxId;
    /* 0x04 */ Vec3f* pos;
    /* 0x08 */ u8 token;
    /* 0x0C */ f32* freqScale;
    /* 0x10 */ f32* vol;
    /* 0x14 */ s8* reverbAdd;
} MlSoundRequest; // code_800F7260.c's SoundRequest

extern HOST_DATA MlSoundRequest sSoundRequests[0x100];
extern HOST_DATA u8 sSoundRequestWriteIndex;
extern HOST_DATA u8 sSoundRequestReadIndex;

// ML has no Navi: its hook on Audio_PlaySfxGeneral (0x800F738C -> 0x80602D00) drops her sounds (fairy dash, vanish and
// flight, her "Hey!", "Listen!", "Watch out!" and call voices, the second greeting) and the Skull Kid laugh, which
// Navi's talk also plays. It compares the ID with bits 9-11 masked off (0xF1FF), so every variant of each is dropped.
static const u16 sMlMutedSfx[] = {
    NA_SE_EV_FAIRY_DASH, NA_SE_EV_NAVY_VANISH, NA_SE_EV_NAVY_FLY, NA_SE_VO_NAVY_ENEMY, NA_SE_VO_NAVY_HELLO,
    NA_SE_VO_NAVY_HEAR,  NA_SE_VO_NAVY_CALL,   NA_SE_VO_NA_HELLO_2, NA_SE_VO_SK_LAUGH,
};

// The frame's requests wait in the queue until the audio update, which runs after OnGameFrameUpdate. A request with
// ID 0 is skipped there.
static void MlWorld_DropMutedSfx(void) {
    for (u8 i = sSoundRequestReadIndex; i != sSoundRequestWriteIndex; i++) {
        for (u32 j = 0; j < ARRAY_COUNT(sMlMutedSfx); j++) {
            if ((sSoundRequests[i].sfxId & 0xF1FF) == (sMlMutedSfx[j] & 0xF1FF)) {
                sSoundRequests[i].sfxId = 0;
                break;
            }
        }
    }
}

// --- sparkles ---

extern HOST_DATA EffectSsInfo sEffectSsInfo;
void func_809AABF0(PlayState* play, u32 index, EffectSs* this); // Effect_Ss_KiraKira's update for dispersed sparkles

// ML turns dispersed sparkles off outright (jr $ra at EffectSsKiraKira_SpawnDispersed, 0x80028BB0). They are the only
// KiraKira effects that get this update. Deleted before the frame draws, they are never seen.
static void MlWorld_DeleteSparkles(void) {
    if (sEffectSsInfo.table == NULL) {
        return;
    }
    for (s32 i = 0; i < sEffectSsInfo.tableSize; i++) {
        if (ML_IS_HOST_FN(sEffectSsInfo.table[i].update, func_809AABF0)) {
            EffectSs_Delete(&sEffectSsInfo.table[i]);
        }
    }
}

// --- the world map ---

// ML runs the area a scene's misc settings give through a table (hook 0x800993E4 -> 0x80607D00, pairs at 0x80607CF0;
// the first match wins), so its scenes light up the regions of its redrawn world map: its Kokiri Forest's 4 becomes
// 9, Deku Road's 9 becomes 1, Deep Woods' 10 becomes 6, Monktown Valley's 6 becomes 10, and 1 becomes 4. 14 maps to
// itself.
static const u8 sMlAreaMap[][2] = { { 1, 4 }, { 4, 9 }, { 10, 6 }, { 14, 14 }, { 9, 1 }, { 6, 10 } };

static u32 sMlAreaData;   // the visited areas as the frame ended, so as they were before a new scene's commands
static u8 sMlSceneLoaded; // the scene's commands have run; AfterSceneCommands now fires for its rooms

static void MlWorld_AfterSceneCommands(int16_t sceneNum) {
    s32 area = gSaveContext.worldMapArea;
    s32 mapped = area;

    if (sMlSceneLoaded) {
        return;
    }
    for (u32 i = 0; i < ARRAY_COUNT(sMlAreaMap); i++) {
        if (area == sMlAreaMap[i][0]) {
            mapped = sMlAreaMap[i][1];
            break;
        }
    }
    if (mapped == area) {
        return;
    }
    gSaveContext.worldMapArea = mapped;
    // the command also marked the area visited (in the overworld, outside cutscenes): mark ML's instead
    if (((sceneNum >= SCENE_HYRULE_FIELD) && (sceneNum <= SCENE_OUTSIDE_GANONS_CASTLE)) ||
        ((sceneNum >= SCENE_MARKET_ENTRANCE_DAY) && (sceneNum <= SCENE_TEMPLE_OF_TIME_EXTERIOR_RUINS))) {
        if (gSaveContext.cutsceneIndex < 0xFFF0) {
            gSaveContext.worldMapAreaData = sMlAreaData | gBitFlags[mapped];
        }
    }
}

static void MlWorld_OnSceneInit(int16_t sceneNum) {
    sMlSceneLoaded = true;
}

static void MlWorld_OnPlayDestroy(void) {
    sMlSceneLoaded = false;
}

// --- the scene refusal ---

// ML has 25 of vanilla's scene slots; oot.o2r still holds vanilla's scenes for the others, so a stray entrance, the
// console or a cheat would show vanilla OoT. Refuse those: log it and land in Kokiri Forest with no cutscene or
// respawn point (MissingShip ledger row 119). Play_Init asks VB_PLAY_TRANSITION_CS (Cutscene_HandleConditionalTriggers)
// with the entrance, cutscene and time of day set, just before it picks the layer the same way as here and spawns the
// scene; it then picks the layer for Kokiri Forest. The conditional cutscenes it starts after asking (Requiem, Nocturne,
// Saria on the bridge, the light arrows, the castle collapse) each stay in their entrance's scene.
static void MlWorld_RefuseScene(bool* should, va_list args) {
    s32 layer;
    s16 sceneNum;

    MlEntrances_Apply(); // ML's entrances' rows lead into its scenes (the randomizer resets the table on leaving a game)
    if (gSaveContext.gameMode != GAMEMODE_NORMAL || gSaveContext.cutsceneIndex >= 0xFFF0) {
        layer = SCENE_LAYER_CUTSCENE_FIRST + (gSaveContext.cutsceneIndex & 0xF);
    } else if (!LINK_IS_ADULT) {
        layer = IS_DAY ? SCENE_LAYER_CHILD_DAY : SCENE_LAYER_CHILD_NIGHT;
    } else {
        layer = IS_DAY ? SCENE_LAYER_ADULT_DAY : SCENE_LAYER_ADULT_NIGHT;
    }
    if ((gSaveContext.entranceIndex < 0) || (gSaveContext.entranceIndex + layer >= EntranceDB_GetEntryCount())) {
        return; // Play_Init's own range check handles it
    }
    sceneNum = gEntranceTable[gSaveContext.entranceIndex + layer].scene;
    if (MlScenes_Exists(sceneNum)) {
        return;
    }
    LUSLOG_ERROR("[MissingLink] entrance %d (layer %d) leads to scene %#x, which The Missing Link doesn't have; loading "
                 "Kokiri Forest instead",
                 gSaveContext.entranceIndex, layer, sceneNum);
    gSaveContext.entranceIndex = ENTR_KOKIRI_FOREST_0;
    gSaveContext.cutsceneIndex = 0;
    gSaveContext.respawnFlag = 0; // a respawn point belongs to the refused scene
}

// --- the next game state ---

static void MlWorld_OnGameFrameUpdate(void) {
    MlWorld_DropMutedSfx();
    sMlAreaData = gSaveContext.worldMapAreaData;

    if (gPlayState != NULL) {
        return;
    }

    // Opening_SetupTitleScreen has just set up the title screen's play state. ML's title runs with child Link on its
    // spot00 cutscene: linkAge 1, cutsceneIndex 0xFFF0 (vanilla's 0 and 0xFFF3); ML's spot00 has no alternate
    // headers, so the layer ends up on its main header either way.
    if ((gSaveContext.gameMode == GAMEMODE_TITLE_SCREEN) && (gSaveContext.fileNum == 0xFF) &&
        (gSaveContext.cutsceneIndex == 0xFFF3)) {
        gSaveContext.linkAge = LINK_AGE_CHILD;
        gSaveContext.cutsceneIndex = 0xFFF0;
        gSaveContext.sceneLayer = 0;
    }
}

static void MlWorld_OnPlayDrawBegin(void) {
    MlWorld_DeleteSparkles();
}

void MlWorld_Register(const SOHModApi* api) {
    SOH_REGISTER_HOOK(api, OnGameFrameUpdate, MlWorld_OnGameFrameUpdate);
    SOH_REGISTER_HOOK(api, OnPlayDrawBegin, MlWorld_OnPlayDrawBegin);
    SOH_REGISTER_HOOK(api, AfterSceneCommands, MlWorld_AfterSceneCommands);
    SOH_REGISTER_HOOK(api, OnSceneInit, MlWorld_OnSceneInit);
    SOH_REGISTER_HOOK(api, OnPlayDestroy, MlWorld_OnPlayDestroy);
    api->RegisterVB(VB_PLAY_TRANSITION_CS, MlWorld_RefuseScene); // before ml_cutscenes.c's, which reads the entrance
    api->Log("[MissingLink] ML's sound mute, sparkles, world map, title setup and scene refusal");
}
