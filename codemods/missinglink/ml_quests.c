/*
 * Quests: ML's world replaces the game's for every file, and ML plays only Original Quest. Randomizer, Master Quest,
 * Boss Rush and the speedrun quests read vanilla's world, items and checks, which ML's layer has replaced, so on the
 * file select:
 *  - a new file skips the quest choice and is Original Quest, as the game does when only one quest is shown;
 *  - a file of another quest doesn't open: the error sound plays and the player is told why.
 * Only while ML's layer is mounted (registered after the porter); the porter alone leaves the file select as it is.
 *
 * A host with mod quests (RegisterQuest) does both itself: ML is "The Missing Link" on the quest menu, the game's own
 * quests are hidden, and files of other quests are greyed out. ML only tells the player why such a file doesn't open.
 * Files made before (Original Quest, no mod quest) open as ML's.
 */
#include "missinglink.h"

#include "overlays/gamestates/ovl_file_choose/file_choose.h"
#include "soh/SaveManager.h"

#include <libultraship/bridge/consolevariablebridge.h>
#include <stdarg.h>
#include <stdio.h>

#define PORTER_ASKS "gEnhancements.MissingLink.PorterAsks" // ml_porter.c

static const SOHModApi* sApi;

#ifdef SOH_MOD_QUEST_REPLACES_WORLD
static const SOHModQuest sQuest = { sizeof(SOHModQuest), "missinglink", "The Missing Link", "ML",
                                    SOH_MOD_QUEST_REPLACES_WORLD | SOH_MOD_QUEST_ADOPTS_ORIGINAL_FILES };
static bool sHostHasQuests;
#endif

static void Log(const char* fmt, ...) {
    char line[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    sApi->Log(line);
}

static const char* QuestName(u8 quest) {
    switch (quest) {
        case QUEST_MASTER:
            return "Master Quest";
        case QUEST_RANDOMIZER:
            return "Randomizer";
        case QUEST_BOSSRUSH:
            return "Boss Rush";
        case QUEST_SPEEDRUN:
            return "Speedrun";
        case QUEST_SPEEDRUN_MASTER:
            return "Speedrun Master Quest";
        default:
            return "another quest";
    }
}

// Every way into a quest's own menus, which only a new file reaches: the quest choice, and each quest's settings when
// it is the only one shown.
static bool IsQuestMenu(s16 configMode) {
    switch (configMode) {
        case CM_START_QUEST_MENU:
        case CM_QUEST_MENU:
        case CM_ROTATE_TO_BOSS_RUSH_MENU:
        case CM_START_BOSS_RUSH_MENU:
        case CM_BOSS_RUSH_MENU:
        case CM_ROTATE_TO_SETTINGS_MENU:
        case CM_START_SETTINGS_MENU:
        case CM_SETTINGS_MENU:
            return true;
        default:
            return false;
    }
}

static void RefuseFile(s16 file, u8 quest, bool playError) {
    char message[512];

    Log("[MissingLink] quests: file %d is %s, not opened", file + 1, QuestName(quest));
    if (playError) {
        Audio_PlaySfxGeneral(NA_SE_SY_FSEL_ERROR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }
    if (!CVarGetInteger(PORTER_ASKS, 1) || !SOH_MOD_API_HAS(sApi, TellPlayer)) {
        return;
    }
    snprintf(message, sizeof(message),
             "File %d is a %s file. The Missing Link plays only its own files, which are Original Quest: ML's world "
             "replaces the game's, and %s needs the game's.\n\n"
             "Start a new file for ML, or take missinglink.o2r and MissingLink_Unbound.o2r out of the mods folder "
             "(or uninstall ML in NEI-Store) to play this one.",
             file + 1, QuestName(quest), QuestName(quest));
    sApi->TellPlayer("Not an ML file", message);
}

// At the top of FileChoose_Main, before the menus update and draw.
static void MlQuests_OnFileChooseMain(void* gameState) {
    FileChooseContext* this = gameState;
    Input* input = &this->state.input[0];
    SaveFileMetaInfo* meta;

    if (this->menuMode != FS_MENU_MODE_CONFIG) {
        return;
    }
#ifdef SOH_MOD_QUEST_REPLACES_WORLD
    // The host greys the file out and plays the error sound; ML says why. Another mod's quest isn't ML's to explain.
    if (sHostHasQuests) {
        if ((this->configMode == CM_MAIN_MENU) && (this->buttonIndex <= FS_BTN_MAIN_FILE_3) &&
            CHECK_BTN_ANY(input->press.button, BTN_A | BTN_START)) {
            meta = Save_GetSaveMetaInfo(this->buttonIndex);
            if (meta->valid && (meta->modQuest[0] == '\0') && (meta->quest != QUEST_NORMAL)) {
                RefuseFile(this->buttonIndex, meta->quest, false);
            }
        }
        return;
    }
#endif
    if (IsQuestMenu(this->configMode) && (this->buttonIndex <= FS_BTN_MAIN_FILE_3)) {
        // As FileChoose_RotateToQuest does when only one quest is shown.
        Log("[MissingLink] quests: file %d starts as Original Quest", this->buttonIndex + 1);
        this->questType[this->buttonIndex] = QUEST_NORMAL;
        gSaveContext.ship.quest.id = QUEST_NORMAL;
        this->windowRot = 628.0f;
        this->logoAlpha = 0;
        this->configMode = CM_START_NAME_ENTRY;
        this->prevConfigMode = CM_MAIN_MENU;
        return;
    }
    if ((this->configMode != CM_MAIN_MENU) || (this->buttonIndex > FS_BTN_MAIN_FILE_3) ||
        !CHECK_BTN_ANY(input->press.button, BTN_A | BTN_START)) {
        return;
    }
    meta = Save_GetSaveMetaInfo(this->buttonIndex);
    if (!meta->valid || (meta->quest == QUEST_NORMAL)) {
        return;
    }
    input->press.button &= ~(BTN_A | BTN_START);
    RefuseFile(this->buttonIndex, meta->quest, true);
}

void MlQuests_Register(const SOHModApi* api) {
    sApi = api;
#ifdef SOH_MOD_QUEST_REPLACES_WORLD
    sHostHasQuests = SOH_MOD_API_HAS(api, RegisterQuest) && api->RegisterQuest(&sQuest);
    Log("[MissingLink] quests: %s", sHostHasQuests ? "ML is a mod quest" : "the host has no mod quests");
#endif
    SOH_REGISTER_HOOK(api, OnFileChooseMain, MlQuests_OnFileChooseMain);
}
