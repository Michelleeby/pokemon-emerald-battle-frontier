#include "global.h"
#include "battle.h"
#include "battle_dome.h"
#include "battle_setup.h"
#include "battle_tower.h"
#include "event_data.h"
#include "frontier_util.h"
#include "pokemon.h"
#include "string_util.h"
#include "test.h"
#include "constants/battle_dome.h"
#include "constants/battle_frontier.h"
#include "constants/battle_frontier_trainers.h"
#include "constants/frontier_util.h"
#include "constants/moves.h"
#include "constants/pokemon.h"
#include "constants/trainers.h"
#include "constants/vars.h"

void CallBattleDomeFunction(void);

static void CallDome(u16 function)
{
    gSpecialVar_0x8004 = function;
    CallBattleDomeFunction();
}

static void SetDomeContext(u8 challengeMode, u8 battleMode, u8 lvlMode)
{
    VarSet(VAR_FRONTIER_FACILITY, FRONTIER_FACILITY_DOME);
    VarSet(VAR_FRONTIER_BATTLE_MODE, battleMode);
    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_SET_DATA;
    gSpecialVar_0x8005 = FRONTIER_DATA_CHALLENGE_MODE;
    gSpecialVar_0x8006 = challengeMode;
    CallFrontierUtilFunc();
    gSpecialVar_0x8005 = FRONTIER_DATA_LVL_MODE;
    gSpecialVar_0x8006 = lvlMode;
    CallFrontierUtilFunc();
}

static void SetFrontierData(u16 data, u16 value)
{
    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_SET_DATA;
    gSpecialVar_0x8005 = data;
    gSpecialVar_0x8006 = value;
    CallFrontierUtilFunc();
}

static void SetDomeData(u16 data, u16 value)
{
    gSpecialVar_0x8005 = data;
    gSpecialVar_0x8006 = value;
    CallDome(BATTLE_DOME_FUNC_SET_DATA);
}

static u16 GetDomeData(u16 data)
{
    gSpecialVar_0x8005 = data;
    CallDome(BATTLE_DOME_FUNC_GET_DATA);
    return gSpecialVar_Result;
}

static void CreateDomePlayerParty(u8 level)
{
    CreateMon(&gPlayerParty[0], SPECIES_TREECKO, level, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
    CreateMon(&gPlayerParty[1], SPECIES_TORCHIC, level, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
    CreateMon(&gPlayerParty[2], SPECIES_MUDKIP, level, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
    gPlayerPartyCount = 3;
    gSaveBlock2Ptr->frontier.selectedPartyMons[0] = 1;
    gSaveBlock2Ptr->frontier.selectedPartyMons[1] = 2;
    gSaveBlock2Ptr->frontier.selectedPartyMons[2] = 3;
}

static void InitTournament(u16 seed, u8 challengeMode, u8 battleMode, u8 lvlMode, u8 level)
{
    TestResetFixture(seed);
    SetDomeContext(challengeMode, battleMode, lvlMode);
    CreateDomePlayerParty(level);
    CallDome(BATTLE_DOME_FUNC_SET_TRAINERS);
    CallDome(BATTLE_DOME_FUNC_INIT);
    CallDome(BATTLE_DOME_FUNC_INIT_TRAINERS);
}

static void TestNormalAndHardChallengeInit(void)
{
    TestResetFixture(0x1101);
    SetDomeContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.domeWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 12;
    gSaveBlock2Ptr->frontier.domeHardWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 37;
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = DOME_FINAL;
    gSaveBlock2Ptr->frontier.challengePaused = TRUE;
    gSaveBlock2Ptr->frontier.disableRecordBattle = TRUE;
    CallDome(BATTLE_DOME_FUNC_INIT);
    TEST_ASSERT_EQ(0, gSaveBlock2Ptr->frontier.curChallengeBattleNum);
    TEST_ASSERT(!gSaveBlock2Ptr->frontier.challengePaused);
    TEST_ASSERT(!gSaveBlock2Ptr->frontier.disableRecordBattle);
    TEST_ASSERT_EQ(0, gSaveBlock2Ptr->frontier.domeWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50]);
    TEST_ASSERT_EQ(37, gSaveBlock2Ptr->frontier.domeHardWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50]);

    TestResetFixture(0x1102);
    SetDomeContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_DOUBLES, FRONTIER_LVL_OPEN);
    gSaveBlock2Ptr->frontier.domeWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN] = 23;
    gSaveBlock2Ptr->frontier.domeHardWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN] = 9;
    gSaveBlock2Ptr->frontier.winStreakActiveFlags = STREAK_DOME_HARD_DOUBLES_OPEN;
    CallDome(BATTLE_DOME_FUNC_INIT);
    TEST_ASSERT_EQ(9, gSaveBlock2Ptr->frontier.domeHardWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN]);
    TEST_ASSERT_EQ(23, gSaveBlock2Ptr->frontier.domeWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN]);
    TEST_ASSERT_EQ(FRONTIER_CHALLENGE_HARD, GetDomeData(DOME_DATA_CHALLENGE_MODE));
}

