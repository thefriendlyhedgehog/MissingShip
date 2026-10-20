#ifndef ML_PORTER_ZIP_H
#define ML_PORTER_ZIP_H

// Zip archives in memory for the porter (ml_porter.c): reading Kaze's patch zip and the layer's pack, writing the
// layer. Only what those need: stored and deflated entries, no zip64, no encryption.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint32_t MlZip_Crc32(uint32_t crc, const uint8_t* data, size_t size);

typedef struct {
    const uint8_t* data;
    size_t size;
    uint32_t count;     // entries in the central directory
    size_t directory;   // offset of its first entry
} MlZip;

typedef struct {
    char name[512];
    uint16_t method;    // 0 stored, 8 deflated
    uint32_t crc;
    uint32_t packedSize;
    uint32_t size;
    uint32_t localHeader;
} MlZipEntry;

// True if data is a zip archive this reader takes.
bool MlZip_Open(MlZip* zip, const uint8_t* data, size_t size);
// The central directory's next entry: start with *cursor = 0; false at its end, or if it is broken.
bool MlZip_Next(const MlZip* zip, size_t* cursor, MlZipEntry* entry);
// The entry's bytes (malloc'd, CRC checked); NULL if it can't be read.
uint8_t* MlZip_Read(const MlZip* zip, const MlZipEntry* entry);
// The entry named `name`'s bytes, or NULL.
uint8_t* MlZip_ReadNamed(const MlZip* zip, const char* name, uint32_t* size);

// Raw deflate (RFC 1951) into out[0..outSize); true if it fills exactly outSize bytes.
bool MlZip_Inflate(const uint8_t* in, size_t inSize, uint8_t* out, size_t outSize);

typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
    uint8_t* directory;     // the central directory, written at the end
    size_t directorySize;
    size_t directoryCapacity;
    uint32_t count;
    bool failed;
} MlZipWriter;

// Adds a stored entry; false (and the writer failed) if memory or the format's limits run out.
bool MlZipWriter_Add(MlZipWriter* w, const char* name, const uint8_t* data, size_t size);
// Finishes the archive: w->data, w->size hold it (the caller frees w->data). False if anything failed.
bool MlZipWriter_Finish(MlZipWriter* w);
void MlZipWriter_Free(MlZipWriter* w);

#endif
