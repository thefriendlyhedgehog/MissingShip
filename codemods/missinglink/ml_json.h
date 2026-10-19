/*
 * A small JSON reader for ML's registries (unbound/<name>.json in ML's layer), which a stock Unbound does not read.
 *
 * The MissingShip builder's --stock layer ships each registry a second time as an LUS Blob, missinglink/<name>, so
 * the mod can read its bytes through the resource manager (a .json file in an archive is parsed by LUS as a resource
 * of its own, not handed over as text). MlJson_Load parses it once into a tree that lives for the rest of the game.
 */
#ifndef ML_JSON_H
#define ML_JSON_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ML_JSON_NULL,
    ML_JSON_BOOL,
    ML_JSON_NUMBER,
    ML_JSON_STRING,
    ML_JSON_ARRAY,
    ML_JSON_OBJECT,
} MlJsonType;

typedef struct MlJson {
    MlJsonType type;
    const char* key;      // its key, in an object
    const char* str;      // a string's text (UTF-8, unescaped)
    double num;           // a number, or a bool's 0 / 1
    struct MlJson* child; // an array's or object's first member
    struct MlJson* next;  // the next member of its parent
    int32_t count;        // an array's or object's member count
} MlJson;

// The registry missinglink/<name> (e.g. "items"), parsed once and kept; NULL when the layer has none or it is not
// valid JSON (logged once).
const MlJson* MlJson_Load(const char* name);
// Parses `size` bytes of JSON text; NULL when it is not valid JSON. The tree is never freed.
const MlJson* MlJson_Parse(const char* text, size_t size);

const MlJson* MlJson_Get(const MlJson* object, const char* key); // NULL when not an object or no such key
const MlJson* MlJson_At(const MlJson* array, int32_t index);     // NULL when not an array or out of range
int32_t MlJson_Int(const MlJson* value, int32_t fallback);       // a number or bool, else the fallback
const char* MlJson_Str(const MlJson* value, const char* fallback);
int32_t MlJson_IsTrue(const MlJson* value); // true, or a number other than 0

#define ML_JSON_FOR_EACH(member, parent) \
    for (const MlJson* member = ((parent) != NULL) ? (parent)->child : NULL; member != NULL; member = member->next)

#ifdef __cplusplus
}
#endif

#endif
