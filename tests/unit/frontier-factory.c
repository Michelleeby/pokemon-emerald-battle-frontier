#include "global.h"
#include "battle.h"
#include "battle_factory.h"
#include "battle_setup.h"
#include "battle_tower.h"
#include "event_data.h"
#include "frontier_util.h"
#include "pokemon.h"
#include "test.h"
#include "constants/battle_ai.h"
#include "constants/battle_factory.h"
#include "constants/battle_frontier.h"
#include "constants/battle_frontier_mons.h"
#include "constants/battle_frontier_trainers.h"
#include "constants/frontier_util.h"
#include "constants/moves.h"
#include "constants/pokemon.h"
#include "constants/vars.h"

static void CallFactory(u16 function)
{
    gSpecialVar_0x8004 = function;
    CallBattleFactoryFunction();
}

static void SetFrontierData(u16 data, u16 value)
{
    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_SET_DATA;
    gSpecialVar_0x8005 = data;
    gSpecialVar_0x8006 = value;
    CallFrontierUtilFunc();
}

static void SetFactoryContext(u8 challengeMode, u8 battleMode, u8 lvlMode)
{
    VarSet(VAR_FRONTIER_FACILITY, FRONTIER_FACILITY_FACTORY);
    VarSet(VAR_FRONTIER_BATTLE_MODE, battleMode);
    SetFrontierData(FRONTIER_DATA_CHALLENGE_MODE, challengeMode);
    SetFrontierData(FRONTIER_DATA_LVL_MODE, lvlMode);
}

static void SetFactoryData(u16 data, u16 value)
{
    gSpecialVar_0x8005 = data;
    gSpecialVar_0x8006 = value;
    CallFactory(BATTLE_FACTORY_FUNC_SET_DATA);
}

static u16 GetFactoryData(u16 data)
{
    gSpecialVar_0x8005 = data;
    CallFactory(BATTLE_FACTORY_FUNC_GET_DATA);
    return gSpecialVar_Result;
}

static void AssertRentalDraft(u16 minimum, u16 maximum)
{
    u32 i;
    u32 j;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        u16 monId = gSaveBlock2Ptr->frontier.rentalMons[i].monId;
        TEST_ASSERT(monId >= minimum);
        TEST_ASSERT(monId <= maximum);
        for (j = 0; j < i; j++)
            TEST_ASSERT(monId != gSaveBlock2Ptr->frontier.rentalMons[j].monId);
    }
}

static void TestNormalAndHardChallengeEntry(void)
{
    u32 i;

    TestResetFixture(0x6101);
    SetFactoryContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.factoryWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 12;
    gSaveBlock1Ptr->frontierHardMode.factoryWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 35;
    CallFactory(BATTLE_FACTORY_FUNC_INIT);
    TEST_ASSERT_EQ(0, gSaveBlock2Ptr->frontier.factoryWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50]);
    TEST_ASSERT_EQ(35, gSaveBlock1Ptr->frontierHardMode.factoryWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50]);
    TEST_ASSERT_EQ(0, gSaveBlock2Ptr->frontier.curChallengeBattleNum);
    TEST_ASSERT(!gSaveBlock2Ptr->frontier.challengePaused);
    for (i = 0; i < PARTY_SIZE; i++)
        TEST_ASSERT_EQ(0xFFFF, gSaveBlock2Ptr->frontier.rentalMons[i].monId);

    TestResetFixture(0x6102);
    SetFactoryContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_DOUBLES, FRONTIER_LVL_OPEN);
    gSaveBlock2Ptr->frontier.factoryWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN] = 18;
    gSaveBlock1Ptr->frontierHardMode.factoryWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN] = 7;
    gSaveBlock1Ptr->frontierHardMode.factoryWinStreakActiveFlags = STREAK_FACTORY_DOUBLES_OPEN;
    CallFactory(BATTLE_FACTORY_FUNC_INIT);
    TEST_ASSERT_EQ(7, gSaveBlock1Ptr->frontierHardMode.factoryWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN]);
    TEST_ASSERT_EQ(18, gSaveBlock2Ptr->frontier.factoryWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN]);
}

