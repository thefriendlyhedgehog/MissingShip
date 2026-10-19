/*
 * The Missing Link's changes to vanilla scenes, as a code mod: draw configs, save return points and item
 * restrictions.
 *
 * MissingShip's Unbound branch reads these as `drawConfig`, `saveReturn` and `restrictions` in unbound/scenes.json;
 * stock Unbound refuses entries for vanilla scenes. The builder's --stock layer ships them by scene number as
 * missinglink/scenes (ML's scene names are in the same document; ml_labels.cpp sets them).
 * - Draw configs: ML points 25 of its scenes at another scene's draw handler (ML's scene table; MissingShip
 *   pipeline/ml_scene_map.json). Its room models rely on what that handler sets: Deku Tree's room 0 draws through
 *   segment 8, which only SDC_GROTTOS sets there. With vanilla's handler the segment is left pointing at whatever was
 *   there before, so the game draws from it and crashes on arrival (seen in the Deku Tree, the Forest Temple, Desert
 *   Colossus, Kakariko and Bongo Bongo's room). This sets ML's config once the scene is in, before its first frame is
 *   drawn.
 * - Save return points: Sram_OpenSave picks where a loaded save starts from the scene it was saved in, and the branch
 *   then takes ML's for that scene. FileChoose_LoadGame runs OnLoadGame shortly after Sram_OpenSave, so it is set there
 *   (the warps run it too, after setting their own entrance; they are told apart by the respawn point the file
 *   select sets). For a scene Sram_OpenSave's switch does not name (every scene outside the dungeons), ML nops
 *   vanilla's default case (Sram_OpenSave +0x148..+0x184): the save resumes at the entrance it was written at.
 *   Vanilla's sends a child to Link's house, which in ML is the prologue's stub, so a save made in Kokiri Forest
 *   would replay the prologue. OnLoadFile runs inside Sram_OpenSave (Save_LoadFile), before its switch, so the
 *   saved entrance is kept there and put back when vanilla's default replaced it.
 * - Restrictions: Interface_SetSceneRestrictions, at the end of Play_Init, sets which items a scene allows from
 *   vanilla's table; the branch reads ML's row for the scene instead, a field ML does not set being 0. This sets ML's
 *   on the first frame after the scene's init, before anything reads them.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/unbound/SceneDB.h"

#include <stdio.h>

#include "missinglink.h"
#include "ml_json.h"

static u8 sMlRestrictionsPending;

static const MlJson* MlScenes_Get(s32 sceneNum) {
    char key[12];

    snprintf(key, sizeof(key), "%d", sceneNum);
    return MlJson_Get(MlJson_Load("scenes"), key);
}

static void MlScenes_OnSceneInit(int16_t sceneNum) {
    s32 config = MlJson_Int(MlJson_Get(MlScenes_Get(sceneNum), "drawConfig"), -1);

    if (gPlayState == NULL) {
        return;
    }
    if (config >= 0) {
        gPlayState->sceneConfig = config;
    }
    sMlRestrictionsPending = true;
}

// Whether a scene is one of ML's: the 25 slots it has a draw config for, or a custom scene. oot.o2r still holds
// vanilla's scenes for the others.
s32 MlScenes_Exists(s16 sceneNum) {
    return (MlJson_Get(MlScenes_Get(sceneNum), "drawConfig") != NULL) || SceneDB_IsCustom(sceneNum);
}

static s32 sMlSavedEntrance = -1;

// The entrance the save was written at, before Sram_OpenSave's switch
static void MlScenes_OnLoadFile(int32_t fileNum) {
    sMlSavedEntrance = gSaveContext.entranceIndex;
}

// OnLoadGame also runs for the debug warps and the map select's debug save; only FileChoose_LoadGame marks the first
// respawn point as the opening, as Sram_OpenSave returns
static void MlScenes_OnLoadGame(int32_t fileNum) {
    s32 entrance = MlJson_Int(MlJson_Get(MlScenes_Get(gSaveContext.savedSceneNum), "saveReturn"), -1);
    s32 saved = sMlSavedEntrance;

    sMlSavedEntrance = -1;
    if (gSaveContext.respawn[RESPAWN_MODE_DOWN].entranceIndex != ENTR_LOAD_OPENING) {
        return;
    }
    if (entrance >= 0) {
        gSaveContext.entranceIndex = entrance;
    } else if ((saved >= 0) && ((gSaveContext.entranceIndex == ENTR_LINKS_HOUSE_CHILD_SPAWN) ||
                                (gSaveContext.entranceIndex == ENTR_TEMPLE_OF_TIME_WARP_PAD))) {
        // vanilla's default case ran (no dungeon case gives these): ML keeps the saved entrance
        gSaveContext.entranceIndex = saved;
    }
}

// interfaceCtx.restrictions, in z_parameter.c's order (SPEC.md §7.1 `restrictions`)
static void MlScenes_OnGameStateMainStart(void) {
    PlayState* play = gPlayState;
    const MlJson* fields;
    InterfaceContext* interfaceCtx;

    if (!sMlRestrictionsPending || (play == NULL) || !play->state.running) {
        return;
    }
    sMlRestrictionsPending = false;
    fields = MlJson_Get(MlScenes_Get(play->sceneNum), "restrictions");
    if (fields == NULL) {
        return;
    }
    interfaceCtx = &play->interfaceCtx;
    interfaceCtx->restrictions.hGauge = MlJson_Int(MlJson_Get(fields, "hGauge"), 0);
    interfaceCtx->restrictions.bButton = MlJson_Int(MlJson_Get(fields, "bButton"), 0);
    interfaceCtx->restrictions.aButton = MlJson_Int(MlJson_Get(fields, "aButton"), 0);
    interfaceCtx->restrictions.bottles = MlJson_Int(MlJson_Get(fields, "bottles"), 0);
    interfaceCtx->restrictions.tradeItems = MlJson_Int(MlJson_Get(fields, "tradeItems"), 0);
    interfaceCtx->restrictions.hookshot = MlJson_Int(MlJson_Get(fields, "hookshot"), 0);
    interfaceCtx->restrictions.ocarina = MlJson_Int(MlJson_Get(fields, "ocarina"), 0);
    interfaceCtx->restrictions.warpSongs = MlJson_Int(MlJson_Get(fields, "warpSongs"), 0);
    interfaceCtx->restrictions.sunsSong = MlJson_Int(MlJson_Get(fields, "sunsSong"), 0);
    interfaceCtx->restrictions.farores = MlJson_Int(MlJson_Get(fields, "farores"), 0);
    interfaceCtx->restrictions.dinsNayrus = MlJson_Int(MlJson_Get(fields, "dinsNayrus"), 0);
    interfaceCtx->restrictions.all = MlJson_Int(MlJson_Get(fields, "all"), 0);
}

void MlScenes_Register(const SOHModApi* api) {
    SOH_REGISTER_HOOK(api, OnSceneInit, MlScenes_OnSceneInit);
    SOH_REGISTER_HOOK(api, OnLoadFile, MlScenes_OnLoadFile);
    SOH_REGISTER_HOOK(api, OnLoadGame, MlScenes_OnLoadGame);
    SOH_REGISTER_HOOK(api, OnGameStateMainStart, MlScenes_OnGameStateMainStart);
}
