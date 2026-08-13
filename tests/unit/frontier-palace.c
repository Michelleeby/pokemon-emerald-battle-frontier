#include "global.h"
#include "battle.h"
#include "battle_controllers.h"
#include "battle_gfx_sfx_util.h"
#include "battle_script_commands.h"
#include "battle_setup.h"
#include "battle_tower.h"
#include "event_data.h"
#include "frontier_util.h"
#include "malloc.h"
#include "pokemon.h"
#include "random.h"
#include "test.h"
#include "battle_util2.h"
#include "util.h"
#include "constants/battle.h"
#include "constants/battle_frontier.h"
#include "constants/battle_frontier_trainers.h"
#include "constants/battle_palace.h"
#include "constants/battle_tower.h"
#include "constants/flags.h"
#include "constants/frontier_util.h"
#include "constants/moves.h"
#include "constants/pokemon.h"
#include "constants/vars.h"

void CallBattlePalaceFunction(void);

static void SetPalaceContext(u8 challengeMode, u8 battleMode, u8 lvlMode)
{
    VarSet(VAR_FRONTIER_FACILITY, FRONTIER_FACILITY_PALACE);
    VarSet(VAR_FRONTIER_BATTLE_MODE, battleMode);
    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_SET_DATA;
    gSpecialVar_0x8005 = FRONTIER_DATA_CHALLENGE_MODE;
    gSpecialVar_0x8006 = challengeMode;
    CallFrontierUtilFunc();
    gSpecialVar_0x8005 = FRONTIER_DATA_LVL_MODE;
    gSpecialVar_0x8006 = lvlMode;
    CallFrontierUtilFunc();
}

static void CallPalace(u16 function)
{
    gSpecialVar_0x8004 = function;
    CallBattlePalaceFunction();
}

static void SetPalaceData(u16 data, u16 value)
{
    gSpecialVar_0x8005 = data;
    gSpecialVar_0x8006 = value;
    CallPalace(BATTLE_PALACE_FUNC_SET_DATA);
}

static void SetFrontierData(u16 data, u16 value)
{
    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_SET_DATA;
    gSpecialVar_0x8005 = data;
    gSpecialVar_0x8006 = value;
    CallFrontierUtilFunc();
}

static void SetSharedOpponent(void)
{
    gSpecialVar_0x8004 = BATTLE_TOWER_FUNC_SET_OPPONENT;
    CallBattleTowerFunc();
}

static void AssertTrainerInRange(u16 minimum, u16 maximum)
{
    TEST_ASSERT(gTrainerBattleOpponent_A >= minimum);
    TEST_ASSERT(gTrainerBattleOpponent_A <= maximum);
}

static void TestNormalAndHardInit(void)
{
    TestResetFixture(0x8101);
    SetPalaceContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.palaceWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 12;
    gSaveBlock1Ptr->frontierHardMode.palaceWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 27;
    CallPalace(BATTLE_PALACE_FUNC_INIT);
    TEST_ASSERT_EQ(0, gSaveBlock2Ptr->frontier.palaceWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50]);
    TEST_ASSERT_EQ(27, gSaveBlock1Ptr->frontierHardMode.palaceWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50]);

    TestResetFixture(0x8102);
    SetPalaceContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_DOUBLES, FRONTIER_LVL_OPEN);
    gSaveBlock2Ptr->frontier.palaceWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN] = 9;
    gSaveBlock1Ptr->frontierHardMode.palaceWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN] = 18;
    gSaveBlock1Ptr->frontierHardMode.palaceWinStreakActiveFlags = STREAK_PALACE_DOUBLES_OPEN;
    CallPalace(BATTLE_PALACE_FUNC_INIT);
    TEST_ASSERT_EQ(18, gSaveBlock1Ptr->frontierHardMode.palaceWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN]);
    TEST_ASSERT_EQ(9, gSaveBlock2Ptr->frontier.palaceWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN]);
}

