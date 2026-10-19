/*
 * The Missing Link's breakable wall (ledger row 38), as a code mod.
 *
 * ML's Bg_Hidan_Kowarerukabe draws one model and collision of its own for all three wall types, from object 0xA3
 * (object_ml_crackwall in ML's layer), which ML's rooms load instead of the Fire Temple's objects. MissingShip's Unbound
 * branch makes it a type of the vanilla actor with those assets swapped, `ml/crack_wall`; stock Unbound has no types
 * of vanilla actors, so this registers the same thing under that name: vanilla's code for breaking (its Update and
 * Destroy, and its collider set-up), with ML's model and collision. ML's rooms place it by name (the builder renames
 * ML's placements of the vanilla actor), so the Fire Temple's walls are unchanged.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "align_asset_macro.h"
#include "overlays/actors/ovl_Bg_Hidan_Kowarerukabe/z_bg_hidan_kowarerukabe.h"

#include <stdio.h>

#include "missinglink.h"

#define dgMlCrackWallDL "__OTR__objects/object_ml_crackwall/gMlCrackWallDL"
static const ALIGN_ASSET(2) char gMlCrackWallDL[] = dgMlCrackWallDL;
#define dgMlCrackWallCol "__OTR__objects/object_ml_crackwall/gMlCrackWallCol"
static const ALIGN_ASSET(2) char gMlCrackWallCol[] = dgMlCrackWallCol;

#define ML_OBJECT_CRACKWALL 0xA3    // vanilla's OBJECT_RU1 slot, which ML's ROM fills with its wall

// vanilla's, from z_bg_hidan_kowarerukabe.c
void BgHidanKowarerukabe_Destroy(Actor* thisx, PlayState* play);
void BgHidanKowarerukabe_Update(Actor* thisx, PlayState* play);
void BgHidanKowarerukabe_InitColliderSphere(BgHidanKowarerukabe* this, PlayState* play);
void BgHidanKowarerukabe_OffsetActorYPos(BgHidanKowarerukabe* this);

static InitChainEntry sInitChain[] = {
    ICHAIN_F32(uncullZoneForward, 2000, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneScale, 400, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneDownward, 1000, ICHAIN_STOP),
};

static void MlCrackWall_Init(Actor* thisx, PlayState* play) {
    BgHidanKowarerukabe* this = (BgHidanKowarerukabe*)thisx;
    CollisionHeader* colHeader = NULL;

    DynaPolyActor_Init(&this->dyna, DPM_UNK);
    CollisionHeader_GetVirtual((void*)gMlCrackWallCol, &colHeader);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, colHeader);

    // types 0-2: the cracked floor, the wall, the large wall (they differ in collider sphere and height offset)
    if ((thisx->params & 0xFF) > 2) {
        Actor_Kill(thisx);
        return;
    }
    if (Flags_GetSwitch(play, (thisx->params >> 8) & 0x3F)) {
        Actor_Kill(thisx);
        return;
    }

    Actor_ProcessInitChain(thisx, sInitChain);
    Actor_SetScale(thisx, 0.1f);
    BgHidanKowarerukabe_InitColliderSphere(this, play);
    BgHidanKowarerukabe_OffsetActorYPos(this);
}

static void MlCrackWall_Draw(Actor* thisx, PlayState* play) {
    BgHidanKowarerukabe* this = (BgHidanKowarerukabe*)thisx;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gMlCrackWallDL);

    Collider_UpdateSpheres(0, &this->collider);

    CLOSE_DISPS(play->state.gfxCtx);
}


void MlCrackWall_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ml/crack_wall";
    definition.description = "The Missing Link: breakable wall";
    definition.category = ACTORCAT_BG;
    definition.actorFlags = 0;
    definition.objectId = ML_OBJECT_CRACKWALL;
    definition.instanceSize = sizeof(BgHidanKowarerukabe);
    definition.init = MlCrackWall_Init;
    definition.destroy = BgHidanKowarerukabe_Destroy;
    definition.update = BgHidanKowarerukabe_Update;
    definition.draw = MlCrackWall_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ml/crack_wall is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
