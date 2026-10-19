/*
 * The Missing Link's fairy (ledger row 10), as a code mod.
 *
 * The same code as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_Elf),
 * registered through the mod API.
 *
 * ML dma 147, in vanilla's unused slot 0x001, is a copy of En_Elf's overlay with one call removed: a Kokiri-type fairy
 * no longer steers toward its parent (func_80A02C98 in func_80A0353C is a nop). ML's own changes to En_Elf (Navi
 * hidden, other fairies parked once Link has the Soul-Charged sword) are not in the copy. En_Elf keeps no state of its
 * own outside the instance and sets that action only at Init, so this reuses the vanilla actor and swaps in the one
 * changed action. Ported from MissingShip's SoH fork (Ml_Elf in ovl_En_Elf); spawned by name by ML_Fishpond as a
 * Navi-type fairy. ML's Volvagia's fight (Ml_BossVa, not yet ported) spawns four Kokiri-type ones as its children.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Elf/z_en_elf.h"

#include <stdio.h>

#include "missinglink.h"

ML_HOST_FN_DECL(func_80A0353C);

// vanilla's, from z_en_elf.c
void EnElf_Init(Actor* thisx, PlayState* play);
void EnElf_Destroy(Actor* thisx, PlayState* play);
void EnElf_Update(Actor* thisx, PlayState* play);
void EnElf_Draw(Actor* thisx, PlayState* play);
void EnElf_SetupAction(EnElf* this, EnElfActionFunc actionFunc);
void func_80A0353C(EnElf* this, PlayState* play);
void func_80A02A20(EnElf* this, PlayState* play);

static void MlElf_KokiriFollow(EnElf* this, PlayState* play);

// Vanilla's Init; a Kokiri-type fairy then takes ML's action in place of vanilla's
static void MlElf_Init(Actor* thisx, PlayState* play) {
    EnElf* this = (EnElf*)thisx;

    EnElf_Init(thisx, play);
    if (ML_IS_HOST_FN(this->actionFunc, func_80A0353C)) {
        EnElf_SetupAction(this, MlElf_KokiriFollow);
    }
}

// Vanilla's func_80A0353C, with ML's patch: the call that steers the fairy toward its parent is gone, so it keeps
// only the velocity its parent gives it
static void MlElf_KokiriFollow(EnElf* this, PlayState* play) {
    Actor* parent;

    SkelAnime_Update(&this->skelAnime);
    func_80A02A20(this, play);
    parent = this->actor.parent;

    if ((parent == NULL) || (parent->update == NULL)) {
        Actor_Kill(&this->actor);
    }

    this->unk_2BC = Math_Atan2S(this->actor.velocity.z, this->actor.velocity.x);
}

void MlElf_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_Elf";
    definition.description = "The Missing Link: fairy";
    definition.category = ACTORCAT_ITEMACTION;
    definition.actorFlags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED | ACTOR_FLAG_UPDATE_DURING_OCARINA;
    definition.objectId = OBJECT_GAMEPLAY_KEEP;
    definition.instanceSize = sizeof(EnElf);
    definition.init = MlElf_Init;
    definition.destroy = EnElf_Destroy;
    definition.update = EnElf_Update;
    definition.draw = EnElf_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_Elf is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
