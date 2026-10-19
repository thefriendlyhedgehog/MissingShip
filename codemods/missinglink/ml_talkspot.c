/*
 * The Missing Link's talk spot (ledger row 62), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_TalkSpot),
 * registered through the mod API.
 *
 * Source overlay: ML dma 203, text 0x80. Init sets the focus 100 units up and text 0x6026; Update offers the
 * conversation within 150 units. Nothing is drawn. Placed once in spot01 (params 0xFFFF, unused).
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"

#include <stdio.h>

#include "missinglink.h"

typedef struct MlTalkSpot {
    Actor actor;
} MlTalkSpot; // ML instanceSize = 0x14C

static void MlTalkSpot_Init(Actor* thisx, PlayState* play) {
    Actor_SetFocus(thisx, 100.0f);
    thisx->textId = 0x6026;
}

static void MlTalkSpot_Destroy(Actor* thisx, PlayState* play) {
}

static void MlTalkSpot_Update(Actor* thisx, PlayState* play) {
    Actor_OfferTalk(thisx, play, 150.0f); // offer to talk within 150 units
}

static void MlTalkSpot_Draw(Actor* thisx, PlayState* play) {
}

void MlTalkSpot_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_TalkSpot";
    definition.description = "The Missing Link: talk spot";
    definition.category = ACTORCAT_PROP;
    definition.actorFlags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_UPDATE_CULLING_DISABLED;
    definition.objectId = OBJECT_GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(MlTalkSpot);
    definition.init = MlTalkSpot_Init;
    definition.destroy = MlTalkSpot_Destroy;
    definition.update = MlTalkSpot_Update;
    definition.draw = MlTalkSpot_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_TalkSpot is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
