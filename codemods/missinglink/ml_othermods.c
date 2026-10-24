/*
 * Other mods: ML is tested on its own (this mod and the layer its porter makes), and other mods can crash it (custom
 * music packs did in players' logs, as did skijer's MM masks). Each start logs the other mods mounted; when they change,
 * the first frame tells the player that ML may crash with them. The game's own archives hold none of these folders:
 *  - bin/<platform>/<mod>.<ext>: a code mod's binary (pack_mod.py), one name per mod;
 *  - custom/: custom music and fanfares (Ship of Harkinian's Audio Editor);
 *  - alt/: a texture or model pack.
 * Not in test homes, where the porter doesn't ask either (gEnhancements.MissingLink.PorterAsks 0).
 */
#include "missinglink.h"

#include "macros.h"

#include <libultraship/bridge/consolevariablebridge.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define OTHER_MODS_SEEN "gEnhancements.MissingLink.OtherModsSeen"   // not in the menu: the mods last told about
#define PORTER_ASKS "gEnhancements.MissingLink.PorterAsks"           // ml_porter.c
#define MAX_CODE_MODS 32

typedef struct {
    char codeMods[MAX_CODE_MODS][64];
    u32 codeModCount;
    u32 musicFiles;
    u32 altFiles;
} OtherMods;

// This mod, and mlplay (the playthrough harness, mod-sdk/mods/mlplay).
static const char* const sOurs[] = { "missinglink", "mlplay" };

static const SOHModApi* sApi;
static OtherMods sFound;
static bool sTold;

static void Log(const char* fmt, ...) {
    char line[768];
    va_list args;

    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    sApi->Log(line);
}

// bin/<platform>/<mod>.<ext>: <mod>, once for every platform's binary.
static bool AddCodeMod(const char* path, void* userData) {
    OtherMods* found = userData;
    const char* name = strrchr(path, '/');
    char mod[64];
    size_t len;

    name = (name != NULL) ? name + 1 : path;
    len = strcspn(name, ".");
    if ((len == 0) || (len >= sizeof(mod))) {
        return true;
    }
    memcpy(mod, name, len);
    mod[len] = '\0';
    for (u32 i = 0; i < ARRAY_COUNT(sOurs); i++) {
        if (strcmp(mod, sOurs[i]) == 0) {
            return true;
        }
    }
    for (u32 i = 0; i < found->codeModCount; i++) {
        if (strcmp(mod, found->codeMods[i]) == 0) {
            return true;
        }
    }
    if (found->codeModCount < MAX_CODE_MODS) {
        strcpy(found->codeMods[found->codeModCount++], mod);
    }
    return true;
}

static bool CountFile(const char* path, void* userData) {
    (*(u32*)userData)++;
    return true;
}

// Which mods these are, without the counts, so a song added to a pack doesn't tell the player again.
static void Signature(const OtherMods* found, char* out, size_t size) {
    size_t used = 0;

    out[0] = '\0';
    for (u32 i = 0; i < found->codeModCount; i++) {
        used += snprintf(out + used, (used < size) ? size - used : 0, "%s,", found->codeMods[i]);
    }
    if (found->musicFiles != 0) {
        used += snprintf(out + used, (used < size) ? size - used : 0, "custom/,");
    }
    if (found->altFiles != 0) {
        snprintf(out + used, (used < size) ? size - used : 0, "alt/,");
    }
}

// One line per kind of mod, each starting with `bullet`.
static void Describe(const OtherMods* found, const char* bullet, const char* end, char* out, size_t size) {
    size_t used = 0;

    out[0] = '\0';
    for (u32 i = 0; i < found->codeModCount; i++) {
        // childshield is ML's shield rule alone, for players without ML.
        used += snprintf(out + used, (used < size) ? size - used : 0, "%s%s (code mod%s)%s", bullet,
                         found->codeMods[i],
                         (strcmp(found->codeMods[i], "childshield") == 0) ? ", not needed: ML has its shield" : "",
                         end);
    }
    if (found->musicFiles != 0) {
        used += snprintf(out + used, (used < size) ? size - used : 0, "%scustom music (%u files)%s", bullet,
                         found->musicFiles, end);
    }
    if (found->altFiles != 0) {
        snprintf(out + used, (used < size) ? size - used : 0, "%sa texture or model pack (%u files)%s", bullet,
                 found->altFiles, end);
    }
}

static void MlOtherMods_OnGameFrameUpdate(void) {
    char seen[1024];
    char list[2048];
    char message[2560];

    if (sTold) {
        return;
    }
    sTold = true;
    Signature(&sFound, seen, sizeof(seen));
    if ((seen[0] == '\0') || (strcmp(seen, CVarGetString(OTHER_MODS_SEEN, "")) == 0)) {
        return;
    }
    if (!CVarGetInteger(PORTER_ASKS, 1)) {
        Log("[MissingLink] other mods: not telling (%s is 0)", PORTER_ASKS);
        return;
    }
    Describe(&sFound, "- ", "\n", list, sizeof(list));
    snprintf(message, sizeof(message),
             "The Missing Link is running with other mods:\n\n%s\n"
             "ML needs only missinglink.o2r and the MissingLink_Unbound.o2r it makes, and is tested only with those. "
             "Other mods, and packs made for vanilla Ocarina of Time's world, textures or music, may crash the game "
             "(custom music packs and MM masks have).\n\n"
             "If the game crashes, take the other mods out of the mods folder (or uninstall them in NEI-Store) and try "
             "again before reporting it.\n\n"
             "This shows again only when your mods change.",
             list);
    sApi->TellPlayer("Other mods", message);
    CVarSetString(OTHER_MODS_SEEN, seen);
    CVarSave();
    Log("[MissingLink] other mods: told the player");
}

void MlOtherMods_Register(const SOHModApi* api) {
    char list[2048];

    sApi = api;
    memset(&sFound, 0, sizeof(sFound));
    if (!SOH_MOD_API_HAS(api, ListArchiveFiles)) {
        return;
    }
    api->ListArchiveFiles("bin/*", AddCodeMod, &sFound);
    api->ListArchiveFiles("custom/*", CountFile, &sFound.musicFiles);
    api->ListArchiveFiles("alt/*", CountFile, &sFound.altFiles);
    Describe(&sFound, "", "; ", list, sizeof(list));
    if (strlen(list) >= 2) {
        list[strlen(list) - 2] = '\0';
    }
    Log("[MissingLink] other mods: %s", (list[0] != '\0') ? list : "none");
    if (list[0] == '\0') {
        // Told again if they come back.
        if (CVarGetString(OTHER_MODS_SEEN, "")[0] != '\0') {
            CVarSetString(OTHER_MODS_SEEN, "");
            CVarSave();
        }
    } else if (SOH_MOD_API_HAS(api, TellPlayer)) {
        SOH_REGISTER_HOOK(api, OnGameFrameUpdate, MlOtherMods_OnGameFrameUpdate);
    }
}
