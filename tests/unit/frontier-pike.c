#include "global.h"
#include "battle.h"
#include "battle_pike.h"
#include "battle_setup.h"
#include "battle_tower.h"
#include "event_data.h"
#include "frontier_util.h"
#include "pokemon.h"
#include "random.h"
#include "test.h"
#include "constants/battle_frontier.h"
#include "constants/battle_frontier_trainers.h"
#include "constants/battle_pike.h"
#include "constants/frontier_util.h"
#include "constants/flags.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/pokemon.h"
#include "constants/vars.h"

static void SetPikeContext(u8 challengeMode, u8 lvlMode)
{
    VarSet(VAR_FRONTIER_FACILITY, FRONTIER_FACILITY_PIKE);
    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_SET_DATA;
    gSpecialVar_0x8005 = FRONTIER_DATA_CHALLENGE_MODE;
    gSpecialVar_0x8006 = challengeMode;
    CallFrontierUtilFunc();
    gSpecialVar_0x8005 = FRONTIER_DATA_LVL_MODE;
    gSpecialVar_0x8006 = lvlMode;
    CallFrontierUtilFunc();
}

static void CallPike(u16 function)
{
    gSpecialVar_0x8004 = function;
    CallBattlePikeFunction();
}

static void SetPikeData(u16 data, u16 value)
{
    gSpecialVar_0x8005 = data;
    gSpecialVar_0x8006 = value;
    CallPike(BATTLE_PIKE_FUNC_SET_DATA);
}

static void SetFrontierData(u16 data, u16 value)
{
    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_SET_DATA;
    gSpecialVar_0x8005 = data;
    gSpecialVar_0x8006 = value;
    CallFrontierUtilFunc();
}

static void IncrementStreak(void)
{
    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_INCREMENT_STREAK;
    CallFrontierUtilFunc();
}

static void AssertTrainerInRange(u16 trainer, u16 minimum, u16 maximum)
{
    TEST_ASSERT(trainer >= minimum);
    TEST_ASSERT(trainer <= maximum);
}

static void SetMatchingRoom(u8 roomType, u8 roomIndex)
{
    gSaveBlock2Ptr->frontier.pikeHintedRoomType = roomType;
    gSaveBlock2Ptr->frontier.pikeHintedRoomIndex = roomIndex;
    gSpecialVar_0x8007 = roomIndex;
    CallPike(BATTLE_PIKE_FUNC_SET_ROOM_TYPE);
    CallPike(BATTLE_PIKE_FUNC_GET_ROOM_TYPE);
    TEST_ASSERT_EQ(roomType, gSpecialVar_Result);
}

static void TestNormalAndHardInit(void)
{
    TestResetFixture(0x6101);
    SetPikeContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.pikeWinStreaks[FRONTIER_LVL_50] = 12;
    gSaveBlock1Ptr->frontierHardMode.pikeWinStreaks[FRONTIER_LVL_50] = 31;
    CallPike(BATTLE_PIKE_FUNC_INIT);
    TEST_ASSERT_EQ(0, gSaveBlock2Ptr->frontier.pikeWinStreaks[FRONTIER_LVL_50]);
    TEST_ASSERT_EQ(31, gSaveBlock1Ptr->frontierHardMode.pikeWinStreaks[FRONTIER_LVL_50]);
    TEST_ASSERT(!gSaveBlock2Ptr->frontier.challengePaused);

    TestResetFixture(0x6102);
    SetPikeContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_OPEN);
    gSaveBlock2Ptr->frontier.pikeWinStreaks[FRONTIER_LVL_OPEN] = 8;
    gSaveBlock1Ptr->frontierHardMode.pikeWinStreaks[FRONTIER_LVL_OPEN] = 19;
    gSaveBlock1Ptr->frontierHardMode.pikeWinStreakActiveFlags = STREAK_PIKE_OPEN;
    CallPike(BATTLE_PIKE_FUNC_INIT);
    TEST_ASSERT_EQ(19, gSaveBlock1Ptr->frontierHardMode.pikeWinStreaks[FRONTIER_LVL_OPEN]);
    TEST_ASSERT_EQ(8, gSaveBlock2Ptr->frontier.pikeWinStreaks[FRONTIER_LVL_OPEN]);
}

