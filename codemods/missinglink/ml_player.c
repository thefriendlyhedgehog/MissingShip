/*
 * The Missing Link's changes to the player, as a code mod.
 *
 * MissingShip's Unbound branch edits z_player.c and z_player_lib.c in place while ML is mounted, with its helpers in
 * soh/soh/missinglink (MlHourglass.c, MlPlayerAnim.c, MlSoulSword.c). The mod API can't edit the player, so this
 * takes the player's ActorDB entry (its init and update, around vanilla's) and uses SoH's hooks:
 * - ML's Magic Hourglass, in the boomerang's slot, and its room counter. Before vanilla's update it keeps the history
 *   and, while a rewind plays, runs instead of the update (the branch skips only Player_UpdateCommon; here the whole
 *   Player_Update is skipped, and the OnPlayerUpdate hooks still run). The throw is vanilla's aim and throw with ML's
 *   two changes, swapped into upperActionFunc before vanilla's update calls them. The cucco keeper follows each frame;
 *   a void-out through Player_Action_8084F88C resets the counter (VB_TRIGGER_VOIDOUT).
 * - ML's cucco, ML_Cucco, is a cucco to the player: in ML it has En_Niw's id, so the player glides with it, throws it
 *   in the air and offers to. Here it has its own id, so while it is held its id is En_Niw's during the update.
 * - The triple jump: a running jump plays the next of ML's three jumps. The branch swaps the animation inside
 *   LinkAnimation_Change; here the swap follows vanilla's change in the same update, before the jump's first frame.
 *   The same hook holds Link in the horse idle while Epona carries him off from the Fishing Pond.
 * - The Soul-Charged Kokiri Sword and its glow (ML's sword display lists, patched while slingshot ammo is 40).
 * - ML's child hands (the fist and Hylian Shield hand; closed hands for the hookshot and hammer slots), and ML's
 *   child-shield rule (VB_BE_CHILD_WITH_HYLIAN_SHIELD): ML nops vanilla's "^ PLAYER_SHIELD_HYLIAN", so a child holds
 *   the Hylian Shield like any shield, in ML's fist-and-shield hand (the nop's side effect, the rule for a child with
 *   no shield, is left out).
 * - The Bunny Hood: run speed limit 1100 (550 without it); target speed x1.4 (ML adds 0x400000 to the float's bits,
 *   x1.33 to x1.5 by value; x1.4 is the full-stick run's); the water speed factor 1.25 out of water (0.5 in it, as
 *   vanilla's), which scales its jumps, rolls, turn rate, ledge heights and door walk too.
 * - No Navi toggle on A, no damage on a void-out, no conversation with text 0xE0 (Saria's song's call to Navi), music
 *   kept through holes and pit exits (the hole silent in the Deku Tree), sequence mode always "still" (no enemy or
 *   moving music), and the place-name title card at y 80.
 * - ML spawns no revival fairy: En_Elf's slot drops those (ml_en_elf.c).
 * The child-shield rule and the water factor use hooks offered to skijer's unbound-modapi (skijer/Shipwright#33).
 * Not reproduced: ML's wall push-out skip on a room's first frames and during the Hourglass rewind (ML's hook
 * 0x80602C80 on Actor_UpdateBgCheckInfo; no case needs it, and the hook was withdrawn); its third use, after the Deep
 * Woods corridor's loop, ml_fogwall.c covers without a hook. No gap: Player_StartTalking keeping talkActor (only for
 * text 0xE0, which never starts, below) and LOD 0 (ML's child limbs draw the same display list at both LODs).
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "macros.h"
#include "variables.h"
#include "soh/ActorDB.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ModApi/Player/PlayerHookTypes.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/object_link_child/object_link_child.h"
#include "align_asset_macro.h"

#include <stdarg.h>
#include <string.h>

#include "missinglink.h"

ML_HOST_FN_DECL(Player_Action_80842180);
ML_HOST_FN_DECL(Player_Action_8084227C);
ML_HOST_FN_DECL(Player_Action_8084F88C);
ML_HOST_FN_DECL(Player_Action_Roll);
ML_HOST_FN_DECL(func_808358F0);
ML_HOST_FN_DECL(func_808359FC);
ML_HOST_FN_DECL(func_80835B60);

// vanilla's, from z_player.c
s32 func_808358F0(Player* this, PlayState* play); // aim the boomerang
s32 func_808359FC(Player* this, PlayState* play); // throw it
s32 func_80835B60(Player* this, PlayState* play); // boomerang out
s32 func_80834EB8(Player* this, PlayState* play);
LinkAnimationHeader* func_808334E4(Player* this);
LinkAnimationHeader* func_80833528(Player* this);
LinkAnimationHeader* func_808335B0(Player* this);
LinkAnimationHeader* func_808335F4(Player* this);
void Player_SetUpperActionFunc(Player* this, UpperActionFunc upperActionFunc);
void Player_AnimPlayOnceAdjusted(PlayState* play, Player* this, LinkAnimationHeader* anim);
void Player_Action_8084F88C(Player* this, PlayState* play); // falling into a void or pit exit
void Player_Action_80842180(Player* this, PlayState* play); // run
void Player_Action_8084227C(Player* this, PlayState* play); // run, locked on
void Player_Action_Roll(Player* this, PlayState* play);
// vanilla's, from z_player_lib.c
extern HOST_DATA Gfx* sPlayerRightHandShieldDLs[];
extern HOST_DATA Gfx* sPlayerRightHandHookshotDLs[];
extern HOST_DATA Gfx* sPlayerLeftHandHammerDLs[];
// vanilla's, from the audio code
extern HOST_DATA u32 sAudioSeqCmds[0x100];
extern HOST_DATA u8 sSeqCmdWrPos;
extern HOST_DATA u8 sPrevSeqMode;
extern HOST_DATA u32 sNumFramesStill;
extern HOST_DATA u32 sNumFramesMoving;
extern HOST_DATA u8 sSeqModeInput;

#define dgMlChildRightFistAndHylianShieldDL "__OTR__objects/object_link_child/gMlChildRightFistAndHylianShieldDL"
static const ALIGN_ASSET(2) char gMlChildRightFistAndHylianShieldDL[] = dgMlChildRightFistAndHylianShieldDL;
#define dgMlChildKokiriSwordHiltDL "__OTR__objects/object_link_child/gMlChildKokiriSwordHiltDL"
static const ALIGN_ASSET(2) char gMlChildKokiriSwordHiltDL[] = dgMlChildKokiriSwordHiltDL;
#define dgMlChildKokiriSwordBladeDL "__OTR__objects/object_link_child/gMlChildKokiriSwordBladeDL"
static const ALIGN_ASSET(2) char gMlChildKokiriSwordBladeDL[] = dgMlChildKokiriSwordBladeDL;
#define dgMlChildSoulSheathDL "__OTR__objects/object_link_child/gMlChildSoulSheathDL"
static const ALIGN_ASSET(2) char gMlChildSoulSheathDL[] = dgMlChildSoulSheathDL;
#define dgMlChildSoulSwordHiltDL "__OTR__objects/object_link_child/gMlChildSoulSwordHiltDL"
static const ALIGN_ASSET(2) char gMlChildSoulSwordHiltDL[] = dgMlChildSoulSwordHiltDL;
#define dgMlChildSoulSwordBladeDL "__OTR__objects/object_link_child/gMlChildSoulSwordBladeDL"
static const ALIGN_ASSET(2) char gMlChildSoulSwordBladeDL[] = dgMlChildSoulSwordBladeDL;

static ActorFunc sPlayerInit;
static ActorFunc sPlayerUpdate;
static s16 sMlCuccoId = -1;

// Animations are resource paths here; each file has its own copy of a path, so compare the text.
static s32 MlPlayer_AnimIs(const void* animation, const char* path) {
    return (animation == path) ||
           ((animation != NULL) && ResourceMgr_OTRSigCheck((char*)animation) && (strcmp(animation, path) == 0));
}

static s32 MlPlayer_IsMlCucco(Actor* actor) {
    return (actor != NULL) && (sMlCuccoId >= 0) && (actor->id == sMlCuccoId);
}

// ---- Magic Hourglass and room counter (the branch's MlHourglass.c) ----
//
// ML replaces Player_Update's call to Player_UpdateCommon (player +0x17E5C -> 0x80600E00) and wraps it:
// - It keys the current scene and room as sceneNum | (roomNum << 4) and counts frames since the key changed
//   (0x80601680). At frame 17 it fills the whole history with Link's spot and jumps the count to its cap, 169, the
//   value at which the Hourglass works. (MissingShip's MS-150: not before the previous room has unloaded.)
// - It keeps the history: 164 entries of Link's position and facing, newest first, shifted every frame.
// - The Magic Hourglass sits in the boomerang's slot: the throw fires at once, with no aiming hold (player +0x378C),
//   and on its frame 6 sets the trigger instead of spawning En_Boom (player +0x38CC). Once the count is at its cap, the
//   trigger spawns the Song of Time wipe and rewinds Link through entries 69..161, 4 a frame, with the update skipped
//   until it ends. Otherwise the trigger is dropped.
// - Then the cucco keeper (0x80601000, see MlHourglass_CuccoFrame).

#define ML_REWIND_LEN 164

typedef struct {
    Vec3f pos;
    s16 yaw;
} MlRewindEntry;

static u16 sRoomFrames = 0;                  // ML 0x80609FF0
static u16 sRoomKey = 0;                     // ML 0x80609FF2
static u16 sRewindStep = 0;                  // ML 0x80609FF4
static u8 sTrigger = 0;                      // ML 0x80609FFF
static Actor* sWipe = NULL;                  // ML 0x80609FEC
static MlRewindEntry sRewind[ML_REWIND_LEN]; // ML 0x8060AC00

// ML 0x80600AA8 / 0x80600AAA: ML's cucco, carried through a void-out
static u16 sCuccoHealth = 0;
static u16 sCuccoVoided = 0;

u16 MlHourglass_RewindStep(void) {
    return sRewindStep;
}

static s32 MlHourglass_CuccoInRoom(PlayState* play) {
    Actor* actor;

    for (actor = play->actorCtx.actorLists[ACTORCAT_PROP].head; actor != NULL; actor = actor->next) {
        if (MlPlayer_IsMlCucco(actor)) {
            return true;
        }
    }
    return false;
}

// ML 0x80600480, before Play_TriggerVoidOut in Player_Action_8084F88C
static void MlHourglass_TriggerVoidOut(bool* should, va_list args) {
    va_list copy;
    Player* player;

    va_copy(copy, args);
    player = va_arg(copy, Player*);
    va_end(copy);
    if (!*should || (player == NULL) || !ML_IS_HOST_FN(player->actionFunc, Player_Action_8084F88C)) {
        return;
    }
    sRoomFrames = 0;
    if (MlHourglass_CuccoInRoom(gPlayState)) {
        sCuccoHealth = 0x77;
        sCuccoVoided = 1;
    }
}

// ML 0x80601000: one frame into a room after a void-out with a cucco around (not at Lon Lon Ranch or in the Shadow
// Temple slot), and with none here, ML spawns its own cucco beside Link, in no room so it follows him. While Link holds
// a cucco, the health it keeps is recorded; ML's own starts at 0x77.
static void MlHourglass_CuccoFrame(Player* player, PlayState* play) {
    if ((sRoomFrames == 1) && (play->sceneNum != SCENE_LON_LON_RANCH) && (play->sceneNum != SCENE_SHADOW_TEMPLE) &&
        (sCuccoHealth == 0x77) && !MlHourglass_CuccoInRoom(play) && (sMlCuccoId >= 0)) {
        s16 yaw = player->actor.focus.rot.y;
        Actor* cucco = Actor_Spawn(&play->actorCtx, play, sMlCuccoId,
                                   player->actor.world.pos.x + Math_SinS(yaw) * 152.0f, player->actor.world.pos.y,
                                   player->actor.world.pos.z + Math_CosS(yaw) * 184.0f, 0, yaw, 0, 0x77);

        if (cucco != NULL) {
            cucco->room = -1;
        }
        sCuccoVoided = 0;
    }
    if (sCuccoVoided == 0) {
        Actor* held = player->actor.child;

        sCuccoHealth = 0xFFFF;
        if (MlPlayer_IsMlCucco(held)) {
            sCuccoHealth = held->colChkInfo.health;
        }
    }
}

// Returns false while a rewind plays (the update is skipped).
static s32 MlHourglass_PlayerFrame(Player* player, PlayState* play) {
    if (sRewindStep == 0) {
        u16 key = play->sceneNum | (play->roomCtx.curRoom.num << 4);
        u16 frames;
        s32 i;

        memmove(&sRewind[1], &sRewind[0], sizeof(sRewind[0]) * (ML_REWIND_LEN - 1));
        sRewind[0].pos = player->actor.world.pos;
        sRewind[0].yaw = player->actor.shape.rot.y;

        if (key != sRoomKey) {
            sRoomKey = key;
            sRoomFrames = 0;
        }
        frames = sRoomFrames + 1;
        // MissingShip (MS-150): ML refills the history 17 frames after curRoom changes, but a door changes it while
        // Link is still in the doorway, so the refilled spot can be in the old room, which then unloads: rewinding
        // there leaves Link behind a shut door in an undrawn room. Wait until the old room is gone instead.
        if ((frames == 17) && (play->roomCtx.prevRoom.num >= 0)) {
            frames = 16;
        }
        if (frames == 17) {
            for (i = 0; i < ML_REWIND_LEN; i++) {
                sRewind[i].pos = player->actor.world.pos;
                sRewind[i].yaw = 0; // ML stores the facing as a word here, so it reads back as 0
            }
            frames = 178;
        }
        sRoomFrames = MIN(frames, 169);

        if ((sRoomFrames == 169) && sTrigger) {
            sRewindStep = 1;
            sWipe = Actor_Spawn(&play->actorCtx, play, ACTOR_OCEFF_WIPE, player->actor.world.pos.x,
                                player->actor.world.pos.y, player->actor.world.pos.z, 0, 0, 0, 1);
            Audio_PlayActorSound2(&player->actor, NA_SE_IT_SHIELD_REFLECT_MG);
            return false;
        }
        sTrigger = 0;
        return true;
    } else {
        MlRewindEntry* entry;

        sRoomFrames = 0;
        sRewindStep += 4;
        entry = &sRewind[64 + sRewindStep];
        player->actor.world.pos = player->actor.home.pos = entry->pos;
        player->actor.shape.rot.y = entry->yaw;
        if (sWipe != NULL) {
            sWipe->world.pos = entry->pos;
            sWipe->shape.rot.y = entry->yaw;
        }
        sTrigger = 0;
        if (sRewindStep == 97) {
            sRewindStep = 0;
        }
        return false;
    }
}

// Vanilla's func_808358F0 (aim the boomerang) with ML's change: the Hourglass fires at once, with no aiming hold.
static s32 MlHourglass_Aim(Player* this, PlayState* play) {
    LinkAnimationHeader* animSeg = this->skelAnime.animation;

    if ((func_808334E4(this) == animSeg) || (func_80833528(this) == animSeg) || (func_808335B0(this) == animSeg) ||
        (func_808335F4(this) == animSeg)) {
        AnimationContext_SetCopyAll(play, this->skelAnime.limbCount, this->upperSkelAnime.jointTable,
                                    this->skelAnime.jointTable);
    } else if (!CVarGetInteger("gEnhancements.BoomerangReticle", 0)) {
        LinkAnimation_Update(play, &this->upperSkelAnime);
    }

    func_80834EB8(this, play);

    Player_SetUpperActionFunc(this, (UpperActionFunc)ML_HOST_FN(func_808359FC));
    LinkAnimation_PlayOnce(play, &this->upperSkelAnime,
                           (LinkAnimationHeader*)((this->unk_870 < 0.5f) ? gPlayerAnim_link_boom_throwR
                                                                         : gPlayerAnim_link_boom_throwL));
    return true;
}

// Vanilla's func_808359FC (throw the boomerang) with ML's change: no boomerang, just the rewind trigger.
static s32 MlHourglass_Throw(Player* this, PlayState* play) {
    if (LinkAnimation_Update(play, &this->upperSkelAnime)) {
        Player_SetUpperActionFunc(this, (UpperActionFunc)ML_HOST_FN(func_80835B60));
        this->unk_834 = 0;
    } else if (LinkAnimation_OnFrame(&this->upperSkelAnime, 6.0f)) {
        sTrigger = 1;
        this->boomerangActor = NULL;
    }
    return true;
}

// ---- triple jump (the branch's MlPlayerAnim.c) ----
//
// When a running jump starts, ML plays sJumpAnims[sJumpStep] instead: the second and third jumps reuse the two
// sword-turn recovery slots, whose data ML replaced (its layer ships them). Each frame ML advances the step after each
// jump and drops it back to the first jump once the player has been out of all three for 16 frames.

static const char* sJumpAnims[] = {
    gPlayerAnim_link_normal_run_jump,
    gPlayerAnim_link_fighter_turn_kiruL_end,
    gPlayerAnim_link_fighter_turn_kiruR_end,
};
static s8 sJumpStep = 0;    // ML 0x80600008
static s8 sJumpPlayed = -1; // ML 0x80600004: the jump that just started, as a sJumpAnims index
static u32 sJumpTimer = 0;  // ML 0x80603614: frames since the player left the last jump

// The animation before vanilla's update, to see a jump start
static void* sPrevAnim = NULL;
static f32 sPrevAnimFrame = 0.0f;

static void MlPlayerAnim_JumpStart(Player* player, PlayState* play) {
    void* anim = player->skelAnime.animation;

    if (!MlPlayer_AnimIs(anim, gPlayerAnim_link_normal_run_jump) ||
        ((anim == sPrevAnim) && (player->skelAnime.curFrame >= sPrevAnimFrame))) {
        return;
    }
    // The first jump keeps vanilla's pointer: the player's landing compares it with its own copy of the path
    if (sJumpStep != 0) {
        Player_AnimPlayOnceAdjusted(play, player, (LinkAnimationHeader*)sJumpAnims[sJumpStep]);
    }
    sJumpPlayed = sJumpStep;
}

static void MlPlayerAnim_JumpUpdate(Player* player) {
    s32 i;

    if (sJumpTimer >= 16) {
        sJumpStep = 0;
    }
    switch (sJumpPlayed) {
        case 0:
            sJumpStep = 1;
            sJumpTimer = 1;
            break;
        case 1:
            sJumpStep = 2;
            sJumpTimer = 1;
            break;
        case 2:
            sJumpStep = 0;
            sJumpTimer = 0;
            break;
        default:
            if (sJumpStep != 0) {
                sJumpTimer++;
            }
            break;
    }
    sJumpPlayed = -1;
    for (i = 0; i < ARRAY_COUNT(sJumpAnims); i++) {
        if (MlPlayer_AnimIs(player->skelAnime.animation, sJumpAnims[i])) {
            sJumpTimer = 0;
        }
    }
}

// ---- riding Epona off from the Fishing Pond (the branch's MlPlayerAnim.c) ----
//
// After the pond intro (ML entrance 0x420) ML_EventSpawner carries Link on young Epona, and ML's animation hook makes
// every animation Link starts there the horse idle; without it he stands inside her.

static void MlPlayerAnim_RideUpdate(Player* player, PlayState* play) {
    static s32 sRideEntrance = -1;

    if (play->sceneNum != SCENE_FISHING_POND) {
        return;
    }
    if (sRideEntrance < 0) {
        sRideEntrance = MlEntrance_Index("SCENE_FISHING_POND/ml/fishing_pond_event");
    }
    if ((gSaveContext.entranceIndex != sRideEntrance) ||
        MlPlayer_AnimIs(player->skelAnime.animation, gPlayerAnim_link_uma_wait_2)) {
        return;
    }
    LinkAnimation_Change(play, &player->skelAnime, (LinkAnimationHeader*)gPlayerAnim_link_uma_wait_2, 1.0f, 0.0f,
                         Animation_GetLastFrame(gPlayerAnim_link_uma_wait_2), player->skelAnime.mode, 0.0f);
}

// ---- Soul-Charged Kokiri Sword (the branch's MlSoulSword.c, ledger row 99) ----
//
// ML marks the upgrade with slingshot ammo 40. While it is set, ML rewrites object_link_child every frame: the sheath,
// hilt and blade DLs call the soul sword it appended instead, and the blade's 8 prim colours pulse green while the
// sword is powered (charging a spin, a combo swing at full health, or normal2fighter_free); otherwise they are white.
// SoH's DLs are shared resources, so the same edits are patches, undone when the ammo marker is gone.

static s32 sMlSoulPatched = false;
static s8 sMlSoulGlowOn = false;                // 0x80603610
static u32 sMlSoulGlowTimer = 0;                // 0x8060360C
static u8 sMlSoulGlow[3] = { 255, 255, 255 };   // 0x80603600

static void MlSoulSword_Redirect(const char* dl, const char* to, s32 on) {
    if (on) {
        ResourceMgr_PatchCustomGfxByName(dl, "mlSoulSword0", 0, (Gfx)gsSPDisplayListOTRFilePath(to));
        ResourceMgr_PatchCustomGfxByName(dl, "mlSoulSword1", 1, (Gfx)gsSPEndDisplayList());
    } else {
        ResourceMgr_UnpatchGfxByName(dl, "mlSoulSword0");
        ResourceMgr_UnpatchGfxByName(dl, "mlSoulSword1");
    }
}

static void MlSoulSword_SetBladeColor(u8 r, u8 g, u8 b) {
    static const char* sPatchNames[] = { "mlSoulGlow0", "mlSoulGlow1", "mlSoulGlow2", "mlSoulGlow3",
                                         "mlSoulGlow4", "mlSoulGlow5", "mlSoulGlow6", "mlSoulGlow7" };
    Gfx* gfx = ResourceMgr_LoadGfxByName(gMlChildSoulSwordBladeDL);
    s32 n = 0;

    if (gfx == NULL) {
        return;
    }
    for (s32 i = 0; (n < ARRAY_COUNT(sPatchNames)) && ((gfx[i].words.w0 >> 24) != G_ENDDL); i++) {
        u32 op = gfx[i].words.w0 >> 24;

        if (op == G_SETPRIMCOLOR) {
            ResourceMgr_PatchCustomGfxByName(gMlChildSoulSwordBladeDL, sPatchNames[n++], i,
                                             (Gfx)gsDPSetPrimColor(0, 0x80, r, g, b, 255));
        } else if ((op == 0x20) || (op == 0x31) || (op == 0x32) || (op == 0x33) || (op == 0x35) || (op == 0x36) ||
                   (op == 0x42)) {
            i++; // LUS's two-word commands
        }
    }
}

static void MlSoulSword_Update(Player* player, s32 unsheathing) {
    s32 on = (AMMO(ITEM_SLINGSHOT) == 40);
    s32 powered;

    if (on != sMlSoulPatched) {
        MlSoulSword_Redirect(gLinkChildSheathNearDL, gMlChildSoulSheathDL, on);
        MlSoulSword_Redirect(gMlChildKokiriSwordHiltDL, gMlChildSoulSwordHiltDL, on);
        MlSoulSword_Redirect(gMlChildKokiriSwordBladeDL, gMlChildSoulSwordBladeDL, on);
        sMlSoulPatched = on;
    }
    if (!on) {
        return;
    }

    powered = (player->stateFlags1 & PLAYER_STATE1_CHARGING_SPIN_ATTACK) ||
              ((player->unk_845 & 1) && (gSaveContext.healthCapacity == gSaveContext.health)) || unsheathing;
    if (!powered) {
        sMlSoulGlow[0] = sMlSoulGlow[1] = sMlSoulGlow[2] = 255;
        sMlSoulGlowTimer = 0;
        sMlSoulGlowOn = false;
    } else if (!sMlSoulGlowOn) {
        sMlSoulGlow[0] = 0;
        sMlSoulGlow[1] = 255;
        sMlSoulGlow[2] = 150;
        sMlSoulGlowOn = true;
    } else {
        s8 step = (sMlSoulGlowTimer < 10) ? -8 : 8;

        sMlSoulGlow[1] += step;
        sMlSoulGlow[2] += step;
        if (++sMlSoulGlowTimer == 20) {
            sMlSoulGlowTimer = 0;
        }
    }
    MlSoulSword_SetBladeColor(sMlSoulGlow[0], sMlSoulGlow[1], sMlSoulGlow[2]);
}

// The branch's MlActors.cpp OnPlayerUpdate (the player's unsheathing animation, compared by path), with the jump
// swap, which follows vanilla's LinkAnimation_Change in the same update.
static void MlPlayer_OnPlayerUpdate(void) {
    Player* player = GET_PLAYER(gPlayState);
    s32 unsheathing = MlPlayer_AnimIs(player->skelAnime.animation, gPlayerAnim_link_normal_normal2fighter_free);

    MlPlayerAnim_JumpStart(player, gPlayState);
    MlSoulSword_Update(player, unsheathing);
    MlPlayerAnim_JumpUpdate(player);
    MlPlayerAnim_RideUpdate(player, gPlayState);
}

// ---- music ----

// func_800F6964(5)'s commands, as Audio_SeqCmd1 and Audio_SeqCmd6 queue them: stop the main BGM and the fanfare over 7
// frames, fade each sfx channel (2 frames), then stop the sub BGM.
#define ML_HOLE_STOP_MAIN 0x100700FF
#define ML_HOLE_STOP_FANFARE 0x110700FF
#define ML_HOLE_STOP_SUB 0x130700FF
#define ML_HOLE_FADE_SFX 0x62020000

// Takes func_800F6964(5)'s commands back out of the queue, if vanilla's hole exit queued them since `from`.
static s32 MlPlayer_DropHoleMusicStop(u8 from) {
    u8 i;

    for (i = from; i != sSeqCmdWrPos; i++) {
        u8 j = i + 1;
        u8 end;
        u8 src;

        if ((sAudioSeqCmds[i] != ML_HOLE_STOP_MAIN) || (j == sSeqCmdWrPos) ||
            (sAudioSeqCmds[j] != ML_HOLE_STOP_FANFARE)) {
            continue;
        }
        for (j++; (j != sSeqCmdWrPos) && ((sAudioSeqCmds[j] & 0xFFFF00FF) == ML_HOLE_FADE_SFX); j++) {}
        if ((j == sSeqCmdWrPos) || (sAudioSeqCmds[j] != ML_HOLE_STOP_SUB)) {
            return false;
        }
        end = j + 1;
        for (src = end; src != sSeqCmdWrPos; src++, i++) {
            sAudioSeqCmds[i] = sAudioSeqCmds[src];
        }
        sSeqCmdWrPos = i;
        return true;
    }
    return false;
}

// The player's frame state before vanilla's update
typedef struct {
    PlayerActionFunc actionFunc;
    u32 stateFlags1;
    u32 stateFlags2;
    u8 seqId;
    u8 natureAmbienceId;
    u8 transitionTrigger;
    u8 seqCmdWrPos;
    u8 prevSeqMode;
    u8 seqModeInput;
    u32 numFramesStill;
    u32 numFramesMoving;
} MlPlayerFrame;

#define ML_SEQ_MODE_UNSET 0xFF

static void MlPlayer_BeforeUpdate(Player* player, PlayState* play, MlPlayerFrame* frame) {
    frame->actionFunc = player->actionFunc;
    frame->stateFlags1 = player->stateFlags1;
    frame->stateFlags2 = player->stateFlags2;
    frame->seqId = gSaveContext.seqId;
    frame->natureAmbienceId = gSaveContext.natureAmbienceId;
    frame->transitionTrigger = play->transitionTrigger;
    frame->seqCmdWrPos = sSeqCmdWrPos;
    frame->prevSeqMode = sPrevSeqMode;
    frame->numFramesStill = sNumFramesStill;
    frame->numFramesMoving = sNumFramesMoving;
    frame->seqModeInput = sSeqModeInput;
    sSeqModeInput = ML_SEQ_MODE_UNSET; // to see whether the player sets the mode this frame
    sPrevAnim = player->skelAnime.animation;
    sPrevAnimFrame = player->skelAnime.curFrame;

    // The Hourglass's throw
    if (ML_IS_HOST_FN(player->upperActionFunc, func_808358F0)) {
        player->upperActionFunc = MlHourglass_Aim;
    } else if (ML_IS_HOST_FN(player->upperActionFunc, func_808359FC)) {
        player->upperActionFunc = MlHourglass_Throw;
    }

    // ML's Bunny Hood doubles the run speed limit (func_8083DF68)
    R_RUN_SPEED_LIMIT = (player->currentMask == PLAYER_MASK_BUNNY) ? 1100 : 550;

    // ML never starts a conversation with text 0xE0, which only Saria's song gives Navi (Player_StartTalking and its
    // callers, through naviTextId -0xE0): she never talks
    if (player->naviTextId == -0xE0) {
        player->naviTextId = 0;
    }
}

static void MlPlayer_AfterUpdate(Player* player, PlayState* play, MlPlayerFrame* frame) {
    // ML has no Navi to toggle with A (Player_ActionHandler_Roll's xor mask is 0)
    if ((player->stateFlags2 ^ frame->stateFlags2) & PLAYER_STATE2_NAVI_ACTIVE) {
        player->stateFlags2 ^= PLAYER_STATE2_NAVI_ACTIVE;
    }

    // ML keeps the music playing through a hole (floor type 11), which is silent in the Deku Tree (its hole-hopping
    // slot). Its one write lands on the upper half of this respawn's collectible flags, probably meant for seqId;
    // kept as ML does it.
    if ((gSaveContext.seqId == (u8)NA_BGM_DISABLED) && MlPlayer_DropHoleMusicStop(frame->seqCmdWrPos)) {
        gSaveContext.seqId = frame->seqId;
        gSaveContext.natureAmbienceId = frame->natureAmbienceId;
        if (play->sceneNum == SCENE_DEKU_TREE) {
            Audio_StopSfxById(NA_SE_OC_SECRET_HOLE_OUT);
        } else {
            gSaveContext.respawn[RESPAWN_MODE_DOWN].tempCollectFlags |= 0xFFFF0000;
        }
    }
    // ...and through a pit exit (Player_Action_8084F88C's non-void branch)
    if (ML_IS_HOST_FN(frame->actionFunc, Player_Action_8084F88C) && (frame->transitionTrigger == TRANS_TRIGGER_OFF) &&
        (play->transitionTrigger == TRANS_TRIGGER_START) && (player->av1.actionVar1 == 0) &&
        (gSaveContext.seqId == (u8)NA_BGM_DISABLED)) {
        gSaveContext.seqId = frame->seqId;
    }

    // ML always passes SEQ_MODE_STILL: no moving music (Hyrule Field's). The enemy mode is off at its source
    // (VB_DETECT_BGM_ENEMY). Vanilla's call is undone and made again with "still".
    if (sSeqModeInput == ML_SEQ_MODE_UNSET) {
        sSeqModeInput = frame->seqModeInput;
    } else if (sSeqModeInput != SEQ_MODE_STILL) {
        sPrevSeqMode = frame->prevSeqMode;
        sNumFramesStill = frame->numFramesStill;
        sNumFramesMoving = frame->numFramesMoving;
        Audio_SetSequenceMode(SEQ_MODE_STILL);
    }
}

static void MlPlayer_Update(Actor* thisx, PlayState* play) {
    Player* player = (Player*)thisx;
    MlPlayerFrame frame;
    Actor* cucco = NULL;

    if (!MlHourglass_PlayerFrame(player, play)) {
        // A rewind plays: the update is skipped, the hooks still run
        sPrevAnim = player->skelAnime.animation;
        sPrevAnimFrame = player->skelAnime.curFrame;
        GameInteractor_ExecuteOnPlayerUpdate();
        MlHourglass_CuccoFrame(player, play);
        return;
    }

    MlPlayer_BeforeUpdate(player, play, &frame);
    // ML's cucco has En_Niw's id in ML, so the player glides with it, throws it in the air and offers to
    if ((player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) && MlPlayer_IsMlCucco(player->heldActor)) {
        cucco = player->heldActor;
        cucco->id = ACTOR_EN_NIW;
    }
    sPlayerUpdate(thisx, play);
    if (cucco != NULL) {
        cucco->id = sMlCuccoId;
    }
    MlPlayer_AfterUpdate(player, play, &frame);
    MlHourglass_CuccoFrame(player, play);
}

// ML's place-name title card is at y 80 (vanilla's 120), set in Player_Init.
static void MlPlayer_Init(Actor* thisx, PlayState* play) {
    TitleCardContext* titleCtx = &play->actorCtx.titleCtx;
    void* texture = titleCtx->texture;
    u8 delayTimer = titleCtx->delayTimer;

    sPlayerInit(thisx, play);
    if (((titleCtx->texture != texture) || (titleCtx->delayTimer != delayTimer)) && (titleCtx->x == 160) &&
        (titleCtx->y == 120) && (titleCtx->width == 144) && (titleCtx->height == 24)) {
        titleCtx->y = 80;
    }
}

// ML's Bunny Hood adds 0x400000 to the target speed's float bits in func_8083DF68 (x1.33 to x1.5 by value). The hook
// sees no speed, so this is the full-stick run's factor (6.72 -> 9.44), on the three actions that step toward it.
// Its water speed factor is 1.25 out of water with the hood on (Player_UpdateCommon).
static void MlPlayer_ResolveMotionScale(Player* player, int32_t kind, float* scale) {
    if ((kind == SOH_PLAYER_MOTION_STICK_SPEED) && (player->currentMask == PLAYER_MASK_BUNNY) &&
        (ML_IS_HOST_FN(player->actionFunc, Player_Action_80842180) ||
         ML_IS_HOST_FN(player->actionFunc, Player_Action_8084227C) ||
         ML_IS_HOST_FN(player->actionFunc, Player_Action_Roll))) {
        *scale *= 1.4f;
    }
    if ((kind == SOH_PLAYER_MOTION_WATER_SPEED) && (player->currentMask == PLAYER_MASK_BUNNY) &&
        !(player->stateFlags1 & PLAYER_STATE1_IN_WATER)) {
        *scale = 1.25f;
    }
}

// ML nops Player_IsChildWithHylianShield's "^ PLAYER_SHIELD_HYLIAN": a child holds the Hylian Shield like any shield.
// That nop also made the rule true for a child with no shield, which shows only once the shield is taken off; it's left
// out, so this is never true.
static void MlPlayer_ChildWithHylianShield(bool* should, va_list args) {
    *should = false;
}

// ML voids out without damage (func_808382DC's -16 is 0)
static void MlPlayer_InflictVoidDamage(bool* should, va_list args) {
    *should = false;
}

// ML's sequence mode is never "enemy"
static void MlPlayer_DetectBgmEnemy(bool* should, va_list args) {
    *should = false;
}

static void MlPlayer_OnLoadGame(int32_t fileNum) {
    sRoomFrames = sRoomKey = sRewindStep = 0;
    sTrigger = 0;
    sWipe = NULL;
    sCuccoHealth = sCuccoVoided = 0;
    sJumpStep = 0;
    sJumpPlayed = -1;
    sJumpTimer = 0;
}

void MlPlayer_Register(const SOHModApi* api) {
    ActorDBEntry* entry = ActorDB_Retrieve(ACTOR_PLAYER);

    sMlCuccoId = ActorDB_RetrieveId("ML_Cucco");
    sPlayerInit = entry->init;
    sPlayerUpdate = entry->update;
    entry->init = MlPlayer_Init;
    entry->update = MlPlayer_Update;

    // ML's child hands (ledger rows 77 and 95): a child holding the Hylian Shield draws ML's fist-and-shield hand (one
    // LOD for both; ML lets a child use the shield), and the child entries of the hookshot and hammer hands are the
    // closed right hand and the left fist
    sPlayerRightHandShieldDLs[PLAYER_SHIELD_HYLIAN * 4 + 1] = (Gfx*)gMlChildRightFistAndHylianShieldDL;
    sPlayerRightHandShieldDLs[PLAYER_SHIELD_HYLIAN * 4 + 3] = (Gfx*)gMlChildRightFistAndHylianShieldDL;
    sPlayerRightHandHookshotDLs[1] = (Gfx*)gLinkChildRightHandClosedNearDL;
    sPlayerRightHandHookshotDLs[3] = (Gfx*)gLinkChildRightHandClosedFarDL;
    sPlayerLeftHandHammerDLs[1] = (Gfx*)gLinkChildLeftFistNearDL;
    sPlayerLeftHandHammerDLs[3] = (Gfx*)gLinkChildLeftFistFarDL;

    SOH_REGISTER_HOOK(api, OnPlayerUpdate, MlPlayer_OnPlayerUpdate);
    SOH_REGISTER_HOOK(api, OnPlayerResolveMotionScale, MlPlayer_ResolveMotionScale);
    SOH_REGISTER_HOOK(api, OnLoadGame, MlPlayer_OnLoadGame);
    api->RegisterVB(VB_TRIGGER_VOIDOUT, MlHourglass_TriggerVoidOut);
    api->RegisterVB(VB_INFLICT_VOID_DAMAGE, MlPlayer_InflictVoidDamage);
    api->RegisterVB(VB_DETECT_BGM_ENEMY, MlPlayer_DetectBgmEnemy);
    api->RegisterVB(VB_BE_CHILD_WITH_HYLIAN_SHIELD, MlPlayer_ChildWithHylianShield);
    api->Log("[MissingLink] The player runs ML's changes: Magic Hourglass, triple jump, Soul-Charged sword and the rest");
}
