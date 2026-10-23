/*
 * The player's ROMs for the porter (see ml_porter_rom.h): pipeline/build_mod.py's ROM handling, pipeline/bps_apply.py
 * and pipeline/yaz0.py in C, and tools/deltapack.py's ROM views, which must stay byte for byte the same as these.
 */
#include "ml_porter_rom.h"
#include "ml_porter_zip.h"

#include <stdlib.h>
#include <string.h>

#define ML_DMADATA 0x12F70  // ML v2.0's dmadata (deltapack.py ML_DMADATA)

static const char* const sViewNames[ML_ROM_VIEWS] = { "", "w16/", "w32/", "vtx0/", "vtx8/", "json/" };

static uint32_t ReadBe32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

MlRomOrder MlRom_Order(const uint8_t* data, size_t size) {
    if (size < 0x1000) {
        return ML_ROM_ORDER_NONE;
    }
    if ((data[0] == 0x80) && (data[1] == 0x37) && (data[2] == 0x12) && (data[3] == 0x40)) {
        return ML_ROM_ORDER_Z64;
    }
    if ((data[0] == 0x37) && (data[1] == 0x80) && (data[2] == 0x40) && (data[3] == 0x12)) {
        return ML_ROM_ORDER_V64;
    }
    if ((data[0] == 0x40) && (data[1] == 0x12) && (data[2] == 0x37) && (data[3] == 0x80)) {
        return ML_ROM_ORDER_N64;
    }
    return ML_ROM_ORDER_NONE;
}

void MlRom_Swap16(uint8_t* data, size_t size) {
    for (size_t i = 0; i + 1 < size; i += 2) {
        uint8_t t = data[i];

        data[i] = data[i + 1];
        data[i + 1] = t;
    }
}

void MlRom_Swap32(uint8_t* data, size_t size) {
    for (size_t i = 0; i + 3 < size; i += 4) {
        uint8_t a = data[i], b = data[i + 1];

        data[i] = data[i + 3];
        data[i + 1] = data[i + 2];
        data[i + 2] = b;
        data[i + 3] = a;
    }
}

void MlRom_ToZ64(uint8_t* data, size_t size, MlRomOrder order) {
    if (order == ML_ROM_ORDER_V64) {
        MlRom_Swap16(data, size);
    } else if (order == ML_ROM_ORDER_N64) {
        MlRom_Swap32(data, size);
    }
}

// --- BPS ---

static bool ReadVarint(const uint8_t* patch, size_t end, size_t* pos, uint64_t* out) {
    uint64_t data = 0, shift = 1;

    for (int32_t i = 0; i < 10; i++) {
        uint8_t x;

        if (*pos >= end) {
            return false;
        }
        x = patch[(*pos)++];
        data += (uint64_t)(x & 0x7F) * shift;
        if (x & 0x80) {
            *out = data;
            return true;
        }
        shift <<= 7;
        data += shift;
    }
    return false;
}

static uint32_t ReadLe32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool MlBps_Crcs(const uint8_t* patch, size_t size, uint32_t* source, uint32_t* target) {
    if ((size < 16) || (memcmp(patch, "BPS1", 4) != 0)) {
        return false;
    }
    *source = ReadLe32(patch + size - 12);
    *target = ReadLe32(patch + size - 8);
    return true;
}

