/*
 * What SoH's own windows call The Missing Link's items, quest items, events and scenes, ML's Time Splits list, the
 * counts its gameplay stats list and its countdown icon, as a code mod.
 *
 * MissingShip's Unbound branch reads these from unbound/labels.json (Labels.cpp, Unbound SPEC §7.7), and SoH's save
 * editor, gameplay stats and time splits ask it for them. A stock Unbound reads none of it, so this sets them from the
 * same document (the layer's missinglink/labels, ml_json.h) through the mod API's label entries, which work on every
 * platform:
 * - items and quest items (`SetLabel`), in English as the branch's;
 * - timestamps (`SetLabel`, kept by the host through the stats window's own setup): ML's events, and ML's items where
 *   the stats list them (the layer writes those out; on the branch an item's label names its listed timestamp);
 * - scenes (`SetLabel`), which the save editor and the stats name scenes by, from the names ML gives its scenes
 *   (MissingShip's branch reads them as `name` in unbound/scenes.json; the builder ships them by scene number in
 *   missinglink/scenes);
 * - time splits (`SetTimeSplits`, `SetTimeSplitGroup`): ML's list, in ML's order, with ML's names, icons and colours,
 *   and ML's groups;
 * - `countsShown`: the stats' Enemy, Ammo and Buttons details list a count ML doesn't name only once it isn't 0
 *   (VB_SHOW_GAMEPLAY_STATS_COUNT);
 * - the countdown timer's icon (`icons.countdownTimer`, the Kokiri Sword: ML has no Master Sword): SoH's Additional
 *   Timers window draws a countdown with the Master Sword's icon (TimeDisplay.cpp), so that icon is ML's while a
 *   countdown runs (`SetItemIcon`), and its own otherwise. Other windows that show the Master Sword's icon (the save
 *   editor, Plandomizer) show ML's for it during a countdown too.
 */
#include "soh/ModApi/ModApi.h"

#include "ml_json.h"

extern "C" {
#include "z64.h"
#include "variables.h"
}

#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>

extern "C" int ActorDB_RetrieveId(const char* name);

// The layer's split types (9.2.3's SplitType) as 9.3.0's: its item, upgrade, equipment and quest splits are all ITEM_*
// ids (9.3.0's item splits), its entrance splits SCENE_* ids (scene splits); a boss stays a boss
static const struct {
    const char* name;
    uint32_t type;
} sMlSplitTypes[] = { { "item", 0 }, { "upgrade", 0 }, { "equipment", 0 }, { "quest", 0 }, { "boss", 2 }, { "entrance", 1 } };

static const SOHModApi* sApi;
static std::set<int32_t> sCountsShown;
static int32_t sCountdownIcon = -1;
static bool sCountdownShown = false;

static const char* MlLabels_Eng(const MlJson* label) {
    return MlJson_Str(MlJson_Get(label, "eng"), MlJson_Str(MlJson_Get(label, "all"), nullptr));
}

static void MlLabels_SetNames(const MlJson* labels, SOHLabelKind kind) {
    ML_JSON_FOR_EACH(label, labels) {
        const char* name = MlLabels_Eng(label);

        if (name != nullptr) {
            sApi->SetLabel(kind, LANGUAGE_ENG, atoi(label->key), name);
        }
    }
}

