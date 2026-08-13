#include "global.h"
#include "battle.h"
#include "battle_setup.h"
#include "battle_tower.h"
#include "event_data.h"
#include "frontier_util.h"
#include "pokemon.h"
#include "test.h"
#include "constants/battle_frontier.h"
#include "constants/battle_frontier_trainers.h"
#include "constants/battle_tower.h"
#include "constants/flags.h"
#include "constants/frontier_util.h"
#include "constants/pokemon.h"
#include "constants/vars.h"

static void SetTowerContext(u8 challengeMode, u8 battleMode, u8 lvlMode)
{
    VarSet(VAR_FRONTIER_FACILITY, FRONTIER_FACILITY_TOWER);
    VarSet(VAR_FRONTIER_BATTLE_MODE, battleMode);
    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_SET_DATA;
    gSpecialVar_0x8005 = FRONTIER_DATA_CHALLENGE_MODE;
    gSpecialVar_0x8006 = challengeMode;
    CallFrontierUtilFunc();
    gSpecialVar_0x8005 = FRONTIER_DATA_LVL_MODE;
    gSpecialVar_0x8006 = lvlMode;
    CallFrontierUtilFunc();
}

static void CallTower(u16 function)
{
    gSpecialVar_0x8004 = function;
    CallBattleTowerFunc();
}

static void IncrementFrontierStreak(void)
{
    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_INCREMENT_STREAK;
    CallFrontierUtilFunc();
}

static void SetFrontierData(u16 data, u16 value)
{
    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_SET_DATA;
    gSpecialVar_0x8005 = data;
    gSpecialVar_0x8006 = value;
    CallFrontierUtilFunc();
}

static void SetTowerData(u16 data, u16 value)
{
    gSpecialVar_0x8005 = data;
    gSpecialVar_0x8006 = value;
    CallTower(BATTLE_TOWER_FUNC_SET_DATA);
}

static void AssertTrainerInRange(u16 minimum, u16 maximum)
{
    TEST_ASSERT(gTrainerBattleOpponent_A >= minimum);
    TEST_ASSERT(gTrainerBattleOpponent_A <= maximum);
}

static void TestNormalAndHardChallengeInit(void)
{
    u32 i;

    TEST_ASSERT(FALSE);
    TestResetFixture(0x1001);
    SetTowerContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 12;
    gSaveBlock1Ptr->frontierHardMode.towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 37;
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = 5;
    gSaveBlock2Ptr->frontier.challengePaused = TRUE;
    gSaveBlock2Ptr->frontier.disableRecordBattle = TRUE;
    CallTower(BATTLE_TOWER_FUNC_INIT);
    TEST_ASSERT_EQ(CHALLENGE_STATUS_SAVING, gSaveBlock2Ptr->frontier.challengeStatus);
    TEST_ASSERT_EQ(0, gSaveBlock2Ptr->frontier.curChallengeBattleNum);
    TEST_ASSERT(!gSaveBlock2Ptr->frontier.challengePaused);
    TEST_ASSERT(!gSaveBlock2Ptr->frontier.disableRecordBattle);
    TEST_ASSERT_EQ(0, gSaveBlock2Ptr->frontier.towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50]);
    TEST_ASSERT_EQ(37, gSaveBlock1Ptr->frontierHardMode.towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50]);
    for (i = 0; i < ARRAY_COUNT(gSaveBlock2Ptr->frontier.trainerIds); i++)
        TEST_ASSERT_EQ(0xFFFF, gSaveBlock2Ptr->frontier.trainerIds[i]);

    TestResetFixture(0x1002);
    SetTowerContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_DOUBLES, FRONTIER_LVL_OPEN);
    gSaveBlock2Ptr->frontier.towerWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN] = 23;
    gSaveBlock1Ptr->frontierHardMode.towerWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN] = 9;
    gSaveBlock1Ptr->frontierHardMode.towerWinStreakActiveFlags = STREAK_TOWER_DOUBLES_OPEN;
    CallTower(BATTLE_TOWER_FUNC_INIT);
    TEST_ASSERT_EQ(9, gSaveBlock1Ptr->frontierHardMode.towerWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN]);
    TEST_ASSERT_EQ(23, gSaveBlock2Ptr->frontier.towerWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN]);
}

