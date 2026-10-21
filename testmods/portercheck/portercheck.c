/*
 * portercheck: a test mod, never shipped. It runs ML's porter (codemods/missinglink, ml_porter.c) without its
 * dialogs, through the service the code mod publishes, so a test can make the layer unattended on any OS:
 *
 *     PORTERCHECK="<layer to write>|<file>|<file>..."   the files as a player would pick them: the OoT USA 1.0 ROM
 *                                                       and Kaze's patch (zip or bps), or an ML ROM
 *
 * On the first frame it logs "[PORTERCHECK] wrote <path> (<n> bytes)" or "[PORTERCHECK] failed: <why>". Built by
 * .github/workflows/codemods.yml beside the code mods, for the porter's tests on each OS (DEVELOPMENT.md).
 */
#include "soh/ModApi/ModApi.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILES 8

typedef uint8_t* (*BuildFunc)(const uint8_t* const*, const uint64_t*, int32_t, uint64_t*, char*, uint32_t);
typedef void (*FreeFunc)(uint8_t*);

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };
static const SOHModApi* sApi;
static int sDone;

static void Log(const char* fmt, ...) {
    char line[1024];
    va_list args;

    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    sApi->Log(line);
}

static uint8_t* ReadWhole(const char* path, uint64_t* size) {
    FILE* f = fopen(path, "rb");
    uint8_t* data = NULL;
    long len;

    if (f == NULL) {
        return NULL;
    }
    if ((fseek(f, 0, SEEK_END) == 0) && ((len = ftell(f)) > 0) && (fseek(f, 0, SEEK_SET) == 0)) {
        data = malloc((size_t)len);
        if ((data != NULL) && (fread(data, 1, (size_t)len, f) != (size_t)len)) {
            free(data);
            data = NULL;
        }
        *size = (uint64_t)len;
    }
    fclose(f);
    return data;
}

static void Run(char* list) {
    BuildFunc build = (BuildFunc)sApi->FindService("missinglink.porter.build");
    FreeFunc release = (FreeFunc)sApi->FindService("missinglink.porter.free");
    const uint8_t* files[MAX_FILES];
    uint64_t sizes[MAX_FILES];
    int32_t count = 0;
    char error[512] = "";
    uint64_t outSize = 0;
    char* out = strtok(list, "|");
    char* path;
    uint8_t* layer;
    FILE* f;

    if ((build == NULL) || (release == NULL)) {
        Log("[PORTERCHECK] failed: the code mod publishes no porter");
        return;
    }
    while (((path = strtok(NULL, "|")) != NULL) && (count < MAX_FILES)) {
        uint8_t* data = ReadWhole(path, &sizes[count]);

        if (data == NULL) {
            Log("[PORTERCHECK] failed: can't read %s", path);
            for (int32_t i = 0; i < count; i++) {
                free((void*)files[i]);
            }
            return;
        }
        files[count++] = data;
    }
    layer = build(files, sizes, count, &outSize, error, sizeof(error));
    for (int32_t i = 0; i < count; i++) {
        free((void*)files[i]);
    }
    if (layer == NULL) {
        Log("[PORTERCHECK] failed: %s", error);
        return;
    }
    f = (out != NULL) ? fopen(out, "wb") : NULL;
    if ((f == NULL) || (fwrite(layer, 1, outSize, f) != outSize)) {
        Log("[PORTERCHECK] failed: can't write %s", (out != NULL) ? out : "(no path)");
    } else {
        Log("[PORTERCHECK] wrote %s (%llu bytes)", out, (unsigned long long)outSize);
    }
    if (f != NULL) {
        fclose(f);
    }
    release(layer);
}

static void PorterCheck_OnGameFrameUpdate(void) {
    const char* env;
    char* list;

    if (sDone) {
        return;
    }
    sDone = 1;
    env = getenv("PORTERCHECK");
    if ((env == NULL) || (env[0] == '\0')) {
        Log("[PORTERCHECK] idle: PORTERCHECK is not set");
        return;
    }
    list = malloc(strlen(env) + 1);
    if (list == NULL) {
        return;
    }
    memcpy(list, env, strlen(env) + 1);
    Run(list);
    free(list);
}

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    SOH_REGISTER_HOOK(sApi, OnGameFrameUpdate, PorterCheck_OnGameFrameUpdate);
}