static void TestModeSpecificDataAndProgression(void)
{
    TestResetFixture(0x2201);
    SetDomeContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_DOUBLES, FRONTIER_LVL_OPEN);
    gSaveBlock2Ptr->frontier.domeWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN] = 4;
    gSaveBlock2Ptr->frontier.domeHardWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN] = 8;
    gSaveBlock2Ptr->frontier.domeHardRecordWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN] = 6;
    gSaveBlock2Ptr->frontier.domeHardTotalChampionships[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN] = 2;
    CallDome(BATTLE_DOME_FUNC_INCREMENT_STREAK);
    TEST_ASSERT_EQ(9, gSaveBlock2Ptr->frontier.domeHardWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN]);
    TEST_ASSERT_EQ(9, gSaveBlock2Ptr->frontier.domeHardRecordWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN]);
    TEST_ASSERT_EQ(3, gSaveBlock2Ptr->frontier.domeHardTotalChampionships[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN]);
    TEST_ASSERT_EQ(4, gSaveBlock2Ptr->frontier.domeWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN]);

    SetDomeData(DOME_DATA_WIN_STREAK_ACTIVE, TRUE);
    TEST_ASSERT(GetDomeData(DOME_DATA_WIN_STREAK_ACTIVE));
    TEST_ASSERT(gSaveBlock2Ptr->frontier.winStreakActiveFlags & STREAK_DOME_HARD_DOUBLES_OPEN);
    SetDomeData(DOME_DATA_ATTEMPTED_CHALLENGE, TRUE);
    SetDomeData(DOME_DATA_HAS_WON_CHALLENGE, TRUE);
    TEST_ASSERT(gSaveBlock2Ptr->frontier.domeAttemptedDoublesOpen);
    TEST_ASSERT(gSaveBlock2Ptr->frontier.domeHasWonDoublesOpen);
    TEST_ASSERT(!gSaveBlock2Ptr->frontier.domeAttemptedSinglesOpen);
    TEST_ASSERT(!gSaveBlock2Ptr->frontier.domeHasWonSinglesOpen);
    SetDomeData(DOME_DATA_WIN_STREAK_ACTIVE, FALSE);
    TEST_ASSERT(!(gSaveBlock2Ptr->frontier.winStreakActiveFlags & STREAK_DOME_HARD_DOUBLES_OPEN));
}