static void TestPalaceProgressionCleanupAndResume(void)
{
    TestResetFixture(0x8201);
    SetPalaceContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.palaceWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 2;
    gSaveBlock1Ptr->frontierHardMode.palaceWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 6;
    gSaveBlock1Ptr->frontierHardMode.palaceRecordWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 6;
    gSaveBlock1Ptr->frontierHardMode.palaceWinStreakActiveFlags = STREAK_PALACE_SINGLES_50;
    CallPalace(BATTLE_PALACE_FUNC_INCREMENT_STREAK);
    TEST_ASSERT_EQ(7, gSaveBlock1Ptr->frontierHardMode.palaceWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50]);
    TEST_ASSERT_EQ(7, gSaveBlock1Ptr->frontierHardMode.palaceRecordWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50]);
    TEST_ASSERT_EQ(2, gSaveBlock2Ptr->frontier.palaceWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50]);

    gSaveBlock2Ptr->frontier.curChallengeBattleNum = 3;
    gSpecialVar_0x8005 = CHALLENGE_STATUS_PAUSED;
    CallPalace(BATTLE_PALACE_FUNC_SAVE);
    TEST_ASSERT_EQ(CHALLENGE_STATUS_PAUSED, gSaveBlock2Ptr->frontier.challengeStatus);
    TEST_ASSERT(gSaveBlock2Ptr->frontier.challengePaused);
    TEST_ASSERT_EQ(3, gSaveBlock2Ptr->frontier.curChallengeBattleNum);

    SetPalaceData(PALACE_DATA_WIN_STREAK_ACTIVE, FALSE);
    SetPalaceData(PALACE_DATA_WIN_STREAK, 0);
    TEST_ASSERT(!(gSaveBlock1Ptr->frontierHardMode.palaceWinStreakActiveFlags & STREAK_PALACE_SINGLES_50));
    TEST_ASSERT_EQ(0, gSaveBlock1Ptr->frontierHardMode.palaceWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50]);

    TestResetFixture(0x8202);
    SetPalaceContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_DOUBLES, FRONTIER_LVL_OPEN);
    gSaveBlock2Ptr->frontier.palaceWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN] = 4;
    gSaveBlock2Ptr->frontier.winStreakActiveFlags = STREAK_PALACE_DOUBLES_OPEN;
    gSaveBlock1Ptr->frontierHardMode.palaceWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN] = 11;
    gSaveBlock1Ptr->frontierHardMode.palaceWinStreakActiveFlags = STREAK_PALACE_DOUBLES_OPEN;
    SetFrontierData(FRONTIER_DATA_CHALLENGE_STATUS, CHALLENGE_STATUS_LOST);
    SetPalaceData(PALACE_DATA_WIN_STREAK_ACTIVE, FALSE);
    TEST_ASSERT_EQ(CHALLENGE_STATUS_LOST, gSaveBlock2Ptr->frontier.challengeStatus);
    TEST_ASSERT_EQ(11, gSaveBlock1Ptr->frontierHardMode.palaceWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN]);
    TEST_ASSERT(!(gSaveBlock1Ptr->frontierHardMode.palaceWinStreakActiveFlags & STREAK_PALACE_DOUBLES_OPEN));
    TEST_ASSERT(gSaveBlock2Ptr->frontier.winStreakActiveFlags & STREAK_PALACE_DOUBLES_OPEN);

    TestResetFixture(0x8203);
    SetPalaceContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    gSaveBlock1Ptr->frontierHardMode.palaceWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 9;
    gSaveBlock1Ptr->frontierHardMode.palaceWinStreakActiveFlags = STREAK_PALACE_SINGLES_50;
    FlagSet(FLAG_CANCEL_BATTLE_ROOM_CHALLENGE);
    SetFrontierData(FRONTIER_DATA_CHALLENGE_STATUS, CHALLENGE_STATUS_LOST);
    SetPalaceData(PALACE_DATA_WIN_STREAK_ACTIVE, FALSE);
    TEST_ASSERT(FlagGet(FLAG_CANCEL_BATTLE_ROOM_CHALLENGE));
    TEST_ASSERT_EQ(CHALLENGE_STATUS_LOST, gSaveBlock2Ptr->frontier.challengeStatus);
    TEST_ASSERT_EQ(9, gSaveBlock1Ptr->frontierHardMode.palaceWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50]);
    TEST_ASSERT(!(gSaveBlock1Ptr->frontierHardMode.palaceWinStreakActiveFlags & STREAK_PALACE_SINGLES_50));
}