static void TestTrainerPoolsAndRounds(void)
{
    u16 trainer;

    TestResetFixture(0x2001);
    trainer = GetRandomScaledFrontierTrainerId(0, 0);
    TEST_ASSERT(trainer >= FRONTIER_TRAINER_BRADY && trainer <= FRONTIER_TRAINER_JILL);
    trainer = GetRandomScaledFrontierTrainerId(3, 3);
    TEST_ASSERT(trainer >= FRONTIER_TRAINER_NORTON && trainer <= FRONTIER_TRAINER_JAZLYN);
    trainer = GetRandomScaledFrontierTrainerId(0, FRONTIER_STAGES_PER_CHALLENGE - 1);
    TEST_ASSERT(trainer >= FRONTIER_TRAINER_ERIK && trainer <= FRONTIER_TRAINER_CHLOE);
    trainer = GetRandomScaledFrontierTrainerId(8, 0);
    TEST_ASSERT(trainer >= FRONTIER_TRAINER_JAXON && trainer <= FRONTIER_TRAINER_GRETEL);

    TestResetFixture(0x2002);
    SetTowerContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    CallTower(BATTLE_TOWER_FUNC_INIT);
    CallTower(BATTLE_TOWER_FUNC_SET_OPPONENT);
    AssertTrainerInRange(FRONTIER_TRAINER_BRADY, FRONTIER_TRAINER_JILL);

    TestResetFixture(0x2003);
    SetTowerContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    CallTower(BATTLE_TOWER_FUNC_INIT);
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = FRONTIER_STAGES_PER_CHALLENGE - 1;
    CallTower(BATTLE_TOWER_FUNC_SET_OPPONENT);
    AssertTrainerInRange(FRONTIER_TRAINER_ERIK, FRONTIER_TRAINER_CHLOE);

    TestResetFixture(0x2004);
    SetTowerContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    CallTower(BATTLE_TOWER_FUNC_INIT);
    CallTower(BATTLE_TOWER_FUNC_SET_OPPONENT);
    AssertTrainerInRange(FRONTIER_TRAINER_JAXON, FRONTIER_TRAINER_GRETEL);
}

static void TestBrainBoundaries(void)
{
    TestResetFixture(0x3001);
    SetTowerContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 33;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_NOT_READY, GetFrontierBrainStatus());
    gSaveBlock2Ptr->frontier.towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 34;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_SILVER, GetFrontierBrainStatus());

    TestResetFixture(0x3002);
    SetTowerContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    gSaveBlock1Ptr->frontierHardMode.towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 19;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_NOT_READY, GetFrontierBrainStatus());
    gSaveBlock1Ptr->frontierHardMode.towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 20;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_SILVER, GetFrontierBrainStatus());

    VarSet(VAR_FRONTIER_BATTLE_MODE, FRONTIER_MODE_DOUBLES);
    TEST_ASSERT_EQ(FRONTIER_BRAIN_NOT_READY, GetFrontierBrainStatus());
}

static void TestProgressionAndModeIsolation(void)
{
    TestResetFixture(0x4001);
    SetTowerContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 2;
    gSaveBlock1Ptr->frontierHardMode.towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 6;
    gSaveBlock1Ptr->frontierHardMode.towerWinStreakActiveFlags = STREAK_TOWER_SINGLES_50;
    IncrementFrontierStreak();
    TEST_ASSERT_EQ(7, gSaveBlock1Ptr->frontierHardMode.towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50]);
    TEST_ASSERT_EQ(2, gSaveBlock2Ptr->frontier.towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50]);

    gSaveBlock2Ptr->frontier.curChallengeBattleNum = 6;
    CallTower(BATTLE_TOWER_FUNC_SET_BATTLE_WON);
    TEST_ASSERT_EQ(7, gSaveBlock2Ptr->frontier.curChallengeBattleNum);
    TEST_ASSERT_EQ(7, gSpecialVar_Result);
    TEST_ASSERT_EQ(2, gSaveBlock2Ptr->frontier.towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50]);
}