static void TestTournamentGenerationAndPools(void)
{
    u32 i, j;
    bool8 foundPlayer = FALSE;

    InitTournament(0x3301, FRONTIER_CHALLENGE_NORMAL, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50, 50);
    TEST_ASSERT_EQ(FRONTIER_LVL_50 + 1, gSaveBlock2Ptr->frontier.domeLvlMode);
    TEST_ASSERT_EQ(FRONTIER_MODE_SINGLES + 1, gSaveBlock2Ptr->frontier.domeBattleMode);
    for (i = 0; i < DOME_TOURNAMENT_TRAINERS_COUNT; i++)
    {
        if (gSaveBlock2Ptr->frontier.domeTrainers[i].trainerId == TRAINER_PLAYER)
        {
            foundPlayer = TRUE;
            TEST_ASSERT_EQ(SPECIES_TREECKO, gSaveBlock2Ptr->frontier.domeMonIds[i][0]);
        }
        else
        {
            TEST_ASSERT(gSaveBlock2Ptr->frontier.domeTrainers[i].trainerId < FRONTIER_TRAINERS_COUNT);
            for (j = 0; j < FRONTIER_PARTY_SIZE; j++)
                TEST_ASSERT(gSaveBlock2Ptr->frontier.domeMonIds[i][j] != 0);
        }
        for (j = i + 1; j < DOME_TOURNAMENT_TRAINERS_COUNT; j++)
            TEST_ASSERT(gSaveBlock2Ptr->frontier.domeTrainers[i].trainerId != gSaveBlock2Ptr->frontier.domeTrainers[j].trainerId);
    }
    TEST_ASSERT(foundPlayer);

    InitTournament(0x3302, FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_SINGLES, FRONTIER_LVL_OPEN, 75);
    for (i = 0; i < DOME_TOURNAMENT_TRAINERS_COUNT; i++)
    {
        u16 trainerId = gSaveBlock2Ptr->frontier.domeTrainers[i].trainerId;
        if (trainerId != TRAINER_PLAYER && trainerId != TRAINER_FRONTIER_BRAIN)
        {
            TEST_ASSERT(trainerId >= FRONTIER_TRAINER_JAXON);
            TEST_ASSERT(trainerId <= FRONTIER_TRAINER_GRETEL);
        }
    }
}

static void TestOpponentPreviewLevelsAndFlags(void)
{
    u32 i;

    InitTournament(0x4401, FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_DOUBLES, FRONTIER_LVL_OPEN, 75);
    CallDome(BATTLE_DOME_FUNC_SET_OPPONENT_ID);
    TEST_ASSERT(gTrainerBattleOpponent_A < FRONTIER_TRAINERS_COUNT);
    CallDome(BATTLE_DOME_FUNC_GET_OPPONENT_NAME);
    TEST_ASSERT(gStringVar2[0] != 0xFF);
    CallDome(BATTLE_DOME_FUNC_INIT_OPPONENT_PARTY);
    for (i = 0; i < DOME_BATTLE_PARTY_SIZE; i++)
    {
        TEST_ASSERT(GetMonData(&gEnemyParty[i], MON_DATA_SPECIES) != SPECIES_NONE);
        TEST_ASSERT_EQ(75, GetMonData(&gEnemyParty[i], MON_DATA_LEVEL));
    }
    TEST_ASSERT_EQ(SPECIES_NONE, GetMonData(&gEnemyParty[DOME_BATTLE_PARTY_SIZE], MON_DATA_SPECIES));

    gSpecialVar_0x8004 = SPECIAL_BATTLE_DOME;
    DoSpecialTrainerBattle();
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_TRAINER);
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_DOME);
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_DOUBLE);
}

static void TestBrainBoundaries(void)
{
    TestResetFixture(0x4451);
    SetDomeContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.domeWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 3;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_NOT_READY, GetFrontierBrainStatus());
    gSaveBlock2Ptr->frontier.domeWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 4;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_SILVER, GetFrontierBrainStatus());

    TestResetFixture(0x4452);
    SetDomeContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.domeHardWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 1;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_NOT_READY, GetFrontierBrainStatus());
    gSaveBlock2Ptr->frontier.domeHardWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 2;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_SILVER, GetFrontierBrainStatus());
}