static void TestRentalAndOpponentPools(void)
{
    u32 i;
    u32 j;

    TestResetFixture(0x6201);
    SetFactoryContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    CallFactory(BATTLE_FACTORY_FUNC_INIT);
    CallFactory(BATTLE_FACTORY_FUNC_GENERATE_RENTAL_MONS);
    AssertRentalDraft(FRONTIER_MON_GRIMER, FRONTIER_MON_FURRET_1);
    CallFactory(BATTLE_FACTORY_FUNC_GENERATE_OPPONENT_MONS);
    TEST_ASSERT(gTrainerBattleOpponent_A >= FRONTIER_TRAINER_BRADY);
    TEST_ASSERT(gTrainerBattleOpponent_A <= FRONTIER_TRAINER_JILL);

    TestResetFixture(0x6204);
    SetFactoryContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    CallFactory(BATTLE_FACTORY_FUNC_INIT);
    CallFactory(BATTLE_FACTORY_FUNC_GENERATE_RENTAL_MONS);
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = FRONTIER_STAGES_PER_CHALLENGE - 1;
    CallFactory(BATTLE_FACTORY_FUNC_GENERATE_OPPONENT_MONS);
    TEST_ASSERT(gTrainerBattleOpponent_A >= FRONTIER_TRAINER_ERIK);
    TEST_ASSERT(gTrainerBattleOpponent_A <= FRONTIER_TRAINER_CHLOE);

    TestResetFixture(0x6202);
    SetFactoryContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_DOUBLES, FRONTIER_LVL_50);
    CallFactory(BATTLE_FACTORY_FUNC_INIT);
    CallFactory(BATTLE_FACTORY_FUNC_GENERATE_RENTAL_MONS);
    AssertRentalDraft(FRONTIER_MON_DUGTRIO_1, FRONTIER_MONS_HIGH_TIER);
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = 3;
    CallFactory(BATTLE_FACTORY_FUNC_GENERATE_OPPONENT_MONS);
    TEST_ASSERT(gTrainerBattleOpponent_A >= FRONTIER_TRAINER_JAXON);
    TEST_ASSERT(gTrainerBattleOpponent_A <= FRONTIER_TRAINER_GRETEL);
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
    {
        TEST_ASSERT(gFrontierTempParty[i] >= FRONTIER_MON_DUGTRIO_1);
        TEST_ASSERT(gFrontierTempParty[i] <= FRONTIER_MONS_HIGH_TIER);
        for (j = 0; j < PARTY_SIZE; j++)
            TEST_ASSERT(gFacilityTrainerMons[gFrontierTempParty[i]].species != gFacilityTrainerMons[gSaveBlock2Ptr->frontier.rentalMons[j].monId].species);
    }

    TestResetFixture(0x6203);
    SetFactoryContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_SINGLES, FRONTIER_LVL_OPEN);
    CallFactory(BATTLE_FACTORY_FUNC_INIT);
    CallFactory(BATTLE_FACTORY_FUNC_GENERATE_RENTAL_MONS);
    AssertRentalDraft(FRONTIER_MON_DUGTRIO_1, NUM_FRONTIER_MONS - 1);
}

