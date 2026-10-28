/*
 * The Missing Link's dungeon maps and its missing overworld minimaps, as a code mod.
 *
 * ML redraws the minimaps and pause maps of its dungeons (room textures, compass offsets, floors, palettes, chest and
 * boss marks) and has no overworld minimap. MissingShip's Unbound branch reads these from unbound/maps.json
 * (MapRegistry.cpp, Unbound SPEC §7.8) and patches vanilla's map tables, with hooks in z_map_exp.c, z_map_mark.c and
 * z_lmap_mark.c for the textures, the marks and the minimap. A stock Unbound reads none of it. The tables are the
 * game's own (gMapDataTable points at them), so this patches them as the branch does, from the same document (the
 * layer's missinglink/maps, ml_json.h). Where the branch hooks:
 * - room textures: the minimap draws minimapTableDangeon[the dungeon's offset + room]. While a dungeon with ML's
 *   textures is loaded, its offset is 0 and the table's first 44 slots hold its rooms' textures; the table is put
 *   back when the scene ends.
 * - minimap marks: the dungeon's row of the mark tables points at a copy with ML's marks, covering its rooms.
 * - pause-map marks: ML's floors' entries of the pause mark tables are rewritten in place.
 * - no minimap: while a scene ML gives none is drawn, the minimap is off and L does not toggle it, unless the player
 *   placed it (HUD editor).
 */
// z64map_mark.h (through z64.h) declares these without HOST_DATA, which Windows needs for data the game exports
// (below)
#define gMapMarkDataTableVanilla MlUndeclaredMapMarkDataTableVanilla
#define gMapMarkDataTableMq MlUndeclaredMapMarkDataTableMq
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64map_mark.h"
#include <libultraship/bridge/consolevariablebridge.h>

#include <stdlib.h>
#include <string.h>

#include "missinglink.h"
#include "ml_json.h"
#undef gMapMarkDataTableVanilla
#undef gMapMarkDataTableMq

#define ML_DUNGEONS 10
#define ML_BOSS_DUNGEONS 8
#define ML_ROOMS 44
#define ML_PALETTE_ROOMS 32
#define ML_FLOORS 8
#define ML_PALETTES 14
#define ML_SWITCHES 51
#define ML_MARK_POINTS 12
#define ML_NO_ROOM 0xFF
#define ML_PAUSE_MARK_ENTRIES 34 // z_lmap_mark_data.c: each table's floor maps
#define ML_MINIMAP_TEXTURES 239  // z_map_exp.c minimapTableDangeon

extern HOST_DATA MapData gMapDataTable;
extern HOST_DATA MapMarkData* gMapMarkDataTableVanilla[];
extern HOST_DATA MapMarkData* gMapMarkDataTableMq[];
extern HOST_DATA const char* minimapTableDangeon[ML_MINIMAP_TEXTURES];
extern HOST_DATA PauseMapMarksData gPauseMapMarkDataTable[ML_PAUSE_MARK_ENTRIES];
extern HOST_DATA PauseMapMarksData gPauseMapMarkDataTableMasterQuest[ML_PAUSE_MARK_ENTRIES];

static const char* sMlFloorNames[16] = { "8F", "7F", "6F", "5F", "4F", "3F", "2F", "1F",
                                          "B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8" };

static char* sMlTextures[ML_DUNGEONS][ML_ROOMS]; // "__OTR__" + path
static u8 sMlHasTextures[ML_DUNGEONS];
static u8 sMlNoMinimap[SCENE_ID_MAX];

// --- marks ---

// Marks are grouped by type in order of first appearance, then MAP_MARK_NONE: vanilla's three-group layout.

static s16 MlMaps_MarkType(const MlJson* mark) {
    const char* type = MlJson_Str(MlJson_Get(mark, "type"), "");

    return (strcmp(type, "chest") == 0) ? MAP_MARK_CHEST : (strcmp(type, "boss") == 0) ? MAP_MARK_BOSS : MAP_MARK_NONE;
}

