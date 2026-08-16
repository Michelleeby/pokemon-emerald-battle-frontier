#include "global.h"
#include "battle_dome.h"
#include "event_data.h"
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

static void E2E_SeedActiveFrontierChallenge(u8 facility, u8 challengeMode)
{
    u32 i;

    VarSet(VAR_FRONTIER_FACILITY, facility);
    VarSet(VAR_FRONTIER_BATTLE_MODE, FRONTIER_MODE_SINGLES);
    gSaveBlock2Ptr->frontier.challengeStatus = CHALLENGE_STATUS_SAVING;
    gSaveBlock2Ptr->frontier.lvlMode = FRONTIER_LVL_50;
    gSaveBlock2Ptr->frontier.challengePaused = FALSE;
    gSaveBlock2Ptr->frontier.disableRecordBattle = FALSE;
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = 0;
    gSaveBlock2Ptr->frontier.frontierChallengeMode = challengeMode;
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
        gSaveBlock2Ptr->frontier.selectedPartyMons[i] = i + 1;
    SavePlayerParty();
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
    if (JOY_HELD(A_BUTTON))
    {
        if (JOY_HELD(B_BUTTON))
        {
            map = MAP_BATTLE_FRONTIER_BATTLE_PIKE_THREE_PATH_ROOM;
            x = 6;
            y = 10;
            E2E_SeedActiveFrontierChallenge(
                FRONTIER_FACILITY_PIKE,
                JOY_HELD(SELECT_BUTTON) ? FRONTIER_CHALLENGE_HARD
                                        : FRONTIER_CHALLENGE_NORMAL);
            gSaveBlock2Ptr->frontier.challengeStatus = 99;
            gSaveBlock2Ptr->frontier.curChallengeBattleNum = 11;
            gSaveBlock2Ptr->frontier.selectedPartyMons[3] = 0xE2E1;
            if (JOY_HELD(SELECT_BUTTON))
            {
                gSaveBlock1Ptr->frontierHardMode.pikeWinStreakActiveFlags =
                    STREAK_PIKE_50;
                gSaveBlock1Ptr->frontierHardMode.pikeWinStreaks[FRONTIER_LVL_50] = 10;
            }
            else
            {
                gSaveBlock2Ptr->frontier.winStreakActiveFlags |= STREAK_PIKE_50;
                gSaveBlock2Ptr->frontier.pikeWinStreaks[FRONTIER_LVL_50] = 24;
            }
        }
        else
        {
            map = MAP_BATTLE_FRONTIER_BATTLE_PALACE_BATTLE_ROOM;
            x = 7;
            y = 6;
            if (JOY_HELD(SELECT_BUTTON))
            {
                E2E_SeedActiveFrontierChallenge(
                    FRONTIER_FACILITY_PALACE,
                    FRONTIER_CHALLENGE_HARD);
                gSaveBlock1Ptr->frontierHardMode.palaceWinStreakActiveFlags =
                    STREAK_PALACE_SINGLES_50;
                gSaveBlock1Ptr->frontierHardMode
                    .palaceWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 12;
            }
            else
            {
                E2E_SeedActiveFrontierChallenge(
                    FRONTIER_FACILITY_PALACE,
                    FRONTIER_CHALLENGE_NORMAL);
                gSaveBlock2Ptr->frontier.winStreakActiveFlags |=
                    STREAK_PALACE_SINGLES_50;
                gSaveBlock2Ptr->frontier
                    .palaceWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 19;
            }
        }
    }
    else if (JOY_HELD(L_BUTTON))
    {
        if (JOY_HELD(SELECT_BUTTON))
        {
            map = MAP_BATTLE_FRONTIER_BATTLE_DOME_PRE_BATTLE_ROOM;
            x = 5;
            y = 7;
            E2E_SeedActiveFrontierChallenge(
                FRONTIER_FACILITY_DOME,
                FRONTIER_CHALLENGE_HARD);
            gSaveBlock2Ptr->frontier.winStreakActiveFlags |=
                STREAK_DOME_HARD_SINGLES_50;
            gSaveBlock2Ptr->frontier
                .domeHardWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 2;
            gSaveBlock2Ptr->frontier.selectedPartyMons[3] = 0x0201;
            E2E_InitDomeTournament();
        }
        else if (JOY_HELD(START_BUTTON))
        {
            map = MAP_BATTLE_FRONTIER_BATTLE_DOME_PRE_BATTLE_ROOM;
            x = 5;
            y = 7;
            E2E_SeedActiveFrontierChallenge(
                FRONTIER_FACILITY_DOME,
                FRONTIER_CHALLENGE_NORMAL);
            gSaveBlock2Ptr->frontier.winStreakActiveFlags |=
                STREAK_DOME_SINGLES_50;
            gSaveBlock2Ptr->frontier
                .domeWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 4;
            gSaveBlock2Ptr->frontier.selectedPartyMons[3] = 0x0201;
            E2E_InitDomeTournament();
        }
        else
        {
            map = MAP_BATTLE_FRONTIER_BATTLE_DOME_LOBBY;
            x = 5;
            y = 11;
        }
    }
    else if (JOY_HELD(R_BUTTON))
    {
        if (JOY_HELD(SELECT_BUTTON))
        {
            map = MAP_BATTLE_FRONTIER_BATTLE_ARENA_BATTLE_ROOM;
            x = 7;
            y = 5;
            E2E_SeedActiveFrontierChallenge(
                FRONTIER_FACILITY_ARENA,
                FRONTIER_CHALLENGE_HARD);
            gSaveBlock1Ptr->frontierHardMode.arenaWinStreakActiveFlags =
                STREAK_ARENA_50;
            gSaveBlock1Ptr->frontierHardMode
                .arenaWinStreaks[FRONTIER_LVL_50] = 12;
        }
        else if (JOY_HELD(START_BUTTON))
        {
            map = MAP_BATTLE_FRONTIER_BATTLE_ARENA_BATTLE_ROOM;
            x = 7;
            y = 5;
            E2E_SeedActiveFrontierChallenge(
                FRONTIER_FACILITY_ARENA,
                FRONTIER_CHALLENGE_NORMAL);
            gSaveBlock2Ptr->frontier.winStreakActiveFlags |= STREAK_ARENA_50;
            gSaveBlock2Ptr->frontier.arenaWinStreaks[FRONTIER_LVL_50] = 26;
        }
        else
        {
            map = MAP_BATTLE_FRONTIER_BATTLE_ARENA_LOBBY;
            x = 7;
            y = 8;
        }
    }
    else if (JOY_HELD(B_BUTTON))
    {
        if (JOY_HELD(SELECT_BUTTON))
        {
            map = MAP_BATTLE_FRONTIER_BATTLE_FACTORY_PRE_BATTLE_ROOM;
            x = 8;
            y = 13;
            E2E_SeedActiveFrontierChallenge(
                FRONTIER_FACILITY_FACTORY,
                FRONTIER_CHALLENGE_HARD);
            gSaveBlock2Ptr->frontier.selectedPartyMons[3] = 0xFAC1;
            gSaveBlock1Ptr->frontierHardMode.factoryWinStreakActiveFlags =
                STREAK_FACTORY_SINGLES_50;
            gSaveBlock1Ptr->frontierHardMode
                .factoryWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 12;
        }
        else if (JOY_HELD(START_BUTTON))
        {
            map = MAP_BATTLE_FRONTIER_BATTLE_FACTORY_PRE_BATTLE_ROOM;
            x = 8;
            y = 13;
            E2E_SeedActiveFrontierChallenge(
                FRONTIER_FACILITY_FACTORY,
                FRONTIER_CHALLENGE_NORMAL);
            gSaveBlock2Ptr->frontier.selectedPartyMons[3] = 0xFAC1;
            gSaveBlock2Ptr->frontier.winStreakActiveFlags |=
                STREAK_FACTORY_SINGLES_50;
            gSaveBlock2Ptr->frontier
                .factoryWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 19;
        }
        else
        {
            map = MAP_BATTLE_FRONTIER_BATTLE_FACTORY_LOBBY;
            x = 4;
            y = 8;
        }
    }
    else
    {
        if (JOY_HELD(SELECT_BUTTON))
        {
            map = MAP_BATTLE_FRONTIER_BATTLE_TOWER_BATTLE_ROOM;
            x = 5;
            y = 8;
            E2E_SeedActiveFrontierChallenge(
                FRONTIER_FACILITY_TOWER,
                FRONTIER_CHALLENGE_HARD);
            gSaveBlock1Ptr->frontierHardMode.towerWinStreakActiveFlags =
                STREAK_TOWER_SINGLES_50;
            gSaveBlock1Ptr->frontierHardMode
                .towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 19;
        }
        else if (JOY_HELD(START_BUTTON))
        {
            map = MAP_BATTLE_FRONTIER_BATTLE_TOWER_BATTLE_ROOM;
            x = 5;
            y = 8;
            E2E_SeedActiveFrontierChallenge(
                FRONTIER_FACILITY_TOWER,
                FRONTIER_CHALLENGE_NORMAL);
            gSaveBlock2Ptr->frontier.winStreakActiveFlags |=
                STREAK_TOWER_SINGLES_50;
            gSaveBlock2Ptr->frontier
                .towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 33;
        }
        else
        {
            map = MAP_BATTLE_FRONTIER_BATTLE_TOWER_LOBBY;
            x = 6;
            y = 6;
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
