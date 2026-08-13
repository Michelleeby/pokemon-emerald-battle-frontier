#include "global.h"
#include "battle.h"
#include "battle_arena.h"
#include "battle_setup.h"
#include "battle_tower.h"
#include "battle_util.h"
#include "event_data.h"
#include "frontier_util.h"
#include "malloc.h"
#include "pokemon.h"
#include "test.h"
#include "constants/battle.h"
#include "constants/battle_arena.h"
#include "constants/battle_frontier.h"
#include "constants/battle_frontier_trainers.h"
#include "constants/frontier_util.h"
#include "constants/flags.h"
#include "constants/moves.h"
#include "constants/pokemon.h"
#include "constants/vars.h"

static void SetArenaContext(u8 challengeMode, u8 lvlMode)
{
    VarSet(VAR_FRONTIER_FACILITY, FRONTIER_FACILITY_ARENA);
    VarSet(VAR_FRONTIER_BATTLE_MODE, FRONTIER_MODE_SINGLES);
    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_SET_DATA;
    gSpecialVar_0x8005 = FRONTIER_DATA_CHALLENGE_MODE;
    gSpecialVar_0x8006 = challengeMode;
    CallFrontierUtilFunc();
    gSpecialVar_0x8005 = FRONTIER_DATA_LVL_MODE;
    gSpecialVar_0x8006 = lvlMode;
    CallFrontierUtilFunc();
}

static void CallArena(u16 function)
{
    gSpecialVar_0x8004 = function;
    CallBattleArenaFunction();
}

static void SetArenaData(u16 data, u16 value)
{
    gSpecialVar_0x8005 = data;
    gSpecialVar_0x8006 = value;
    CallArena(BATTLE_ARENA_FUNC_SET_DATA);
}

static void IncrementStreak(void)
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

static void AssertTrainerInRange(u16 trainer, u16 minimum, u16 maximum)
{
    TEST_ASSERT(trainer >= minimum);
    TEST_ASSERT(trainer <= maximum);
}

static void TestNormalAndHardInit(void)
{
    TestResetFixture(0x7101);
    SetArenaContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.arenaWinStreaks[FRONTIER_LVL_50] = 12;
    gSaveBlock1Ptr->frontierHardMode.arenaWinStreaks[FRONTIER_LVL_50] = 31;
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = 5;
    gSaveBlock2Ptr->frontier.challengePaused = TRUE;
    CallArena(BATTLE_ARENA_FUNC_INIT);
    TEST_ASSERT_EQ(0, gSaveBlock2Ptr->frontier.arenaWinStreaks[FRONTIER_LVL_50]);
    TEST_ASSERT_EQ(31, gSaveBlock1Ptr->frontierHardMode.arenaWinStreaks[FRONTIER_LVL_50]);
    TEST_ASSERT_EQ(0, gSaveBlock2Ptr->frontier.curChallengeBattleNum);
    TEST_ASSERT(!gSaveBlock2Ptr->frontier.challengePaused);

    TestResetFixture(0x7102);
    SetArenaContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_OPEN);
    gSaveBlock2Ptr->frontier.arenaWinStreaks[FRONTIER_LVL_OPEN] = 8;
    gSaveBlock1Ptr->frontierHardMode.arenaWinStreaks[FRONTIER_LVL_OPEN] = 19;
    gSaveBlock1Ptr->frontierHardMode.arenaWinStreakActiveFlags = STREAK_ARENA_OPEN;
    CallArena(BATTLE_ARENA_FUNC_INIT);
    TEST_ASSERT_EQ(19, gSaveBlock1Ptr->frontierHardMode.arenaWinStreaks[FRONTIER_LVL_OPEN]);
    TEST_ASSERT_EQ(8, gSaveBlock2Ptr->frontier.arenaWinStreaks[FRONTIER_LVL_OPEN]);
}