static void MlMaps_FillMinimapGroups(MapMarkIconData* groups, const MlJson* marks) {
    const MlJson* first = (marks != NULL) ? marks->child : NULL;
    s16 firstType = (first != NULL) ? MlMaps_MarkType(first) : MAP_MARK_NONE;
    s16 types[2] = { firstType, (firstType == MAP_MARK_NONE) ? MAP_MARK_NONE : (s16)(1 - firstType) };
    s32 count = 0;

    for (s32 t = 0; t < 2; t++) {
        MapMarkIconData* group = &groups[count];
        s32 points = 0;

        if (types[t] == MAP_MARK_NONE) {
            continue;
        }
        ML_JSON_FOR_EACH(mark, marks) {
            if ((MlMaps_MarkType(mark) == types[t]) && (points < ML_MARK_POINTS)) {
                group->points[points].chestFlag = MlJson_Int(MlJson_Get(mark, "flag"), -1);
                group->points[points].x = MlJson_Int(MlJson_Get(mark, "x"), 0);
                group->points[points].y = MlJson_Int(MlJson_Get(mark, "y"), 0);
                points++;
            }
        }
        if (points > 0) {
            group->markType = types[t];
            group->count = points;
            count++;
        }
    }
    for (; count < 3; count++) {
        memset(&groups[count], 0, sizeof(groups[count]));
        groups[count].markType = MAP_MARK_NONE;
    }
}

static void MlMaps_FillPauseGroups(PauseMapMarkData* groups, const MlJson* marks, const PauseMapMarkData* templates) {
    const MlJson* first = (marks != NULL) ? marks->child : NULL;
    s16 firstType = (first != NULL) ? MlMaps_MarkType(first) : MAP_MARK_NONE;
    s16 types[2] = { firstType, (firstType == MAP_MARK_NONE) ? MAP_MARK_NONE : (s16)(1 - firstType) };
    s32 count = 0;

    for (s32 t = 0; t < 2; t++) {
        PauseMapMarkData group;
        s32 points = 0;

        if (types[t] == MAP_MARK_NONE) {
            continue;
        }
        // a group's vertices and size come from a group of the same type in the table
        group = templates[types[t]];
        ML_JSON_FOR_EACH(mark, marks) {
            if ((MlMaps_MarkType(mark) == types[t]) && (points < ML_MARK_POINTS)) {
                group.points[points].chestFlag = MlJson_Int(MlJson_Get(mark, "flag"), -1);
                group.points[points].x = (f32)MlJson_Get(mark, "x")->num;
                group.points[points].y = (f32)MlJson_Get(mark, "y")->num;
                points++;
            }
        }
        if (points > 0) {
            group.markType = types[t];
            group.count = points;
            groups[count++] = group;
        }
    }
    for (; count < 3; count++) {
        memset(&groups[count], 0, sizeof(groups[count]));
        groups[count].markType = PAUSE_MAP_MARK_NONE;
    }
}

static s32 MlMaps_MarksValid(const MlJson* marks) {
    ML_JSON_FOR_EACH(mark, marks) {
        if ((MlMaps_MarkType(mark) == MAP_MARK_NONE) || (MlJson_Get(mark, "x") == NULL) ||
            (MlJson_Get(mark, "y") == NULL)) {
            return false;
        }
    }
    return (marks != NULL) && (marks->type == ML_JSON_ARRAY);
}

// The dungeon's row of a minimap mark table, copied to cover `rooms` rooms, with ML's marks
static MapMarkData* MlMaps_MarkRow(MapMarkData* vanilla, s32 vanillaRooms, s32 rooms, const MlJson* roomDocs) {
    MapMarkData* row = calloc(rooms, sizeof(MapMarkData));

    if (row == NULL) {
        return vanilla;
    }
    for (s32 r = 0; r < rooms; r++) {
        if ((r < vanillaRooms) && (vanilla != NULL)) {
            memcpy(&row[r], &vanilla[r], sizeof(MapMarkData));
        } else {
            MlMaps_FillMinimapGroups(row[r], NULL);
        }
    }
    ML_JSON_FOR_EACH(room, roomDocs) {
        s32 r = atoi(room->key);
        const MlJson* marks = MlJson_Get(room, "marks");

        if ((r >= 0) && (r < rooms) && MlMaps_MarksValid(marks)) {
            MlMaps_FillMinimapGroups(row[r], marks);
        }
    }
    return row;
}

// --- the tables ---