uint8_t* MlBps_Apply(const uint8_t* source, size_t sourceSize, const uint8_t* patch, size_t patchSize,
                     size_t* targetSize) {
    uint64_t srcSize, tgtSize, metaSize;
    size_t pos = 4, end, out = 0;
    int64_t srcRel = 0, tgtRel = 0;
    uint8_t* target;

    if ((patchSize < 16) || (memcmp(patch, "BPS1", 4) != 0) ||
        (MlZip_Crc32(0, patch, patchSize - 4) != ReadLe32(patch + patchSize - 4))) {
        return NULL;
    }
    end = patchSize - 12;
    if (!ReadVarint(patch, end, &pos, &srcSize) || !ReadVarint(patch, end, &pos, &tgtSize) ||
        !ReadVarint(patch, end, &pos, &metaSize) || (srcSize != sourceSize) || (tgtSize > (1u << 30)) ||
        (metaSize > end - pos) || (MlZip_Crc32(0, source, sourceSize) != ReadLe32(patch + patchSize - 12))) {
        return NULL;
    }
    pos += metaSize;
    target = malloc(tgtSize > 0 ? tgtSize : 1);
    if (target == NULL) {
        return NULL;
    }
    while (pos < end) {
        uint64_t data, length, d;
        uint32_t action;

        if (!ReadVarint(patch, end, &pos, &data)) {
            goto fail;
        }
        action = data & 3;
        length = (data >> 2) + 1;
        if (length > tgtSize - out) {
            goto fail;
        }
        if (action == 0) {          // SourceRead
            if (out + length > sourceSize) {
                goto fail;
            }
            memcpy(target + out, source + out, length);
            out += length;
        } else if (action == 1) {   // TargetRead
            if (length > end - pos) {
                goto fail;
            }
            memcpy(target + out, patch + pos, length);
            pos += length;
            out += length;
        } else {                    // SourceCopy, TargetCopy
            int64_t* rel = (action == 2) ? &srcRel : &tgtRel;

            if (!ReadVarint(patch, end, &pos, &d)) {
                goto fail;
            }
            *rel += (d & 1) ? -(int64_t)(d >> 1) : (int64_t)(d >> 1);
            for (uint64_t k = 0; k < length; k++) {
                if ((*rel < 0) || ((action == 2) ? ((uint64_t)*rel >= sourceSize) : ((uint64_t)*rel >= out))) {
                    goto fail;
                }
                target[out++] = (action == 2) ? source[*rel] : target[*rel];
                (*rel)++;
            }
        }
    }
    if ((out != tgtSize) || (MlZip_Crc32(0, target, out) != ReadLe32(patch + patchSize - 8))) {
        goto fail;
    }
    *targetSize = out;
    return target;
fail:
    free(target);
    return NULL;
}

// --- ML's files ---

// Yaz0 into out[0..size); false if the data runs out or reaches outside what it has written.
static bool Yaz0(const uint8_t* src, size_t srcSize, uint8_t* out, uint32_t size) {
    size_t sp = 16;
    uint32_t dp = 0;
    uint8_t code = 0;
    int32_t codeBits = 0;

    while (dp < size) {
        if (codeBits == 0) {
            if (sp >= srcSize) {
                return false;
            }
            code = src[sp++];
            codeBits = 8;
        }
        if (code & 0x80) {
            if (sp >= srcSize) {
                return false;
            }
            out[dp++] = src[sp++];
        } else {
            uint32_t dist, n;
            uint8_t b1, b2;

            if (sp + 2 > srcSize) {
                return false;
            }
            b1 = src[sp];
            b2 = src[sp + 1];
            sp += 2;
            dist = (((uint32_t)(b1 & 0xF) << 8) | b2) + 1;
            n = b1 >> 4;
            if (n == 0) {
                if (sp >= srcSize) {
                    return false;
                }
                n = (uint32_t)src[sp++] + 0x12;
            } else {
                n += 2;
            }
            if ((dist > dp) || (n > size - dp)) {
                return false;
            }
            for (uint32_t k = 0; k < n; k++, dp++) {
                out[dp] = out[dp - dist];
            }
        }
        code <<= 1;
        codeBits--;
    }
    return true;
}

bool MlRomFiles_Load(MlRomFiles* files, const uint8_t* rom, size_t size) {
    uint32_t count = 0;

    memset(files, 0, sizeof(*files));
    while (ML_DMADATA + 16 * ((size_t)count + 1) <= size) {
        const uint8_t* e = rom + ML_DMADATA + 16 * (size_t)count;

        if ((ReadBe32(e) | ReadBe32(e + 4) | ReadBe32(e + 8) | ReadBe32(e + 12)) == 0) {
            break;
        }
        count++;
    }
    files->files = calloc(count > 0 ? count : 1, sizeof(MlRomFile));
    if (files->files == NULL) {
        return false;
    }
    files->count = count;
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t* e = rom + ML_DMADATA + 16 * (size_t)i;
        uint32_t vs = ReadBe32(e), ve = ReadBe32(e + 4), rs = ReadBe32(e + 8), re = ReadBe32(e + 12);
        uint32_t len;
        uint8_t* data;

        if ((rs == 0xFFFFFFFF) || (ve <= vs)) {
            continue;
        }
        len = ve - vs;
        if (re == 0) {
            if ((size_t)rs + len > size) {      // pipeline/yaz0.py get_file: uncompressed
                goto fail;
            }
            data = malloc(len);
            if (data == NULL) {
                goto fail;
            }
            memcpy(data, rom + rs, len);
        } else {
            uint32_t out;

            if ((re <= rs) || (re > size) || (re - rs < 16) || (memcmp(rom + rs, "Yaz0", 4) != 0)) {
                goto fail;
            }
            out = ReadBe32(rom + rs + 4);      // Yaz0's own size, as yaz0.py decompresses to
            data = malloc(out > 0 ? out : 1);
            if ((data == NULL) || !Yaz0(rom + rs, re - rs, data, out)) {
                free(data);
                goto fail;
            }
            len = out;
        }
        files->files[i].data = data;
        files->files[i].size = len;
    }
    return true;
