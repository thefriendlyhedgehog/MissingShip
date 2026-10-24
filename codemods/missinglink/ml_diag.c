/*
 * Lines in the game's log for crash reports, most of which come from players running many mods: each piece of music
 * as it starts (with the Audio Editor's replacement and the sound font it plays with), each scene as it loads (with
 * the save's quest and Link's age), and each exit as it is taken (its entrance). A crash's stack names where it
 * happened; these say what was playing and where the player was going. Registered with or without ML's layer.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"

#include <stdarg.h>
#include <stdio.h>

#include "missinglink.h"

const char* AudioCollection_GetSequenceName(uint16_t seqId); // soh/Enhancements/audio/AudioCollection.h

static const SOHModApi* sApi;
static s16 sExitEntrance = -1; // the entrance of the exit last logged, until the transition ends

static void Log(const char* fmt, ...) {
    char line[512];
    va_list args;

    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    sApi->Log(line);
}

// AudioLoad_SyncInitSeqPlayer, on the audio thread: seqId is the one that plays, after the Audio Editor's replacement.
static void MlDiag_OnSeqPlayerInit(int32_t playerIdx, int32_t seqId) {
    static const char* const sPlayers[] = { "BGM", "fanfare", "SFX", "BGM sub" };
    const char* name;

    if ((playerIdx < 0) || (playerIdx >= ARRAY_COUNT(sPlayers)) || (playerIdx == SEQ_PLAYER_SFX)) {
        return;
    }
    name = AudioCollection_GetSequenceName((u16)seqId);
    Log("[MissingLink] music: %s plays sequence 0x%X (%s), sound font 0x%X", sPlayers[playerIdx], seqId,
        (name != NULL) ? name : "no name", gAudioContext.seqPlayers[playerIdx].defaultFont);
}

static void MlDiag_OnSceneInit(int16_t sceneNum) {
    Log("[MissingLink] scene 0x%X, entrance 0x%X: %s save, %s Link", sceneNum, gSaveContext.entranceIndex,
        IS_MASTER_QUEST ? "Master Quest" : "normal", LINK_IS_ADULT ? "adult" : "young");
}

// Each exit once, before the game reads its entrance (Play_Update's transition setup).
static void MlDiag_OnGameFrameUpdate(void) {
    PlayState* play = gPlayState;
    Player* player;

    if ((play == NULL) || (play->transitionTrigger == TRANS_TRIGGER_OFF)) {
        sExitEntrance = -1;
        return;
    }
    if ((play->transitionTrigger != TRANS_TRIGGER_START) || (play->nextEntranceIndex == sExitEntrance)) {
        return;
    }
    sExitEntrance = play->nextEntranceIndex;
    player = GET_PLAYER(play);
    Log("[MissingLink] exit: scene 0x%X room %d to entrance 0x%X, from (%.0f, %.0f, %.0f)", play->sceneNum,
        play->roomCtx.curRoom.num, (u16)play->nextEntranceIndex, (player != NULL) ? player->actor.world.pos.x : 0.0f,
        (player != NULL) ? player->actor.world.pos.y : 0.0f, (player != NULL) ? player->actor.world.pos.z : 0.0f);
}

void MlDiag_Register(const SOHModApi* api) {
    sApi = api;
    SOH_REGISTER_HOOK(api, OnSeqPlayerInit, MlDiag_OnSeqPlayerInit);
    SOH_REGISTER_HOOK(api, OnSceneInit, MlDiag_OnSceneInit);
    SOH_REGISTER_HOOK(api, OnGameFrameUpdate, MlDiag_OnGameFrameUpdate);
}
