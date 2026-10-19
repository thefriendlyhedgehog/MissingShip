/*
 * A small JSON reader for ML's registries (see ml_json.h). Strings are unescaped in a private copy of the text, and
 * nodes come from malloc; both live as long as the game, as the registries do.
 */
#include "ml_json.h"

#include <libultraship/bridge/resourcebridge.h>
#include <libultraship/log/luslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char* p;
    char* end;
} MlJsonCursor;

static const MlJson* MlJson_ParseValue(MlJsonCursor* c, int32_t depth);

static void MlJson_SkipSpace(MlJsonCursor* c) {
    while ((c->p < c->end) && ((*c->p == ' ') || (*c->p == '\t') || (*c->p == '\n') || (*c->p == '\r'))) {
        c->p++;
    }
}

static MlJson* MlJson_New(MlJsonType type) {
    MlJson* node = calloc(1, sizeof(MlJson));

    if (node != NULL) {
        node->type = type;
    }
    return node;
}

static int32_t MlJson_Hex(const char* s) {
    int32_t v = 0;

    for (int32_t i = 0; i < 4; i++) {
        char ch = s[i];

        v <<= 4;
        if ((ch >= '0') && (ch <= '9')) {
            v |= ch - '0';
        } else if ((ch >= 'a') && (ch <= 'f')) {
            v |= ch - 'a' + 10;
        } else if ((ch >= 'A') && (ch <= 'F')) {
            v |= ch - 'A' + 10;
        } else {
            return -1;
        }
    }
    return v;
}

// A string at c->p (its opening quote), unescaped in place and terminated; NULL when malformed.
static const char* MlJson_ParseString(MlJsonCursor* c) {
    char* out;
    char* start;

    if ((c->p >= c->end) || (*c->p != '"')) {
        return NULL;
    }
    start = out = ++c->p;
    while (c->p < c->end) {
        char ch = *c->p++;

        if (ch == '"') {
            *out = '\0';
            return start;
        }
        if (ch != '\\') {
            *out++ = ch;
            continue;
        }
        if (c->p >= c->end) {
            return NULL;
        }
        ch = *c->p++;
        switch (ch) {
            case 'n': *out++ = '\n'; break;
            case 't': *out++ = '\t'; break;
            case 'r': *out++ = '\r'; break;
            case 'b': *out++ = '\b'; break;
            case 'f': *out++ = '\f'; break;
            case 'u': {
                int32_t cp = ((c->end - c->p) >= 4) ? MlJson_Hex(c->p) : -1;

                if (cp < 0) {
                    return NULL;
                }
                c->p += 4;
                // surrogate pair
                if ((cp >= 0xD800) && (cp < 0xDC00) && ((c->end - c->p) >= 6) && (c->p[0] == '\\') &&
                    (c->p[1] == 'u')) {
                    int32_t lo = MlJson_Hex(c->p + 2);

                    if ((lo >= 0xDC00) && (lo < 0xE000)) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        c->p += 6;
                    }
                }
                // the escape is at least 6 bytes and UTF-8 at most 4, so the text never overtakes the cursor
                if (cp < 0x80) {
                    *out++ = (char)cp;
                } else if (cp < 0x800) {
                    *out++ = (char)(0xC0 | (cp >> 6));
                    *out++ = (char)(0x80 | (cp & 0x3F));
                } else if (cp < 0x10000) {
                    *out++ = (char)(0xE0 | (cp >> 12));
                    *out++ = (char)(0x80 | ((cp >> 6) & 0x3F));
                    *out++ = (char)(0x80 | (cp & 0x3F));
                } else {
                    *out++ = (char)(0xF0 | (cp >> 18));
                    *out++ = (char)(0x80 | ((cp >> 12) & 0x3F));
                    *out++ = (char)(0x80 | ((cp >> 6) & 0x3F));
                    *out++ = (char)(0x80 | (cp & 0x3F));
                }
                break;
            }
            default: *out++ = ch; break; // \" \\ \/
        }
    }
    return NULL;
}

static int32_t MlJson_Match(MlJsonCursor* c, const char* word) {
    size_t n = strlen(word);

    if (((size_t)(c->end - c->p) >= n) && (memcmp(c->p, word, n) == 0)) {
        c->p += n;
        return 1;
    }
    return 0;
}

// The members of an array or object, after its opening bracket.
static const MlJson* MlJson_ParseMembers(MlJsonCursor* c, MlJson* node, char close, int32_t depth) {
    MlJson** link = &node->child;

    MlJson_SkipSpace(c);
    if ((c->p < c->end) && (*c->p == close)) {
        c->p++;
        return node;
    }
    while (c->p < c->end) {
        const char* key = NULL;
        MlJson* member;

        MlJson_SkipSpace(c);
        if (close == '}') {
            key = MlJson_ParseString(c);
            MlJson_SkipSpace(c);
            if ((key == NULL) || (c->p >= c->end) || (*c->p++ != ':')) {
                return NULL;
            }
        }
        member = (MlJson*)MlJson_ParseValue(c, depth + 1);
        if (member == NULL) {
            return NULL;
        }
        member->key = key;
        *link = member;
        link = &member->next;
        node->count++;
        MlJson_SkipSpace(c);
        if (c->p >= c->end) {
            return NULL;
        }
        if (*c->p == ',') {
            c->p++;
        } else if (*c->p == close) {
            c->p++;
            return node;
        } else {
            return NULL;
        }
    }
    return NULL;
}