static void TestRentalRankAndSwapGate(void)
{
    TestResetFixture(0x6301);
    SetFactoryContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    gSaveBlock1Ptr->frontierHardMode.factoryRentsCount[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 14;
    TEST_ASSERT_EQ(0, GetNumPastRentalsRank(FRONTIER_MODE_SINGLES, FRONTIER_LVL_50));
    gSaveBlock1Ptr->frontierHardMode.factoryRentsCount[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 15;
    TEST_ASSERT_EQ(1, GetNumPastRentalsRank(FRONTIER_MODE_SINGLES, FRONTIER_LVL_50));
    gSaveBlock1Ptr->frontierHardMode.factoryRentsCount[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 43;
    TEST_ASSERT_EQ(5, GetNumPastRentalsRank(FRONTIER_MODE_SINGLES, FRONTIER_LVL_50));

    SetFactoryData(FACTORY_DATA_WIN_STREAK_SWAPS, 9);
    TEST_ASSERT_EQ(43, GetFactoryData(FACTORY_DATA_WIN_STREAK_SWAPS));
    CallFactory(BATTLE_FACTORY_FUNC_SET_SWAPPED);
    SetFactoryData(FACTORY_DATA_WIN_STREAK_SWAPS, 44);
    TEST_ASSERT_EQ(44, GetFactoryData(FACTORY_DATA_WIN_STREAK_SWAPS));
    SetFactoryData(FACTORY_DATA_WIN_STREAK_SWAPS, 45);
    TEST_ASSERT_EQ(44, GetFactoryData(FACTORY_DATA_WIN_STREAK_SWAPS));
}

static void TestPartyResumeAndMoveRules(void)
{
    u32 i;

    TestResetFixture(0x6401);
    SetFactoryContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_SINGLES, FRONTIER_LVL_OPEN);
    CallFactory(BATTLE_FACTORY_FUNC_INIT);
    CallFactory(BATTLE_FACTORY_FUNC_GENERATE_RENTAL_MONS);
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
    {
        gSaveBlock2Ptr->frontier.rentalMons[i].personality = 100 + i;
        gSaveBlock2Ptr->frontier.rentalMons[i].ivs = 20 + i;
        gSaveBlock2Ptr->frontier.rentalMons[i].abilityNum = i & 1;
    }
    gSpecialVar_0x8005 = 1;
    CallFactory(BATTLE_FACTORY_FUNC_SET_PARTIES);
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
    {
        TEST_ASSERT_EQ(FRONTIER_MAX_LEVEL_OPEN, GetMonData(&gPlayerParty[i], MON_DATA_LEVEL));
        TEST_ASSERT_EQ(gSaveBlock2Ptr->frontier.rentalMons[i].personality, GetMonData(&gPlayerParty[i], MON_DATA_PERSONALITY));
        TEST_ASSERT_EQ(gSaveBlock2Ptr->frontier.rentalMons[i].ivs, GetMonData(&gPlayerParty[i], MON_DATA_HP_IV));
        TEST_ASSERT_EQ(gSaveBlock2Ptr->frontier.rentalMons[i].abilityNum, GetMonData(&gPlayerParty[i], MON_DATA_ABILITY_NUM));
    }

    SetMonMoveAvoidReturn(&gPlayerParty[0], MOVE_RETURN, 0);
    TEST_ASSERT_EQ(MOVE_FRUSTRATION, GetMonData(&gPlayerParty[0], MON_DATA_MOVE1));
}

static void TestRoundsBrainProgressAndCleanup(void)
{
    TestResetFixture(0x6501);
    SetFactoryContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.factoryWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 19;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_NOT_READY, GetFrontierBrainStatus());
    gSaveBlock2Ptr->frontier.factoryWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 20;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_SILVER, GetFrontierBrainStatus());

    TestResetFixture(0x6502);
    SetFactoryContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    gSaveBlock1Ptr->frontierHardMode.factoryWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 12;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_NOT_READY, GetFrontierBrainStatus());
    gSaveBlock1Ptr->frontierHardMode.factoryWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 13;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_SILVER, GetFrontierBrainStatus());
    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_INCREMENT_STREAK;
    CallFrontierUtilFunc();
    TEST_ASSERT_EQ(14, gSaveBlock1Ptr->frontierHardMode.factoryWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50]);
    TEST_ASSERT_EQ(0, gSaveBlock2Ptr->frontier.factoryWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50]);

    SetFactoryData(FACTORY_DATA_WIN_STREAK_ACTIVE, TRUE);
    TEST_ASSERT(GetFactoryData(FACTORY_DATA_WIN_STREAK_ACTIVE));
    SetFrontierData(FRONTIER_DATA_CHALLENGE_STATUS, CHALLENGE_STATUS_LOST);
    SetFactoryData(FACTORY_DATA_WIN_STREAK_ACTIVE, FALSE);
    TEST_ASSERT_EQ(CHALLENGE_STATUS_LOST, gSaveBlock2Ptr->frontier.challengeStatus);
    TEST_ASSERT(!GetFactoryData(FACTORY_DATA_WIN_STREAK_ACTIVE));
    SetFactoryData(FACTORY_DATA_WIN_STREAK, 0);
    TEST_ASSERT_EQ(0, GetFactoryData(FACTORY_DATA_WIN_STREAK));
}