static void MlLabels_SetTimeSplits(const MlJson* timeSplits) {
    std::vector<SOHTimeSplit> list;
    const MlJson* groups = MlJson_Get(timeSplits, "groups");

    ML_JSON_FOR_EACH(split, MlJson_Get(timeSplits, "splits")) {
        const char* type = MlJson_Str(MlJson_Get(split, "type"), "");
        const MlJson* id = MlJson_Get(split, "id");
        SOHTimeSplit object = {};
        int32_t typeIndex = -1;
        int32_t splitId;

        for (uint32_t t = 0; t < sizeof(sMlSplitTypes) / sizeof(sMlSplitTypes[0]); t++) {
            if (strcmp(type, sMlSplitTypes[t].name) == 0) {
                typeIndex = (int32_t)sMlSplitTypes[t].type;
            }
        }
        // a boss is its actor, by name
        splitId = (id != nullptr && id->type == ML_JSON_STRING) ? ActorDB_RetrieveId(id->str) : MlJson_Int(id, -1);
        if ((typeIndex < 0) || (splitId < 0)) {
            continue;
        }
        object.type = (uint32_t)typeIndex;
        object.id = (uint32_t)splitId;
        object.name = MlJson_Str(MlJson_Get(split, "label"), ""); // 9.3.0 draws the type and id's own image
        list.push_back(object);
    }
    if (!sApi->SetTimeSplits(list.data(), (uint32_t)list.size())) {
        return;
    }
    if (groups != nullptr) {
        sApi->ClearTimeSplitGroups();
        ML_JSON_FOR_EACH(group, groups) {
            std::vector<uint32_t> members;

            ML_JSON_FOR_EACH(member, group) {
                if (member->type == ML_JSON_NUMBER) {
                    members.push_back((uint32_t)member->num);
                }
            }
            sApi->SetTimeSplitGroup((uint32_t)atoi(group->key), members.data(), (uint32_t)members.size());
        }
    }
}

// A count ML doesn't name waits for a non-zero count (the branch's CountShown)
static void MlLabels_ShowCount(bool* should, va_list args) {
    va_list copy;
    int32_t count;

    va_copy(copy, args);
    count = va_arg(copy, int32_t);
    va_end(copy);
    if (*should && (count >= 0) && (count < COUNT_MAX) && (gSaveContext.ship.stats.count[count] == 0) &&
        (sCountsShown.count(count) == 0)) {
        *should = false;
    }
}

// After the game's update, before the frame's windows are drawn: the countdown states TimeDisplay.cpp draws the icon in
static void MlLabels_OnGameFrameUpdate(void) {
    bool countdown = (gPlayState != nullptr) && (gSaveContext.timerState >= TIMER_STATE_DOWN_PREVIEW);

    if (countdown != sCountdownShown) {
        sCountdownShown = countdown;
        sApi->SetItemIcon(ITEM_SWORD_MASTER, countdown ? (uint32_t)sCountdownIcon : ITEM_SWORD_MASTER);
    }
}

extern "C" void MlLabels_Register(const SOHModApi* api) {
    const MlJson* doc = MlJson_Load("labels");
    const MlJson* counts = MlJson_Get(doc, "countsShown");

    if (!SOH_MOD_API_HAS(api, SetItemIcon)) {
        api->Log("[MissingLink] ML's names in SoH's windows: this Unbound has no label entries in its mod API");
        return;
    }
    sApi = api;
    MlLabels_SetNames(MlJson_Get(doc, "items"), SOH_LABEL_ITEM);
    MlLabels_SetNames(MlJson_Get(doc, "questItems"), SOH_LABEL_QUEST_ITEM);
    MlLabels_SetNames(MlJson_Get(doc, "timestamps"), SOH_LABEL_TIMESTAMP);
    ML_JSON_FOR_EACH(scene, MlJson_Load("scenes")) {
        const char* name = MlJson_Str(MlJson_Get(scene, "name"), nullptr);

        if (name != nullptr) {
            api->SetLabel(SOH_LABEL_SCENE, LANGUAGE_ENG, atoi(scene->key), name);
        }
    }
    MlLabels_SetTimeSplits(MlJson_Get(doc, "timeSplits"));

    if (counts != nullptr) {
        ML_JSON_FOR_EACH(count, counts) {
            sCountsShown.insert(MlJson_Int(count, -1));
        }
        api->RegisterVB(VB_SHOW_GAMEPLAY_STATS_COUNT, MlLabels_ShowCount);
    }

    sCountdownIcon = MlJson_Int(MlJson_Get(MlJson_Get(doc, "icons"), "countdownTimerItem"), -1);
    if ((sCountdownIcon >= 0) && (sCountdownIcon != ITEM_SWORD_MASTER)) {
        SOH_REGISTER_HOOK(api, OnGameFrameUpdate, MlLabels_OnGameFrameUpdate);
    }
    api->Log("[MissingLink] ML's names in SoH's windows (items, events, scenes), its Time Splits list, the counts its "
             "stats list and its countdown icon");
}