static void TestPoolsLevelsFlagsAndBrain(void)
{
    u32 i;

    TestResetFixture(0x8301);
    SetPalaceContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    CallPalace(BATTLE_PALACE_FUNC_INIT);
    SetSharedOpponent();
    AssertTrainerInRange(FRONTIER_TRAINER_BRADY, FRONTIER_TRAINER_JILL);
    gSpecialVar_0x8004 = SPECIAL_BATTLE_PALACE;
    DoSpecialTrainerBattle();
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_TRAINER);
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_PALACE);
    TEST_ASSERT(!(gBattleTypeFlags & BATTLE_TYPE_DOUBLE));
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
        TEST_ASSERT_EQ(FRONTIER_MAX_LEVEL_50, GetMonData(&gEnemyParty[i], MON_DATA_LEVEL));

    TestResetFixture(0x8302);
    SetPalaceContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.palaceWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 21;
    gSaveBlock2Ptr->frontier.winStreakActiveFlags = STREAK_PALACE_SINGLES_50;
    CallPalace(BATTLE_PALACE_FUNC_INIT);
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = 3;
    SetSharedOpponent();
    AssertTrainerInRange(FRONTIER_TRAINER_NORTON, FRONTIER_TRAINER_JAZLYN);
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = FRONTIER_STAGES_PER_CHALLENGE - 1;
    SetSharedOpponent();
    AssertTrainerInRange(FRONTIER_TRAINER_ZACHERY, FRONTIER_TRAINER_ALISON);

    TestResetFixture(0x8303);
    SetPalaceContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_DOUBLES, FRONTIER_LVL_OPEN);
    CreateMon(&gPlayerParty[0], SPECIES_TREECKO, 72, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
    gPlayerPartyCount = 1;
    CallPalace(BATTLE_PALACE_FUNC_INIT);
    SetSharedOpponent();
    AssertTrainerInRange(FRONTIER_TRAINER_JAXON, FRONTIER_TRAINER_GRETEL);
    gSpecialVar_0x8004 = SPECIAL_BATTLE_PALACE;
    DoSpecialTrainerBattle();
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_TRAINER);
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_PALACE);
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_DOUBLE);
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
        TEST_ASSERT_EQ(72, GetMonData(&gEnemyParty[i], MON_DATA_LEVEL));

    TestResetFixture(0x8304);
    SetPalaceContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.palaceWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 19;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_NOT_READY, GetFrontierBrainStatus());
    gSaveBlock2Ptr->frontier.palaceWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 20;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_SILVER, GetFrontierBrainStatus());

    TestResetFixture(0x8305);
    SetPalaceContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    gSaveBlock1Ptr->frontierHardMode.palaceWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 12;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_NOT_READY, GetFrontierBrainStatus());
    gSaveBlock1Ptr->frontierHardMode.palaceWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 13;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_SILVER, GetFrontierBrainStatus());
}

static void TestNatureBasedMoveSelectionState(void)
{
    struct ChooseMoveStruct *moveInfo;

    TestResetFixture(0x8401);
    TEST_ASSERT_EQ(61, gBattlePalaceNatureToMoveGroupLikelihood[NATURE_HARDY][0]);
    TEST_ASSERT_EQ(68, gBattlePalaceNatureToMoveGroupLikelihood[NATURE_HARDY][1]);
    TEST_ASSERT_EQ(20, gBattlePalaceNatureToMoveGroupLikelihood[NATURE_LONELY][0]);
    TEST_ASSERT_EQ(45, gBattlePalaceNatureToMoveGroupLikelihood[NATURE_LONELY][1]);
    gBattleStruct = AllocZeroed(sizeof(*gBattleStruct));
    TEST_ASSERT(gBattleStruct != NULL);
    AllocateBattleResources();
    TEST_ASSERT(gBattleResources != NULL);
    gActiveBattler = 0;
    gBattlersCount = 2;
    gBattleMons[0].personality = NATURE_LONELY;
    gBattleMons[0].hp = 100;
    gBattleMons[0].maxHP = 100;
    gBattleMons[0].moves[0] = MOVE_TACKLE;
    gBattleMons[0].moves[1] = MOVE_PROTECT;
    gBattleMons[0].moves[2] = MOVE_GROWL;
    gBattleMons[0].pp[0] = 35;
    gBattleMons[0].pp[1] = 10;
    gBattleMons[0].pp[2] = 40;
    moveInfo = (struct ChooseMoveStruct *)&gBattleBufferA[0][4];
    moveInfo->moves[0] = MOVE_TACKLE;
    moveInfo->moves[1] = MOVE_PROTECT;
    moveInfo->moves[2] = MOVE_GROWL;
    moveInfo->currentPP[0] = 35;
    moveInfo->currentPP[1] = 10;
    moveInfo->currentPP[2] = 40;

    SeedRng(1);
    ChooseMoveAndTargetInBattlePalace();
    TEST_ASSERT_EQ(gBitTable[1], gBattleStruct->palaceFlags >> MAX_BATTLERS_COUNT);

    gBattleStruct->palaceFlags = gBitTable[0];
    SeedRng(1);
    ChooseMoveAndTargetInBattlePalace();
    TEST_ASSERT_EQ(gBitTable[0], gBattleStruct->palaceFlags >> MAX_BATTLERS_COUNT);

    moveInfo->currentPP[0] = 0;
    gBattleMons[0].pp[0] = 0;
    gBattleStruct->palaceFlags = gBitTable[0];
    SeedRng(1);
    ChooseMoveAndTargetInBattlePalace();
    TEST_ASSERT((gBattleStruct->palaceFlags >> MAX_BATTLERS_COUNT) != gBitTable[0]);

    FreeBattleResources();
    Free(gBattleStruct);
    gBattleStruct = NULL;
}

void RunTest(void)
{
    TestNormalAndHardInit();
    TestPalaceProgressionCleanupAndResume();
    TestPoolsLevelsFlagsAndBrain();
    TestNatureBasedMoveSelectionState();
}