static void MlMaps_ApplyDungeon(s32 d, const MlJson* doc, const PauseMapMarkData* templates,
                                const PauseMapMarkData* templatesMq) {
    MapData* m = &gMapDataTable;
    const MlJson* rooms = MlJson_Get(doc, "rooms");
    const MlJson* value;
    s32 lastMarkedRoom = -1;
    s32 vanillaRooms = m->dgnMinimapCount[d];

    ML_PATCH_WATCH(m->roomCompassOffsetX[d]);
    ML_PATCH_WATCH(m->roomCompassOffsetY[d]);
    ML_PATCH_WATCH(m->roomPalette[d]);
    ML_PATCH_WATCH(m->dgnMinimapCount[d]);
    ML_PATCH_WATCH(m->floorID[d]);
    ML_PATCH_WATCH(m->floorCoordY[d]);
    ML_PATCH_WATCH(m->paletteRoom[d]);
    ML_PATCH_WATCH(m->dgnCompassInfo[d]);
    ML_PATCH_WATCH(m->maxPaletteCount[d]);
    if (d < ML_BOSS_DUNGEONS) {
        ML_PATCH_WATCH(m->bossFloor[d]);
    }
    ML_PATCH_WATCH(m->switchEntryCount[d]);
    ML_PATCH_WATCH(m->switchFromRoom[d]);
    ML_PATCH_WATCH(m->switchFromFloor[d]);
    ML_PATCH_WATCH(m->switchToRoom[d]);
    ML_PATCH_WATCH(gMapMarkDataTableVanilla[d]);
    ML_PATCH_WATCH(gMapMarkDataTableMq[d]);

    ML_JSON_FOR_EACH(room, rooms) {
        s32 r = atoi(room->key);
        const MlJson* compass = MlJson_Get(room, "compass");
        const char* texture = MlJson_Str(MlJson_Get(room, "texture"), NULL);

        if ((r < 0) || (r >= ML_ROOMS)) {
            continue;
        }
        if (texture != NULL) {
            sMlTextures[d][r] = malloc(strlen(texture) + 8);
            strcpy(sMlTextures[d][r], "__OTR__");
            strcat(sMlTextures[d][r], texture);
            sMlHasTextures[d] = true;
        }
        if ((compass != NULL) && (compass->count == 2)) {
            m->roomCompassOffsetX[d][r] = MlJson_Int(MlJson_At(compass, 0), 0);
            m->roomCompassOffsetY[d][r] = MlJson_Int(MlJson_At(compass, 1), 0);
        }
        if (((value = MlJson_Get(room, "palette")) != NULL) && (r < ML_PALETTE_ROOMS)) {
            m->roomPalette[d][r] = MlJson_Int(value, 0);
        }
        if (MlMaps_MarksValid(MlJson_Get(room, "marks")) && (r > lastMarkedRoom)) {
            lastMarkedRoom = r;
        }
    }
    // a mark on a room past vanilla's count would never draw (MapMark_DrawForDungeon)
    if (lastMarkedRoom + 1 > m->dgnMinimapCount[d]) {
        m->dgnMinimapCount[d] = lastMarkedRoom + 1;
    }
    if (lastMarkedRoom >= 0) {
        gMapMarkDataTableVanilla[d] =
            MlMaps_MarkRow(gMapMarkDataTableVanilla[d], vanillaRooms, m->dgnMinimapCount[d], rooms);
        gMapMarkDataTableMq[d] = MlMaps_MarkRow(gMapMarkDataTableMq[d], vanillaRooms, m->dgnMinimapCount[d], rooms);
    }

    ML_JSON_FOR_EACH(floor, MlJson_Get(doc, "floors")) {
        s32 slot = atoi(floor->key);
        const MlJson* paletteRooms = MlJson_Get(floor, "paletteRooms");
        const MlJson* marks = MlJson_Get(floor, "marks");

        if ((slot < 0) || (slot >= ML_FLOORS)) {
            continue;
        }
        if ((value = MlJson_Get(floor, "id")) != NULL) {
            const char* name = MlJson_Str(value, "");

            m->floorID[d][slot] = 0;
            for (s32 i = 0; i < 16; i++) {
                if (strcmp(name, sMlFloorNames[i]) == 0) {
                    m->floorID[d][slot] = i + 1;
                }
            }
        }
        if ((value = MlJson_Get(floor, "y")) != NULL) {
            m->floorCoordY[d][slot] = (f32)value->num;
        }
        if ((paletteRooms != NULL) && (paletteRooms->count <= ML_PALETTES)) {
            for (s32 i = 0; i < ML_PALETTES; i++) {
                m->paletteRoom[d][slot][i] = MlJson_Int(MlJson_At(paletteRooms, i), ML_NO_ROOM);
            }
        }
        if (MlMaps_MarksValid(marks)) {
            // the pause map draws a floor's marks by its texture pair (z_lmap_mark.c, R_MAP_TEX_INDEX >> 1)
            s32 entry = (m->dgnMinimapTexIndexBase[d] + m->floorTexIndexOffset[d][slot]) >> 1;

            if ((m->floorID[d][slot] == 0) || (entry < 0) || (entry >= ML_PAUSE_MARK_ENTRIES)) {
                LUSLOG_ERROR("[MissingLink] maps: dungeon %d floor slot %d has no floor; its marks are ignored", d,
                             slot);
            } else {
                MlMaps_FillPauseGroups(gPauseMapMarkDataTable[entry], marks, templates);
                MlMaps_FillPauseGroups(gPauseMapMarkDataTableMasterQuest[entry], marks, templatesMq);
            }
        }
    }
    if (((value = MlJson_Get(doc, "compass")) != NULL)) {
        const MlJson* scale = MlJson_Get(value, "scale");
        const MlJson* offset = MlJson_Get(value, "offset");

        if ((scale != NULL) && (scale->count == 2) && (offset != NULL) && (offset->count == 2)) {
            m->dgnCompassInfo[d][0] = MlJson_Int(MlJson_At(scale, 0), 0);
            m->dgnCompassInfo[d][1] = MlJson_Int(MlJson_At(scale, 1), 0);
            m->dgnCompassInfo[d][2] = MlJson_Int(MlJson_At(offset, 0), 0);
            m->dgnCompassInfo[d][3] = MlJson_Int(MlJson_At(offset, 1), 0);
        }
    }
    if ((value = MlJson_Get(doc, "maxPalettes")) != NULL) {
        m->maxPaletteCount[d] = MlJson_Int(value, m->maxPaletteCount[d]);
    }
    if (((value = MlJson_Get(doc, "bossFloor")) != NULL) && (d < ML_BOSS_DUNGEONS)) {
        m->bossFloor[d] = MlJson_Int(value, m->bossFloor[d]);
    }
    if (((value = MlJson_Get(doc, "roomSwitches")) != NULL) && (value->count <= ML_SWITCHES)) {
        m->switchEntryCount[d] = value->count;
        for (s32 i = 0; i < ML_SWITCHES; i++) {
            const MlJson* entry = MlJson_At(value, i);

            m->switchFromRoom[d][i] = MlJson_Int(MlJson_Get(entry, "fromRoom"), ML_NO_ROOM);
            m->switchFromFloor[d][i] = MlJson_Int(MlJson_Get(entry, "fromFloor"), ML_NO_ROOM);
            m->switchToRoom[d][i] = MlJson_Int(MlJson_Get(entry, "toRoom"), ML_NO_ROOM);
        }
    }
}

