/*
 * The Missing Link's small edits to vanilla objects, as a code mod: Bmori1's sliding platform (Bg_Jya_1flift), the
 * steep shutters (Bg_Mizu_Shutter), the boulder's camera (Bg_Spot16_Bombstone), the blue warps (Door_Warp1), the cows
 * (En_Cow), the weather tags (En_Weather_Tag), the soft soil's Piece of Heart (Obj_Makekinsuta) and the Song of
 * Storms' rain (Oceff_Storm's En_Okarina_Effect).
 *
 * MissingShip's Unbound branch patches these into each actor's code while ML is mounted (ledger rows 37, 40, 55, 58,
 * 62, 74, 83, 88; MissingShip docs/UNBOUND_MAP.md). As the Business Scrubs do, this takes each one's slot (its ActorDB
 * entry) and goes around vanilla's code.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/ActorDB.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "overlays/actors/ovl_Bg_Jya_1flift/z_bg_jya_1flift.h"
#include "overlays/actors/ovl_Bg_Mizu_Shutter/z_bg_mizu_shutter.h"
#include "overlays/actors/ovl_Door_Warp1/z_door_warp1.h"
#include "overlays/actors/ovl_En_Cow/z_en_cow.h"
#include "overlays/actors/ovl_Obj_Makekinsuta/z_obj_makekinsuta.h"

#include <math.h>
#include <stdarg.h>

#include "missinglink.h"

ML_HOST_FN_DECL(BgMizuShutter_WaitForCutscene);
ML_HOST_FN_DECL(BgMizuShutter_WaitForSwitch);
ML_HOST_FN_DECL(EnWeatherTag_Update);
ML_HOST_FN_DECL(ObjMakekinsuta_DoNothing);
ML_HOST_FN_DECL(func_80B98320);

// vanilla's
void BgMizuShutter_WaitForSwitch(BgMizuShutter* this, PlayState* play);
void BgMizuShutter_WaitForCutscene(BgMizuShutter* this, PlayState* play);
void EnCow_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx);
void EnWeatherTag_Update(Actor* thisx, PlayState* play);
void func_80B98320(ObjMakekinsuta* this, PlayState* play);
void ObjMakekinsuta_DoNothing(ObjMakekinsuta* this, PlayState* play);

/*
 * Bg_Jya_1flift: ML replaces the lift's Update (Bmori1 room 0, params 3; row 88). Instead of rising on its switch it
 * slides along z at 4 units a frame, toward z 197 while the switch is set and toward -197 while it is clear, stopping
 * at either end. Its height is pinned to 233 and its y scale to 0.15625 every frame. ML code 0x80606C00 then carries
 * Link along z by the lift's velocity while he is within 48 units of it in x and z and 40 in y; the DynaPoly also
 * moves him when he stands on it, as in ML. The room check and camera changes are gone.
 */