static const MlJson* MlJson_ParseValue(MlJsonCursor* c, int32_t depth) {
    MlJson* node;

    MlJson_SkipSpace(c);
    if ((c->p >= c->end) || (depth > 64)) {
        return NULL;
    }
    switch (*c->p) {
        case '{':
        case '[': {
            char close = (*c->p == '{') ? '}' : ']';

            c->p++;
            node = MlJson_New((close == '}') ? ML_JSON_OBJECT : ML_JSON_ARRAY);
            return (node != NULL) ? MlJson_ParseMembers(c, node, close, depth) : NULL;
        }
        case '"':
            node = MlJson_New(ML_JSON_STRING);
            if (node != NULL) {
                node->str = MlJson_ParseString(c);
            }
            return ((node != NULL) && (node->str != NULL)) ? node : NULL;
        case 't':
        case 'f':
            node = MlJson_New(ML_JSON_BOOL);
            if ((node != NULL) && MlJson_Match(c, "true")) {
                node->num = 1;
                return node;
            }
            if ((node != NULL) && MlJson_Match(c, "false")) {
                return node;
            }
            return NULL;
        case 'n':
            return MlJson_Match(c, "null") ? MlJson_New(ML_JSON_NULL) : NULL;
        default: {
            char* numEnd;

            node = MlJson_New(ML_JSON_NUMBER);
            if (node == NULL) {
                return NULL;
            }
            // the text is terminated past its end (MlJson_Parse), so strtod stops there at the latest
            node->num = strtod(c->p, &numEnd);
            if ((numEnd == c->p) || (numEnd > c->end)) {
                return NULL;
            }
            c->p = numEnd;
            return node;
        }
    }
}

const MlJson* MlJson_Parse(const char* text, size_t size) {
    MlJsonCursor c;
    char* copy = malloc(size + 1);
    const MlJson* root;

    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, size);
    copy[size] = '\0';
    c.p = copy;
    c.end = copy + size;
    root = MlJson_ParseValue(&c, 0);
    MlJson_SkipSpace(&c);
    return ((root != NULL) && (c.p == c.end)) ? root : NULL;
}

typedef struct {
    const char* name;
    const MlJson* root;
} MlJsonRegistry;

static MlJsonRegistry sMlRegistries[16];

const MlJson* MlJson_Load(const char* name) {
    char path[96];
    const char* data;
    size_t size;
    MlJsonRegistry* entry = NULL;

    for (size_t i = 0; i < sizeof(sMlRegistries) / sizeof(sMlRegistries[0]); i++) {
        if (sMlRegistries[i].name == NULL) {
            entry = &sMlRegistries[i];
            break;
        }
        if (strcmp(sMlRegistries[i].name, name) == 0) {
            return sMlRegistries[i].root;
        }
    }
    snprintf(path, sizeof(path), "missinglink/%s", name);
    data = ResourceGetDataByName(path);
    size = (data != NULL) ? ResourceGetSizeByName(path) : 0;
    // 9.3.0's Blobs carry 16 zero bytes of padding after the data, counted in their size
    while ((size != 0) && (data[size - 1] == '\0')) {
        size--;
    }
    if (entry == NULL) {
        return (size != 0) ? MlJson_Parse(data, size) : NULL;
    }
    entry->name = name;
    entry->root = (size != 0) ? MlJson_Parse(data, size) : NULL;
    if (entry->root == NULL) {
        LUSLOG_ERROR("[MissingLink] %s: %s; The Missing Link's layer for a stock Unbound carries it", path,
                     (size != 0) ? "not valid JSON" : "not found");
    }
    return entry->root;
}

const MlJson* MlJson_Get(const MlJson* object, const char* key) {
    if ((object == NULL) || (object->type != ML_JSON_OBJECT)) {
        return NULL;
    }
    for (const MlJson* m = object->child; m != NULL; m = m->next) {
        if (strcmp(m->key, key) == 0) {
            return m;
        }
    }
    return NULL;
}

const MlJson* MlJson_At(const MlJson* array, int32_t index) {
    const MlJson* m;

    if ((array == NULL) || (array->type != ML_JSON_ARRAY) || (index < 0)) {
        return NULL;
    }
    for (m = array->child; (m != NULL) && (index > 0); m = m->next) {
        index--;
    }
    return m;
}

int32_t MlJson_Int(const MlJson* value, int32_t fallback) {
    return ((value != NULL) && ((value->type == ML_JSON_NUMBER) || (value->type == ML_JSON_BOOL))) ? (int32_t)value->num
                                                                                                  : fallback;
}

const char* MlJson_Str(const MlJson* value, const char* fallback) {
    return ((value != NULL) && (value->type == ML_JSON_STRING)) ? value->str : fallback;
}

int32_t MlJson_IsTrue(const MlJson* value) {
    return MlJson_Int(value, 0) != 0;
}
