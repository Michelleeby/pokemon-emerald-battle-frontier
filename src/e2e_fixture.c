#include "global.h"
#include "load_save.h"
#include "main.h"
#include "new_game.h"
#include "overworld.h"
#include "field_player_avatar.h"
#include "pokemon.h"
#include "random.h"
#include "save.h"
#include "script.h"
#include "string_util.h"
#include "constants/maps.h"
#include "constants/moves.h"
#include "constants/pokemon.h"
#include "constants/species.h"
#include "constants/battle_frontier.h"
#include "constants/frontier_util.h"
#include "constants/global.h"

#ifdef E2E_TESTING

#define E2E_FIXTURE_RUNNING 0x45324531
#define E2E_FIXTURE_SAVED   0x45324532
#define E2E_FIXTURE_FAILED  0x453245FF

// Read by the host harness to stop fixture generation deterministically.
EWRAM_DATA volatile u32 gE2EFixtureStatus = E2E_FIXTURE_RUNNING;

static void E2E_SaveFrontierLobbyFixture(void)
{
    PlayerFaceDirection(DIR_NORTH);
    if (TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK)
        gE2EFixtureStatus = E2E_FIXTURE_SAVED;
    else
        gE2EFixtureStatus = E2E_FIXTURE_FAILED;

    gFieldCallback = NULL;
    SetMainCallback2(NULL);
}

void E2E_CreateFrontierLobbyFixture(void)
{
    static const u8 sPlayerName[] = _("E2ETEST");
    u16 map;
    s16 x;
    s16 y;

    SetSaveBlocksPointers(0);
    Save_ResetSaveCounters();
    Sav2_ClearSetDefault();
    NewGameInitData();

    StringCopy(gSaveBlock2Ptr->playerName, sPlayerName);
    SetTrainerId(0x12345678, gSaveBlock2Ptr->playerTrainerId);
    SeedRng(0x1234);
    SeedRng2(0x5678);

    CreateMon(&gPlayerParty[0], SPECIES_TREECKO, 50, 31, TRUE,
              0x11111111, OT_ID_PLAYER_ID, 0);
    CreateMon(&gPlayerParty[1], SPECIES_TORCHIC, 50, 31, TRUE,
              0x22222222, OT_ID_PLAYER_ID, 0);
    CreateMon(&gPlayerParty[2], SPECIES_MUDKIP, 50, 31, TRUE,
              0x33333333, OT_ID_PLAYER_ID, 0);
    SetMonMoveSlot(&gPlayerParty[0], MOVE_LEAF_BLADE, 0);
    SetMonMoveSlot(&gPlayerParty[1], MOVE_FLAMETHROWER, 0);
    SetMonMoveSlot(&gPlayerParty[2], MOVE_SURF, 0);
    gPlayerParty[0].hp = gPlayerParty[0].maxHP = 999;
    gPlayerParty[0].attack = gPlayerParty[0].defense = 999;
    gPlayerParty[0].speed = gPlayerParty[0].spAttack = gPlayerParty[0].spDefense = 999;
    gPlayerPartyCount = 3;

    // The fixture runner selects Factory with ordinary keypad input before
    // advancing the first frame. No host memory mutation chooses the fixture.
    if (JOY_HELD(B_BUTTON))
    {
        map = MAP_BATTLE_FRONTIER_BATTLE_FACTORY_LOBBY;
        x = 4;
        y = 8;
        if (JOY_HELD(SELECT_BUTTON))
        {
            gSaveBlock1Ptr->frontierHardMode.factoryWinStreakActiveFlags =
                STREAK_FACTORY_SINGLES_50;
            gSaveBlock1Ptr->frontierHardMode
                .factoryWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 13;
        }
        else if (JOY_HELD(START_BUTTON))
        {
            gSaveBlock2Ptr->frontier.winStreakActiveFlags |=
                STREAK_FACTORY_SINGLES_50;
            gSaveBlock2Ptr->frontier
                .factoryWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 20;
        }
    }
    else
    {
        map = MAP_BATTLE_FRONTIER_BATTLE_TOWER_LOBBY;
        x = 6;
        y = 6;
        if (JOY_HELD(SELECT_BUTTON))
        {
            gSaveBlock1Ptr->frontierHardMode.towerWinStreakActiveFlags =
                STREAK_TOWER_SINGLES_50;
            gSaveBlock1Ptr->frontierHardMode
                .towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 19;
        }
        else if (JOY_HELD(START_BUTTON))
        {
            gSaveBlock2Ptr->frontier.winStreakActiveFlags |=
                STREAK_TOWER_SINGLES_50;
            gSaveBlock2Ptr->frontier
                .towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 33;
        }
    }

    SetWarpDestination(MAP_GROUP(map), MAP_NUM(map), WARP_ID_NONE, x, y);
    WarpIntoMap();
    ResetInitialPlayerAvatarState();
    ScriptContext_Init();
    UnlockPlayerFieldControls();
    gFieldCallback = E2E_SaveFrontierLobbyFixture;
    SetMainCallback2(CB2_LoadMap);
}

#endif