fail:
    MlRomFiles_Free(files);
    return false;
}

// A byte of ML's message text as a JSON string holds it (deltapack.py JSON_TEXT): itself, or an escape; its length.
static uint32_t JsonByte(uint8_t b, char out[6]) {
    static const char sShort[] = { '"', '"', '\\', '\\', '\b', 'b', '\f', 'f', '\n', 'n', '\r', 'r', '\t', 't' };
    static const char sHex[] = "0123456789abcdef";

    for (uint32_t k = 0; k < sizeof(sShort); k += 2) {
        if (b == (uint8_t)sShort[k]) {
            out[0] = '\\';
            out[1] = sShort[k + 1];
            return 2;
        }
    }
    if ((b >= 0x20) && (b < 0x7F)) {
        out[0] = (char)b;
        return 1;
    }
    memcpy(out, "\\u00", 4);
    out[4] = sHex[b >> 4];
    out[5] = sHex[b & 15];
    return 6;
}

// The file as the text of a JSON string (the "json/" view), so a converted message table copies its text from it.
static bool MakeJsonView(MlRomFile* view, const MlRomFile* file) {
    char piece[6];
    uint32_t size = 0;

    for (uint32_t i = 0; i < file->size; i++) {
        size += JsonByte(file->data[i], piece);
    }
    view->data = malloc(size > 0 ? size : 1);
    if (view->data == NULL) {
        return false;
    }
    view->size = 0;
    for (uint32_t i = 0; i < file->size; i++) {
        uint32_t n = JsonByte(file->data[i], piece);

        memcpy(view->data + view->size, piece, n);
        view->size += n;
    }
    return true;
}

// One file as a ROM view shows it (deltapack.py rom_view).
static bool MakeView(MlRomFile* view, const MlRomFile* file, int32_t kind) {
    uint32_t size = file->size;

    if (kind == 5) {
        return MakeJsonView(view, file);
    }
    if (kind == 1) {
        size &= ~1u;
    } else if (kind == 2) {
        size &= ~3u;
    }
    view->data = malloc(size > 0 ? size : 1);
    if (view->data == NULL) {
        return false;
    }
    view->size = size;
    memcpy(view->data, file->data, size);
    if (kind == 1) {
        MlRom_Swap16(view->data, size);
    } else if (kind == 2) {
        MlRom_Swap32(view->data, size);
    } else {
        uint32_t off = (kind == 3) ? 0 : 8;

        for (uint32_t p = off; p + 15 < size; p += 16) {
            MlRom_Swap16(view->data + p, 12);
        }
    }
    return true;
}

const MlRomFile* MlRomFiles_View(MlRomFiles* files, const char* path) {
    const char* rest;
    int32_t kind = 0;
    char* end;
    unsigned long index;

    if (strncmp(path, ML_ROM_PREFIX, strlen(ML_ROM_PREFIX)) != 0) {
        return NULL;
    }
    rest = path + strlen(ML_ROM_PREFIX);
    for (int32_t k = 1; k < ML_ROM_VIEWS; k++) {
        if (strncmp(rest, sViewNames[k], strlen(sViewNames[k])) == 0) {
            kind = k;
            rest += strlen(sViewNames[k]);
            break;
        }
    }
    if ((*rest < '0') || (*rest > '9')) {
        return NULL;
    }
    index = strtoul(rest, &end, 10);
    if ((*end != '\0') || (index >= files->count) || (files->files[index].data == NULL)) {
        return NULL;
    }
    if (kind == 0) {
        return &files->files[index];
    }
    if (files->views[kind] == NULL) {
        files->views[kind] = calloc(files->count, sizeof(MlRomFile));
        if (files->views[kind] == NULL) {
            return NULL;
        }
    }
    if ((files->views[kind][index].data == NULL) &&
        !MakeView(&files->views[kind][index], &files->files[index], kind)) {
        return NULL;
    }
    return &files->views[kind][index];
}

void MlRomFiles_Free(MlRomFiles* files) {
    for (int32_t k = 0; k < ML_ROM_VIEWS; k++) {
        if (files->views[k] != NULL) {
            for (uint32_t i = 0; i < files->count; i++) {
                free(files->views[k][i].data);
            }
            free(files->views[k]);
        }
    }
    if (files->files != NULL) {
        for (uint32_t i = 0; i < files->count; i++) {
            free(files->files[i].data);
        }
        free(files->files);
    }
    memset(files, 0, sizeof(*files));
}
