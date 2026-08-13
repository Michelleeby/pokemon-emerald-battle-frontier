#include "global.h"
#include "frontier_intro_tutorial.h"
#include "main.h"
#include "pokenav.h"
#include "script.h"

enum
{
    INPUT_PHASE_WAIT,
    INPUT_PHASE_DELAY,
    INPUT_PHASE_PRESS,
    INPUT_PHASE_RELEASE,
};

#define PRESS_FRAMES 8
#define RELEASE_FRAMES 30
#define FAST_RELEASE_FRAMES 15
#define VERY_FAST_RELEASE_FRAMES 6
#define SLOW_DELAY_FRAMES 60
#define SKIP_DOUBLE_TAP_FRAMES 60

struct TutorialAction
{
    u8 checkpoint;
    u8 delayFrames;
    u8 releaseFrames;
    u16 button;
};

struct FrontierIntroTutorial
{
    bool8 active;
    u8 step;
    u8 phase;
    u8 timer;
    u8 skipTapTimer;
    u16 button;
    bool8 skipping;
    bool8 skipTerminal;
};

static EWRAM_DATA struct FrontierIntroTutorial sTutorial = {0};

#define ACTION(checkpoint, key) {FRONTIER_INTRO_CHECKPOINT_##checkpoint, 0, RELEASE_FRAMES, key}
#define FAST_ACTION(checkpoint, key) {FRONTIER_INTRO_CHECKPOINT_##checkpoint, 0, FAST_RELEASE_FRAMES, key}
#define VERY_FAST_ACTION(checkpoint, key) {FRONTIER_INTRO_CHECKPOINT_##checkpoint, 0, VERY_FAST_RELEASE_FRAMES, key}
#define SLOW_ACTION(checkpoint, key) {FRONTIER_INTRO_CHECKPOINT_##checkpoint, SLOW_DELAY_FRAMES, RELEASE_FRAMES, key}

