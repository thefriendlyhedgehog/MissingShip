/*
 * The Missing Link's tree that Mido says to cut down (ML actor 0x005), as a code mod.
 *
 * The same actor as MissingShip's Unbound branch (soh/src/overlays/actors/ovl_ML_TreeBridge), registered through the
 * mod API instead of the engine's ActorDB setup. Its key is its name in ML's rooms, so Kokiri Forest (spot04) places
 * it by name as before. Params 0x0406: low byte = switch flag, high byte = camera data index for the fall. A Blade Beam
 * (En_M_Thunder) within 45 units sets the switch; the tree catches fire (En_Light type 5), topples to 90 degrees with
 * two bounces and becomes the bridge Mido's line 0x1046 asks for. Its model is ML's (objects/object_ml_treebridge in
 * ML's layer); its collision lives in ML's overlay itself, so it is carried here as C data.
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "align_asset_macro.h"

#include <stdio.h>

#include "missinglink.h"

typedef struct MlTreeBridge {
    DynaPolyActor dyna;
    Actor* light;    // En_Light fire spawned while it burns
    s8 finished;     // the fall cutscene has run
    u8 state;
    s8 alreadyDown;  // switch was set before this spawn: lie flat, do nothing
    f32 fallStep;
    s32 timer;
} MlTreeBridge;

#define dgMlTreeBridgeDL "__OTR__objects/object_ml_treebridge/gMlTreeBridgeDL"
static const ALIGN_ASSET(2) char gMlTreeBridgeDL[] = dgMlTreeBridgeDL;
#define dgMlTreeBridgeXluDL "__OTR__objects/object_ml_treebridge/gMlTreeBridgeXluDL"
static const ALIGN_ASSET(2) char gMlTreeBridgeXluDL[] = dgMlTreeBridgeXluDL;

#include "z_ml_treebridge_col.inc"

static void MlTreeBridge_Init(Actor* thisx, PlayState* play) {
    MlTreeBridge* this = (MlTreeBridge*)thisx;
    s32 isDown = Flags_GetSwitch(play, thisx->params & 0xFF);

    // Once the Soul-Charged sword is owned, ML also changes Kokiri Forest's weather here.
    if (gSaveContext.inventory.ammo[SLOT_SLINGSHOT] == 40) {
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_WEATHER_TAG, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0x7607);
    }

    Actor_SetScale(thisx, 1.0f);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, &sMlTreeBridgeCol);
    DynaPolyActor_Init(&this->dyna, DPM_PLAYER);

    if (isDown) {
        this->alreadyDown = true;
        thisx->shape.rot.x = 0x4000;
    }
}

static void MlTreeBridge_Destroy(Actor* thisx, PlayState* play) {
    MlTreeBridge* this = (MlTreeBridge*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
}

static void MlTreeBridge_Update(Actor* thisx, PlayState* play) {
    MlTreeBridge* this = (MlTreeBridge*)thisx;
    s32 switchFlag = thisx->params & 0xFF;
    s32 isDown = Flags_GetSwitch(play, switchFlag);
    // ML looks for any actor 0x57 in the item-action category: the beam, but also a spin being charged here.
    Actor* beam = Actor_FindNearby(play, thisx, ACTOR_EN_M_THUNDER, ACTORCAT_ITEMACTION, 45.0f);

    if (this->alreadyDown) {
        return;
    }
    if (beam != NULL && !isDown) {
        Flags_SetSwitch(play, switchFlag);
        return;
    }
    if (!isDown || this->finished) {
        return;
    }

    Camera_ChangeDataIdx(&play->mainCamera, (u16)thisx->params >> 8);
    this->timer++;

    switch (this->state) {
        case 0:
            func_80064520(play, &play->csCtx);
            this->state = 1;
            break;
        case 1:
            this->light = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_LIGHT, thisx->world.pos.x, thisx->world.pos.y,
                                      thisx->world.pos.z, 0, 0, 0, 5);
            this->state = 2;
            break;
        case 2:
            Audio_PlayActorSound2(thisx, 0x2855);
            this->fallStep = 200.0f;
            this->state = 3;
            break;
        case 3:
            if (this->light != NULL) {
                this->light->scale.x -= 0.00015f;
                this->light->scale.y -= 0.00015f;
                this->light->scale.z -= 0.00015f;
            }
            Math_ScaledStepToS(&thisx->shape.rot.x, 0x4000, (s16)this->fallStep);
            if (this->timer >= 21) {
                this->fallStep += 40.0f;
            }
            if (thisx->shape.rot.x == 0x4000) {
                this->state = 4;
            }
            break;
        case 4: // first bounce
            Math_ScaledStepToS(&thisx->shape.rot.x, 0x3D00, 0x6E);
            if (thisx->shape.rot.x == 0x3D00) {
                Audio_PlayActorSound2(thisx, 0x2839);
                this->state = 5;
            }
            break;
        case 5:
            Math_ScaledStepToS(&thisx->shape.rot.x, 0x4000, 0x5A);
            if (thisx->shape.rot.x == 0x4000) {
                Audio_PlayActorSound2(thisx, 0x2839);
                this->state = 6;
            }
            break;
        case 6:
            Audio_PlayActorSound2(thisx, NA_SE_SY_CORRECT_CHIME);
            if (this->light != NULL) {
                Actor_Kill(this->light);
            }
            func_80064534(play, &play->csCtx);
            this->finished = true;
            break;
    }
}

static void MlTreeBridge_Draw(Actor* thisx, PlayState* play) {
    Gfx_DrawDListOpa(play, (Gfx*)gMlTreeBridgeDL);
    Gfx_DrawDListXlu(play, (Gfx*)gMlTreeBridgeXluDL);
}

void MlTreeBridge_Register(const SOHModApi* api) {
    SOHActorDefinition definition = { 0 };
    definition.structSize = sizeof(definition);
    definition.key = "ML_TreeBridge";
    definition.description = "The Missing Link: tree bridge";
    definition.category = ACTORCAT_BG;
    definition.actorFlags =
        ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    definition.objectId = OBJECT_WOOD02;
    definition.instanceSize = sizeof(MlTreeBridge);
    definition.init = MlTreeBridge_Init;
    definition.destroy = MlTreeBridge_Destroy;
    definition.update = MlTreeBridge_Update;
    definition.draw = MlTreeBridge_Draw;
    char message[64];
    snprintf(message, sizeof(message), "[MissingLink] ML_TreeBridge is actor %d", api->RegisterActor(&definition));
    api->Log(message);
}
