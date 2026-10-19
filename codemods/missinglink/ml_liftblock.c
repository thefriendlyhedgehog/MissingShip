/*
 * The Missing Link's barrier block (ledger row 12), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_LiftBlock),
 * registered through the mod API.
 *
 * ML replaces vanilla's Forest Temple pillar (1344 -> 304 bytes of code) with a static block that
 * shares the lift model (object slot 0x11D). The actor table points slot 0xE3 here; vanilla's
 * z_bg_mori_hashira4.c is untouched.
 *
 * Behaviour: solid, and it removes itself once the player holds any quest item in bits 16-23 of
 * questItems -- Song of Time, Song of Storms, the three Spiritual Stones, the Stone of Agony (ML's Birth
 * Certificate), the Gerudo Card or a Skull Token. ML tests that whole byte (gSaveContext + 0xA5), so the
 * check is kept byte-wide here rather than narrowed to a guess at the one item it was meant for.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "align_asset_macro.h"

#include <stdio.h>

#include "missinglink.h"

typedef struct MlLiftBlock {
    /* 0x0000 */ DynaPolyActor dyna;
} MlLiftBlock; // ML instanceSize = 0x164

#define dgMlLiftCol "__OTR__objects/object_ml_lift/gMlLiftCol"
static const ALIGN_ASSET(2) char gMlLiftCol[] = dgMlLiftCol;
#define dgMlLiftDL "__OTR__objects/object_ml_lift/gMlLiftDL"
static const ALIGN_ASSET(2) char gMlLiftDL[] = dgMlLiftDL;

static void MlLiftBlock_Init(Actor* thisx, PlayState* play) {
    MlLiftBlock* this = (MlLiftBlock*)thisx;
    CollisionHeader* colHeader = NULL;

    Actor_SetScale(thisx, 0.1f);
    thisx->uncullZoneForward = 5000.0f;
    DynaPolyActor_Init(&this->dyna, DPM_PLAYER);
    CollisionHeader_GetVirtual((void*)gMlLiftCol, &colHeader);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, colHeader);
}

static void MlLiftBlock_Destroy(Actor* thisx, PlayState* play) {
    MlLiftBlock* this = (MlLiftBlock*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
}

static void MlLiftBlock_Update(Actor* thisx, PlayState* play) {
    if ((gSaveContext.inventory.questItems >> 16) & 0xFF) {
        Actor_Kill(thisx);
    }
}

static void MlLiftBlock_Draw(Actor* thisx, PlayState* play) {
    Gfx_DrawDListOpa(play, (Gfx*)gMlLiftDL);
}

void MlLiftBlock_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_LiftBlock";
    definition.description = "The Missing Link: barrier block";
    definition.category = ACTORCAT_PROP;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = OBJECT_D_LIFT;
    definition.instanceSize = sizeof(MlLiftBlock);
    definition.init = MlLiftBlock_Init;
    definition.destroy = MlLiftBlock_Destroy;
    definition.update = MlLiftBlock_Update;
    definition.draw = MlLiftBlock_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_LiftBlock is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