static const struct TutorialAction sActions[] =
{
    ACTION(POKENAV_MAIN, DPAD_DOWN),
    ACTION(POKENAV_MAIN, A_BUTTON),
    SLOW_ACTION(POKENAV_CONDITION, A_BUTTON),
    ACTION(TEAM_LAB_PARTY, DPAD_DOWN),
    ACTION(TEAM_LAB_PARTY, A_BUTTON),

    // Species: search the National Pokédex for JKL / Blue / Psychic.
    ACTION(TEAM_LAB_EDITOR, A_BUTTON),
    ACTION(POKEDEX_LIST, SELECT_BUTTON),
    ACTION(POKEDEX_SEARCH_TOP, A_BUTTON),
    ACTION(POKEDEX_SEARCH_MENU, A_BUTTON),
    FAST_ACTION(POKEDEX_SEARCH_PARAMETER, DPAD_DOWN),
    FAST_ACTION(POKEDEX_SEARCH_PARAMETER, DPAD_DOWN),
    FAST_ACTION(POKEDEX_SEARCH_PARAMETER, DPAD_DOWN),
    FAST_ACTION(POKEDEX_SEARCH_PARAMETER, DPAD_DOWN),
    ACTION(POKEDEX_SEARCH_PARAMETER, A_BUTTON),
    ACTION(POKEDEX_SEARCH_MENU, DPAD_DOWN),
    ACTION(POKEDEX_SEARCH_MENU, A_BUTTON),
    FAST_ACTION(POKEDEX_SEARCH_PARAMETER, DPAD_DOWN),
    FAST_ACTION(POKEDEX_SEARCH_PARAMETER, DPAD_DOWN),
    ACTION(POKEDEX_SEARCH_PARAMETER, A_BUTTON),
    ACTION(POKEDEX_SEARCH_MENU, DPAD_DOWN),
    ACTION(POKEDEX_SEARCH_MENU, A_BUTTON),
    VERY_FAST_ACTION(POKEDEX_SEARCH_PARAMETER, DPAD_DOWN),
    VERY_FAST_ACTION(POKEDEX_SEARCH_PARAMETER, DPAD_DOWN),
    VERY_FAST_ACTION(POKEDEX_SEARCH_PARAMETER, DPAD_DOWN),
    VERY_FAST_ACTION(POKEDEX_SEARCH_PARAMETER, DPAD_DOWN),
    VERY_FAST_ACTION(POKEDEX_SEARCH_PARAMETER, DPAD_DOWN),
    VERY_FAST_ACTION(POKEDEX_SEARCH_PARAMETER, DPAD_DOWN),
    VERY_FAST_ACTION(POKEDEX_SEARCH_PARAMETER, DPAD_DOWN),
    VERY_FAST_ACTION(POKEDEX_SEARCH_PARAMETER, DPAD_DOWN),
    VERY_FAST_ACTION(POKEDEX_SEARCH_PARAMETER, DPAD_DOWN),
    VERY_FAST_ACTION(POKEDEX_SEARCH_PARAMETER, DPAD_DOWN),
    VERY_FAST_ACTION(POKEDEX_SEARCH_PARAMETER, DPAD_DOWN),
    VERY_FAST_ACTION(POKEDEX_SEARCH_PARAMETER, DPAD_DOWN),
    VERY_FAST_ACTION(POKEDEX_SEARCH_PARAMETER, DPAD_DOWN),
    VERY_FAST_ACTION(POKEDEX_SEARCH_PARAMETER, DPAD_DOWN),
    ACTION(POKEDEX_SEARCH_PARAMETER, A_BUTTON),
    ACTION(POKEDEX_SEARCH_MENU, DPAD_DOWN),
    ACTION(POKEDEX_SEARCH_MENU, DPAD_DOWN),
    ACTION(POKEDEX_SEARCH_MENU, DPAD_DOWN),
    ACTION(POKEDEX_SEARCH_MENU, A_BUTTON),
    ACTION(POKEDEX_SEARCH_COMPLETE, A_BUTTON),
    ACTION(POKEDEX_RESULTS, A_BUTTON),

    // Timid nature (+Speed, -Attack), Levitate, and Lum Berry.
    ACTION(TEAM_LAB_EDITOR, DPAD_DOWN),
    ACTION(TEAM_LAB_EDITOR, A_BUTTON),
    ACTION(TEAM_LAB_EDITOR, DPAD_DOWN),
    ACTION(TEAM_LAB_EDITOR, DPAD_DOWN),
    ACTION(TEAM_LAB_EDITOR, A_BUTTON),
    ACTION(TEAM_LAB_EDITOR, A_BUTTON),
    ACTION(TEAM_LAB_EDITOR, A_BUTTON),
    ACTION(TEAM_LAB_EDITOR, DPAD_DOWN),
    ACTION(TEAM_LAB_EDITOR, A_BUTTON),
    ACTION(TEAM_LAB_EDITOR, A_BUTTON),
    ACTION(TEAM_LAB_EDITOR, DPAD_DOWN),
    ACTION(TEAM_LAB_EDITOR, DPAD_DOWN),
    ACTION(TEAM_LAB_EDITOR, A_BUTTON),
    ACTION(TEAM_LAB_ITEM, R_BUTTON),
    VERY_FAST_ACTION(TEAM_LAB_ITEM, DPAD_DOWN),
    VERY_FAST_ACTION(TEAM_LAB_ITEM, DPAD_DOWN),
    VERY_FAST_ACTION(TEAM_LAB_ITEM, DPAD_DOWN),
    VERY_FAST_ACTION(TEAM_LAB_ITEM, DPAD_DOWN),
    VERY_FAST_ACTION(TEAM_LAB_ITEM, DPAD_DOWN),
    VERY_FAST_ACTION(TEAM_LAB_ITEM, DPAD_DOWN),
    VERY_FAST_ACTION(TEAM_LAB_ITEM, DPAD_DOWN),
    VERY_FAST_ACTION(TEAM_LAB_ITEM, DPAD_DOWN),
    ACTION(TEAM_LAB_ITEM, A_BUTTON),

    // Stats: 252 Sp. Attack / 252 Speed / 4 HP.
    ACTION(TEAM_LAB_EDITOR, R_BUTTON),
    ACTION(TEAM_LAB_EDITOR, SELECT_BUTTON),
    ACTION(TEAM_LAB_EDITOR, DPAD_DOWN),
    ACTION(TEAM_LAB_EDITOR, DPAD_DOWN),
    ACTION(TEAM_LAB_EDITOR, DPAD_DOWN),
    ACTION(TEAM_LAB_EDITOR, A_BUTTON),
    ACTION(TEAM_LAB_EDITOR, DPAD_DOWN),
    ACTION(TEAM_LAB_EDITOR, DPAD_DOWN),
    ACTION(TEAM_LAB_EDITOR, DPAD_DOWN),
    ACTION(TEAM_LAB_EDITOR, A_BUTTON),
    ACTION(TEAM_LAB_EDITOR, DPAD_DOWN),
    ACTION(TEAM_LAB_EDITOR, DPAD_DOWN),
    ACTION(TEAM_LAB_EDITOR, DPAD_DOWN),
    ACTION(TEAM_LAB_EDITOR, DPAD_DOWN),
    ACTION(TEAM_LAB_EDITOR, DPAD_DOWN),
    ACTION(TEAM_LAB_EDITOR, A_BUTTON),
    ACTION(TEAM_LAB_EDITOR, A_BUTTON),

    // Moves: choose the first empty slot and type PSYCHIC exactly.
    ACTION(TEAM_LAB_EDITOR, R_BUTTON),
    ACTION(TEAM_LAB_EDITOR, A_BUTTON),
    VERY_FAST_ACTION(NAMING_SCREEN, DPAD_DOWN),
    VERY_FAST_ACTION(NAMING_SCREEN, DPAD_DOWN),
    VERY_FAST_ACTION(NAMING_SCREEN, DPAD_RIGHT),
    VERY_FAST_ACTION(NAMING_SCREEN, DPAD_RIGHT),
    VERY_FAST_ACTION(NAMING_SCREEN, DPAD_RIGHT),
    VERY_FAST_ACTION(NAMING_SCREEN, A_BUTTON),
    VERY_FAST_ACTION(NAMING_SCREEN, DPAD_RIGHT),
    VERY_FAST_ACTION(NAMING_SCREEN, DPAD_RIGHT),
    VERY_FAST_ACTION(NAMING_SCREEN, DPAD_RIGHT),
    VERY_FAST_ACTION(NAMING_SCREEN, A_BUTTON),
    VERY_FAST_ACTION(NAMING_SCREEN, DPAD_DOWN),
    VERY_FAST_ACTION(NAMING_SCREEN, DPAD_LEFT),
    VERY_FAST_ACTION(NAMING_SCREEN, A_BUTTON),
    VERY_FAST_ACTION(NAMING_SCREEN, DPAD_UP),
    VERY_FAST_ACTION(NAMING_SCREEN, DPAD_UP),
    VERY_FAST_ACTION(NAMING_SCREEN, DPAD_UP),
    VERY_FAST_ACTION(NAMING_SCREEN, DPAD_LEFT),
    VERY_FAST_ACTION(NAMING_SCREEN, DPAD_LEFT),
    VERY_FAST_ACTION(NAMING_SCREEN, DPAD_LEFT),
    VERY_FAST_ACTION(NAMING_SCREEN, A_BUTTON),
    VERY_FAST_ACTION(NAMING_SCREEN, DPAD_DOWN),
    VERY_FAST_ACTION(NAMING_SCREEN, DPAD_LEFT),
    VERY_FAST_ACTION(NAMING_SCREEN, A_BUTTON),
    VERY_FAST_ACTION(NAMING_SCREEN, DPAD_RIGHT),
    VERY_FAST_ACTION(NAMING_SCREEN, A_BUTTON),
    VERY_FAST_ACTION(NAMING_SCREEN, DPAD_UP),
    VERY_FAST_ACTION(NAMING_SCREEN, A_BUTTON),
    FAST_ACTION(NAMING_SCREEN, START_BUTTON),
    FAST_ACTION(NAMING_SCREEN, A_BUTTON),

    // Discard the demonstration build, return to the party menu, and close Team Lab.
    ACTION(TEAM_LAB_EDITOR, B_BUTTON),
    ACTION(TEAM_LAB_PARTY, B_BUTTON),
};