static void TestResultAndCleanupPaths(void)
{
    TestResetFixture(0x4501);
    SetTowerContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    gSaveBlock1Ptr->frontierHardMode.towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 9;
    gSaveBlock1Ptr->frontierHardMode.towerWinStreakActiveFlags = STREAK_TOWER_SINGLES_50;
    gSaveBlock2Ptr->frontier.winStreakActiveFlags = STREAK_TOWER_SINGLES_50;
    SetFrontierData(FRONTIER_DATA_CHALLENGE_STATUS, CHALLENGE_STATUS_LOST);
    SetTowerData(TOWER_DATA_WIN_STREAK_ACTIVE, FALSE);
    TEST_ASSERT_EQ(CHALLENGE_STATUS_LOST, gSaveBlock2Ptr->frontier.challengeStatus);
    TEST_ASSERT_EQ(9, gSaveBlock1Ptr->frontierHardMode.towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50]);
    TEST_ASSERT(!(gSaveBlock1Ptr->frontierHardMode.towerWinStreakActiveFlags & STREAK_TOWER_SINGLES_50));
    TEST_ASSERT(gSaveBlock2Ptr->frontier.winStreakActiveFlags & STREAK_TOWER_SINGLES_50);

    TestResetFixture(0x4502);
    SetTowerContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    gSaveBlock1Ptr->frontierHardMode.towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 11;
    gSaveBlock1Ptr->frontierHardMode.towerWinStreakActiveFlags = STREAK_TOWER_SINGLES_50;
    FlagSet(FLAG_CANCEL_BATTLE_ROOM_CHALLENGE);
    SetFrontierData(FRONTIER_DATA_CHALLENGE_STATUS, CHALLENGE_STATUS_LOST);
    SetTowerData(TOWER_DATA_WIN_STREAK_ACTIVE, FALSE);
    TEST_ASSERT(FlagGet(FLAG_CANCEL_BATTLE_ROOM_CHALLENGE));
    TEST_ASSERT_EQ(CHALLENGE_STATUS_LOST, gSaveBlock2Ptr->frontier.challengeStatus);
    TEST_ASSERT_EQ(11, gSaveBlock1Ptr->frontierHardMode.towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50]);
    TEST_ASSERT(!(gSaveBlock1Ptr->frontierHardMode.towerWinStreakActiveFlags & STREAK_TOWER_SINGLES_50));

    TestResetFixture(0x4503);
    SetTowerContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_DOUBLES, FRONTIER_LVL_OPEN);
    gSaveBlock1Ptr->frontierHardMode.towerWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN] = 13;
    gSaveBlock1Ptr->frontierHardMode.towerWinStreakActiveFlags = STREAK_TOWER_DOUBLES_OPEN;
    SetTowerData(TOWER_DATA_WIN_STREAK, 0);
    SetTowerData(TOWER_DATA_WIN_STREAK_ACTIVE, FALSE);
    SetFrontierData(FRONTIER_DATA_CHALLENGE_STATUS, 0);
    TEST_ASSERT_EQ(0, gSaveBlock1Ptr->frontierHardMode.towerWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN]);
    TEST_ASSERT(!(gSaveBlock1Ptr->frontierHardMode.towerWinStreakActiveFlags & STREAK_TOWER_DOUBLES_OPEN));
    TEST_ASSERT_EQ(0, gSaveBlock2Ptr->frontier.challengeStatus);

    TestResetFixture(0x4504);
    SetTowerContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_SINGLES, FRONTIER_LVL_OPEN);
    SetFrontierData(FRONTIER_DATA_CHALLENGE_STATUS, CHALLENGE_STATUS_WON);
    SetTowerData(TOWER_DATA_LVL_MODE, 0);
    TEST_ASSERT_EQ(CHALLENGE_STATUS_WON, gSaveBlock2Ptr->frontier.challengeStatus);
    TEST_ASSERT_EQ(FRONTIER_LVL_OPEN, gSaveBlock2Ptr->frontier.towerLvlMode);
}