static void TestBracketAdvancementAndFinalResult(void)
{
    u32 round;
    int playerId;

    InitTournament(0x5501, FRONTIER_CHALLENGE_NORMAL, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50, 50);
    playerId = TrainerIdToDomeTournamentId(TRAINER_PLAYER);
    TEST_ASSERT(playerId < DOME_TOURNAMENT_TRAINERS_COUNT);
    for (round = DOME_ROUND1; round <= DOME_FINAL; round++)
    {
        int opponentId;
        gSaveBlock2Ptr->frontier.curChallengeBattleNum = round;
        CallDome(BATTLE_DOME_FUNC_SET_OPPONENT_ID);
        opponentId = TrainerIdToDomeTournamentId(gTrainerBattleOpponent_A);
        TEST_ASSERT(opponentId < DOME_TOURNAMENT_TRAINERS_COUNT);
        TEST_ASSERT(!gSaveBlock2Ptr->frontier.domeTrainers[opponentId].isEliminated);
        gBattleResults.lastUsedMovePlayer = MOVE_TACKLE;
        gSpecialVar_0x8005 = DOME_PLAYER_WON_MATCH;
        CallDome(BATTLE_DOME_FUNC_RESOLVE_WINNERS);
        TEST_ASSERT(gSaveBlock2Ptr->frontier.domeTrainers[opponentId].isEliminated);
        TEST_ASSERT_EQ(round, gSaveBlock2Ptr->frontier.domeTrainers[opponentId].eliminatedAt);
        TEST_ASSERT_EQ(MOVE_TACKLE, gSaveBlock2Ptr->frontier.domeWinningMoves[opponentId]);
        TEST_ASSERT(!gSaveBlock2Ptr->frontier.domeTrainers[playerId].isEliminated);
    }
}

static void TestLossRetirementAndResumeState(void)
{
    int playerId;

    InitTournament(0x6601, FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50, 50);
    playerId = TrainerIdToDomeTournamentId(TRAINER_PLAYER);
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = DOME_SEMIFINAL;
    CallDome(BATTLE_DOME_FUNC_SET_OPPONENT_ID);
    gBattleResults.lastUsedMoveOpponent = MOVE_EMBER;
    gBattleOutcome = B_OUTCOME_FORFEITED;
    gSpecialVar_0x8005 = DOME_PLAYER_RETIRED;
    CallDome(BATTLE_DOME_FUNC_RESOLVE_WINNERS);
    TEST_ASSERT(gSaveBlock2Ptr->frontier.domeTrainers[playerId].isEliminated);
    TEST_ASSERT_EQ(DOME_SEMIFINAL, gSaveBlock2Ptr->frontier.domeTrainers[playerId].eliminatedAt);
    TEST_ASSERT(gSaveBlock2Ptr->frontier.domeTrainers[playerId].forfeited);
    TEST_ASSERT_EQ(MOVE_EMBER, gSaveBlock2Ptr->frontier.domeWinningMoves[playerId]);

    SetDomeData(DOME_DATA_WIN_STREAK_ACTIVE, TRUE);
    SetFrontierData(FRONTIER_DATA_CHALLENGE_STATUS, CHALLENGE_STATUS_LOST);
    SetDomeData(DOME_DATA_WIN_STREAK_ACTIVE, FALSE);
    TEST_ASSERT_EQ(CHALLENGE_STATUS_LOST, gSaveBlock2Ptr->frontier.challengeStatus);
    TEST_ASSERT(!GetDomeData(DOME_DATA_WIN_STREAK_ACTIVE));

    gSpecialVar_0x8005 = CHALLENGE_STATUS_PAUSED;
    CallDome(BATTLE_DOME_FUNC_SAVE);
    gSaveBlock2Ptr->frontier.disableRecordBattle = TRUE;
    TEST_ASSERT_EQ(FRONTIER_CHALLENGE_HARD, GetDomeData(DOME_DATA_CHALLENGE_MODE));
    TEST_ASSERT_EQ(DOME_SEMIFINAL, gSaveBlock2Ptr->frontier.curChallengeBattleNum);
    TEST_ASSERT_EQ(CHALLENGE_STATUS_PAUSED, gSaveBlock2Ptr->frontier.challengeStatus);
    TEST_ASSERT(gSaveBlock2Ptr->frontier.challengePaused);
    TEST_ASSERT_EQ(0, VarGet(VAR_TEMP_CHALLENGE_STATUS));
    TEST_ASSERT(gSaveBlock2Ptr->frontier.disableRecordBattle);
}

void RunTest(void)
{
    TestLog("dome init");
    TestNormalAndHardChallengeInit();
    TestLog("dome progression");
    TestModeSpecificDataAndProgression();
    TestLog("dome generation");
    TestTournamentGenerationAndPools();
    TestLog("dome opponent");
    TestOpponentPreviewLevelsAndFlags();
    TestBrainBoundaries();
    TestLog("dome advancement");
    TestBracketAdvancementAndFinalResult();
    TestLog("dome retirement");
    TestLossRetirementAndResumeState();
}
