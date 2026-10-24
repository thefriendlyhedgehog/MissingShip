/*
 * The Missing Link's behaviour as a code mod (MissingShip, docs/ROADMAP.md step 5).
 *
 * ML's world (scenes, objects, text) is its layer, MissingLink_Unbound.o2r, which this mod's porter (ml_porter.c)
 * makes in the game from the player's own ROM and Kaze's patch (or the builder makes outside it). This mod adds what
 * ML does in code. Each ML actor registers under the name ML's rooms place it by.
 */
#include "missinglink.h"
#include "ml_porter.h"

static const SOHModRequirements sRequirements = { sizeof(SOHModRequirements), NULL, 0 };

static const SOHModApi* sApi;

SOH_MOD_EXPORT void ModSetApi(const SOHModApi* api) {
    sApi = api;
}

SOH_MOD_EXPORT const SOHModRequirements* ModGetRequirements(void) {
    return &sRequirements;
}

SOH_MOD_EXPORT void ModInit(void) {
    // Lines for crash reports, with or without ML's layer (ml_diag.c).
    MlDiag_Register(sApi);
    // A note that other mods may crash ML, before the porter's questions (ml_othermods.c).
    MlOtherMods_Register(sApi);
    // Without ML's layer only the porter runs: it offers to make the layer from the player's files.
    if (!MlPorter_Register(sApi)) {
        return;
    }
    MlOverrides_Register(sApi);
    MlScenes_Register(sApi);
    MlWorld_Register(sApi);
    MlHud_Register(sApi);
    MlText_Register(sApi);
    MlItems_Register(sApi);
    MlCutscenes_Register(sApi);
    MlMaps_Register(sApi);
    MlPause_Register(sApi);
    MlLabels_Register(sApi);
    MlGrotto_Register(sApi);
    MlCrackWall_Register(sApi);
    MlTreeBridge_Register(sApi);
    MlEnMd_Register(sApi);
    MlSongBlock_Register(sApi);
    MlPrologue_Register(sApi);
    MlBgmTrigger_Register(sApi);
    MlBgSpot02Objects_Register(sApi);
    MlTalkSpot_Register(sApi);
    MlVillager_Register(sApi);
    MlElf_Register(sApi);
    MlFishpond_Register(sApi);
    MlHintNuts_Register(sApi);
    MlHoleScrub_Register(sApi);
    MlLift_Register(sApi);
    MlLiftBlock_Register(sApi);
    MlElevator_Register(sApi);
    MlElevatorSpawner_Register(sApi);
    MlHakaLift_Register(sApi);
    MlRoomClearSwitch_Register(sApi);
    MlSlideBlock_Register(sApi);
    MlPillar_Register(sApi);
    MlThief_Register(sApi);
    MlBeamHeart_Register(sApi);
    MlSkullKid_Register(sApi);
    MlEnding_Register(sApi);
    MlEventSpawner_Register(sApi);
    MlTorch_Register(sApi);
    MlPushBlock_Register(sApi);
    MlDnsShop_Register(sApi);
    MlMalon_Register(sApi);
    MlCucco_Register(sApi);
    MlBossVa_Register(sApi);
    MlBossVaShot_Register(sApi);
    MlLight_Register(sApi);
    MlFogWall_Register(sApi);
    MlMThunder_Register(sApi);
    MlEnDns_Register(sApi);
    MlEnElf_Register(sApi);
    MlObjOshihiki_Register(sApi);
    MlVanillaNpcs_Register(sApi);
    MlVanillaEnemies_Register(sApi);
    MlVanillaObjects_Register(sApi);
    MlEnMag_Register(sApi);
    MlEnRd_Register(sApi);
    MlPlayer_Register(sApi); // after MlCucco_Register: it looks up ML_Cucco's id
}