static void TestPauseResumeState(void)
{
    TestResetFixture(0x4601);
    SetTowerContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    gSaveBlock1Ptr->frontierHardMode.towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 5;
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = 2;
    gSpecialVar_0x8005 = CHALLENGE_STATUS_PAUSED;
    CallTower(BATTLE_TOWER_FUNC_SAVE);
    TEST_ASSERT_EQ(CHALLENGE_STATUS_PAUSED, gSaveBlock2Ptr->frontier.challengeStatus);
    TEST_ASSERT(gSaveBlock2Ptr->frontier.challengePaused);
    TEST_ASSERT_EQ(0, VarGet(VAR_TEMP_CHALLENGE_STATUS));
    TEST_ASSERT_EQ(5, gSaveBlock1Ptr->frontierHardMode.towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50]);

    gSpecialVar_0x8005 = CHALLENGE_STATUS_SAVING;
    CallTower(BATTLE_TOWER_FUNC_SAVE);
    SetFrontierData(FRONTIER_DATA_PAUSED, FALSE);
    TEST_ASSERT_EQ(CHALLENGE_STATUS_SAVING, gSaveBlock2Ptr->frontier.challengeStatus);
    TEST_ASSERT(!gSaveBlock2Ptr->frontier.challengePaused);
    TEST_ASSERT_EQ(2, gSaveBlock2Ptr->frontier.curChallengeBattleNum);
    TEST_ASSERT_EQ(5, gSaveBlock1Ptr->frontierHardMode.towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50]);
}

static void TestPoolsLevelsAndBattleFlags(void)
{
    u32 i;

    TestResetFixture(0x5001);
    SetTowerContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    CallTower(BATTLE_TOWER_FUNC_INIT);
    CallTower(BATTLE_TOWER_FUNC_SET_OPPONENT);
    FillFrontierTrainerParty(FRONTIER_PARTY_SIZE);
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
    {
        TEST_ASSERT(GetMonData(&gEnemyParty[i], MON_DATA_SPECIES) != SPECIES_NONE);
        TEST_ASSERT_EQ(FRONTIER_MAX_LEVEL_50, GetMonData(&gEnemyParty[i], MON_DATA_LEVEL));
    }
    TEST_ASSERT_EQ(SPECIES_NONE, GetMonData(&gEnemyParty[FRONTIER_PARTY_SIZE], MON_DATA_SPECIES));

    TestResetFixture(0x5002);
    SetTowerContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_DOUBLES, FRONTIER_LVL_OPEN);
    CreateMon(&gPlayerParty[0], SPECIES_TREECKO, 75, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
    gPlayerPartyCount = 1;
    CallTower(BATTLE_TOWER_FUNC_INIT);
    CallTower(BATTLE_TOWER_FUNC_SET_OPPONENT);
    gSpecialVar_0x8004 = SPECIAL_BATTLE_TOWER;
    DoSpecialTrainerBattle();
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_TRAINER);
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_BATTLE_TOWER);
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_DOUBLE);
    for (i = 0; i < FRONTIER_DOUBLES_PARTY_SIZE; i++)
    {
        TEST_ASSERT(GetMonData(&gEnemyParty[i], MON_DATA_SPECIES) != SPECIES_NONE);
        TEST_ASSERT_EQ(75, GetMonData(&gEnemyParty[i], MON_DATA_LEVEL));
    }
}

void RunTest(void)
{
    TestNormalAndHardChallengeInit();
    TestTrainerPoolsAndRounds();
    TestBrainBoundaries();
    TestProgressionAndModeIsolation();
    TestResultAndCleanupPaths();
    TestPauseResumeState();
    TestPoolsLevelsAndBattleFlags();
}