static void TestModeDataAndCleanup(void)
{
    TestResetFixture(0x6201);
    SetPikeContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.pikeWinStreaks[FRONTIER_LVL_50] = 4;
    SetPikeData(PIKE_DATA_WIN_STREAK, 17);
    SetPikeData(PIKE_DATA_RECORD_STREAK, 17);
    SetPikeData(PIKE_DATA_TOTAL_STREAKS, 22);
    SetPikeData(PIKE_DATA_WIN_STREAK_ACTIVE, TRUE);
    TEST_ASSERT_EQ(17, gSaveBlock1Ptr->frontierHardMode.pikeWinStreaks[FRONTIER_LVL_50]);
    TEST_ASSERT_EQ(17, gSaveBlock1Ptr->frontierHardMode.pikeRecordStreaks[FRONTIER_LVL_50]);
    TEST_ASSERT_EQ(22, gSaveBlock1Ptr->frontierHardMode.pikeTotalStreaks[FRONTIER_LVL_50]);
    TEST_ASSERT(gSaveBlock1Ptr->frontierHardMode.pikeWinStreakActiveFlags & STREAK_PIKE_50);
    TEST_ASSERT_EQ(4, gSaveBlock2Ptr->frontier.pikeWinStreaks[FRONTIER_LVL_50]);
    SetPikeData(PIKE_DATA_WIN_STREAK_ACTIVE, FALSE);
    SetPikeData(PIKE_DATA_WIN_STREAK, 0);
    TEST_ASSERT_EQ(0, gSaveBlock1Ptr->frontierHardMode.pikeWinStreaks[FRONTIER_LVL_50]);
    TEST_ASSERT(!(gSaveBlock1Ptr->frontierHardMode.pikeWinStreakActiveFlags & STREAK_PIKE_50));

    TestResetFixture(0x6202);
    SetPikeContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_OPEN);
    gSaveBlock2Ptr->frontier.pikeWinStreaks[FRONTIER_LVL_OPEN] = 5;
    gSaveBlock2Ptr->frontier.winStreakActiveFlags = STREAK_PIKE_OPEN;
    gSaveBlock1Ptr->frontierHardMode.pikeWinStreaks[FRONTIER_LVL_OPEN] = 10;
    gSaveBlock1Ptr->frontierHardMode.pikeRecordStreaks[FRONTIER_LVL_OPEN] = 10;
    gSaveBlock1Ptr->frontierHardMode.pikeTotalStreaks[FRONTIER_LVL_OPEN] = 2;
    gSaveBlock1Ptr->frontierHardMode.pikeWinStreakActiveFlags = STREAK_PIKE_OPEN;
    IncrementStreak();
    TEST_ASSERT_EQ(11, gSaveBlock1Ptr->frontierHardMode.pikeWinStreaks[FRONTIER_LVL_OPEN]);
    TEST_ASSERT_EQ(5, gSaveBlock2Ptr->frontier.pikeWinStreaks[FRONTIER_LVL_OPEN]);
    SetPikeData(PIKE_DATA_RECORD_STREAK, 11);
    SetPikeData(PIKE_DATA_TOTAL_STREAKS, 3);
    TEST_ASSERT_EQ(11, gSaveBlock1Ptr->frontierHardMode.pikeRecordStreaks[FRONTIER_LVL_OPEN]);
    TEST_ASSERT_EQ(3, gSaveBlock1Ptr->frontierHardMode.pikeTotalStreaks[FRONTIER_LVL_OPEN]);
    SetFrontierData(FRONTIER_DATA_CHALLENGE_STATUS, CHALLENGE_STATUS_LOST);
    SetPikeData(PIKE_DATA_WIN_STREAK_ACTIVE, FALSE);
    TEST_ASSERT_EQ(CHALLENGE_STATUS_LOST, gSaveBlock2Ptr->frontier.challengeStatus);
    TEST_ASSERT_EQ(11, gSaveBlock1Ptr->frontierHardMode.pikeWinStreaks[FRONTIER_LVL_OPEN]);
    TEST_ASSERT(!(gSaveBlock1Ptr->frontierHardMode.pikeWinStreakActiveFlags & STREAK_PIKE_OPEN));
    TEST_ASSERT(gSaveBlock2Ptr->frontier.winStreakActiveFlags & STREAK_PIKE_OPEN);
}

