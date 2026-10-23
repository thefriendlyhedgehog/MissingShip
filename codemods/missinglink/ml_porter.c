/*
 * The porter: ML's layer, MissingLink_Unbound.o2r, made in the game (see ml_porter.h).
 *
 * The mod carries the layer as a pack of deltas (tools/deltapack.py, assets/missinglink/porter/layer.pack): ML's own
 * structure, plus copies from the player's oot.o2r and Unbound's conversion of it, oot-unbound.o2r (both mounted), and
 * from the player's ML ROM, which the porter makes from their OoT USA 1.0 ROM and Kaze's patch (or takes as it is).
 * So the mod holds none of Nintendo's data and none of ML's art: the player still downloads ML from Kaze's page.
 *
 * On the first frame without the pack's layer the porter asks to make it, asks for the files, rebuilds every file of
 * the layer (each checked by CRC-64 against the one the pack was made from), saves it in the mods folder and asks to
 * restart. The game reads a file at a path from the top archive, so a mounted layer hides the game's files under it,
 * which the pack copies from. The layer therefore keeps the game's copy of each file it covers (PORTER_GAME), and an
 * update reads them from there: it makes the new layer over the old one, which SaveToModsFolder installs at the next
 * start. Over an older layer without those copies it first replaces that with an empty one and restarts. It writes
 * nothing else, so there is nothing to clean up.
 *
 * missinglink.porter.build and missinglink.porter.free (services) do the same without dialogs, for tests.
 */
#include "ml_porter.h"
#include "ml_porter_rom.h"
#include "ml_porter_zip.h"

#include <libultraship/bridge/consolevariablebridge.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORTER_PACK "missinglink/porter/layer.pack"   // in this mod's archive
#define PORTER_MARK "missinglink/layer.json"          // in the layer: which layer it is (tools/unbound_layer.py)
#define PORTER_OLD "missinglink/scenes"               // in every layer, also those made before the mark
#define PORTER_LAYER "MissingLink_Unbound.o2r"
#define PORTER_LIST "deltas.json"                     // the pack's index, not a file of the layer
#define PORTER_CLEARED "missinglink/cleared.txt"      // the empty layer an update leaves for one start
#define PORTER_GAME "missinglink/game/"               // in the layer: the game's copy of each file it covers
#define PORTER_FILES "missinglink/layer-files.txt"    // in the layer: its files, one per line
#define PATCH_PAGE "https://www.romhacking.net/hacks/5334/"
#define PORTER_ASKS "gEnhancements.MissingLink.PorterAsks"   // not in the menu
#define OOT_VERSION "version"   // in oot.o2r (and oot-unbound.o2r): a count, then each ROM's header CRC, big-endian

typedef enum {
    LAYER_NONE,     // no layer mounted
    LAYER_OLD,      // a layer, not the one this mod's pack makes
    LAYER_READY,    // the pack's layer
} LayerState;

static const SOHModApi* sApi;
static LayerState sState;
static bool sOffered;
static bool sCleared;   // the mounted layer is the empty one an update leaves (PORTER_CLEARED)

static void Log(const char* fmt, ...) {
    char line[768];
    va_list args;

    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    sApi->Log(line);
}

// --- CRC-64: libultraship's update_crc64, from all ones (deltapack.py crc64) ---

static uint64_t sCrc64Table[256];
static bool sCrc64Ready;

static uint64_t Crc64Update(uint64_t crc, const uint8_t* data, size_t size) {
    if (!sCrc64Ready) {
        for (uint32_t i = 0; i < 256; i++) {
            uint64_t c = (uint64_t)i << 56;

            for (int32_t k = 0; k < 8; k++) {
                c = (c & 0x8000000000000000ull) ? ((c << 1) ^ 0x42F0E1EBA9EA3693ull) : (c << 1);
            }
            sCrc64Table[i] = c;
        }
        sCrc64Ready = true;
    }
    for (size_t i = 0; i < size; i++) {
        crc = sCrc64Table[((crc >> 56) ^ data[i]) & 0xFF] ^ (crc << 8);
    }
    return crc;
}

// --- the mounted archives ---

typedef struct {
    uint8_t* data;
    uint64_t size;
    bool found;
} MountedFile;

static bool CopyMounted(const char* path, const uint8_t* data, uint64_t size, void* userData) {
    MountedFile* file = userData;

    (void)path;
    file->data = malloc(size > 0 ? size : 1);
    if (file->data != NULL) {
        memcpy(file->data, data, size);
        file->size = size;
        file->found = true;
    }
    return false;
}

// The top archive's file at exactly `path` (malloc'd), or NULL. The mask is a glob, so a path with glob characters
// can't be named exactly; deltapack.py makes no pack that needs one.
static uint8_t* ReadMounted(const char* path, uint64_t* size) {
    MountedFile file = { NULL, 0, false };

    if (strpbrk(path, "*?[\\") != NULL) {
        return NULL;
    }
    sApi->ReadArchiveFiles(path, CopyMounted, &file);
    *size = file.size;
    return file.found ? file.data : NULL;
}

