/*
 * The Missing Link's text changes and its own options, as a code mod.
 *
 * - ML's text runs at twice vanilla's speed, and slowed text (the text-speed control code) reveals 2 characters a
 *   step (0x80109908). MissingShip's Unbound branch makes 2 the default of SoH's Text Speed (TextSpeed.cpp) and
 *   reveals i + 2 in Message_DrawText (z_message_PAL.c).
 * - "B Cancels Choices" (MS-018, default on): on a choice prompt, B picks the "no / cancel" option and confirms it,
 *   and holding B (SoH's Skip Text) no longer confirms the highlighted one. The branch puts this in
 *   Message_ShouldAdvance (soh/soh/missinglink/MlChoice.c).
 * - Both of ML's own options go in Enhancements > Missing Link, as on the branch. SoH's options are never repeated
 *   there: each setting has one place.
 *
 * A mod cannot change SoH's default or Message_DrawText, so: while the player has never set Text Speed, it reads 2
 * while the play state draws (set at OnPlayDrawBegin, cleared at the end of the frame, before anything can save it);
 * Message_DrawText is its only reader there. OnPlayDrawBegin comes before Message_Draw on every frame, also those
 * Interface_Draw skips (SoH's "no UI" effect, the title screen and credits game modes). The slow-text step is
 * lengthened by one right after Message_Draw makes it. B is handled at the start of the frame, in the input every
 * actor reads that frame: the game copies fresh input over it the next frame. The only other reader of that input on a
 * choice prompt is SoH's gameplay stats, which count the frame's button presses in Play_Update; they are corrected
 * there to count the B press as the branch does (B, not A).
 */
#include "soh/ModApi/ModApi.h"

#include "functions.h"
#include "macros.h"
#include "variables.h"
#include <libultraship/bridge/consolevariablebridge.h>

#include "missinglink.h"

#define ML_CVAR_TEXT_SPEED "gEnhancements.TextSpeed"
#define ML_CVAR_B_CANCELS_CHOICE "gEnhancements.MissingLink.BCancelsChoice"
#define ML_CVAR_NO_BEAN_SELLER_FLASH "gEnhancements.MissingLink.NoBeanSellerFlash"

// --- text speed ---

static u8 sMlTextSpeedSet;
static u8 sMlTextStepPending;
static u16 sMlTextDrawPos;

static void MlText_OnPlayDrawBegin(void) {
    MessageContext* msgCtx;

    if (gPlayState == NULL) {
        return;
    }
    msgCtx = &gPlayState->msgCtx;
    if (CVarGetInteger(ML_CVAR_TEXT_SPEED, -1) == -1) { // never set
        CVarSetInteger(ML_CVAR_TEXT_SPEED, 2);
        sMlTextSpeedSet = true;
    }

    // Message_DrawText reveals the next character of slowed text once its delay timer runs out (i + 1)
    sMlTextStepPending = (msgCtx->textDelay != 0) && (msgCtx->textDelayTimer == 0);
    sMlTextDrawPos = msgCtx->textDrawPos;
}

static void MlText_OnGameFrameUpdate(void) {
    if (sMlTextSpeedSet) {
        CVarClear(ML_CVAR_TEXT_SPEED);
        sMlTextSpeedSet = false;
    }
    if (sMlTextStepPending && (gPlayState != NULL)) {
        MessageContext* msgCtx = &gPlayState->msgCtx;

        // the step was taken (the timer restarted, one more character): ML reveals two
        if ((msgCtx->textDelayTimer == msgCtx->textDelay) && (msgCtx->textDrawPos == sMlTextDrawPos + 1)) {
            msgCtx->textDrawPos++;
        }
    }
    sMlTextStepPending = false;
}

// --- B Cancels Choices ---

typedef struct {
    u16 textId;
    s8 bChoice; // option B selects, or -1: B does nothing (no cancel option; press A)
} MlChoiceBException;

// B selects the LAST option: in 128 of ML's 142 choice messages that is a refusal. The rest, found by MissingShip's
// tools/audit_choices.py, are below; keep this the same as the branch's MlChoice.c.
static const MlChoiceBException sMlChoiceBException[] = {
    { 0x003B, 2 },  // Farore's Wind: Return / Dispel / Exit
    { 0x006A, 1 },  // shop: I want to shop / Just looking
    { 0x10B3, 1 },  // OK / I can't
    { 0x2036, 0 },  // "go home?": No, I want to ride / Yes, I'm done  (refusal first)
    { 0x2066, 1 },  // "Did you get all that?": No / Yes  (Yes ends the explanation)
    { 0x4084, 2 },  // fishing: Weigh my fish / Let's talk / I want to quit
    { 0x4098, 0 },  // No, I don't want it / Yes, I want to keep it anyway  (refusal first)
    { 0x4099, 1 },  // Keep it anyway / I don't want it
    { 0x409A, -1 }, // Use it now / Get rid of it  (both act; B must not discard)
    { 0x1035, -1 }, // question menus: no cancel option
    { 0x300A, -1 },
    { 0x3034, -1 },
    { 0x6020, -1 }, // story reply: What if I am? / I hate Ganondorf!
    { 0x703A, 1 },  // OK / I don't believe you
};