static void TestRoomsAndFinalBoundary(void)
{
    u32 i;
    TestResetFixture(0x6301);
    SetPikeContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_LVL_50);
    gSpecialVar_0x8007 = 0;
    gSaveBlock2Ptr->frontier.pikeHintedRoomIndex = 0;
    gSaveBlock2Ptr->frontier.pikeHintedRoomType = PIKE_ROOM_NPC;
    for (i = 0; i < 16; i++)
    {
        CallPike(BATTLE_PIKE_FUNC_SET_ROOM_TYPE);
        CallPike(BATTLE_PIKE_FUNC_GET_ROOM_TYPE);
        TEST_ASSERT(gSpecialVar_Result < NUM_PIKE_ROOM_TYPES);
    }
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = NUM_PIKE_ROOMS;
    CallPike(BATTLE_PIKE_FUNC_IS_FINAL_ROOM);
    TEST_ASSERT(!gSpecialVar_Result);
    gSaveBlock2Ptr->frontier.curChallengeBattleNum++;
    CallPike(BATTLE_PIKE_FUNC_IS_FINAL_ROOM);
    TEST_ASSERT(gSpecialVar_Result);

    TestResetFixture(0x6302);
    SetPikeContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.pikeWinStreaks[FRONTIER_LVL_50] = 26;
    CallPike(BATTLE_PIKE_FUNC_SET_HINT_ROOM);
    TEST_ASSERT(gSpecialVar_Result);
    TEST_ASSERT_EQ(PIKE_ROOM_BRAIN, gSaveBlock2Ptr->frontier.pikeHintedRoomType);
    CallPike(BATTLE_PIKE_FUNC_GET_ROOM_TYPE_HINT);
    TEST_ASSERT_EQ(PIKE_HINT_BRAIN, gSpecialVar_Result);

    TestResetFixture(0x6303);
    SetPikeContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_LVL_50);
    gSpecialVar_0x8005 = TRUE;
    CallPike(BATTLE_PIKE_FUNC_SET_HEAL_ROOMS_DISABLED);
    for (i = 0; i < 16; i++)
    {
        CallPike(BATTLE_PIKE_FUNC_SET_HINT_ROOM);
        TEST_ASSERT(gSaveBlock2Ptr->frontier.pikeHintedRoomType != PIKE_ROOM_HEAL_FULL);
        TEST_ASSERT(gSaveBlock2Ptr->frontier.pikeHintedRoomType != PIKE_ROOM_HEAL_PART);
    }

}

static void TestStatusRooms(void)
{
    u32 i;
    u32 afflicted = 0;

    TestResetFixture(0x6351);
    SetPikeContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_LVL_50);
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
        CreateMon(&gPlayerParty[i], SPECIES_TREECKO + i, 50, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = 1;
    SetMatchingRoom(PIKE_ROOM_STATUS, 0);
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
    {
        if (GetMonData(&gPlayerParty[i], MON_DATA_STATUS) != 0)
            afflicted++;
    }
    TEST_ASSERT_EQ(1, afflicted);
    CallPike(BATTLE_PIKE_FUNC_GET_ROOM_STATUS);
    TEST_ASSERT(gSpecialVar_Result <= PIKE_STATUS_SLEEP);
    CallPike(BATTLE_PIKE_FUNC_GET_ROOM_STATUS_MON);
    TEST_ASSERT(gSpecialVar_Result == PIKE_STATUSMON_KIRLIA || gSpecialVar_Result == PIKE_STATUSMON_DUSCLOPS);
}

