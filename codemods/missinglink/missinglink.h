#ifndef MISSINGLINK_H
#define MISSINGLINK_H

// The Missing Link's behaviour as a code mod: one Register function per actor, called from ModInit.

#include "soh/ModApi/ModApi.h"

// A game function's own address. In a Windows mod a game function's plain name is the mod's import thunk (a jump to
// it), so `this->actionFunc == EnMd_Walk` never holds there, and an actionFunc set to it is one the game's own
// compares (EnFloormas_SmWait's) never match. Calls by the plain name are fine. Declare the function at file scope
// with ML_HOST_FN_DECL(f); ML_IS_HOST_FN(p, f) compares a function pointer with it, ML_SET_HOST_FN(p, f) sets one.
#if defined(_WIN32)
#define ML_HOST_FN_DECL(f) extern void* __imp_##f
#define ML_HOST_FN(f) (__imp_##f)
#else
#define ML_HOST_FN_DECL(f) extern int MlHostFnDecl_##f
#define ML_HOST_FN(f) ((void*)(f))
#endif
#define ML_IS_HOST_FN(p, f) ((void*)(p) == ML_HOST_FN(f))
#define ML_SET_HOST_FN(p, f) (*(void**)&(p) = ML_HOST_FN(f))

void MlTreeBridge_Register(const SOHModApi* api);
void MlEnMd_Register(const SOHModApi* api);
void MlSongBlock_Register(const SOHModApi* api);
void MlPrologue_Register(const SOHModApi* api);
void MlBgmTrigger_Register(const SOHModApi* api);
void MlBgSpot02Objects_Register(const SOHModApi* api);
void MlTalkSpot_Register(const SOHModApi* api);
void MlVillager_Register(const SOHModApi* api);
void MlElf_Register(const SOHModApi* api);
void MlFishpond_Register(const SOHModApi* api);
void MlHintNuts_Register(const SOHModApi* api);
void MlHoleScrub_Register(const SOHModApi* api);
void MlLift_Register(const SOHModApi* api);
void MlLiftBlock_Register(const SOHModApi* api);
void MlElevator_Register(const SOHModApi* api);
void MlElevatorSpawner_Register(const SOHModApi* api);
void MlHakaLift_Register(const SOHModApi* api);
void MlRoomClearSwitch_Register(const SOHModApi* api);
void MlSlideBlock_Register(const SOHModApi* api);
void MlPillar_Register(const SOHModApi* api);
void MlThief_Register(const SOHModApi* api);
void MlBeamHeart_Register(const SOHModApi* api);
void MlSkullKid_Register(const SOHModApi* api);
void MlEnding_Register(const SOHModApi* api);
void MlEventSpawner_Register(const SOHModApi* api);
void MlTorch_Register(const SOHModApi* api);
void MlPushBlock_Register(const SOHModApi* api);
void MlDnsShop_Register(const SOHModApi* api);
void MlMalon_Register(const SOHModApi* api);
void MlCucco_Register(const SOHModApi* api);
void MlBossVa_Register(const SOHModApi* api);
void MlBossVaShot_Register(const SOHModApi* api);
void MlLight_Register(const SOHModApi* api);
void MlFogWall_Register(const SOHModApi* api);
void MlMThunder_Register(const SOHModApi* api);
void MlEnDns_Register(const SOHModApi* api);
void MlEnElf_Register(const SOHModApi* api);
void MlObjOshihiki_Register(const SOHModApi* api);
void MlVanillaNpcs_Register(const SOHModApi* api);
void MlVanillaEnemies_Register(const SOHModApi* api);
void MlVanillaObjects_Register(const SOHModApi* api);
void MlEnMag_Register(const SOHModApi* api);
void MlEnRd_Register(const SOHModApi* api);
void MlGrotto_Register(const SOHModApi* api);
void MlCrackWall_Register(const SOHModApi* api);
void MlPlayer_Register(const SOHModApi* api);
// not actors: ML's changes to vanilla actors (flags, and Bg_Treemouth not spawned), and its scenes' draw configs
void MlOverrides_Register(const SOHModApi* api);
void MlScenes_Register(const SOHModApi* api);
// not actors: ML's code patches elsewhere (the HUD and title cards, the text, sounds, sparkles, the world map, the
// title screen's setup and the scene refusal)
void MlHud_Register(const SOHModApi* api);
void MlText_Register(const SOHModApi* api);
void MlWorld_Register(const SOHModApi* api);
// not actors: ML's registries, which a stock Unbound does not read (ML's layer's missinglink/<name>, ml_json.h)
void MlItems_Register(const SOHModApi* api);
void MlCutscenes_Register(const SOHModApi* api);
void MlMaps_Register(const SOHModApi* api);
void MlPause_Register(const SOHModApi* api);
void MlLabels_Register(const SOHModApi* api);

// ML's Magic Hourglass (ml_player.c): the rewind's step, 0 when none plays
u16 MlHourglass_RewindStep(void);
// ML's 25 scene slots and custom scenes (ml_scenes.c)
s32 MlScenes_Exists(s16 sceneNum);
// ML's event entrances on a stock Unbound (ml_cutscenes.c): their rows pointed at ML's scenes, and an entrance's index
// by name (-1 when unknown)
void MlEntrances_Apply(void);
s32 MlEntrance_Index(const char* name);

#endif
