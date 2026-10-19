/*
 * The Missing Link's torch (ML_Torch), as a code mod.
 *
 * ML's Boss_Dodongo slot holds a copy of Obj_Syokudai, placed twice in Bmori1. MissingShip's Unbound branch registers
 * it as ML_Torch on Obj_Syokudai's own functions, which tell it apart by its id (z_obj_syokudai.c, MlTorch_Is). It is
 * Z-targetable and keeps its own lit-torch count, and its Update differs in four places: a group's switch no longer
 * relights its torches; lighting any torch of a group sets the switch at once; a partly lit group burns for
 * 50 x count - 640 frames instead of + 110; and a torch going out clears the switch. Here vanilla's Init, Destroy and
 * Draw run as they are, and Update is vanilla's (SoH 9.2.3) with those four changes.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "overlays/actors/ovl_Obj_Syokudai/z_obj_syokudai.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#include <stdio.h>

#include "missinglink.h"

// vanilla's, from z_obj_syokudai.c
void ObjSyokudai_Init(Actor* thisx, PlayState* play);
void ObjSyokudai_Destroy(Actor* thisx, PlayState* play);
void ObjSyokudai_Draw(Actor* thisx, PlayState* play);

static s32 sLitTorchCount; // ML's own count, apart from vanilla's torches

static void MlTorch_Init(Actor* thisx, PlayState* play) {
    ObjSyokudai_Init(thisx, play);
    sLitTorchCount = 0;
}

static void MlTorch_Update(Actor* thisx, PlayState* play2) {
    PlayState* play = play2;
    ObjSyokudai* this = (ObjSyokudai*)thisx;
    s32 torchCount = (this->actor.params >> 6) & 0xF;
    s32 switchFlag = this->actor.params & 0x3F;
    s32 torchType = this->actor.params & 0xF000;
    s32 litTimeScale;
    WaterBox* dummy;
    f32 waterSurface;
    s32 lightRadius = -1;
    u8 brightness = 0;
    Player* player;
    EnArrow* arrow;
    s32 interactionType;
    u32 dmgFlags;
    Vec3f tipToFlame;
    s32 pad;

    litTimeScale = torchCount;
    if (torchCount == 10) {
        torchCount = 24;
    }
    if (WaterBox_GetSurfaceImpl(play, &play->colCtx, this->actor.world.pos.x, this->actor.world.pos.z, &waterSurface,
                                &dummy) &&
        ((waterSurface - this->actor.world.pos.y) > 52.0f)) {
        this->litTimer = 0;
        if (torchType == 1) {
            Flags_UnsetSwitch(play, switchFlag);
            if (torchCount != 0) {
                this->litTimer = 1;
            }
        }
    } else {
        player = GET_PLAYER(play);
        interactionType = 0;
        if (this->actor.params & 0x400) {
            this->litTimer = -1;
        }
        // ML: the group's switch no longer relights its torches; a torch lit for good starts burning down
        if ((torchCount != 0) && (this->litTimer < 0)) {
            this->litTimer = 20;
        }
        if (this->colliderFlame.base.acFlags & AC_HIT) {
            dmgFlags = this->colliderFlame.info.acHitInfo->toucher.dmgFlags;
            if (dmgFlags & 0x20820) {
                interactionType = 1;
            }
        } else if (player->heldItemAction == PLAYER_IA_DEKU_STICK) {
            Math_Vec3f_Diff(&player->meleeWeaponInfo[0].tip, &this->actor.world.pos, &tipToFlame);
            tipToFlame.y -= 67.0f;
            if ((SQ(tipToFlame.x) + SQ(tipToFlame.y) + SQ(tipToFlame.z)) < SQ(20.0f)) {
                interactionType = -1;
            }
        }
        if (interactionType != 0) {
            if (this->litTimer != 0) {
                if (interactionType < 0) {
                    if (player->unk_860 == 0) {
                        player->unk_860 = 210;
                        Audio_PlaySfxGeneral(NA_SE_EV_FLAME_IGNITION, &this->actor.projectedPos, 4,
                                               &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                                               &gSfxDefaultReverb);
                    } else if (player->unk_860 < 200) {
                        player->unk_860 = 200;
                    }
                } else if (dmgFlags & 0x20) {
                    arrow = (EnArrow*)this->colliderFlame.base.ac;
                    if ((arrow->actor.update != NULL) && (arrow->actor.id == ACTOR_EN_ARROW)) {
                        arrow->actor.params = 0;
                        arrow->collider.info.toucher.dmgFlags = 0x800;
                    }
                }
                if ((0 <= this->litTimer) && (this->litTimer < (50 * litTimeScale + 100)) && (torchType != 0)) {
                    this->litTimer = 50 * litTimeScale + 100;
                }
            } else if ((torchType != 0) && (((interactionType > 0) && (dmgFlags & 0x20800)) ||
                                            ((interactionType < 0) && (player->unk_860 != 0)))) {

                if ((interactionType < 0) && (player->unk_860 < 200)) {
                    player->unk_860 = 200;
                }
                if (torchCount == 0) {
                    this->litTimer = -1;
                    if (torchType != 2) {
                        Flags_SetSwitch(play, switchFlag);
                        OnePointCutscene_Attention(play, &this->actor);
                    }
                } else {
                    // ML sets the switch as soon as any torch of the group is lit, and a partly lit group burns for
                    // 50 x count - 640 frames (vanilla: + 110)
                    Flags_SetSwitch(play, switchFlag);
                    sLitTorchCount++;
                    if (sLitTorchCount >= torchCount) {
                        OnePointCutscene_Attention(play, &this->actor);
                        this->litTimer = -1;
                    } else {
                        this->litTimer = (litTimeScale * 50) - 640;
                    }
                }
                Audio_PlaySfxGeneral(NA_SE_EV_FLAME_IGNITION, &this->actor.projectedPos, 4,
                                       &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            }
        }
    }

    Collider_UpdateCylinder(&this->actor, &this->colliderStand);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->colliderStand.base);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->colliderStand.base);

    Collider_UpdateCylinder(&this->actor, &this->colliderFlame);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->colliderFlame.base);

    if (GameInteractor_Should(VB_SWITCH_TIMER_TICK, this->litTimer > 0, this, &this->litTimer)) {
        this->litTimer--;
        if (this->litTimer == 0) {
            Flags_UnsetSwitch(play, switchFlag); // ML: a torch going out clears the switch
        }
        if ((this->litTimer == 0) && (torchType != 0)) {
            sLitTorchCount--;
        }
    }
    if (this->litTimer != 0) {
        if ((this->litTimer < 0) || (this->litTimer >= 20)) {
            lightRadius = 200;
        } else {
            lightRadius = (this->litTimer * 200.0f) / 20.0f;
        }
        brightness = (u8)(Rand_ZeroOne() * 127.0f) + 128;
        Actor_PlaySfx_Flagged(&this->actor, NA_SE_EV_TORCH - SFX_FLAG);
    }
    Lights_PointSetColorAndRadius(&this->lightInfo, brightness, brightness, 0, lightRadius);
    this->flameTexScroll++;
}

void MlTorch_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_Torch";
    definition.description = "The Missing Link: torch";
    definition.category = ACTORCAT_PROP;
    definition.actorFlags =
        ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER | ACTOR_FLAG_ATTENTION_ENABLED;
    definition.objectId = OBJECT_SYOKUDAI;
    definition.instanceSize = sizeof(ObjSyokudai);
    definition.init = MlTorch_Init;
    definition.destroy = ObjSyokudai_Destroy;
    definition.update = MlTorch_Update;
    definition.draw = ObjSyokudai_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_Torch is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