#undef ACTION
#undef FAST_ACTION
#undef VERY_FAST_ACTION
#undef SLOW_ACTION

static void FinishTutorial(void)
{
    sTutorial.active = FALSE;
    sTutorial.button = 0;
    ScriptContext_Enable();
}

static void BeginSkipAction(u8 checkpoint)
{
    sTutorial.button = B_BUTTON;
    sTutorial.skipTerminal = FALSE;

    switch (checkpoint)
    {
    case FRONTIER_INTRO_CHECKPOINT_POKENAV_MAIN:
    case FRONTIER_INTRO_CHECKPOINT_TEAM_LAB_PARTY:
        sTutorial.skipTerminal = TRUE;
        break;
    case FRONTIER_INTRO_CHECKPOINT_POKENAV_CONDITION:
    case FRONTIER_INTRO_CHECKPOINT_TEAM_LAB_EDITOR:
    case FRONTIER_INTRO_CHECKPOINT_POKEDEX_LIST:
    case FRONTIER_INTRO_CHECKPOINT_POKEDEX_SEARCH_TOP:
    case FRONTIER_INTRO_CHECKPOINT_POKEDEX_SEARCH_MENU:
    case FRONTIER_INTRO_CHECKPOINT_POKEDEX_SEARCH_PARAMETER:
    case FRONTIER_INTRO_CHECKPOINT_POKEDEX_RESULTS:
    case FRONTIER_INTRO_CHECKPOINT_TEAM_LAB_ITEM:
        break;
    default:
        return;
    }

    sTutorial.phase = INPUT_PHASE_PRESS;
    sTutorial.timer = PRESS_FRAMES;
}