// The first chest and boss group of a pause mark table, before any change
static void MlMaps_PauseTemplates(PauseMapMarksData* table, PauseMapMarkData* templates) {
    u8 found[2] = { false, false };

    for (s32 i = 0; i < ML_PAUSE_MARK_ENTRIES; i++) {
        for (s32 g = 0; g < 3; g++) {
            s16 type = table[i][g].markType;

            if (((type == PAUSE_MAP_MARK_CHEST) || (type == PAUSE_MAP_MARK_BOSS)) && !found[type]) {
                templates[type] = table[i][g];
                found[type] = true;
            }
        }
    }
}

static void MlMaps_Load(void) {
    const MlJson* doc = MlJson_Load("maps");
    PauseMapMarkData templates[2] = { 0 };
    PauseMapMarkData templatesMq[2] = { 0 };
    s32 dungeons = 0;
    s32 noMinimap = 0;

    ML_PATCH_WATCH(gPauseMapMarkDataTable);
    ML_PATCH_WATCH(gPauseMapMarkDataTableMasterQuest);
    MlMaps_PauseTemplates(gPauseMapMarkDataTable, templates);
    MlMaps_PauseTemplates(gPauseMapMarkDataTableMasterQuest, templatesMq);
    ML_JSON_FOR_EACH(scene, doc) {
        s32 sceneNum = atoi(scene->key);
        const MlJson* minimap = MlJson_Get(scene, "minimap");

        if ((sceneNum < 0) || (sceneNum >= SCENE_ID_MAX)) {
            continue;
        }
        if ((minimap != NULL) && (minimap->type == ML_JSON_BOOL) && !MlJson_IsTrue(minimap)) {
            sMlNoMinimap[sceneNum] = true;
            noMinimap++;
        }
        if ((sceneNum >= SCENE_DEKU_TREE) && (sceneNum <= SCENE_ICE_CAVERN) && (scene->count > (minimap != NULL))) {
            MlMaps_ApplyDungeon(sceneNum - SCENE_DEKU_TREE, scene, templates, templatesMq);
            dungeons++;
        }
    }
    LUSLOG_INFO("[MissingLink] maps: %d dungeon map(s) changed, %d scene(s) without a minimap", dungeons, noMinimap);
}