static void TestProgressionCleanupAndResume(void)
{
    TestResetFixture(0x7201);
    SetArenaContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.arenaWinStreaks[FRONTIER_LVL_50] = 3;
    gSaveBlock1Ptr->frontierHardMode.arenaWinStreaks[FRONTIER_LVL_50] = 6;
    gSaveBlock1Ptr->frontierHardMode.arenaWinStreakActiveFlags = STREAK_ARENA_50;
    IncrementStreak();
    TEST_ASSERT_EQ(7, gSaveBlock1Ptr->frontierHardMode.arenaWinStreaks[FRONTIER_LVL_50]);
    TEST_ASSERT_EQ(3, gSaveBlock2Ptr->frontier.arenaWinStreaks[FRONTIER_LVL_50]);
    SetArenaData(ARENA_DATA_WIN_STREAK_ACTIVE, FALSE);
    TEST_ASSERT(!(gSaveBlock1Ptr->frontierHardMode.arenaWinStreakActiveFlags & STREAK_ARENA_50));

    gSaveBlock2Ptr->frontier.curChallengeBattleNum = 4;
    gSpecialVar_0x8005 = CHALLENGE_STATUS_PAUSED;
    CallArena(BATTLE_ARENA_FUNC_SAVE);
    TEST_ASSERT_EQ(CHALLENGE_STATUS_PAUSED, gSaveBlock2Ptr->frontier.challengeStatus);
    TEST_ASSERT(gSaveBlock2Ptr->frontier.challengePaused);
    TEST_ASSERT_EQ(4, gSaveBlock2Ptr->frontier.curChallengeBattleNum);
    TEST_ASSERT_EQ(7, gSaveBlock1Ptr->frontierHardMode.arenaWinStreaks[FRONTIER_LVL_50]);

    SetArenaData(ARENA_DATA_WIN_STREAK, 0);
    TEST_ASSERT_EQ(0, gSaveBlock1Ptr->frontierHardMode.arenaWinStreaks[FRONTIER_LVL_50]);

    TestResetFixture(0x7202);
    SetArenaContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_OPEN);
    gSaveBlock2Ptr->frontier.arenaWinStreaks[FRONTIER_LVL_OPEN] = 4;
    gSaveBlock2Ptr->frontier.winStreakActiveFlags = STREAK_ARENA_OPEN;
    gSaveBlock1Ptr->frontierHardMode.arenaWinStreaks[FRONTIER_LVL_OPEN] = 11;
    gSaveBlock1Ptr->frontierHardMode.arenaWinStreakActiveFlags = STREAK_ARENA_OPEN;
    SetFrontierData(FRONTIER_DATA_CHALLENGE_STATUS, CHALLENGE_STATUS_LOST);
    SetArenaData(ARENA_DATA_WIN_STREAK_ACTIVE, FALSE);
    TEST_ASSERT_EQ(CHALLENGE_STATUS_LOST, gSaveBlock2Ptr->frontier.challengeStatus);
    TEST_ASSERT_EQ(11, gSaveBlock1Ptr->frontierHardMode.arenaWinStreaks[FRONTIER_LVL_OPEN]);
    TEST_ASSERT(!(gSaveBlock1Ptr->frontierHardMode.arenaWinStreakActiveFlags & STREAK_ARENA_OPEN));
    TEST_ASSERT(gSaveBlock2Ptr->frontier.winStreakActiveFlags & STREAK_ARENA_OPEN);

    TestResetFixture(0x7203);
    SetArenaContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_50);
    gSaveBlock1Ptr->frontierHardMode.arenaWinStreaks[FRONTIER_LVL_50] = 9;
    gSaveBlock1Ptr->frontierHardMode.arenaWinStreakActiveFlags = STREAK_ARENA_50;
    FlagSet(FLAG_CANCEL_BATTLE_ROOM_CHALLENGE);
    SetFrontierData(FRONTIER_DATA_CHALLENGE_STATUS, CHALLENGE_STATUS_LOST);
    SetArenaData(ARENA_DATA_WIN_STREAK_ACTIVE, FALSE);
    TEST_ASSERT(FlagGet(FLAG_CANCEL_BATTLE_ROOM_CHALLENGE));
    TEST_ASSERT_EQ(CHALLENGE_STATUS_LOST, gSaveBlock2Ptr->frontier.challengeStatus);
    TEST_ASSERT_EQ(9, gSaveBlock1Ptr->frontierHardMode.arenaWinStreaks[FRONTIER_LVL_50]);
    TEST_ASSERT(!(gSaveBlock1Ptr->frontierHardMode.arenaWinStreakActiveFlags & STREAK_ARENA_50));
}

static void TestPoolsLevelsFlagsAndBrain(void)
{
    u32 i;
    u16 trainer;

    TestResetFixture(0x7301);
    trainer = GetRandomScaledFrontierTrainerId(0, 0);
    AssertTrainerInRange(trainer, FRONTIER_TRAINER_BRADY, FRONTIER_TRAINER_JILL);
    trainer = GetRandomScaledFrontierTrainerId(3, 3);
    AssertTrainerInRange(trainer, FRONTIER_TRAINER_NORTON, FRONTIER_TRAINER_JAZLYN);
    trainer = GetRandomScaledFrontierTrainerId(0, FRONTIER_STAGES_PER_CHALLENGE - 1);
    AssertTrainerInRange(trainer, FRONTIER_TRAINER_ERIK, FRONTIER_TRAINER_CHLOE);
    trainer = GetRandomScaledFrontierTrainerId(8, 0);
    AssertTrainerInRange(trainer, FRONTIER_TRAINER_JAXON, FRONTIER_TRAINER_GRETEL);

    SetArenaContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.arenaWinStreaks[FRONTIER_LVL_50] = 26;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_NOT_READY, GetFrontierBrainStatus());
    gSaveBlock2Ptr->frontier.arenaWinStreaks[FRONTIER_LVL_50] = 27;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_SILVER, GetFrontierBrainStatus());

    TestResetFixture(0x7302);
    SetArenaContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_50);
    CallArena(BATTLE_ARENA_FUNC_INIT);
    gTrainerBattleOpponent_A = GetRandomScaledFrontierTrainerId(8, 0);
    TEST_ASSERT(gTrainerBattleOpponent_A >= FRONTIER_TRAINER_JAXON);
    TEST_ASSERT(gTrainerBattleOpponent_A <= FRONTIER_TRAINER_GRETEL);
    gSpecialVar_0x8004 = SPECIAL_BATTLE_ARENA;
    DoSpecialTrainerBattle();
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_TRAINER);
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_ARENA);
    TEST_ASSERT(!(gBattleTypeFlags & BATTLE_TYPE_DOUBLE));
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
        TEST_ASSERT_EQ(FRONTIER_MAX_LEVEL_50, GetMonData(&gEnemyParty[i], MON_DATA_LEVEL));

    gSaveBlock1Ptr->frontierHardMode.arenaWinStreaks[FRONTIER_LVL_50] = 12;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_NOT_READY, GetFrontierBrainStatus());
    gSaveBlock1Ptr->frontierHardMode.arenaWinStreaks[FRONTIER_LVL_50] = 13;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_SILVER, GetFrontierBrainStatus());

    TestResetFixture(0x7303);
    SetArenaContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_OPEN);
    CreateMon(&gPlayerParty[0], SPECIES_TREECKO, 75, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
    gPlayerPartyCount = 1;
    CallArena(BATTLE_ARENA_FUNC_INIT);
    gTrainerBattleOpponent_A = GetRandomScaledFrontierTrainerId(8, 0);
    gSpecialVar_0x8004 = SPECIAL_BATTLE_ARENA;
    DoSpecialTrainerBattle();
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
        TEST_ASSERT_EQ(75, GetMonData(&gEnemyParty[i], MON_DATA_LEVEL));
}

