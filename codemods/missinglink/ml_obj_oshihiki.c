/*
 * The Missing Link's push-block colours (ledger row 39), as a code mod.
 *
 * ML rewrites two rows of Obj_Oshihiki's colour table (overlay data +0x40 and +0x88), all four entries of each: the
 * Deku Tree's (its Deep Woods grotto) and the Shadow Temple's (its Hollow Root Keep). MissingShip's Unbound branch
 * swaps the row inside vanilla's ObjOshihiki_SetColor while ML is mounted (soh/soh/missinglink/MlObjOshihiki.c).
 * Vanilla sets the colour once, at Init, from the scene's row, so this takes Obj_Oshihiki's ActorDB entry and sets
 * ML's colour after vanilla's Init. ML_PushBlock (ml_pushblock.c) draws its own model and is not touched.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ActorDB.h"
#include "overlays/actors/ovl_Obj_Oshihiki/z_obj_oshihiki.h"

#include "missinglink.h"

// vanilla's, from z_obj_oshihiki.c
void ObjOshihiki_Init(Actor* thisx, PlayState* play);

static Color_RGB8 sMlDekuTree = { 240, 196, 111 };
static Color_RGB8 sMlShadowTemple = { 154, 240, 111 };

static void MlObjOshihiki_Init(Actor* thisx, PlayState* play) {
    ObjOshihiki* this = (ObjOshihiki*)thisx;

    ObjOshihiki_Init(thisx, play);
    // vanilla's rows: by scene, whatever the block's colour index
    if (play->sceneNum == SCENE_DEKU_TREE) {
        this->color = sMlDekuTree;
    } else if (play->sceneNum == SCENE_SHADOW_TEMPLE) {
        this->color = sMlShadowTemple;
    }
}

void MlObjOshihiki_Register(const SOHModApi* api) {
    ActorDBEntry* entry = ActorDB_Retrieve(ACTOR_OBJ_OSHIHIKI);

    entry->init = MlObjOshihiki_Init;
    api->Log("[MissingLink] Obj_Oshihiki's slot takes ML's colours");
}