static void TestPauseBattleFlagsAiAndHardIv(void)
{
    u32 i;
    u32 expectedAi = AI_SCRIPT_CHECK_BAD_MOVE | AI_SCRIPT_TRY_TO_FAINT | AI_SCRIPT_CHECK_VIABILITY;

    TestResetFixture(0x6601);
    SetFactoryContext(FRONTIER_CHALLENGE_HARD, FRONTIER_MODE_DOUBLES, FRONTIER_LVL_50);
    CallFactory(BATTLE_FACTORY_FUNC_INIT);
    CallFactory(BATTLE_FACTORY_FUNC_GENERATE_RENTAL_MONS);
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = FRONTIER_STAGES_PER_CHALLENGE - 1;
    CallFactory(BATTLE_FACTORY_FUNC_GENERATE_OPPONENT_MONS);
    TEST_ASSERT_EQ(expectedAi, GetAiScriptsInBattleFactory());
    gSpecialVar_0x8004 = SPECIAL_BATTLE_FACTORY;
    DoSpecialTrainerBattle();
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_TRAINER);
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_FACTORY);
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_DOUBLE);
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
    {
        TEST_ASSERT_EQ(FRONTIER_MAX_LEVEL_50, GetMonData(&gEnemyParty[i], MON_DATA_LEVEL));
        TEST_ASSERT_EQ(GetFactoryMonFixedIV(7, TRUE), GetMonData(&gEnemyParty[i], MON_DATA_HP_IV));
    }
    CallFactory(BATTLE_FACTORY_FUNC_SET_OPPONENT_MONS);
    ZeroEnemyPartyMons();
    gSpecialVar_0x8005 = 2;
    CallFactory(BATTLE_FACTORY_FUNC_SET_PARTIES);
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
    {
        TEST_ASSERT_EQ(gFacilityTrainerMons[gSaveBlock2Ptr->frontier.rentalMons[i + FRONTIER_PARTY_SIZE].monId].species,
                       GetMonData(&gEnemyParty[i], MON_DATA_SPECIES));
        TEST_ASSERT_EQ(gSaveBlock2Ptr->frontier.rentalMons[i + FRONTIER_PARTY_SIZE].personality,
                       GetMonData(&gEnemyParty[i], MON_DATA_PERSONALITY));
        TEST_ASSERT_EQ(gSaveBlock2Ptr->frontier.rentalMons[i + FRONTIER_PARTY_SIZE].ivs,
                       GetMonData(&gEnemyParty[i], MON_DATA_HP_IV));
    }

    gSpecialVar_0x8005 = CHALLENGE_STATUS_PAUSED;
    CallFactory(BATTLE_FACTORY_FUNC_SAVE);
    TEST_ASSERT_EQ(CHALLENGE_STATUS_PAUSED, gSaveBlock2Ptr->frontier.challengeStatus);
    TEST_ASSERT(gSaveBlock2Ptr->frontier.challengePaused);
    TEST_ASSERT_EQ(0, VarGet(VAR_TEMP_CHALLENGE_STATUS));
}

void RunTest(void)
{
    TestNormalAndHardChallengeEntry();
    TestRentalAndOpponentPools();
    TestRentalRankAndSwapGate();
    TestPartyResumeAndMoveRules();
    TestRoundsBrainProgressAndCleanup();
    TestPauseBattleFlagsAiAndHardIv();
}
