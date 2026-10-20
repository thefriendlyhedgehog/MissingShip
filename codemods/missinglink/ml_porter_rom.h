#ifndef ML_PORTER_ROM_H
#define ML_PORTER_ROM_H

// The player's ROMs for the porter (ml_porter.c): byte orders, Kaze's BPS patch, and ML's files with the views of them
// the layer's pack copies from (tools/deltapack.py, ROM views).

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ML_ROM_CRC 0x59CD42A3u     // The Missing Link v2.0, .z64 byte order
#define ML_OOT10_CRC 0xCD16C529u   // OoT NTSC-U 1.0, .z64 byte order: the ROM Kaze's patch is made for

typedef enum {
    ML_ROM_ORDER_NONE,  // not an N64 ROM
    ML_ROM_ORDER_Z64,   // big-endian
    ML_ROM_ORDER_V64,   // 16-bit byte-swapped
    ML_ROM_ORDER_N64,   // 32-bit little-endian
} MlRomOrder;

MlRomOrder MlRom_Order(const uint8_t* data, size_t size);
// data in place as .z64 order (it was in `order`).
void MlRom_ToZ64(uint8_t* data, size_t size, MlRomOrder order);
void MlRom_Swap16(uint8_t* data, size_t size);
void MlRom_Swap32(uint8_t* data, size_t size);

// A BPS patch's footer: the CRC-32s of the ROM it is made for and of the one it makes.
bool MlBps_Crcs(const uint8_t* patch, size_t size, uint32_t* source, uint32_t* target);
// The patched ROM (malloc'd, CRCs checked), or NULL.
uint8_t* MlBps_Apply(const uint8_t* source, size_t sourceSize, const uint8_t* patch, size_t patchSize,
                     size_t* targetSize);

typedef struct {
    uint8_t* data;
    uint32_t size;
} MlRomFile;

typedef struct {
    MlRomFile* files;       // by ML's dmadata index; data NULL where no file is
    uint32_t count;
    MlRomFile* views[5];    // the ROM views (ROM_VIEWS in deltapack.py), made as they are asked for
} MlRomFiles;

// ML's files from its ROM (.z64 order, CRC checked by the caller), decompressed.
bool MlRomFiles_Load(MlRomFiles* files, const uint8_t* rom, size_t size);
// The bytes at a ROM view path ("missinglink-rom/<view>/<index>"), or NULL if it names none.
const MlRomFile* MlRomFiles_View(MlRomFiles* files, const char* path);
void MlRomFiles_Free(MlRomFiles* files);

#define ML_ROM_PREFIX "missinglink-rom/"

#endif