static void TestWildRooms(void)
{
    u32 i;

    TestResetFixture(0x6381);
    SetPikeContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_LVL_50);
    CreateMon(&gEnemyParty[0], SPECIES_SEVIPER, 50, 0, FALSE, 0, OT_ID_PLAYER_ID, 0);
    TEST_ASSERT_EQ(0, GetBattlePikeWildMonHeaderId());
    TEST_ASSERT(TryGenerateBattlePikeWildMon(FALSE));
    TEST_ASSERT_EQ(46, GetMonData(&gEnemyParty[0], MON_DATA_LEVEL));
    TEST_ASSERT_EQ(MOVE_TOXIC, GetMonData(&gEnemyParty[0], MON_DATA_MOVE1));
    gSaveBlock2Ptr->frontier.pikeWinStreaks[FRONTIER_LVL_50] = 20 * NUM_PIKE_ROOMS + 1;
    TEST_ASSERT_EQ(1, GetBattlePikeWildMonHeaderId());
    gSaveBlock2Ptr->frontier.pikeWinStreaks[FRONTIER_LVL_50] = 40 * NUM_PIKE_ROOMS + 1;
    TEST_ASSERT_EQ(2, GetBattlePikeWildMonHeaderId());
    gSaveBlock2Ptr->frontier.pikeWinStreaks[FRONTIER_LVL_50] = 60 * NUM_PIKE_ROOMS + 1;
    TEST_ASSERT_EQ(3, GetBattlePikeWildMonHeaderId());

    TestResetFixture(0x6382);
    SetPikeContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_OPEN);
    CreateMon(&gPlayerParty[0], SPECIES_PIDGEOT, 70, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
    CreateMon(&gEnemyParty[0], SPECIES_DUSCLOPS, 70, 0, FALSE, 0, OT_ID_PLAYER_ID, 0);
    TEST_ASSERT(TryGenerateBattlePikeWildMon(FALSE));
    TEST_ASSERT_EQ(65, GetMonData(&gEnemyParty[0], MON_DATA_LEVEL));
    for (i = 0; i < MAX_MON_MOVES; i++)
        TEST_ASSERT(GetMonData(&gEnemyParty[0], MON_DATA_MOVE1 + i) != MOVE_NONE);
    SeedRng(1);
    TEST_ASSERT(!TryGenerateBattlePikeWildMon(TRUE));
}

static void TestHealingAndHeldItems(void)
{
    u16 hp = 1;
    u16 item = ITEM_LEFTOVERS;
    u32 status = STATUS1_BURN;
    u32 i;
    TestResetFixture(0x6401);
    SetPikeContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_50);
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
    {
        CreateMon(&gPlayerParty[i], SPECIES_TREECKO + i, 50, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
        SetMonData(&gPlayerParty[i], MON_DATA_HP, &hp);
        SetMonData(&gPlayerParty[i], MON_DATA_STATUS, &status);
        gSaveBlock2Ptr->frontier.selectedPartyMons[i] = i + 1;
        gSaveBlock1Ptr->playerParty[i] = gPlayerParty[i];
        SetMonData(&gSaveBlock1Ptr->playerParty[i], MON_DATA_HELD_ITEM, &item);
    }
    CallPike(BATTLE_PIKE_FUNC_IS_PARTY_FULL_HEALTH);
    TEST_ASSERT(!gSpecialVar_Result);
    CallPike(BATTLE_PIKE_FUNC_HEAL_ONE_TWO_MONS);
    TEST_ASSERT(gSpecialVar_Result == 1 || gSpecialVar_Result == 2);
    CallPike(BATTLE_PIKE_FUNC_SAVE_HELD_ITEMS);
    item = ITEM_NONE;
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
        SetMonData(&gPlayerParty[i], MON_DATA_HELD_ITEM, &item);
    CallPike(BATTLE_PIKE_FUNC_RESET_HELD_ITEMS);
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
        TEST_ASSERT_EQ(ITEM_LEFTOVERS, GetMonData(&gPlayerParty[i], MON_DATA_HELD_ITEM));
}

