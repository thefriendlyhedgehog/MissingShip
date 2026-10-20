/*
 * Zip archives in memory for the porter (see ml_porter_zip.h). The inflater follows RFC 1951 the way zlib's puff.c
 * does: canonical Huffman codes decoded a bit at a time, which is slow but small, and fast enough for a few MB.
 */
#include "ml_porter_zip.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// --- CRC-32 (zip's, reflected 0xEDB88320) ---

static uint32_t sCrcTable[256];
static bool sCrcReady;

uint32_t MlZip_Crc32(uint32_t crc, const uint8_t* data, size_t size) {
    if (!sCrcReady) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;

            for (int32_t k = 0; k < 8; k++) {
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            sCrcTable[i] = c;
        }
        sCrcReady = true;
    }
    crc = ~crc;
    for (size_t i = 0; i < size; i++) {
        crc = sCrcTable[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

static uint16_t Read16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t Read32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// --- inflate ---

#define INF_MAXBITS 15
#define INF_MAXLCODES 286
#define INF_MAXDCODES 30
#define INF_FIXLCODES 288

typedef struct {
    const uint8_t* in;
    size_t inSize;
    size_t inPos;
    uint8_t* out;
    size_t outSize;
    size_t outPos;
    uint32_t bitBuf;
    int32_t bitCount;
    bool error;
} Inflater;

typedef struct {
    int16_t count[INF_MAXBITS + 1];
    int16_t symbol[INF_FIXLCODES];
} Huffman;

static int32_t Bits(Inflater* s, int32_t need) {
    uint32_t value = s->bitBuf;

    while (s->bitCount < need) {
        if (s->inPos >= s->inSize) {
            s->error = true;
            return 0;
        }
        value |= (uint32_t)s->in[s->inPos++] << s->bitCount;
        s->bitCount += 8;
    }
    s->bitBuf = value >> need;
    s->bitCount -= need;
    return (int32_t)(value & ((1u << need) - 1));
}

static bool Stored(Inflater* s) {
    uint32_t len;

    s->bitBuf = 0;
    s->bitCount = 0;
    if (s->inPos + 4 > s->inSize) {
        return false;
    }
    len = Read16(s->in + s->inPos);
    if ((uint16_t)~len != Read16(s->in + s->inPos + 2)) {
        return false;
    }
    s->inPos += 4;
    if ((s->inPos + len > s->inSize) || (s->outPos + len > s->outSize)) {
        return false;
    }
    memcpy(s->out + s->outPos, s->in + s->inPos, len);
    s->inPos += len;
    s->outPos += len;
    return true;
}

static int32_t Decode(Inflater* s, const Huffman* h) {
    int32_t code = 0, first = 0, index = 0;

    for (int32_t len = 1; len <= INF_MAXBITS; len++) {
        int32_t count;

        code |= Bits(s, 1);
        if (s->error) {
            return -1;
        }
        count = h->count[len];
        if (code - count < first) {
            return h->symbol[index + (code - first)];
        }
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

// Builds a canonical code from code lengths; false if the lengths over-subscribe it.
static bool Construct(Huffman* h, const int16_t* length, int32_t n) {
    int16_t offs[INF_MAXBITS + 1];
    int32_t left = 1;

    memset(h->count, 0, sizeof(h->count));
    for (int32_t symbol = 0; symbol < n; symbol++) {
        h->count[length[symbol]]++;
    }
    for (int32_t len = 1; len <= INF_MAXBITS; len++) {
        left <<= 1;
        left -= h->count[len];
        if (left < 0) {
            return false;
        }
    }
    offs[1] = 0;
    for (int32_t len = 1; len < INF_MAXBITS; len++) {
        offs[len + 1] = (int16_t)(offs[len] + h->count[len]);
    }
    for (int32_t symbol = 0; symbol < n; symbol++) {
        if (length[symbol] != 0) {
            h->symbol[offs[length[symbol]]++] = (int16_t)symbol;
        }
    }
    return true;
}

static const int16_t sLengthBase[29] = { 3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                         31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const int16_t sLengthExtra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                          2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const int16_t sDistBase[30] = { 1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
                                       193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const int16_t sDistExtra[30] = { 0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                        6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

static bool Codes(Inflater* s, const Huffman* lencode, const Huffman* distcode) {
    for (;;) {
        int32_t symbol = Decode(s, lencode);

        if (symbol < 0) {
            return false;
        }
        if (symbol < 256) {
            if (s->outPos >= s->outSize) {
                return false;
            }
            s->out[s->outPos++] = (uint8_t)symbol;
        } else if (symbol == 256) {
            return true;
        } else {
            int32_t len, dist;

            symbol -= 257;
            if (symbol >= 29) {
                return false;
            }
            len = sLengthBase[symbol] + Bits(s, sLengthExtra[symbol]);
            symbol = Decode(s, distcode);
            if ((symbol < 0) || (symbol >= 30)) {
                return false;
            }
            dist = sDistBase[symbol] + Bits(s, sDistExtra[symbol]);
            if (s->error || ((size_t)dist > s->outPos) || (s->outPos + (size_t)len > s->outSize)) {
                return false;
            }
            while (len-- > 0) {
                s->out[s->outPos] = s->out[s->outPos - (size_t)dist];
                s->outPos++;
            }
        }
    }
}

static bool Fixed(Inflater* s) {
    static Huffman sLen, sDist;
    static bool sBuilt;

    if (!sBuilt) {
        int16_t lengths[INF_FIXLCODES];
        int32_t symbol = 0;

        for (; symbol < 144; symbol++) {
            lengths[symbol] = 8;
        }
        for (; symbol < 256; symbol++) {
            lengths[symbol] = 9;
        }
        for (; symbol < 280; symbol++) {
            lengths[symbol] = 7;
        }
        for (; symbol < INF_FIXLCODES; symbol++) {
            lengths[symbol] = 8;
        }
        Construct(&sLen, lengths, INF_FIXLCODES);
        for (symbol = 0; symbol < INF_MAXDCODES; symbol++) {
            lengths[symbol] = 5;
        }
        Construct(&sDist, lengths, INF_MAXDCODES);
        sBuilt = true;
    }
    return Codes(s, &sLen, &sDist);
}

static bool Dynamic(Inflater* s) {
    static const int16_t sOrder[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
    int16_t lengths[INF_MAXLCODES + INF_MAXDCODES];
    Huffman lencode, distcode;
    int32_t nlen = Bits(s, 5) + 257;
    int32_t ndist = Bits(s, 5) + 1;
    int32_t ncode = Bits(s, 4) + 4;
    int32_t index;

    if (s->error || (nlen > INF_MAXLCODES) || (ndist > INF_MAXDCODES)) {
        return false;
    }
    for (index = 0; index < ncode; index++) {
        lengths[sOrder[index]] = (int16_t)Bits(s, 3);
    }
    for (; index < 19; index++) {
        lengths[sOrder[index]] = 0;
    }
    if (s->error || !Construct(&lencode, lengths, 19)) {
        return false;
    }
    index = 0;
    while (index < nlen + ndist) {
        int32_t symbol = Decode(s, &lencode);
        int32_t len = 0;
        int32_t repeat;

        if (symbol < 0) {
            return false;
        }
        if (symbol < 16) {
            lengths[index++] = (int16_t)symbol;
            continue;
        }
        if (symbol == 16) {
            if (index == 0) {
                return false;
            }
            len = lengths[index - 1];
            repeat = 3 + Bits(s, 2);
        } else if (symbol == 17) {
            repeat = 3 + Bits(s, 3);
        } else {
            repeat = 11 + Bits(s, 7);
        }
        if (s->error || (index + repeat > nlen + ndist)) {
            return false;
        }
        while (repeat-- > 0) {
            lengths[index++] = (int16_t)len;
        }
    }
    if (lengths[256] == 0) {
        return false;
    }
    if (!Construct(&lencode, lengths, nlen) || !Construct(&distcode, lengths + nlen, ndist)) {
        return false;
    }
    return Codes(s, &lencode, &distcode);
}

bool MlZip_Inflate(const uint8_t* in, size_t inSize, uint8_t* out, size_t outSize) {
    Inflater s;
    int32_t last;

    memset(&s, 0, sizeof(s));
    s.in = in;
    s.inSize = inSize;
    s.out = out;
    s.outSize = outSize;
    do {
        int32_t type;
        bool ok;

        last = Bits(&s, 1);
        type = Bits(&s, 2);
        if (s.error) {
            return false;
        }
        ok = (type == 0) ? Stored(&s) : (type == 1) ? Fixed(&s) : (type == 2) ? Dynamic(&s) : false;
        if (!ok || s.error) {
            return false;
        }
    } while (!last);
    return s.outPos == outSize;
}

// --- reading ---

bool MlZip_Open(MlZip* zip, const uint8_t* data, size_t size) {
    size_t floor = (size > 0xFFFF + 22) ? size - (0xFFFF + 22) : 0;

    memset(zip, 0, sizeof(*zip));
    if (size < 22) {
        return false;
    }
    for (size_t p = size - 22 + 1; p-- > floor;) {
        if (Read32(data + p) == 0x06054B50) {
            uint32_t dirSize = Read32(data + p + 12);
            uint32_t dirOffset = Read32(data + p + 16);

            if (((size_t)dirOffset + dirSize > p) || (Read16(data + p + 10) == 0xFFFF)) {
                return false;   // a broken directory, or zip64
            }
            zip->data = data;
            zip->size = size;
            zip->count = Read16(data + p + 10);
            zip->directory = dirOffset;
            return true;
        }
    }
    return false;
}

bool MlZip_Next(const MlZip* zip, size_t* cursor, MlZipEntry* entry) {
    size_t p = (*cursor == 0) ? zip->directory : *cursor;
    uint16_t nameLen, extraLen, commentLen;

    if ((*cursor == SIZE_MAX) || (p + 46 > zip->size) || (Read32(zip->data + p) != 0x02014B50)) {
        return false;
    }
    nameLen = Read16(zip->data + p + 28);
    extraLen = Read16(zip->data + p + 30);
    commentLen = Read16(zip->data + p + 32);
    if ((p + 46 + nameLen > zip->size) || (nameLen >= sizeof(entry->name)) || (Read16(zip->data + p + 8) & 1)) {
        *cursor = SIZE_MAX;     // a broken entry, too long a name, or encryption: stop here
        return false;
    }
    entry->method = Read16(zip->data + p + 10);
    entry->crc = Read32(zip->data + p + 16);
    entry->packedSize = Read32(zip->data + p + 20);
    entry->size = Read32(zip->data + p + 24);
    entry->localHeader = Read32(zip->data + p + 42);
    memcpy(entry->name, zip->data + p + 46, nameLen);
    entry->name[nameLen] = '\0';
    *cursor = p + 46 + (size_t)nameLen + extraLen + commentLen;
    return true;
}

uint8_t* MlZip_Read(const MlZip* zip, const MlZipEntry* entry) {
    size_t p = entry->localHeader;
    size_t start;
    uint8_t* out;

    if ((p + 30 > zip->size) || (Read32(zip->data + p) != 0x04034B50)) {
        return NULL;
    }
    start = p + 30 + Read16(zip->data + p + 26) + Read16(zip->data + p + 28);
    if (start + entry->packedSize > zip->size) {
        return NULL;
    }
    out = malloc(entry->size > 0 ? entry->size : 1);
    if (out == NULL) {
        return NULL;
    }
    if (entry->method == 0) {
        if (entry->packedSize != entry->size) {
            free(out);
            return NULL;
        }
        memcpy(out, zip->data + start, entry->size);
    } else if ((entry->method != 8) || !MlZip_Inflate(zip->data + start, entry->packedSize, out, entry->size)) {
        free(out);
        return NULL;
    }
    if (MlZip_Crc32(0, out, entry->size) != entry->crc) {
        free(out);
        return NULL;
    }
    return out;
}

uint8_t* MlZip_ReadNamed(const MlZip* zip, const char* name, uint32_t* size) {
    MlZipEntry entry;
    size_t cursor = 0;

    while (MlZip_Next(zip, &cursor, &entry)) {
        if (strcmp(entry.name, name) == 0) {
            *size = entry.size;
            return MlZip_Read(zip, &entry);
        }
    }
    return NULL;
}

// --- writing ---

static bool Reserve(uint8_t** buf, size_t* capacity, size_t need) {
    if (need <= *capacity) {
        return true;
    }
    size_t grown = (*capacity > 0) ? *capacity : 1 << 16;

    while (grown < need) {
        grown *= 2;
    }
    uint8_t* next = realloc(*buf, grown);
    if (next == NULL) {
        return false;
    }
    *buf = next;
    *capacity = grown;
    return true;
}

static void Put16(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void Put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

// The header fields local and central entries share, from "version needed" on (26 bytes); the date is 1980-01-01,
// so the same layer is the same file.
static void PutCommon(uint8_t* p, uint32_t crc, size_t size, size_t nameLen) {
    Put16(p + 0, 10);
    Put16(p + 2, 0x0800);   // names are UTF-8
    Put16(p + 4, 0);        // stored
    Put16(p + 6, 0);
    Put16(p + 8, 0x0021);
    Put32(p + 10, crc);
    Put32(p + 14, (uint32_t)size);
    Put32(p + 18, (uint32_t)size);
    Put16(p + 22, (uint32_t)nameLen);
    Put16(p + 24, 0);
}

bool MlZipWriter_Add(MlZipWriter* w, const char* name, const uint8_t* data, size_t size) {
    size_t nameLen = strlen(name);
    uint32_t crc;
    uint8_t* p;

    if (w->failed || (nameLen > 0xFFFF) || (size > 0xFFFFFFF0u) || (w->count >= 0xFFFF) ||
        (w->size + 30 + nameLen + size > 0xFFFFFFF0u)) {
        w->failed = true;
        return false;
    }
    if (!Reserve(&w->data, &w->capacity, w->size + 30 + nameLen + size) ||
        !Reserve(&w->directory, &w->directoryCapacity, w->directorySize + 46 + nameLen)) {
        w->failed = true;
        return false;
    }
    crc = MlZip_Crc32(0, data, size);
    p = w->directory + w->directorySize;
    Put32(p, 0x02014B50);
    Put16(p + 4, 20);
    PutCommon(p + 6, crc, size, nameLen);
    Put16(p + 32, 0);
    Put16(p + 34, 0);
    Put16(p + 36, 0);
    Put32(p + 38, 0);
    Put32(p + 42, (uint32_t)w->size);
    memcpy(p + 46, name, nameLen);
    w->directorySize += 46 + nameLen;

    p = w->data + w->size;
    Put32(p, 0x04034B50);
    PutCommon(p + 4, crc, size, nameLen);
    memcpy(p + 30, name, nameLen);
    if (size > 0) {
        memcpy(p + 30 + nameLen, data, size);
    }
    w->size += 30 + nameLen + size;
    w->count++;
    return true;
}

bool MlZipWriter_Finish(MlZipWriter* w) {
    size_t offset = w->size;
    uint8_t* p;

    if (w->failed || (w->size + w->directorySize + 22 > 0xFFFFFFF0u) ||
        !Reserve(&w->data, &w->capacity, w->size + w->directorySize + 22)) {
        w->failed = true;
        return false;
    }
    if (w->directorySize > 0) {
        memcpy(w->data + w->size, w->directory, w->directorySize);
    }
    w->size += w->directorySize;
    p = w->data + w->size;
    Put32(p, 0x06054B50);
    Put16(p + 4, 0);
    Put16(p + 6, 0);
    Put16(p + 8, w->count);
    Put16(p + 10, w->count);
    Put32(p + 12, (uint32_t)w->directorySize);
    Put32(p + 16, (uint32_t)offset);
    Put16(p + 20, 0);
    w->size += 22;
    free(w->directory);
    w->directory = NULL;
    return true;
}

void MlZipWriter_Free(MlZipWriter* w) {
    free(w->data);
    free(w->directory);
    memset(w, 0, sizeof(*w));
}