static void BeginCurrentStep(void)
{
    if (sTutorial.step == ARRAY_COUNT(sActions))
    {
        FinishTutorial();
        return;
    }

    sTutorial.button = sActions[sTutorial.step].button;
    if (sActions[sTutorial.step].delayFrames != 0)
    {
        sTutorial.phase = INPUT_PHASE_DELAY;
        sTutorial.timer = sActions[sTutorial.step].delayFrames;
    }
    else
    {
        sTutorial.phase = INPUT_PHASE_PRESS;
        sTutorial.timer = PRESS_FRAMES;
    }
}

void StartFrontierIntroTutorial(void)
{
    sTutorial.active = TRUE;
    sTutorial.step = 0;
    sTutorial.phase = INPUT_PHASE_WAIT;
    sTutorial.timer = 0;
    sTutorial.button = 0;
    sTutorial.skipTapTimer = 0;
    sTutorial.skipping = FALSE;
    sTutorial.skipTerminal = FALSE;
    OpenPokenavFromOverworld();
}

#ifdef TESTING
void FrontierIntroTutorial_TestStart(void)
{
    sTutorial.active = TRUE;
    sTutorial.step = 0;
    sTutorial.phase = INPUT_PHASE_WAIT;
    sTutorial.timer = 0;
    sTutorial.button = 0;
    sTutorial.skipTapTimer = 0;
    sTutorial.skipping = FALSE;
    sTutorial.skipTerminal = FALSE;
}

bool8 FrontierIntroTutorial_TestIsActive(void)
{
    return sTutorial.active;
}

u8 FrontierIntroTutorial_TestGetStep(void)
{
    return sTutorial.step;
}
#endif

void FrontierIntroTutorial_NotifyReady(u8 checkpoint)
{
    if (!sTutorial.active || sTutorial.phase != INPUT_PHASE_WAIT)
        return;

    if (sTutorial.skipping)
    {
        BeginSkipAction(checkpoint);
        return;
    }

    if ((sTutorial.step < ARRAY_COUNT(sActions)
      && checkpoint == sActions[sTutorial.step].checkpoint)
     || (sTutorial.step == ARRAY_COUNT(sActions)
      && checkpoint == FRONTIER_INTRO_CHECKPOINT_TEAM_LAB_EDITOR))
        BeginCurrentStep();
}

bool8 FrontierIntroTutorial_IsSkipping(void)
{
    return sTutorial.active && sTutorial.skipping;
}

void FrontierIntroTutorial_CompleteSkip(void)
{
    if (FrontierIntroTutorial_IsSkipping())
        FinishTutorial();
}

void FrontierIntroTutorial_ApplyInput(void)
{
    if (!sTutorial.active)
        return;

    if (sTutorial.skipTapTimer != 0)
        sTutorial.skipTapTimer--;
    if (gMain.newKeys & B_BUTTON)
    {
        if (sTutorial.skipTapTimer != 0)
        {
            sTutorial.skipping = TRUE;
            sTutorial.skipTapTimer = 0;
            sTutorial.skipTerminal = FALSE;
            sTutorial.phase = INPUT_PHASE_WAIT;
            sTutorial.timer = 0;
            sTutorial.button = 0;
        }
        else
        {
            sTutorial.skipTapTimer = SKIP_DOUBLE_TAP_FRAMES;
        }
    }

    gMain.heldKeys = 0;
    gMain.newKeys = 0;
    gMain.newAndRepeatedKeys = 0;

    if (sTutorial.phase == INPUT_PHASE_WAIT)
        return;

    if (sTutorial.phase == INPUT_PHASE_PRESS)
    {
        gMain.heldKeys = sTutorial.button;
        if (sTutorial.timer == PRESS_FRAMES)
        {
            gMain.newKeys = sTutorial.button;
            gMain.newAndRepeatedKeys = sTutorial.button;
        }
    }

    if (sTutorial.timer != 0)
        sTutorial.timer--;

    if (sTutorial.timer == 0)
    {
        if (sTutorial.phase == INPUT_PHASE_DELAY)
        {
            sTutorial.phase = INPUT_PHASE_PRESS;
            sTutorial.timer = PRESS_FRAMES;
        }
        else if (sTutorial.phase == INPUT_PHASE_PRESS)
        {
            sTutorial.phase = INPUT_PHASE_RELEASE;
            sTutorial.timer = sActions[sTutorial.step].releaseFrames;
        }
        else
        {
            sTutorial.phase = INPUT_PHASE_WAIT;
            if (sTutorial.skipping)
            {
                sTutorial.skipTerminal = FALSE;
            }
            else
            {
                sTutorial.step++;
                if (sTutorial.step == ARRAY_COUNT(sActions))
                    BeginCurrentStep();
            }
        }
    }
}