static void TestPauseResumeAndBattleFlags(void)
{
    u32 i;

    TestResetFixture(0x6501);
    SetPikeContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_OPEN);
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = 6;
    gSpecialVar_0x8005 = CHALLENGE_STATUS_PAUSED;
    CallPike(BATTLE_PIKE_FUNC_SAVE);
    TEST_ASSERT_EQ(CHALLENGE_STATUS_PAUSED, gSaveBlock2Ptr->frontier.challengeStatus);
    TEST_ASSERT(gSaveBlock2Ptr->frontier.challengePaused);
    TEST_ASSERT_EQ(6, gSaveBlock2Ptr->frontier.curChallengeBattleNum);

    CreateMon(&gPlayerParty[0], SPECIES_TREECKO, 70, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
    gPlayerPartyCount = 1;
    gSpecialVar_0x8004 = SPECIAL_BATTLE_PIKE_SINGLE;
    DoSpecialTrainerBattle();
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_TRAINER);
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_BATTLE_TOWER);
    TEST_ASSERT(!(gBattleTypeFlags & BATTLE_TYPE_PIKE));
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
        TEST_ASSERT_EQ(70, GetMonData(&gEnemyParty[i], MON_DATA_LEVEL));

    TestResetFixture(0x6502);
    SetPikeContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_50);
    gSaveBlock1Ptr->frontierHardMode.pikeWinStreaks[FRONTIER_LVL_50] = 8;
    gSaveBlock1Ptr->frontierHardMode.pikeWinStreakActiveFlags = STREAK_PIKE_50;
    FlagSet(FLAG_CANCEL_BATTLE_ROOM_CHALLENGE);
    SetFrontierData(FRONTIER_DATA_CHALLENGE_STATUS, CHALLENGE_STATUS_LOST);
    SetPikeData(PIKE_DATA_WIN_STREAK_ACTIVE, FALSE);
    TEST_ASSERT(FlagGet(FLAG_CANCEL_BATTLE_ROOM_CHALLENGE));
    TEST_ASSERT_EQ(CHALLENGE_STATUS_LOST, gSaveBlock2Ptr->frontier.challengeStatus);
    TEST_ASSERT_EQ(8, gSaveBlock1Ptr->frontierHardMode.pikeWinStreaks[FRONTIER_LVL_50]);
    TEST_ASSERT(!(gSaveBlock1Ptr->frontierHardMode.pikeWinStreakActiveFlags & STREAK_PIKE_50));
}