static void TestJudgingCategories(void)
{
    TestResetFixture(0x7401);
    gBattleStruct = AllocZeroed(sizeof(*gBattleStruct));
    TEST_ASSERT(gBattleStruct != NULL);
    gBattleMons[0].hp = 100;
    gBattleMons[1].hp = 80;
    BattleArena_InitPoints();
    TEST_ASSERT_EQ(100, gBattleStruct->arenaStartHp[0]);
    TEST_ASSERT_EQ(80, gBattleStruct->arenaStartHp[1]);

    gCurrentMove = MOVE_TACKLE;
    BattleArena_AddMindPoints(0);
    gCurrentMove = MOVE_PROTECT;
    BattleArena_AddMindPoints(1);
    TEST_ASSERT_EQ(1, gBattleStruct->arenaMindPoints[0]);
    TEST_ASSERT_EQ(-1, gBattleStruct->arenaMindPoints[1]);

    gHitMarker = HITMARKER_OBEYS;
    gMoveResultFlags = MOVE_RESULT_SUPER_EFFECTIVE;
    BattleArena_AddSkillPoints(0);
    gMoveResultFlags = MOVE_RESULT_NOT_VERY_EFFECTIVE;
    BattleArena_AddSkillPoints(1);
    TEST_ASSERT_EQ(2, gBattleStruct->arenaSkillPoints[0]);
    TEST_ASSERT_EQ(-1, gBattleStruct->arenaSkillPoints[1]);
    Free(gBattleStruct);
    gBattleStruct = NULL;
}

static void TestThreeTurnBoundaryTiesAndForcedResult(void)
{
    u8 state = 7;

    TestResetFixture(0x7501);
    gBattleTextBuff1[0] = 4;
    gBattleTextBuff2[0] = 4;
    TEST_ASSERT_EQ(ARENA_RESULT_TIE, BattleArena_ShowJudgmentWindow(&state));
    TEST_ASSERT_EQ(8, state);

    state = 7;
    gBattleTextBuff1[0] = 5;
    gBattleTextBuff2[0] = 3;
    TEST_ASSERT_EQ(ARENA_RESULT_PLAYER_WON, BattleArena_ShowJudgmentWindow(&state));
    TEST_ASSERT_EQ(0, gBattleScripting.battler);

    state = 7;
    gBattleTextBuff1[0] = 2;
    gBattleTextBuff2[0] = 6;
    TEST_ASSERT_EQ(ARENA_RESULT_PLAYER_LOST, BattleArena_ShowJudgmentWindow(&state));
    TEST_ASSERT_EQ(1, gBattleScripting.battler);

    TestResetFixture(0x7502);
    gBattleStruct = AllocZeroed(sizeof(*gBattleStruct));
    TEST_ASSERT(gBattleStruct != NULL);
    gBattleTypeFlags = BATTLE_TYPE_ARENA;
    gBattleMons[0].hp = 10;
    gBattleMons[1].hp = 10;
    gBattleStruct->wishPerishSongState = 2;
    gBattleStruct->arenaTurnCounter = 1;
    TEST_ASSERT(!HandleWishPerishSongOnTurnEnd());
    gBattleStruct->wishPerishSongState = 2;
    gBattleStruct->arenaTurnCounter = 2;
    TEST_ASSERT(HandleWishPerishSongOnTurnEnd());
    Free(gBattleStruct);
    gBattleStruct = NULL;
}

void RunTest(void)
{
    TestNormalAndHardInit();
    TestProgressionCleanupAndResume();
    TestPoolsLevelsFlagsAndBrain();
    TestJudgingCategories();
    TestThreeTurnBoundaryTiesAndForcedResult();
}