// --- room textures ---

static s32 sMlTexturesDungeon = -1; // the dungeon whose textures are in the table's first slots
static u16 sMlTexturesOffset;
static const char* sMlTexturesSaved[ML_ROOMS];

static void MlMaps_PutTexturesBack(void) {
    if (sMlTexturesDungeon < 0) {
        return;
    }
    memcpy(minimapTableDangeon, sMlTexturesSaved, sizeof(sMlTexturesSaved));
    gMapDataTable.dgnMinimapTexIndexOffset[sMlTexturesDungeon] = sMlTexturesOffset;
    sMlTexturesDungeon = -1;
}

// Play_SpawnScene, before Map_Init loads the first room's minimap
static void MlMaps_OnSceneInit(int16_t sceneNum) {
    s32 d = sceneNum - SCENE_DEKU_TREE;
    const char* textures[ML_ROOMS];
    u16 offset;

    MlMaps_PutTexturesBack();
    if ((sceneNum < SCENE_DEKU_TREE) || (sceneNum > SCENE_ICE_CAVERN) || !sMlHasTextures[d]) {
        return;
    }
    offset = gMapDataTable.dgnMinimapTexIndexOffset[d];
    for (s32 r = 0; r < ML_ROOMS; r++) {
        textures[r] = (sMlTextures[d][r] != NULL)               ? sMlTextures[d][r]
                      : (offset + r < ML_MINIMAP_TEXTURES) ? minimapTableDangeon[offset + r]
                                                               : minimapTableDangeon[offset];
    }
    memcpy(sMlTexturesSaved, minimapTableDangeon, sizeof(sMlTexturesSaved));
    memcpy(minimapTableDangeon, textures, sizeof(textures));
    sMlTexturesOffset = offset;
    gMapDataTable.dgnMinimapTexIndexOffset[d] = 0;
    sMlTexturesDungeon = d;
}

static void MlMaps_OnPlayDestroy(void) {
    MlMaps_PutTexturesBack();
}

// --- no minimap ---

static u8 sMlMinimapHidden;
static s32 sMlMinimapWasDisabled;
static u16 sMlLPressed;

static void MlMaps_OnPlayDrawBegin(void) {
    PlayState* play = gPlayState;

    if ((play == NULL) || (play->sceneNum < 0) || (play->sceneNum >= SCENE_ID_MAX) || !sMlNoMinimap[play->sceneNum] ||
        (CVarGetInteger("gCosmetics.HUD.Minimap.PosType", -1) != -1)) {
        return;
    }
    sMlMinimapHidden = true;
    sMlMinimapWasDisabled = R_MINIMAP_DISABLED;
    R_MINIMAP_DISABLED = 1;
    // Minimap_Draw toggles it on L
    sMlLPressed = play->state.input[0].press.button & BTN_L;
    play->state.input[0].press.button &= ~BTN_L;
}

// After the frame: OnPlayDrawEnd comes before the interface is drawn (and not at all while paused)
static void MlMaps_OnGameFrameUpdate(void) {
    if (!sMlMinimapHidden || (gPlayState == NULL)) {
        return;
    }
    R_MINIMAP_DISABLED = sMlMinimapWasDisabled;
    gPlayState->state.input[0].press.button |= sMlLPressed;
    sMlMinimapHidden = false;
}

void MlMaps_Register(const SOHModApi* api) {
    MlMaps_Load();
    SOH_REGISTER_HOOK(api, OnSceneInit, MlMaps_OnSceneInit);
    SOH_REGISTER_HOOK(api, OnPlayDestroy, MlMaps_OnPlayDestroy);
    SOH_REGISTER_HOOK(api, OnPlayDrawBegin, MlMaps_OnPlayDrawBegin);
    SOH_REGISTER_HOOK(api, OnGameFrameUpdate, MlMaps_OnGameFrameUpdate);
    api->Log("[MissingLink] ML's dungeon maps and its missing overworld minimaps");
}