static void TestTrainerRoomsAndQueen(void)
{
    u16 hp = 1;
    u32 i;

    TestResetFixture(0x6601);
    SetPikeContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = 1;
    SetMatchingRoom(PIKE_ROOM_SINGLE_BATTLE, 0);
    CallPike(BATTLE_PIKE_FUNC_SET_ROOM_OBJECTS);
    AssertTrainerInRange(gTrainerBattleOpponent_A, FRONTIER_TRAINER_BRADY, FRONTIER_TRAINER_JILL);
    gSpecialVar_0x8004 = SPECIAL_BATTLE_PIKE_SINGLE;
    DoSpecialTrainerBattle();
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_TRAINER);
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_BATTLE_TOWER);
    TEST_ASSERT(!(gBattleTypeFlags & BATTLE_TYPE_DOUBLE));
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
        TEST_ASSERT_EQ(FRONTIER_MAX_LEVEL_50, GetMonData(&gEnemyParty[i], MON_DATA_LEVEL));

    TestResetFixture(0x6602);
    SetPikeContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_OPEN);
    CreateMon(&gPlayerParty[0], SPECIES_TREECKO, 70, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
    gPlayerPartyCount = 1;
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = 1;
    SetMatchingRoom(PIKE_ROOM_HARD_BATTLE, 0);
    CallPike(BATTLE_PIKE_FUNC_SET_ROOM_OBJECTS);
    AssertTrainerInRange(gTrainerBattleOpponent_A, FRONTIER_TRAINER_JAXON, FRONTIER_TRAINER_GRETEL);

    TestResetFixture(0x6603);
    SetPikeContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = 2;
    SetMatchingRoom(PIKE_ROOM_DOUBLE_BATTLE, 0);
    CallPike(BATTLE_PIKE_FUNC_SET_ROOM_OBJECTS);
    AssertTrainerInRange(gTrainerBattleOpponent_A, FRONTIER_TRAINER_JAXON, FRONTIER_TRAINER_GRETEL);
    AssertTrainerInRange(gTrainerBattleOpponent_B, FRONTIER_TRAINER_JAXON, FRONTIER_TRAINER_GRETEL);
    TEST_ASSERT(gTrainerBattleOpponent_A != gTrainerBattleOpponent_B);
    gSpecialVar_0x8004 = SPECIAL_BATTLE_PIKE_DOUBLE;
    DoSpecialTrainerBattle();
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_DOUBLE);
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_TWO_OPPONENTS);

    TestResetFixture(0x6604);
    SetPikeContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.pikeWinStreaks[FRONTIER_LVL_50] = 26;
    CallPike(BATTLE_PIKE_FUNC_GET_QUEEN_FIGHT_TYPE);
    TEST_ASSERT_EQ(FRONTIER_BRAIN_NOT_READY, gSpecialVar_Result);
    gSaveBlock2Ptr->frontier.pikeWinStreaks[FRONTIER_LVL_50] = 27;
    CallPike(BATTLE_PIKE_FUNC_GET_QUEEN_FIGHT_TYPE);
    TEST_ASSERT_EQ(FRONTIER_BRAIN_SILVER, gSpecialVar_Result);

    TestResetFixture(0x6605);
    SetPikeContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_50);
    gSaveBlock1Ptr->frontierHardMode.pikeWinStreaks[FRONTIER_LVL_50] = 12;
    CallPike(BATTLE_PIKE_FUNC_GET_QUEEN_FIGHT_TYPE);
    TEST_ASSERT_EQ(FRONTIER_BRAIN_NOT_READY, gSpecialVar_Result);
    gSaveBlock1Ptr->frontierHardMode.pikeWinStreaks[FRONTIER_LVL_50] = 13;
    CallPike(BATTLE_PIKE_FUNC_GET_QUEEN_FIGHT_TYPE);
    TEST_ASSERT_EQ(FRONTIER_BRAIN_SILVER, gSpecialVar_Result);
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
    {
        CreateMon(&gPlayerParty[i], SPECIES_TREECKO + i, 50, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
        SetMonData(&gPlayerParty[i], MON_DATA_HP, &hp);
    }
    gSaveBlock2Ptr->frontier.pikeHintedRoomIndex = 0;
    gSpecialVar_0x8007 = 0;
    CallPike(BATTLE_PIKE_FUNC_HEAL_MONS_BEFORE_QUEEN);
    TEST_ASSERT_EQ(2, gSpecialVar_Result);
}

void RunTest(void)
{
    TestNormalAndHardInit();
    TestModeDataAndCleanup();
    TestRoomsAndFinalBoundary();
    TestStatusRooms();
    TestWildRooms();
    TestHealingAndHeldItems();
    TestPauseResumeAndBattleFlags();
    TestTrainerRoomsAndQueen();
}