static void MlBgJya1flift_Update(Actor* thisx, PlayState* play) {
    BgJya1flift* this = (BgJya1flift*)thisx;
    Player* player = GET_PLAYER(play);

    this->dyna.actor.scale.y = 0.15625f;
    this->dyna.actor.velocity.z = Flags_GetSwitch(play, this->dyna.actor.params & 0x3F) ? 4.0f : -4.0f;
    this->dyna.actor.world.pos.z += this->dyna.actor.velocity.z;
    if (!(-197.0f < this->dyna.actor.world.pos.z)) {
        this->dyna.actor.velocity.z = 0.0f;
        this->dyna.actor.world.pos.z = -197.0f;
    }
    if (!(this->dyna.actor.world.pos.z < 197.0f)) {
        this->dyna.actor.velocity.z = 0.0f;
        this->dyna.actor.world.pos.z = 197.0f;
    }

    if ((fabsf(player->actor.world.pos.x - this->dyna.actor.world.pos.x) < 48.0f) &&
        (fabsf(player->actor.world.pos.z - this->dyna.actor.world.pos.z) < 48.0f) &&
        (fabsf(player->actor.world.pos.y - this->dyna.actor.world.pos.y) < 40.0f)) {
        player->actor.world.pos.z += this->dyna.actor.velocity.z;
    }

    Collider_UpdateCylinder(thisx, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    this->dyna.actor.world.pos.y = 233.0f;
}

/* Bg_Mizu_Shutter: ML drops the steep shutters' camera cutscene and waits 6 frames, not 30 (row 74) */

static void MlBgMizuShutter_WaitForSwitch(BgMizuShutter* this, PlayState* play) {
    if (Flags_GetSwitch(play, (u16)this->dyna.actor.params & 0x3F)) {
        OnePointCutscene_Attention(play, &this->dyna.actor);
        ML_SET_HOST_FN(this->actionFunc, BgMizuShutter_WaitForCutscene);
        this->timer = 6;
    }
}

static ActorFunc sBgMizuShutterUpdate;

static void MlBgMizuShutter_Update(Actor* thisx, PlayState* play) {
    BgMizuShutter* this = (BgMizuShutter*)thisx;

    if (ML_IS_HOST_FN(this->actionFunc, BgMizuShutter_WaitForSwitch)) {
        this->actionFunc = MlBgMizuShutter_WaitForSwitch;
    }
    sBgMizuShutterUpdate(thisx, play);
}

/*
 * Bg_Spot16_Bombstone: ML drops the camera cut while a bomb is near the boulder and the one when it is hit (both
 * one-point cutscene 4180, row 55). Vanilla's code keeps its bomb in a file-local variable, so rather than copy it,
 * the cutscene is refused while the boulder's Update runs.
 */

static ActorFunc sBgSpot16BombstoneUpdate;
static s32 sMlInBombstoneUpdate = false;

static void MlBgSpot16Bombstone_Update(Actor* thisx, PlayState* play) {
    sMlInBombstoneUpdate = true;
    sBgSpot16BombstoneUpdate(thisx, play);
    sMlInBombstoneUpdate = false;
}

static void MlBgSpot16Bombstone_ShouldPlayCs(bool* should, va_list args) {
    va_list copy;
    s16* csId;

    if (!sMlInBombstoneUpdate) {
        return;
    }
    va_copy(copy, args);
    csId = va_arg(copy, s16*);
    va_end(copy);
    if (*csId == 4180) {
        *should = false;
    }
}

/*
 * Door_Warp1: ML recompiled Door_Warp1 with five entrance literals changed (row 83): the first blue warp out of the
 * Forest, Water, Spirit and Shadow Temple bosses (vanilla's Chamber of Sages) and the child's later Shadow Temple warp
 * (vanilla's Graveyard warp pad) go to ML's 0x41C, the Fishing Pond, where ML_Fishpond sends ML's fairy after the
 * player. The rest of each branch (flags, medallion, cutscene numbers) is vanilla's, as in ML. Here the destination is
 * changed as soon as the warp sets it, before the transition reads it.
 */

static ActorFunc sDoorWarp1Update;

static void MlDoorWarp1_Update(Actor* thisx, PlayState* play) {
    sDoorWarp1Update(thisx, play);
    switch (play->sceneNum) {
        case SCENE_SHADOW_TEMPLE_BOSS:
            if (play->nextEntranceIndex == ENTR_GRAVEYARD_WARP_PAD) {
                play->nextEntranceIndex = ENTR_FISHING_POND_0;
            }
            // fallthrough
        case SCENE_FOREST_TEMPLE_BOSS:
        case SCENE_WATER_TEMPLE_BOSS:
        case SCENE_SPIRIT_TEMPLE_BOSS:
            if (play->nextEntranceIndex == ENTR_CHAMBER_OF_THE_SAGES_0) {
                play->nextEntranceIndex = ENTR_FISHING_POND_0;
            }
            break;
    }
}

/* En_Cow: ML nops the store that hid limb 5, so its cows draw it (row 40) */

static s32 MlEnCow_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                    void* thisx) {
    EnCow* this = (EnCow*)thisx;

    if (limbIndex == 2) {
        rot->y += this->someRot.y;
        rot->x += this->someRot.x;
    }
    return false;
}

static void MlEnCow_Draw(Actor* thisx, PlayState* play) {
    EnCow* this = (EnCow*)thisx;

    Gfx_SetupDL_37Opa(play->state.gfxCtx);
    SkelAnime_DrawSkeletonOpa(play, &this->skelAnime, MlEnCow_OverrideLimbDraw, EnCow_PostLimbDraw, this);
}

/*
 * En_Weather_Tag: ML removes all five story-flag kills from the tags' Init, so its weather tags always run (row 37).
 * Vanilla's Init sets the tag's action after removing it, so here a removed tag is simply put back.
 */

static ActorFunc sEnWeatherTagInit;

static void MlEnWeatherTag_Init(Actor* thisx, PlayState* play) {
    sEnWeatherTagInit(thisx, play);
    if (thisx->update == NULL) {
        ML_SET_HOST_FN(thisx->update, EnWeatherTag_Update);
    }
}

/*
 * Obj_Makekinsuta: ML rewrote this soft-soil spot (row 62). Bugs dropped on it for 60 frames still trigger it, but it
 * brings up a Piece of Heart (collectible flag 0x0C) instead of a Gold Skulltula: the heart rises 40 units out of the
 * soil, 10 a frame, then settles back to the soil. ML keeps two fields past vanilla's, so the slot is made bigger.
 */

typedef struct {
    ObjMakekinsuta base;
    Actor* heartPiece;
    s16 risen;
} MlObjMakekinsuta;