// The option B selects on the current choice prompt, or -1 when B does nothing there.
static s32 MlChoice_BIndex(MessageContext* msgCtx) {
    for (u32 i = 0; i < ARRAY_COUNT(sMlChoiceBException); i++) {
        if (sMlChoiceBException[i].textId == msgCtx->textId) {
            return sMlChoiceBException[i].bChoice;
        }
    }
    return (msgCtx->textboxEndType == TEXTBOX_ENDTYPE_3_CHOICE) ? 2 : 1;
}

// Whether Play_Update counts this frame's button presses for the gameplay stats: FrameAdvance_Update's answer (it reads
// the second controller), worked out before Play_Update runs it
static s32 MlChoice_StatsCountPresses(PlayState* play) {
    FrameAdvanceContext* frameAdv = &play->frameAdvCtx;
    Input* input = &play->state.input[1];
    s32 enabled = frameAdv->enabled;

    if (gSaveContext.ship.stats.gameComplete) {
        return false;
    }
    if (CHECK_BTN_ALL(input->cur.button, BTN_R) && CHECK_BTN_ALL(input->press.button, BTN_DDOWN)) {
        enabled = !enabled;
    }
    return !enabled || CVarGetInteger("gDeveloperTools.FrameAdvanceTick", 0) ||
           (CHECK_BTN_ALL(input->cur.button, BTN_Z) &&
            (CHECK_BTN_ALL(input->press.button, BTN_R) ||
             (CHECK_BTN_ALL(input->cur.button, BTN_R) && (frameAdv->timer + 1 >= 9))));
}

// On a choice prompt, a B press becomes an A press on the cancel option, and B is taken out of the input so that
// Message_ShouldAdvance never sees it, pressed or held. A and C-Up confirm the highlighted option as before.
static void MlChoice_OnGameStateMainStart(void) {
    PlayState* play = gPlayState;
    Input* input;

    if ((play == NULL) || !CVarGetInteger(ML_CVAR_B_CANCELS_CHOICE, 1) ||
        (Message_GetState(&play->msgCtx) != TEXT_STATE_CHOICE)) {
        return;
    }
    input = &play->state.input[0];
    if (CHECK_BTN_ALL(input->press.button, BTN_B) && !CHECK_BTN_ALL(input->press.button, BTN_A) &&
        !CHECK_BTN_ALL(input->press.button, BTN_CUP)) {
        s32 index = MlChoice_BIndex(&play->msgCtx);

        // the stats count the press as B, as on the branch (Play_Update adds 1 to A for the A put in its place)
        if (MlChoice_StatsCountPresses(play)) {
            gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_B]++;
            if (index >= 0) {
                gSaveContext.ship.stats.count[COUNT_BUTTON_PRESSES_A]--;
            }
        }
        if (index >= 0) {
            play->msgCtx.choiceIndex = index;
            input->press.button |= BTN_A;
        }
    }
    input->press.button &= ~BTN_B;
    input->cur.button &= ~BTN_B;
}

// --- options ---

static void MlText_RegisterOptions(const SOHModApi* api) {
    SOHModMenuWidget widget = { 0 };

    api->RegisterMenuSidebar("Enhancements", "Missing Link", "gSettings.Menu.EnhancementsSidebarSection", 1);

    widget.structSize = sizeof(widget);
    widget.section = "Enhancements";
    widget.sidebar = "Missing Link";
    widget.column = 0;
    widget.type = SOH_MOD_MENU_CHECKBOX;
    widget.label = "B Cancels Choices";
    widget.cvar = ML_CVAR_B_CANCELS_CHOICE;
    widget.tooltip = "On a Yes/No prompt, B picks the \"no\" / cancel answer instead of whatever is highlighted, and "
                     "holding B to skip text never answers a prompt. Where a prompt has no cancel answer (e.g. \"Use "
                     "it now / Get rid of it\"), B does nothing: press A.";
    widget.defaultInt = 1;
    api->RegisterMenuWidget(&widget);

    widget.label = "No Bean Seller Flashing";
    widget.cvar = ML_CVAR_NO_BEAN_SELLER_FLASH;
    widget.tooltip = "In The Missing Link, the bean seller who trades the Bunny Hood flashes every few seconds. This "
                     "stops the flashing. Off keeps The Missing Link's behaviour.";
    widget.defaultInt = 0;
    api->RegisterMenuWidget(&widget);
}

void MlText_Register(const SOHModApi* api) {
    SOH_REGISTER_HOOK(api, OnPlayDrawBegin, MlText_OnPlayDrawBegin);
    SOH_REGISTER_HOOK(api, OnGameFrameUpdate, MlText_OnGameFrameUpdate);
    SOH_REGISTER_HOOK(api, OnGameStateMainStart, MlChoice_OnGameStateMainStart);
    MlText_RegisterOptions(api);
    api->Log("[MissingLink] ML's text speed and B Cancels Choices");
}
