/*
 * The Missing Link's entrances, cutscene destinations, intros and continue points, as a code mod.
 *
 * ML adds 18 entrances of its own (event layers of its scenes, a grotto's second slot, a Temple of Time arrival), ends
 * 21 cutscenes somewhere other than vanilla does, and changes the flags of vanilla entrances. MissingShip's Unbound
 * branch registers those from unbound/scenes.json and reads the destinations from unbound/cutscenes.json in
 * Cutscene_Command_Terminator (z_demo.c, CutsceneRegistry.cpp). A stock Unbound reads neither: it refuses entries for
 * vanilla scenes, and its only C entrance calls are lookups.
 * - Entrances: the MissingShip builder puts each of ML's entrances on vanilla entrance rows a stock Unbound never
 *   reaches with ML (rows into scenes ML does not have, which ML's code mod refuses, that the game's code never names
 *   and no exit of ML's leads to) and ships them as missinglink/entrances; ML's exits to them are rewritten to those
 *   rows. This points the rows at ML's scene, spawn and transition flags, as the branch's table does. ML's flags on
 *   vanilla entrances (missinglink/entrancechanges) are set over the rows' own.
 * - Destinations: the terminator sets the cutscene state to CS_STATE_UNSKIPPABLE_EXEC when it runs the destination
 *   code, and nothing else sets it. At the end of that frame, after the code's case did what it does besides moving
 *   the player, the destination is replaced as the branch does; Play_Update starts the transition the next frame.
 *   SoH's CreditsFix runs the code of some credits cases, which ML reuses, a second time a few frames later (on an
 *   NTSC ROM, at the start frame and again after its delay); the replaced destination is put back then too.
 * - Intros: the branch replaces vanilla's arrival cutscene table (z_demo.c sEntranceCutsceneTable) on every entrance
 *   unbound/scenes.json sets `intro` on, with ML's ten and none on the vanilla ones ML removed. The builder ships
 *   them by entrance as missinglink/intros. ML's is set up after the scene's commands, just before the game's table
 *   runs, under the table's conditions and SoH's Skip Entrance Cutscenes (VB_PLAY_ENTRANCE_CS); a vanilla row for
 *   that entrance is then refused with its flag put back, and the arrival triggers keyed to it don't run.
 * - Continue points: where the game over's Continue resumes after Link arrived by an entrance (`continueAt` in
 *   missinglink/entrancechanges), set after vanilla's boss-room rule as the branch does.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/unbound/SceneDB.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include <libultraship/bridge/resourcebridge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "missinglink.h"
#include "ml_json.h"

#define ML_OTR_PREFIX "__OTR__"

// --- entrances ---

// A field of an entrance's row, if ML sets it
static u16 MlEntrances_Flag(u16 field, const MlJson* changes, const char* key, u16 mask, s32 shift) {
    const MlJson* value = MlJson_Get(changes, key);

    if (value == NULL) {
        return field;
    }
    return (field & ~mask) | ((MlJson_Int(value, 0) << shift) & mask);
}

void MlEntrances_Apply(void) {
    ML_JSON_FOR_EACH(entrance, MlJson_Load("entrances")) {
        s32 index = MlJson_Int(MlJson_Get(entrance, "index"), -1);
        s32 row = 0;

        ML_JSON_FOR_EACH(fields, MlJson_Get(entrance, "rows")) {
            if ((index < 0) || (index + row >= EntranceDB_GetEntryCount())) {
                break;
            }
            gEntranceTable[index + row].scene = MlJson_Int(MlJson_Get(fields, "scene"), 0);
            gEntranceTable[index + row].spawn = MlJson_Int(MlJson_Get(fields, "spawn"), 0);
            gEntranceTable[index + row].field = MlJson_Int(MlJson_Get(fields, "field"), 0);
            row++;
        }
    }
    // ML's flags on vanilla entrances (the fields it sets over the row's own): music carried over, title card,
    // transitions
    ML_JSON_FOR_EACH(changes, MlJson_Load("entrancechanges")) {
        s32 index = atoi(changes->key);

        if ((index < 0) || (index >= EntranceDB_GetEntryCount())) {
            continue;
        }
        gEntranceTable[index].field =
            MlEntrances_Flag(gEntranceTable[index].field, changes, "continueBgm", ENTRANCE_INFO_CONTINUE_BGM_FLAG, 15);
        gEntranceTable[index].field = MlEntrances_Flag(gEntranceTable[index].field, changes, "showTitleCard",
                                                       ENTRANCE_INFO_DISPLAY_TITLE_CARD_FLAG, 14);
        gEntranceTable[index].field =
            MlEntrances_Flag(gEntranceTable[index].field, changes, "endTransition", ENTRANCE_INFO_END_TRANS_TYPE_MASK,
                             ENTRANCE_INFO_END_TRANS_TYPE_SHIFT);
        gEntranceTable[index].field =
            MlEntrances_Flag(gEntranceTable[index].field, changes, "startTransition",
                             ENTRANCE_INFO_START_TRANS_TYPE_MASK, ENTRANCE_INFO_START_TRANS_TYPE_SHIFT);
    }
}

s32 MlEntrance_Index(const char* name) {
    s32 index = EntranceDB_RetrieveIndex(name);

    if (index < 0) {
        index = MlJson_Int(MlJson_Get(MlJson_Get(MlJson_Load("entrances"), name), "index"), -1);
    }
    return index;
}

// The rows an entrance has: an event entrance's are its own, a vanilla one's run on into the table
static s32 MlEntrance_Rows(s32 index) {
    ML_JSON_FOR_EACH(entrance, MlJson_Load("entrances")) {
        if (MlJson_Int(MlJson_Get(entrance, "index"), -1) == index) {
            return MlJson_Get(entrance, "rows")->count;
        }
    }
    return 0x7FFF;
}

// --- game-over continue points ---

// The game over's Continue (z_kaleido_scope_PAL.c, pause state 0x10) sends a boss room back to its dungeon's
// entrance; the branch then takes ML's `continueAt` for the entrance Link arrived by. VB_BE_ABLE_TO_SAVE is asked
// there once Continue is chosen, just before that switch: the arrival is noted then, and ML's point set at the end of
// the frame, frames before the respawn reads it.
static s32 sMlContinueFrom = -1;

static void MlContinue_AbleToSave(bool* should, va_list args) {
    if (*should && (gPlayState != NULL) && (gPlayState->pauseCtx.state == 0x10)) {
        sMlContinueFrom = gSaveContext.entranceIndex;
    }
}

static void MlContinue_Apply(void) {
    char key[12];
    s32 continueAt;

    if (sMlContinueFrom < 0) {
        return;
    }
    snprintf(key, sizeof(key), "%d", sMlContinueFrom);
    continueAt = MlJson_Int(MlJson_Get(MlJson_Get(MlJson_Load("entrancechanges"), key), "continueAt"), -1);
    if ((continueAt >= 0) && (continueAt < EntranceDB_GetEntryCount())) {
        LUSLOG_INFO("[MissingLink] game over after entrance %d: Continue resumes at %d", sMlContinueFrom, continueAt);
        gSaveContext.entranceIndex = continueAt;
    }
    sMlContinueFrom = -1;
}

// --- cutscene destinations ---

static u8 sMlCsState;
static u8 sMlTriggerBefore;

// The destination given to the cutscene playing, kept until the scene ends: SoH's CreditsFix runs the destination
// code of a credits case a second time, a few frames after the first, and ML reuses those cases
static struct {
    void* segment;
    s32 entrance;
    u16 cutsceneIndex;
    u8 transitionType;
} sMlRedirect;

// The destination of the cutscene playing, or NULL. An actor's or an entrance's cutscene is named by its asset string;
// a scene's own is its loaded data, the same pointer the resource cache gives for its path.
static const MlJson* MlCutscenes_Destination(void* segment) {
    const MlJson* cutscenes = MlJson_Load("cutscenes");

    if ((segment == NULL) || (cutscenes == NULL)) {
        return NULL;
    }
    if (strncmp((const char*)segment, ML_OTR_PREFIX, strlen(ML_OTR_PREFIX)) == 0) {
        return MlJson_Get(MlJson_Get(cutscenes, (const char*)segment + strlen(ML_OTR_PREFIX)), "destination");
    }
    ML_JSON_FOR_EACH(cutscene, cutscenes) {
        if (ResourceGetDataByName(cutscene->key) == segment) {
            return MlJson_Get(cutscene, "destination");
        }
    }
    return NULL;
}

static void MlCutscenes_Redirect(PlayState* play, const MlJson* dest) {
    s32 entrance = MlJson_Int(MlJson_Get(dest, "entrance"), -1);
    s32 setup = MlJson_Int(MlJson_Get(dest, "setup"), -1);
    s32 transition = MlJson_Int(MlJson_Get(dest, "transition"), -1);
    s32 time = MlJson_Int(MlJson_Get(dest, "time"), -1);
    s32 caseMoved = (play->transitionTrigger == TRANS_TRIGGER_START) && (sMlTriggerBefore != TRANS_TRIGGER_START);

    if ((entrance < 0) || (entrance >= EntranceDB_GetEntryCount())) {
        LUSLOG_ERROR("[MissingLink] cutscene destination %d is no entrance; the cutscene keeps vanilla's", entrance);
        return;
    }
    play->nextEntranceIndex = entrance;
    gSaveContext.cutsceneIndex = 0;
    if ((setup >= SCENE_LAYER_CUTSCENE_FIRST) && (setup < SCENE_LAYER_CUTSCENE_FIRST + 16) &&
        (setup < MlEntrance_Rows(entrance))) {
        gSaveContext.cutsceneIndex = 0xFFF0 + (setup - SCENE_LAYER_CUTSCENE_FIRST);
    }
    if ((time >= 0) && (time <= 0xFFFF)) {
        gSaveContext.dayTime = gSaveContext.skyboxTime = time;
    }
    play->transitionTrigger = TRANS_TRIGGER_START;
    if (transition >= 0) {
        play->transitionType = transition;
    } else if (!caseMoved) {
        play->transitionType = TRANS_TYPE_FADE_BLACK;
    }
    // under the black fill a new game starts in (the opening), Play_Update would drop the transition
    if ((play->transitionMode == TRANS_MODE_CS_BLACK_FILL_INIT) || (play->transitionMode == TRANS_MODE_CS_BLACK_FILL)) {
        play->transitionMode = TRANS_MODE_OFF;
        gSaveContext.cutsceneTransitionControl = 0;
    }
    sMlRedirect.segment = play->csCtx.segment;
    sMlRedirect.entrance = entrance;
    sMlRedirect.cutsceneIndex = gSaveContext.cutsceneIndex;
    sMlRedirect.transitionType = play->transitionType;
    LUSLOG_INFO("[MissingLink] cutscene ends at entrance %d, cutscene %#x", entrance, gSaveContext.cutsceneIndex);
}

// The destination code ran again (CreditsFix): it moved the player to vanilla's destination once more
static void MlCutscenes_Reapply(PlayState* play) {
    if ((sMlRedirect.segment == NULL) || (play->csCtx.segment != sMlRedirect.segment) ||
        ((play->nextEntranceIndex == sMlRedirect.entrance) &&
         (gSaveContext.cutsceneIndex == sMlRedirect.cutsceneIndex))) {
        return;
    }
    play->nextEntranceIndex = sMlRedirect.entrance;
    gSaveContext.cutsceneIndex = sMlRedirect.cutsceneIndex;
    play->transitionType = sMlRedirect.transitionType;
    LUSLOG_INFO("[MissingLink] cutscene ran its destination again; it still ends at entrance %d", sMlRedirect.entrance);
}

static void MlCutscenes_OnGameFrameUpdate(void) {
    PlayState* play = gPlayState;

    MlContinue_Apply();
    if (play == NULL) {
        sMlCsState = 0;
        return;
    }
    if ((play->csCtx.state == CS_STATE_UNSKIPPABLE_EXEC) && (sMlCsState != CS_STATE_UNSKIPPABLE_EXEC)) {
        const MlJson* dest = MlCutscenes_Destination(play->csCtx.segment);

        if (dest != NULL) {
            MlCutscenes_Redirect(play, dest);
        }
    } else {
        MlCutscenes_Reapply(play);
    }
    sMlCsState = play->csCtx.state;
    sMlTriggerBefore = play->transitionTrigger;
}

static void MlCutscenes_OnPlayDestroy(void) {
    sMlCsState = 0;
    sMlRedirect.segment = NULL;
}

// --- intros ---

static char sMlIntroSegment[256];
static u16 sMlEventChkInf[ARRAY_COUNT(gSaveContext.eventChkInf)];
static u8 sMlArrival; // from the scene's commands to the first frame, on an entrance ML set: vanilla's table runs then
static u8 sMlOwnIntro;
static u8 sMlIntroChecked; // a scene's commands can run twice as it loads (the pond's do)

// The entrance's entry in missinglink/intros: ML's intro (an object), false for a vanilla intro ML removed, or NULL
static const MlJson* MlIntros_Get(s32 entrance) {
    char key[12];

    snprintf(key, sizeof(key), "%d", entrance);
    return MlJson_Get(MlJson_Load("intros"), key);
}

// Cutscene_HandleEntranceTriggers' conditions, with ML's row
static void MlIntros_AfterSceneCommands(int16_t sceneNum) {
    PlayState* play = gPlayState;
    const MlJson* intro = MlIntros_Get(gSaveContext.entranceIndex);
    const char* age = MlJson_Str(MlJson_Get(intro, "age"), NULL);
    const char* cutscene = MlJson_Str(MlJson_Get(intro, "cutscene"), NULL);
    s32 flag = MlJson_Int(MlJson_Get(intro, "flag"), -1);

    if (sMlIntroChecked) {
        return;
    }
    sMlIntroChecked = true;
    sMlArrival = (intro != NULL);
    if ((play != NULL) && (intro != NULL) && (intro->type == ML_JSON_OBJECT) && (cutscene != NULL) && (flag >= 0) &&
        (!Flags_GetEventChkInf(flag) || MlJson_IsTrue(MlJson_Get(intro, "repeats"))) &&
        (gSaveContext.cutsceneIndex < 0xFFF0) &&
        ((age == NULL) || (gSaveContext.linkAge == ((strcmp(age, "child") == 0) ? LINK_AGE_CHILD : LINK_AGE_ADULT))) &&
        (gSaveContext.respawnFlag <= 0)) {
        Flags_SetEventChkInf(flag);
        sMlOwnIntro = true;
        if (GameInteractor_Should(VB_PLAY_ENTRANCE_CS, true, flag, (s32)gSaveContext.entranceIndex)) {
            snprintf(sMlIntroSegment, sizeof(sMlIntroSegment), "%s%s", ML_OTR_PREFIX, cutscene);
            Cutscene_SetSegment(play, sMlIntroSegment);
            gSaveContext.cutsceneTrigger = 2;
            gSaveContext.showTitleCard = false;
            LUSLOG_INFO("[MissingLink] intro %s at entrance %d", cutscene, gSaveContext.entranceIndex);
        }
        sMlOwnIntro = false;
    }
    memcpy(sMlEventChkInf, gSaveContext.eventChkInf, sizeof(sMlEventChkInf));
}

// Vanilla's row for an entrance ML set: not played, and its flag (set just before) put back as it was
static void MlIntros_EntranceCs(bool* should, va_list args) {
    va_list copy;
    s32 flag;

    if (!sMlArrival || sMlOwnIntro) {
        return;
    }
    va_copy(copy, args);
    flag = va_arg(copy, s32);
    va_end(copy);
    *should = false;
    gSaveContext.eventChkInf[flag >> 4] &= ~(1 << (flag & 0xF));
    gSaveContext.eventChkInf[flag >> 4] |= sMlEventChkInf[flag >> 4] & (1 << (flag & 0xF));
}

// The arrival triggers keyed to an entrance (Requiem, Nocturne, Saria on the bridge) don't run on one ML set; the
// others are keyed to scenes and never meet these entrances
static void MlIntros_TransitionCs(bool* should, va_list args) {
    s32 entrance = gSaveContext.entranceIndex;

    if (*should &&
        ((entrance == ENTR_DESERT_COLOSSUS_OUTSIDE_TEMPLE) || (entrance == ENTR_KAKARIKO_VILLAGE_FRONT_GATE) ||
         (entrance == ENTR_LOST_WOODS_BRIDGE_EAST_EXIT)) &&
        (MlIntros_Get(entrance) != NULL)) {
        *should = false;
    }
}

static void MlIntros_OnGameStateMainStart(void) {
    sMlArrival = false;
    sMlIntroChecked = false;
}

static void MlCutscenes_OnLoadGame(int32_t fileNum) {
    MlEntrances_Apply();
}

void MlCutscenes_Register(const SOHModApi* api) {
    MlEntrances_Apply();
    SOH_REGISTER_HOOK(api, OnGameFrameUpdate, MlCutscenes_OnGameFrameUpdate);
    SOH_REGISTER_HOOK(api, OnPlayDestroy, MlCutscenes_OnPlayDestroy);
    SOH_REGISTER_HOOK(api, OnLoadGame, MlCutscenes_OnLoadGame);
    SOH_REGISTER_HOOK(api, AfterSceneCommands, MlIntros_AfterSceneCommands);
    SOH_REGISTER_HOOK(api, OnGameStateMainStart, MlIntros_OnGameStateMainStart);
    api->RegisterVB(VB_PLAY_ENTRANCE_CS, MlIntros_EntranceCs);
    api->RegisterVB(VB_PLAY_TRANSITION_CS, MlIntros_TransitionCs);
    api->RegisterVB(VB_BE_ABLE_TO_SAVE, MlContinue_AbleToSave);
    api->Log("[MissingLink] ML's entrances, cutscene destinations, intros and continue points");
}
