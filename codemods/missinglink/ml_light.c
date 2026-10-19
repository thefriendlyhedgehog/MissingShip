/*
 * The Missing Link's flame, as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_Light), registered through the mod API.
 *
 * Description: The Missing Link's flame (ML actor 0x022), which ML_BossVa spreads over its body as it dies
 *
 * ML dma 172, in a slot vanilla leaves unset, is a copy of En_Light's overlay whose Init skips both
 * Lights_Point*SetInfo calls, so the light it inserts keeps a zeroed info (no radius until the update's colour and
 * radius, at the origin). Everything else is vanilla's En_Light. Ported from MissingShip's SoH fork (Ml_Light in
 * ovl_En_Light).
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Light/z_en_light.h"
#include <string.h>

#include <stdio.h>

#include "missinglink.h"

// vanilla's, from z_en_light.c
void EnLight_Init(Actor* thisx, PlayState* play);
void EnLight_Destroy(Actor* thisx, PlayState* play);
void EnLight_Update(Actor* thisx, PlayState* play);
void EnLight_Draw(Actor* thisx, PlayState* play);



// Vanilla's Init, then the info it set is cleared: an instance starts zeroed, which is all ML's copy leaves it
static void MlLight_Init(Actor* thisx, PlayState* play) {
    EnLight* this = (EnLight*)thisx;

    EnLight_Init(thisx, play);
    memset(&this->lightInfo, 0, sizeof(this->lightInfo));
}

void MlLight_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_Light";
    definition.description = "The Missing Link: flame";
    definition.category = ACTORCAT_ITEMACTION;
    definition.actorFlags = 0;
    definition.objectId = OBJECT_GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnLight);
    definition.init = MlLight_Init;
    definition.destroy = EnLight_Destroy;
    definition.update = EnLight_Update;
    definition.draw = EnLight_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_Light is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