static void MlObjMakekinsuta_RaiseHeartPiece(ObjMakekinsuta* thisx, PlayState* play) {
    MlObjMakekinsuta* this = (MlObjMakekinsuta*)thisx;
    Actor* heartPiece = play->actorCtx.actorLists[ACTORCAT_MISC].head;

    // ML keeps moving the pointer even if Link grabbed the heart mid-rise (harmless stale memory on N64); here, stop
    // once it is gone
    while ((heartPiece != NULL) && (heartPiece != this->heartPiece)) {
        heartPiece = heartPiece->next;
    }
    if (heartPiece == NULL) {
        ML_SET_HOST_FN(thisx->actionFunc, ObjMakekinsuta_DoNothing);
        return;
    }

    if (!this->risen) {
        if (heartPiece->world.pos.y < thisx->actor.world.pos.y + 40.0f) {
            heartPiece->world.pos.y += 10.0f;
        } else {
            this->risen = true;
        }
    } else if (thisx->actor.world.pos.y < heartPiece->world.pos.y) {
        heartPiece->world.pos.y -= 10.0f;
    } else {
        heartPiece->world.pos.y = thisx->actor.world.pos.y;
        ML_SET_HOST_FN(thisx->actionFunc, ObjMakekinsuta_DoNothing);
    }
}

static void MlObjMakekinsuta_Wait(ObjMakekinsuta* thisx, PlayState* play) {
    MlObjMakekinsuta* this = (MlObjMakekinsuta*)thisx;

    if (thisx->unk_152 != 0) {
        if ((thisx->timer >= 60) && !func_8002DEEC(GET_PLAYER(play))) {
            this->heartPiece = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_ITEM00, thisx->actor.world.pos.x,
                                           thisx->actor.world.pos.y, thisx->actor.world.pos.z, 0,
                                           thisx->actor.shape.rot.y, 0, (0x0C << 8) | ITEM00_HEART_PIECE);
            thisx->actionFunc = MlObjMakekinsuta_RaiseHeartPiece;
        } else {
            thisx->timer++;
        }
    } else {
        thisx->timer = 0;
    }
}

static ActorFunc sObjMakekinsutaUpdate;

static void MlObjMakekinsuta_Update(Actor* thisx, PlayState* play) {
    ObjMakekinsuta* this = (ObjMakekinsuta*)thisx;

    if (ML_IS_HOST_FN(this->actionFunc, func_80B98320)) {
        this->actionFunc = MlObjMakekinsuta_Wait;
    }
    sObjMakekinsutaUpdate(thisx, play);
}

/* Oceff_Storm: ML deletes En_Okarina_Effect, so the Song of Storms brings no rain (row 58) */

static void MlEnOkarinaEffect_Init(Actor* thisx, PlayState* play) {
    Actor_Kill(thisx);
}

static void MlEnOkarinaEffect_Destroy(Actor* thisx, PlayState* play) {
}

void MlVanillaObjects_Register(const SOHModApi* api) {
    ActorDBEntry* entry;

    entry = ActorDB_Retrieve(ACTOR_BG_JYA_1FLIFT);
    entry->update = MlBgJya1flift_Update;

    entry = ActorDB_Retrieve(ACTOR_BG_MIZU_SHUTTER);
    sBgMizuShutterUpdate = entry->update;
    entry->update = MlBgMizuShutter_Update;

    entry = ActorDB_Retrieve(ACTOR_BG_SPOT16_BOMBSTONE);
    sBgSpot16BombstoneUpdate = entry->update;
    entry->update = MlBgSpot16Bombstone_Update;
    api->RegisterVB(VB_PLAY_ONEPOINT_CS, MlBgSpot16Bombstone_ShouldPlayCs);

    entry = ActorDB_Retrieve(ACTOR_DOOR_WARP1);
    sDoorWarp1Update = entry->update;
    entry->update = MlDoorWarp1_Update;

    entry = ActorDB_Retrieve(ACTOR_EN_COW);
    entry->draw = MlEnCow_Draw;

    entry = ActorDB_Retrieve(ACTOR_EN_WEATHER_TAG);
    sEnWeatherTagInit = entry->init;
    entry->init = MlEnWeatherTag_Init;

    entry = ActorDB_Retrieve(ACTOR_OBJ_MAKEKINSUTA);
    sObjMakekinsutaUpdate = entry->update;
    entry->update = MlObjMakekinsuta_Update;
    entry->instanceSize = sizeof(MlObjMakekinsuta);

    entry = ActorDB_Retrieve(ACTOR_EN_OKARINA_EFFECT);
    entry->init = MlEnOkarinaEffect_Init;
    entry->destroy = MlEnOkarinaEffect_Destroy;

    api->Log("[MissingLink] ML's edits to Bg_Jya_1flift, Bg_Mizu_Shutter, Bg_Spot16_Bombstone, Door_Warp1, En_Cow, "
             "En_Weather_Tag, Obj_Makekinsuta and Oceff_Storm are on");
}
