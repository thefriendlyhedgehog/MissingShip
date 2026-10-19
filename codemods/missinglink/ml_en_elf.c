/*
 * The Missing Link's changes to vanilla's fairy, En_Elf (ledger row 51), as a code mod.
 *
 * ML's En_Elf is a recompile of vanilla with exactly two edits, both around vanilla's own update and draw: once Link
 * has the Soul-Charged Kokiri Sword, every fairy but Navi is parked far below the level; and Navi herself is never
 * drawn (ML has no Navi). MissingShip's Unbound branch puts these on En_Elf's slot while ML is mounted
 * (soh/soh/missinglink/MlEnElf.c). The game spawns fairies by number, so this takes En_Elf's ActorDB entry the same
 * way. ML's own fairy, ML_Elf (ml_elf.c), is a copy without the edits and calls vanilla's functions directly.
 * ML's player also never spawns a revival fairy (the branch's Player_SpawnFairy edit); that is dropped here too.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ActorDB.h"
#include "overlays/actors/ovl_En_Elf/z_en_elf.h"

#include "missinglink.h"

// vanilla's, from z_en_elf.c
void EnElf_Init(Actor* thisx, PlayState* play);
void EnElf_Update(Actor* thisx, PlayState* play);
void EnElf_Draw(Actor* thisx, PlayState* play);

// ML removes the player's revival fairy spawn (Player_SpawnFairy for anything but Navi), so a bottled fairy or one
// that saves Link from death never appears. Link still heals: the player does that. The player is the only spawner of
// these two types, and ignores what the spawn returns.
static void MlEnElf_Init(Actor* thisx, PlayState* play) {
    if ((thisx->params == FAIRY_REVIVE_BOTTLE) || (thisx->params == FAIRY_REVIVE_DEATH)) {
        Actor_Kill(thisx);
        return;
    }
    EnElf_Init(thisx, play);
}

// ML hook 0x80602000 at the end of EnElf_Update: once Link has the Soul-Charged Kokiri Sword (ML marks it with
// slingshot ammo), every fairy but Navi is parked far below the level. Healing, bottle and Kokiri fairies all run this
// update; Navi switches to her own.
static void MlEnElf_Update(Actor* thisx, PlayState* play) {
    EnElf_Update(thisx, play);
    if (gSaveContext.inventory.ammo[SLOT_SLINGSHOT] != 0) {
        thisx->world.pos.y = -2048.0f;
    }
}

// ML has no Navi. Her draw starts by parking her far below the level and returning (ML En_Elf +0x3C1C / +0x42C8), so
// she is never seen and her light never reaches the level.
static void MlEnElf_Draw(Actor* thisx, PlayState* play) {
    if (thisx->params == FAIRY_NAVI) {
        thisx->world.pos.y = -524288.0f;
        return;
    }
    EnElf_Draw(thisx, play);
}

void MlEnElf_Register(const SOHModApi* api) {
    ActorDBEntry* entry = ActorDB_Retrieve(ACTOR_EN_ELF);

    entry->init = MlEnElf_Init;
    entry->update = MlEnElf_Update;
    entry->draw = MlEnElf_Draw;
    api->Log("[MissingLink] En_Elf's slot runs ML's fairy edits");
}