// A sorted list of paths, for lookups.
typedef struct {
    char* text;
    char** paths;
    size_t count;
} FileList;

static int ComparePaths(const void* a, const void* b) {
    return strcmp(*(char* const*)a, *(char* const*)b);
}

// From one path per line; the list keeps a copy.
static void FileList_FromText(FileList* list, const uint8_t* data, size_t size) {
    size_t lines = 0;

    memset(list, 0, sizeof(*list));
    list->text = malloc(size + 1);
    if (list->text == NULL) {
        return;
    }
    memcpy(list->text, data, size);
    list->text[size] = '\0';
    for (size_t i = 0; i < size; i++) {
        lines += (data[i] == '\n');
    }
    list->paths = malloc((lines + 1) * sizeof(char*));
    if (list->paths == NULL) {
        return;
    }
    for (char* line = strtok(list->text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        list->paths[list->count++] = line;
    }
    qsort(list->paths, list->count, sizeof(char*), ComparePaths);
}

static bool FileList_Has(const FileList* list, const char* path) {
    return (list->count > 0) && (bsearch(&path, list->paths, list->count, sizeof(char*), ComparePaths) != NULL);
}

static void FileList_Free(FileList* list) {
    free(list->paths);
    free(list->text);
    memset(list, 0, sizeof(*list));
}

typedef struct {
    char* text;
    size_t size;
    bool failed;
} PathText;

static bool AddPathLine(const char* path, void* userData) {
    PathText* t = userData;
    size_t len = strlen(path);
    char* grown = realloc(t->text, t->size + len + 1);

    if (grown == NULL) {
        t->failed = true;
        return false;
    }
    t->text = grown;
    memcpy(t->text + t->size, path, len);
    t->text[t->size + len] = '\n';
    t->size += len + 1;
    return true;
}

// Every path the mounted archives hold, listed once while a layer is made, so lookups skip the paths nothing holds
// (each lookup searches every archive). Unlisted (an older host, or no memory): every path may be held.
static FileList sMounted;
static bool sMountedListed;

static void Mounted_List(void) {
    PathText t = { NULL, 0, false };

    sMountedListed = false;
    if (!SOH_MOD_API_HAS(sApi, ListArchiveFiles)) {
        return;
    }
    sApi->ListArchiveFiles("*", AddPathLine, &t);
    if (!t.failed && (t.text != NULL)) {
        FileList_FromText(&sMounted, (const uint8_t*)t.text, t.size);
        sMountedListed = (sMounted.paths != NULL);
    }
    free(t.text);
}

static void Mounted_Free(void) {
    FileList_Free(&sMounted);
    sMountedListed = false;
}

static bool MayBeMounted(const char* path) {
    return !sMountedListed || FileList_Has(&sMounted, path);
}

// The mounted layer's files (PORTER_FILES); empty without that list.
static void FileList_Load(FileList* list) {
    uint64_t size = 0;
    uint8_t* data = MayBeMounted(PORTER_FILES) ? ReadMounted(PORTER_FILES, &size) : NULL;

    memset(list, 0, sizeof(*list));
    if (data != NULL) {
        FileList_FromText(list, data, size);
        free(data);
    }
}

// The game's own file at `path` (malloc'd), or NULL. Under a layer the porter made, from that layer's copy of it.
static uint8_t* ReadGame(const char* path, uint64_t* size) {
    char kept[512];
    uint8_t* data = NULL;

    if ((snprintf(kept, sizeof(kept), "%s%s", PORTER_GAME, path) < (int)sizeof(kept)) && MayBeMounted(kept)) {
        data = ReadMounted(kept, size);
    }
    if ((data == NULL) && MayBeMounted(path)) {
        data = ReadMounted(path, size);
    }
    return data;
}

static bool IsMounted(const char* path) {
    uint64_t size;
    uint8_t* data = ReadMounted(path, &size);

    free(data);
    return data != NULL;
}

// Mounted files the pack copies from, read once each.
#define CACHE_BUCKETS 4096

typedef struct CacheEntry {
    struct CacheEntry* next;
    uint8_t* data;      // NULL: nothing is mounted at the path
    uint64_t size;
    char path[];
} CacheEntry;

typedef struct {
    CacheEntry* buckets[CACHE_BUCKETS];
} Cache;

static uint32_t HashPath(const char* path) {
    uint32_t h = 2166136261u;

    for (; *path != '\0'; path++) {
        h = (h ^ (uint8_t)*path) * 16777619u;
    }
    return h % CACHE_BUCKETS;
}

static const CacheEntry* CacheGet(Cache* cache, const char* path) {
    uint32_t b = HashPath(path);
    CacheEntry* e;

    for (e = cache->buckets[b]; e != NULL; e = e->next) {
        if (strcmp(e->path, path) == 0) {
            return e;
        }
    }
    e = malloc(sizeof(CacheEntry) + strlen(path) + 1);
    if (e == NULL) {
        return NULL;
    }
    strcpy(e->path, path);
    e->data = ReadGame(path, &e->size);
    e->next = cache->buckets[b];
    cache->buckets[b] = e;
    return e;
}

static void CacheFree(Cache* cache) {
    for (int32_t b = 0; b < CACHE_BUCKETS; b++) {
        CacheEntry* e = cache->buckets[b];

        while (e != NULL) {
            CacheEntry* next = e->next;

            free(e->data);
            free(e);
            e = next;
        }
        cache->buckets[b] = NULL;
    }
}

// --- rebuilding the pack's files (deltapack.py rebuild) ---

typedef struct {
    MlRomFiles rom;
    Cache cache;
    char* error;
    size_t errorSize;
} Porter;

static void Fail(Porter* p, const char* fmt, const char* a) {
    if (p->error[0] == '\0') {
        snprintf(p->error, p->errorSize, fmt, a);
    }
}

static uint32_t Le32(const uint8_t* q) {
    return (uint32_t)q[0] | ((uint32_t)q[1] << 8) | ((uint32_t)q[2] << 16) | ((uint32_t)q[3] << 24);
}

static uint64_t Le64(const uint8_t* q) {
    return (uint64_t)Le32(q) | ((uint64_t)Le32(q + 4) << 32);
}

static bool SourceAt(Porter* p, const char* path, const uint8_t** data, uint64_t* size) {
    if (strncmp(path, ML_ROM_PREFIX, strlen(ML_ROM_PREFIX)) == 0) {
        const MlRomFile* view = MlRomFiles_View(&p->rom, path);

        if (view == NULL) {
            return false;
        }
        *data = view->data;
        *size = view->size;
        return true;
    }
    const CacheEntry* e = CacheGet(&p->cache, path);
    if ((e == NULL) || (e->data == NULL)) {
        return false;
    }
    *data = e->data;
    *size = e->size;
    return true;
}

// The CRC-64 of what the ops copy from source s of `src`, piece after piece; false if a copy reaches past its end.
static bool CopiedCrc(const uint8_t* ops, size_t opsSize, uint32_t s, const uint8_t* src, uint64_t srcSize,
                      uint64_t* crc) {
    uint64_t c = ~0ull;

    for (size_t q = 0; q < opsSize;) {
        if (ops[q] == 0) {
            uint32_t off, len;

            if (q + 10 > opsSize) {
                return false;
            }
            off = Le32(ops + q + 2);
            len = Le32(ops + q + 6);
            if (ops[q + 1] == s) {
                if ((uint64_t)off + len > srcSize) {
                    return false;
                }
                c = Crc64Update(c, src + off, len);
            }
            q += 10;
        } else if (ops[q] == 1) {
            uint32_t len;

            if (q + 5 > opsSize) {
                return false;
            }
            len = Le32(ops + q + 1);
            if (len > opsSize - q - 5) {
                return false;
            }
            q += 5 + (size_t)len;
        } else {
            return false;
        }
    }
    *crc = ~c;
    return true;
}

#define MAX_DELTA_SOURCES 255

// One variant of a delta, from its sources; NULL (and why, in p->error) if they aren't all there.
static uint8_t* RebuildVariant(Porter* p, const uint8_t* body, size_t bodySize, uint32_t* outSize, const char* name) {
    const uint8_t* srcData[MAX_DELTA_SOURCES];
    uint64_t srcSize[MAX_DELTA_SOURCES];
    size_t q = 13;
    uint32_t size, sources;
    uint64_t want;
    uint8_t* out;
    size_t done = 0;

    if (bodySize < 13) {
        return NULL;
    }
    size = Le32(body);
    want = Le64(body + 4);
    sources = body[12];
    // Each source: its paths, then the CRC of what is copied from it; the ops follow the last one.
    const uint8_t* srcHeaders[MAX_DELTA_SOURCES];
    for (uint32_t s = 0; s < sources; s++) {
        uint32_t paths;

        if (q >= bodySize) {
            return NULL;
        }
        srcHeaders[s] = body + q;
        paths = body[q++];
        for (uint32_t k = 0; k < paths; k++) {
            if (q + 2 > bodySize) {
                return NULL;
            }
            q += 2 + (size_t)(body[q] | (body[q + 1] << 8));
        }
        q += 8;
        if (q > bodySize) {
            return NULL;
        }
    }
    const uint8_t* ops = body + q;
    size_t opsSize = bodySize - q;

    for (uint32_t s = 0; s < sources; s++) {
        const uint8_t* h = srcHeaders[s];
        uint32_t paths = *h++;
        const uint8_t* crcAt = h;
        bool found = false;
        char path[512];

        for (uint32_t k = 0; k < paths; k++) {
            crcAt += 2 + (size_t)(crcAt[0] | (crcAt[1] << 8));
        }
        for (uint32_t k = 0; (k < paths) && !found; k++) {
            uint32_t len = h[0] | (h[1] << 8);
            const uint8_t* data;
            uint64_t dataSize, crc;

            if (len >= sizeof(path)) {
                return NULL;
            }
            memcpy(path, h + 2, len);
            path[len] = '\0';
            h += 2 + (size_t)len;
            if (SourceAt(p, path, &data, &dataSize) && CopiedCrc(ops, opsSize, s, data, dataSize, &crc) &&
                (crc == Le64(crcAt))) {
                srcData[s] = data;
                srcSize[s] = dataSize;
                found = true;
            }
        }
        if (!found) {
            Fail(p, "%s: a file it copies from isn't the one it was made from", name);
            return NULL;
        }
    }
    out = malloc(size > 0 ? size : 1);
    if (out == NULL) {
        return NULL;
    }
    for (size_t k = 0; k < opsSize;) {
        uint32_t len;

        if ((ops[k] == 0) && (k + 10 <= opsSize)) {
            uint32_t s = ops[k + 1], off = Le32(ops + k + 2);

            len = Le32(ops + k + 6);
            if ((s >= sources) || ((uint64_t)off + len > srcSize[s]) || (len > size - done)) {
                free(out);
                return NULL;
            }
            memcpy(out + done, srcData[s] + off, len);
            k += 10;
        } else if ((ops[k] == 1) && (k + 5 <= opsSize)) {
            len = Le32(ops + k + 1);
            if ((len > opsSize - k - 5) || (len > size - done)) {
                free(out);
                return NULL;
            }
            memcpy(out + done, ops + k + 5, len);
            k += 5 + (size_t)len;
        } else {
            free(out);
            return NULL;
        }
        done += len;
    }
    if ((done != size) || (~Crc64Update(~0ull, out, size) != want)) {
        free(out);
        return NULL;
    }
    *outSize = size;
    return out;
}

// A file of the pack as the layer holds it: itself, or the first variant of its delta whose sources are all there.
static uint8_t* Rebuild(Porter* p, uint8_t* rec, uint32_t recSize, uint32_t* outSize, const char* name) {
    size_t q = 6;

    if ((recSize < 6) || (memcmp(rec, "LUSD", 4) != 0)) {
        *outSize = recSize;
        return rec;
    }
    if (rec[4] != 1) {
        Fail(p, "%s: a delta of a newer kind", name);
        free(rec);
        return NULL;
    }
    for (uint32_t v = 0, count = rec[5]; v < count; v++) {
        uint32_t len;
        uint8_t* out;

        if (q + 4 > recSize) {
            break;
        }
        len = Le32(rec + q);
        if (len > recSize - q - 4) {
            break;
        }
        out = RebuildVariant(p, rec + q + 4, len, outSize, name);
        if (out != NULL) {
            p->error[0] = '\0';
            free(rec);
            return out;
        }
        q += 4 + (size_t)len;
    }
    Fail(p, "%s: can't be rebuilt", name);
    free(rec);
    return NULL;
}

// False if the mounted oot.o2r was made from another release than OoT USA 1.0, which the pack's deltas copy from (its
// scenes differ in places; tools/porterpack.py).
static bool OotIs10(void) {
    static const uint8_t sUsa10[4] = { 0xEC, 0x70, 0x11, 0xB7 };
    uint64_t size = 0;
    uint8_t* version = ReadGame(OOT_VERSION, &size);
    bool ok = (version == NULL) || (size < 5) || (memcmp(version + 1, sUsa10, sizeof(sUsa10)) == 0);

    free(version);
    return ok;
}

#define NOT_USA10 "your Unbound was set up from another release of Ocarina of Time than USA 1.0"

// The game's copy of each file of the layer, and the layer's file list, so that the next update can read the game's
// files this layer will hide. Over an older layer, a file it covers without a copy of the game's is ML's own: no copy.
static bool AddGameCopies(const MlZip* pack, MlZipWriter* w) {
    FileList old;
    MlZipEntry entry;
    size_t cursor = 0;
    char* list = NULL;
    size_t listSize = 0;
    bool ok = true;

    FileList_Load(&old);
    while (ok && MlZip_Next(pack, &cursor, &entry)) {
        char kept[512];
        size_t nameLen = strlen(entry.name);
        uint64_t size = 0;
        uint8_t* data;
        char* grown;

        if (strcmp(entry.name, PORTER_LIST) == 0) {
            continue;
        }
        grown = realloc(list, listSize + nameLen + 1);
        if ((grown == NULL) || (snprintf(kept, sizeof(kept), "%s%s", PORTER_GAME, entry.name) >= (int)sizeof(kept))) {
            ok = (grown != NULL);
            list = (grown != NULL) ? grown : list;
            continue;
        }
        list = grown;
        memcpy(list + listSize, entry.name, nameLen);
        list[listSize + nameLen] = '\n';
        listSize += nameLen + 1;
        data = MayBeMounted(kept) ? ReadMounted(kept, &size) : NULL;
        if ((data == NULL) && !FileList_Has(&old, entry.name) && MayBeMounted(entry.name)) {
            data = ReadMounted(entry.name, &size);
        }
        if (data != NULL) {
            ok = MlZipWriter_Add(w, kept, data, size);
            free(data);
        }
    }
    ok = ok && MlZipWriter_Add(w, PORTER_FILES, (const uint8_t*)list, listSize);
    free(list);
    FileList_Free(&old);
    return ok;
}

static uint8_t* BuildLayerListed(const uint8_t* mlRom, size_t mlSize, uint64_t* outSize, char* error,
                                 size_t errorSize) {
    Porter p;
    MlZipWriter w;
    MlZip pack;
    MlZipEntry entry;
    size_t cursor = 0;
    uint64_t packSize;
    uint8_t* packData = ReadMounted(PORTER_PACK, &packSize);
    uint32_t files = 0;

    memset(&p, 0, sizeof(p));
    memset(&w, 0, sizeof(w));
    p.error = error;
    p.errorSize = errorSize;
    error[0] = '\0';
    if (!OotIs10()) {
        Fail(&p, "%s", NOT_USA10);
        free(packData);
        return NULL;
    }
    if ((packData == NULL) || !MlZip_Open(&pack, packData, packSize)) {
        Fail(&p, "%s is missing from The Missing Link's code mod", PORTER_PACK);
        free(packData);
        return NULL;
    }
    if (!MlRomFiles_Load(&p.rom, mlRom, mlSize)) {
        Fail(&p, "%s", "The Missing Link's ROM files can't be read");
        free(packData);
        return NULL;
    }
    while (MlZip_Next(&pack, &cursor, &entry)) {
        uint8_t* data;
        uint32_t size;

        if (strcmp(entry.name, PORTER_LIST) == 0) {
            continue;
        }
        data = MlZip_Read(&pack, &entry);
        if (data == NULL) {
            Fail(&p, "%s can't be read from the pack", entry.name);
            break;
        }
        data = Rebuild(&p, data, entry.size, &size, entry.name);
        if (data == NULL) {
            break;
        }
        if (!MlZipWriter_Add(&w, entry.name, data, size)) {
            Fail(&p, "%s doesn't fit in the layer", entry.name);
            free(data);
            break;
        }
        free(data);
        files++;
    }
    if ((error[0] == '\0') && (files == 0)) {
        Fail(&p, "%s", "the pack holds no files");
    }
    if ((error[0] == '\0') && !AddGameCopies(&pack, &w)) {
        Fail(&p, "%s", "the game's files couldn't be kept in the layer");
    }
    CacheFree(&p.cache);
    MlRomFiles_Free(&p.rom);
    free(packData);
    if ((error[0] != '\0') || !MlZipWriter_Finish(&w)) {
        Fail(&p, "%s", "the layer couldn't be written");
        MlZipWriter_Free(&w);
        return NULL;
    }
    *outSize = w.size;
    return w.data;
}

// The layer (a zip archive, malloc'd) from the ML ROM in .z64 order, or NULL and why.
static uint8_t* BuildLayer(const uint8_t* mlRom, size_t mlSize, uint64_t* outSize, char* error, size_t errorSize) {
    uint8_t* layer;

    Mounted_List();
    layer = BuildLayerListed(mlRom, mlSize, outSize, error, errorSize);
    Mounted_Free();
    return layer;
}

// --- the player's files (pipeline/build_mod.py ml_rom) ---

typedef struct {
    const uint8_t* data;
    size_t size;
} Item;

#define MAX_ITEMS 32

// CRC-32 of a .z64 ROM as `order` lays it out, without a copy.
static uint32_t CrcAs(const uint8_t* z64, size_t size, MlRomOrder order) {
    uint8_t chunk[1 << 14];
    uint32_t crc = 0;

    if (order == ML_ROM_ORDER_Z64) {
        return MlZip_Crc32(0, z64, size);
    }
    for (size_t at = 0; at < size; at += sizeof(chunk)) {
        size_t n = (size - at < sizeof(chunk)) ? size - at : sizeof(chunk);

        memcpy(chunk, z64 + at, n);
        if (order == ML_ROM_ORDER_V64) {
            MlRom_Swap16(chunk, n);
        } else {
            MlRom_Swap32(chunk, n);
        }
        crc = MlZip_Crc32(crc, chunk, n);
    }
    return crc;
}

typedef enum {
    FILES_ML,           // the ML ROM is made
    FILES_NEED_PATCH,   // OoT USA 1.0, but no patch
    FILES_NEED_ROM,     // a patch, but no ROM
    FILES_WRONG,        // neither ROM, or no patch that makes ML from OoT USA 1.0
} FilesState;

// The ML v2.0 ROM in .z64 order (malloc'd) from the given files: each an N64 ROM, a BPS patch, or a zip holding
// them. The ML ROM as given, or OoT USA 1.0 with a patch applied in the byte order the patch is made for.
static uint8_t* MlRomFrom(const uint8_t* const* files, const uint64_t* sizes, int32_t count, size_t* mlSize,
                          FilesState* state, char* error, size_t errorSize) {
    Item items[MAX_ITEMS];
    uint8_t* owned[MAX_ITEMS];
    int32_t n = 0, owns = 0;
    uint8_t* oot = NULL;
    uint8_t* ml = NULL;
    size_t ootSize = 0;
    bool anyPatch = false, anyRom = false;

    error[0] = '\0';
    for (int32_t f = 0; f < count; f++) {
        MlZip zip;

        if (MlZip_Open(&zip, files[f], sizes[f])) {
            MlZipEntry entry;
            size_t cursor = 0;

            while (MlZip_Next(&zip, &cursor, &entry) && (n < MAX_ITEMS) && (owns < MAX_ITEMS)) {
                uint8_t* data;

                if ((entry.size < 4) || (entry.size > (1u << 30))) {
                    continue;
                }
                data = MlZip_Read(&zip, &entry);
                if (data != NULL) {
                    owned[owns++] = data;
                    items[n].data = data;
                    items[n++].size = entry.size;
                }
            }
        } else if (n < MAX_ITEMS) {
            items[n].data = files[f];
            items[n++].size = sizes[f];
        }
    }
    for (int32_t i = 0; (i < n) && (ml == NULL); i++) {
        MlRomOrder order = MlRom_Order(items[i].data, items[i].size);
        uint8_t* rom;
        uint32_t crc;

        if (order == ML_ROM_ORDER_NONE) {
            anyPatch |= (items[i].size >= 4) && (memcmp(items[i].data, "BPS1", 4) == 0);
            continue;
        }
        anyRom = true;
        rom = malloc(items[i].size);
        if (rom == NULL) {
            continue;
        }
        memcpy(rom, items[i].data, items[i].size);
        MlRom_ToZ64(rom, items[i].size, order);
        crc = MlZip_Crc32(0, rom, items[i].size);
        if (crc == ML_ROM_CRC) {
            ml = rom;
            *mlSize = items[i].size;
        } else if ((crc == ML_OOT10_CRC) && (oot == NULL)) {
            oot = rom;
            ootSize = items[i].size;
        } else {
            free(rom);
        }
    }
    for (int32_t i = 0; (i < n) && (ml == NULL) && (oot != NULL); i++) {
        static const MlRomOrder sForms[3] = { ML_ROM_ORDER_Z64, ML_ROM_ORDER_V64, ML_ROM_ORDER_N64 };
        uint32_t source, target;

        if (!MlBps_Crcs(items[i].data, items[i].size, &source, &target)) {
            continue;
        }
        for (int32_t k = 0; (k < 3) && (ml == NULL); k++) {
            uint8_t* form;
            uint8_t* out;
            size_t outSize;
            MlRomOrder order;

            if (CrcAs(oot, ootSize, sForms[k]) != source) {
                continue;
            }
            form = malloc(ootSize);
            if (form == NULL) {
                break;
            }
            memcpy(form, oot, ootSize);
            if (sForms[k] == ML_ROM_ORDER_V64) {
                MlRom_Swap16(form, ootSize);
            } else if (sForms[k] == ML_ROM_ORDER_N64) {
                MlRom_Swap32(form, ootSize);
            }
            out = MlBps_Apply(form, ootSize, items[i].data, items[i].size, &outSize);
            free(form);
            order = (out != NULL) ? MlRom_Order(out, outSize) : ML_ROM_ORDER_NONE;
            if (order != ML_ROM_ORDER_NONE) {
                MlRom_ToZ64(out, outSize, order);
                if (MlZip_Crc32(0, out, outSize) == ML_ROM_CRC) {
                    ml = out;
                    *mlSize = outSize;
                    break;
                }
            }
            free(out);
        }
    }
    for (int32_t i = 0; i < owns; i++) {
        free(owned[i]);
    }
    *state = (ml != NULL)                     ? FILES_ML
             : ((oot != NULL) && !anyPatch) ? FILES_NEED_PATCH
             : (!anyRom && anyPatch)        ? FILES_NEED_ROM
                                            : FILES_WRONG;
    snprintf(error, errorSize, "%s",
             (*state == FILES_ML)           ? ""
             : (*state == FILES_NEED_PATCH) ? "That's Ocarina of Time USA 1.0, but no patch came with it: choose "
                                              "Kaze's v2.0 patch too (the .zip from his page, or its .bps)."
             : (*state == FILES_NEED_ROM)   ? "That's a patch, but no ROM came with it: choose your Ocarina of Time "
                                              "USA 1.0 ROM too."
             : (oot != NULL)                ? "None of the patches makes The Missing Link v2.0 from OoT USA 1.0: use "
                                              "the v2.0 patch from Kaze's page (" PATCH_PAGE ")."
                                            : "That isn't The Missing Link v2.0 or Ocarina of Time USA 1.0 (NTSC "
                                              "1.0). Kaze's patch needs exactly OoT USA 1.0.");
    free(oot);
    return ml;
}

// --- the services (tests) ---

static uint8_t* Porter_ServiceBuild(const uint8_t* const* files, const uint64_t* sizes, int32_t count,
                                    uint64_t* outSize, char* error, uint32_t errorSize) {
    size_t mlSize = 0;
    FilesState state;
    uint8_t* ml = MlRomFrom(files, sizes, count, &mlSize, &state, error, errorSize);
    uint8_t* layer;

    if (ml == NULL) {
        return NULL;
    }
    layer = BuildLayer(ml, mlSize, outSize, error, errorSize);
    free(ml);
    return layer;
}

static void Porter_ServiceFree(uint8_t* data) {
    free(data);
}

// --- in the game ---

static bool Pick(const char* title, const char* filterName, const char* patterns, SOHUserFile* file) {
    memset(file, 0, sizeof(*file));
    file->structSize = sizeof(*file);
    return sApi->PickUserFile(title, filterName, patterns, file);
}

// Instead of asking for the files, when the game wasn't set up from OoT USA 1.0 (Master Quest alone, or another
// release).
static bool TellNotUsa10(void) {
    if (OotIs10()) {
        return false;
    }
    Log("[MissingLink] porter: %s", NOT_USA10);
    sApi->TellPlayer("Making The Missing Link",
                     "The Missing Link is made over Ocarina of Time USA 1.0, but " NOT_USA10 ".\n\nClose Unbound, "
                     "move oot.o2r, oot-mq.o2r and oot-unbound.o2r out of its folder, and start it again with your "
                     "USA 1.0 ROM. The Missing Link then asks again. You can put oot-mq.o2r back after.");
    return true;
}

static void Make(bool update) {
    SOHUserFile picked[2];
    const uint8_t* files[2];
    uint64_t sizes[2];
    int32_t count = 0;
    char error[512];
    size_t mlSize = 0;
    FilesState state = FILES_NEED_ROM;
    uint8_t* ml = NULL;
    uint8_t* layer;
    uint64_t layerSize = 0;

    if (TellNotUsa10()) {
        return;
    }
    // The ROM first; then whichever of the two is still missing, once.
    while ((ml == NULL) && (count < 2) && ((state == FILES_NEED_ROM) || (state == FILES_NEED_PATCH))) {
        bool rom = (state == FILES_NEED_ROM);

        if (!Pick(rom ? "Choose your Ocarina of Time USA 1.0 ROM (or a Missing Link v2.0 ROM)"
                      : "Choose Kaze's Missing Link v2.0 patch (the .zip from his page, or its .bps)",
                  rom ? "N64 ROMs" : "Patches", rom ? "*.z64 *.n64 *.v64 *.zip" : "*.zip *.bps", &picked[count])) {
            break;
        }
        files[count] = picked[count].data;
        sizes[count] = picked[count].size;
        count++;
        ml = MlRomFrom(files, sizes, count, &mlSize, &state, error, sizeof(error));
    }
    for (int32_t i = 0; i < count; i++) {
        sApi->FreeUserFile(&picked[i]);
    }
    if (ml == NULL) {
        if (count > 0) {
            sApi->TellPlayer("Making The Missing Link", error);
        }
        return;
    }
    layer = BuildLayer(ml, mlSize, &layerSize, error, sizeof(error));
    free(ml);
    if (layer == NULL) {
        char text[768];

        Log("[MissingLink] porter: %s", error);
        snprintf(text, sizeof(text),
                 "The Missing Link couldn't be made: %s.\n\nThis version of it is made for the Unbound build named on "
                 "its page; with another, use that build.",
                 error);
        sApi->TellPlayer("Making The Missing Link", text);
        return;
    }
    if (sApi->SaveToModsFolder(PORTER_LAYER, layer, layerSize)) {
        Log("[MissingLink] porter: wrote %s (%u kB)", PORTER_LAYER, (unsigned)(layerSize / 1024));
        free(layer);
        sApi->RequestRestart(update ? "to load the new version of The Missing Link" : "to load The Missing Link");
        return;
    }
    free(layer);
}

// Over an older layer: an empty one in its place for one start, so the next can read the pack's sources.
static void Clear(void) {
    static const char sNote[] = "The Missing Link's layer, cleared for an update: ML's code mod makes the new one.\n";
    MlZipWriter w;

    memset(&w, 0, sizeof(w));
    if (!MlZipWriter_Add(&w, PORTER_CLEARED, (const uint8_t*)sNote, sizeof(sNote) - 1) || !MlZipWriter_Finish(&w)) {
        MlZipWriter_Free(&w);
        return;
    }
    if (sApi->SaveToModsFolder(PORTER_LAYER, w.data, w.size)) {
        Log("[MissingLink] porter: cleared %s for the update", PORTER_LAYER);
        sApi->RequestRestart("to clear the old version of The Missing Link");
    }
    MlZipWriter_Free(&w);
}

// The player's answer, logged, so a test run shows what was asked and what was chosen.
static bool Ask(const char* title, const char* message) {
    bool yes = sApi->AskPlayer(title, message);

    Log("[MissingLink] porter: asked \"%s\": %s", title, yes ? "yes" : "no");
    return yes;
}

static void Porter_OnGameFrameUpdate(void) {
    if (sOffered || (sState == LAYER_READY)) {
        return;
    }
    sOffered = true;
    if (!CVarGetInteger(PORTER_ASKS, 1)) {     // off in test homes, which make the layer through the service
        Log("[MissingLink] porter: not asking (%s is 0)", PORTER_ASKS);
        return;
    }
    if ((sState == LAYER_NONE) && TellNotUsa10()) {
        return;
    }
    if ((sState == LAYER_NONE) && sCleared) {
        if (Ask("Finish updating The Missing Link",
                "The old version of The Missing Link is cleared. To make the new one, choose the same files as "
                "before: your Ocarina of Time USA 1.0 ROM and Kaze's v2.0 patch (or a Missing Link v2.0 ROM).\n\n"
                "Choose them now? It takes a few seconds, then Unbound closes. Open it again to play.")) {
            Make(false);
        }
    } else if (sState == LAYER_NONE) {
        if (Ask("Make The Missing Link",
                "The Missing Link isn't made yet. It's made here from your own files: your Ocarina of Time USA 1.0 "
                "ROM and Kaze's v2.0 patch, from " PATCH_PAGE " (the .zip as downloaded is fine), or a Missing "
                "Link v2.0 ROM.\n\nMake it now? It takes a few seconds, then Unbound closes. Open it again to "
                "play.")) {
            Make(false);
        }
    } else if (IsMounted(PORTER_FILES)) {   // a layer the porter made, with the game's files it covers
        if (Ask("Update The Missing Link",
                "This version of The Missing Link needs its world made again from your files: your Ocarina of Time "
                "USA 1.0 ROM and Kaze's v2.0 patch (or a Missing Link v2.0 ROM), the same as before.\n\nUpdate now? "
                "It takes a few seconds, then Unbound closes. Open it again to play.")) {
            Make(true);
        }
    } else if (Ask("Update The Missing Link",
                   "This version of The Missing Link needs its world made again from your files.\n\nUpdate now? "
                   "Unbound closes to clear the old version. Open it again and choose the same files as before: "
                   "your Ocarina of Time USA 1.0 ROM and Kaze's v2.0 patch. Then it closes once more, and The "
                   "Missing Link starts the next time you open it.")) {
        Clear();
    }
}

// The 16 hex digits of a field of the layer's mark, {"layer": "<hex>", "codemod": "<hex>"} (unbound_layer.py
// mark_layer); false if it has none.
static bool MarkField(const uint8_t* mark, uint64_t size, const char* key, char out[17]) {
    char pattern[32];
    size_t len;

    snprintf(pattern, sizeof(pattern), "\"%s\": \"", key);
    len = strlen(pattern);
    for (uint64_t i = 0; i + len + 16 <= size; i++) {
        if (memcmp(mark + i, pattern, len) == 0) {
            memcpy(out, mark + i + len, 16);
            out[16] = '\0';
            return true;
        }
    }
    return false;
}

bool MlPorter_Register(const SOHModApi* api) {
    uint64_t markSize = 0, packSize = 0;
    uint8_t* mark;
    uint8_t* packData;

    sApi = api;
    if (!SOH_MOD_API_HAS(api, RequestRestart)) {
        sState = LAYER_READY;   // a host without the file services: ML runs over whatever layer it has
        return true;
    }
    api->PublishService("missinglink.porter.build", (void*)Porter_ServiceBuild);
    api->PublishService("missinglink.porter.free", (void*)Porter_ServiceFree);

    mark = ReadMounted(PORTER_MARK, &markSize);
    packData = ReadMounted(PORTER_PACK, &packSize);
    sState = LAYER_NONE;
    if (mark != NULL) {
        MlZip pack;
        uint32_t wantSize = 0;
        uint8_t* want = (packData != NULL) && MlZip_Open(&pack, packData, packSize)
                            ? MlZip_ReadNamed(&pack, PORTER_MARK, &wantSize)
                            : NULL;

        char made[17], ours[17];
        bool same = (want != NULL) && (wantSize == markSize) && (memcmp(want, mark, markSize) == 0);

        // A layer the builder made beside this code mod names its pack: the builder made both, for the player's
        // Unbound, which may convert vanilla differently from the one the pack is made for.
        if (!same && (packData != NULL) && MarkField(mark, markSize, "codemod", made)) {
            snprintf(ours, sizeof(ours), "%016llx", (unsigned long long)~Crc64Update(~0ull, packData, packSize));
            same = (strcmp(made, ours) == 0);
        }
        // Without a pack (a test build) any marked layer is the one.
        sState = ((packData == NULL) || same) ? LAYER_READY : LAYER_OLD;
        free(want);
    } else if (IsMounted(PORTER_OLD)) {
        sState = (packData == NULL) ? LAYER_READY : LAYER_OLD;
    }
    sCleared = (sState == LAYER_NONE) && IsMounted(PORTER_CLEARED);
    free(mark);
    free(packData);
    Log("[MissingLink] porter: layer %s",
        (sState == LAYER_READY) ? "ready"
        : (sState == LAYER_OLD) ? "older than the code mod"
        : sCleared              ? "cleared for an update, not made yet"
                                : "not made yet");
    if (sState != LAYER_READY) {
        SOH_REGISTER_HOOK(api, OnGameFrameUpdate, Porter_OnGameFrameUpdate);
    }
    return sState != LAYER_NONE;
}
