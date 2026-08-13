#include "global.h"
#include "battle.h"
#include "battle_pyramid.h"
#include "battle_setup.h"
#include "battle_tower.h"
#include "event_data.h"
#include "fieldmap.h"
#include "frontier_util.h"
#include "load_save.h"
#include "pokemon.h"
#include "script_pokemon_util.h"
#include "test.h"
#include "constants/battle_frontier.h"
#include "constants/battle_pyramid.h"
#include "constants/event_objects.h"
#include "constants/frontier_util.h"
#include "constants/items.h"
#include "constants/layouts.h"
#include "constants/moves.h"
#include "constants/pokemon.h"
#include "constants/trainers.h"
#include "constants/vars.h"

static void SetPyramidContext(u8 challengeMode, u8 lvlMode)
{
    VarSet(VAR_FRONTIER_FACILITY, FRONTIER_FACILITY_PYRAMID);
    gSpecialVar_0x8004 = FRONTIER_UTIL_FUNC_SET_DATA;
    gSpecialVar_0x8005 = FRONTIER_DATA_CHALLENGE_MODE;
    gSpecialVar_0x8006 = challengeMode;
    CallFrontierUtilFunc();
    gSpecialVar_0x8005 = FRONTIER_DATA_LVL_MODE;
    gSpecialVar_0x8006 = lvlMode;
    CallFrontierUtilFunc();
}

static void CallPyramid(u16 function)
{
    gSpecialVar_0x8004 = function;
    CallBattlePyramidFunction();
}

static void SetPyramidData(u16 data, u16 value)
{
    gSpecialVar_0x8005 = data;
    gSpecialVar_0x8006 = value;
    CallPyramid(BATTLE_PYRAMID_FUNC_SET_DATA);
}

static void TestNormalAndHardInit(void)
{
    TestResetFixture(0x7101);
    SetPyramidContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.pyramidWinStreaks[FRONTIER_LVL_50] = 11;
    gSaveBlock1Ptr->frontierHardMode.pyramidWinStreaks[FRONTIER_LVL_50] = 29;
    CallPyramid(BATTLE_PYRAMID_FUNC_INIT);
    TEST_ASSERT_EQ(0, gSaveBlock2Ptr->frontier.pyramidWinStreaks[FRONTIER_LVL_50]);
    TEST_ASSERT_EQ(29, gSaveBlock1Ptr->frontierHardMode.pyramidWinStreaks[FRONTIER_LVL_50]);
    TEST_ASSERT(!gSaveBlock2Ptr->frontier.challengePaused);

    TestResetFixture(0x7102);
    SetPyramidContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_OPEN);
    gSaveBlock2Ptr->frontier.pyramidWinStreaks[FRONTIER_LVL_OPEN] = 7;
    gSaveBlock1Ptr->frontierHardMode.pyramidWinStreaks[FRONTIER_LVL_OPEN] = 18;
    gSaveBlock1Ptr->frontierHardMode.pyramidWinStreakActiveFlags = STREAK_PYRAMID_OPEN;
    CallPyramid(BATTLE_PYRAMID_FUNC_INIT);
    TEST_ASSERT_EQ(18, gSaveBlock1Ptr->frontierHardMode.pyramidWinStreaks[FRONTIER_LVL_OPEN]);
    TEST_ASSERT_EQ(7, gSaveBlock2Ptr->frontier.pyramidWinStreaks[FRONTIER_LVL_OPEN]);
}

static void TestDataProgressionAndCleanup(void)
{
    TestResetFixture(0x7201);
    SetPyramidContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.pyramidWinStreaks[FRONTIER_LVL_50] = 5;
    SetPyramidData(PYRAMID_DATA_WIN_STREAK, 20);
    SetPyramidData(PYRAMID_DATA_WIN_STREAK_ACTIVE, TRUE);
    CallPyramid(BATTLE_PYRAMID_FUNC_UPDATE_STREAK);
    TEST_ASSERT_EQ(21, gSaveBlock1Ptr->frontierHardMode.pyramidWinStreaks[FRONTIER_LVL_50]);
    TEST_ASSERT_EQ(21, gSaveBlock1Ptr->frontierHardMode.pyramidRecordStreaks[FRONTIER_LVL_50]);
    TEST_ASSERT_EQ(5, gSaveBlock2Ptr->frontier.pyramidWinStreaks[FRONTIER_LVL_50]);
    SetPyramidData(PYRAMID_DATA_WIN_STREAK_ACTIVE, FALSE);
    SetPyramidData(PYRAMID_DATA_WIN_STREAK, 0);
    TEST_ASSERT_EQ(0, gSaveBlock1Ptr->frontierHardMode.pyramidWinStreaks[FRONTIER_LVL_50]);
    TEST_ASSERT(!(gSaveBlock1Ptr->frontierHardMode.pyramidWinStreakActiveFlags & STREAK_PYRAMID_50));
}

static void TestFloorSeedTrainerAndItemEvents(void)
{
    u32 i;
    u32 trainers = 0;
    u32 items = 0;
    bool32 anyRandom = FALSE;
    TestResetFixture(0x7301);
    SetPyramidContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_LVL_50);
    gSaveBlock2Ptr->frontier.pyramidTrainerFlags = 0xFF;
    CallPyramid(BATTLE_PYRAMID_FUNC_SEED_FLOOR);
    TEST_ASSERT_EQ(0, gSaveBlock2Ptr->frontier.pyramidTrainerFlags);
    for (i = 0; i < ARRAY_COUNT(gSaveBlock2Ptr->frontier.pyramidRandoms); i++)
        anyRandom |= gSaveBlock2Ptr->frontier.pyramidRandoms[i] != 0;
    TEST_ASSERT(anyRandom);

    InitBattlePyramidMap(FALSE);
    TEST_ASSERT(gBackupMapLayout.map != NULL);
    TEST_ASSERT(gBackupMapLayout.width > 32);
    TEST_ASSERT(gBackupMapLayout.height > 32);
    TEST_ASSERT(gSaveBlock1Ptr->pos.x < gBackupMapLayout.width);
    TEST_ASSERT(gSaveBlock1Ptr->pos.y < gBackupMapLayout.height);
    LoadBattlePyramidObjectEventTemplates();
    LoadBattlePyramidFloorObjectEventScripts();
    TEST_ASSERT(GetNumBattlePyramidObjectEvents() > 0);
    for (i = 0; i < GetNumBattlePyramidObjectEvents(); i++)
    {
        struct ObjectEventTemplate *event = &gSaveBlock1Ptr->objectEventTemplates[i];
        TEST_ASSERT(event->localId != LOCALID_NONE);
        TEST_ASSERT(event->x < gBackupMapLayout.width);
        TEST_ASSERT(event->y < gBackupMapLayout.height);
        if (event->graphicsId == OBJ_EVENT_GFX_ITEM_BALL)
            items++;
        else
        {
            trainers++;
            TEST_ASSERT(LocalIdToPyramidTrainerId(event->localId) != 0xFFFF);
        }
    }
    TEST_ASSERT(trainers > 0);
    TEST_ASSERT(items > 0);

    CallPyramid(BATTLE_PYRAMID_FUNC_SET_TRAINERS);
    TEST_ASSERT(gFacilityTrainers == gBattleFrontierTrainers);
    gSpecialVar_LastTalked = trainers + 1;
    CallPyramid(BATTLE_PYRAMID_FUNC_SET_ITEM);
    TEST_ASSERT(gSpecialVar_0x8000 != ITEM_NONE);
    TEST_ASSERT_EQ(1, gSpecialVar_0x8001);
    CallPyramid(BATTLE_PYRAMID_FUNC_HIDE_ITEM);
    TEST_ASSERT_EQ(INT16_MAX, gSaveBlock1Ptr->objectEventTemplates[gSpecialVar_LastTalked - 1].x);
    TEST_ASSERT_EQ(INT16_MAX, gSaveBlock1Ptr->objectEventTemplates[gSpecialVar_LastTalked - 1].y);

    SetPyramidData(PYRAMID_DATA_TRAINER_FLAGS, 0x25);
    TEST_ASSERT_EQ(0x25, gSaveBlock2Ptr->frontier.pyramidTrainerFlags);
    gSaveBlock2Ptr->frontier.trainerIds[2] = 123;
    TEST_ASSERT_EQ(123, LocalIdToPyramidTrainerId(3));
}

static void TestLightRadiusAndHeldItems(void)
{
    u16 item = ITEM_LEFTOVERS;
    u32 i;
    TestResetFixture(0x7401);
    SetPyramidContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_OPEN);
    gSpecialVar_0x8005 = 40;
    gSpecialVar_0x8006 = PYRAMID_LIGHT_SET_RADIUS;
    CallPyramid(BATTLE_PYRAMID_FUNC_UPDATE_LIGHT);
    TEST_ASSERT_EQ(40, gSaveBlock2Ptr->frontier.pyramidLightRadius);
    gSpecialVar_0x8005 = 100;
    gSpecialVar_0x8006 = PYRAMID_LIGHT_SET_RADIUS;
    CallPyramid(BATTLE_PYRAMID_FUNC_UPDATE_LIGHT);
    TEST_ASSERT_EQ(100, gSaveBlock2Ptr->frontier.pyramidLightRadius);
    gSpecialVar_Result = 0;
    gSpecialVar_0x8005 = 25;
    gSpecialVar_0x8006 = PYRAMID_LIGHT_INCR_RADIUS;
    CallPyramid(BATTLE_PYRAMID_FUNC_UPDATE_LIGHT);
    TEST_ASSERT_EQ(1, gSpecialVar_Result);
    while (gSpecialVar_Result == 1)
        CallPyramid(BATTLE_PYRAMID_FUNC_UPDATE_LIGHT);
    TEST_ASSERT_EQ(120, gSaveBlock2Ptr->frontier.pyramidLightRadius);

    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
    {
        CreateMon(&gPlayerParty[i], SPECIES_TREECKO + i, 60, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
        SetMonData(&gPlayerParty[i], MON_DATA_HELD_ITEM, &item);
        gSaveBlock2Ptr->frontier.selectedPartyMons[i] = i + 1;
    }
    CallPyramid(BATTLE_PYRAMID_FUNC_CLEAR_HELD_ITEMS);
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
        TEST_ASSERT_EQ(ITEM_NONE, GetMonData(&gPlayerParty[i], MON_DATA_HELD_ITEM));
}

static void TestWildBattleAndLevels(void)
{
    TestResetFixture(0x7500);
    SetPyramidContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_LVL_50);
    CreateMon(&gEnemyParty[0], SPECIES_BULBASAUR, 50, 0, FALSE, 0, OT_ID_PLAYER_ID, 0);
    GenerateBattlePyramidWildMon();
    TEST_ASSERT_EQ(SPECIES_PLUSLE, GetMonData(&gEnemyParty[0], MON_DATA_SPECIES));
    TEST_ASSERT_EQ(MOVE_THUNDER_WAVE, GetMonData(&gEnemyParty[0], MON_DATA_MOVE1));
    TEST_ASSERT(GetMonData(&gEnemyParty[0], MON_DATA_LEVEL) >= 30);
    TEST_ASSERT(GetMonData(&gEnemyParty[0], MON_DATA_LEVEL) <= 40);

    TestResetFixture(0x7501);
    SetPyramidContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_50);
    gSaveBlock1Ptr->frontierHardMode.pyramidWinStreaks[FRONTIER_LVL_50] = 140;
    CreateMon(&gEnemyParty[0], SPECIES_BULBASAUR, 50, 0, FALSE, 0, OT_ID_PLAYER_ID, 0);
    GenerateBattlePyramidWildMon();
    TEST_ASSERT_EQ(SPECIES_PLUSLE, GetMonData(&gEnemyParty[0], MON_DATA_SPECIES));
    TEST_ASSERT(GetMonData(&gEnemyParty[0], MON_DATA_LEVEL) >= 30);
    TEST_ASSERT(GetMonData(&gEnemyParty[0], MON_DATA_LEVEL) <= 40);
    TEST_ASSERT(GetMonData(&gEnemyParty[0], MON_DATA_HP_IV) >= 15);
    TEST_ASSERT(GetMonData(&gEnemyParty[0], MON_DATA_HP_IV) <= 31);

    TestResetFixture(0x7502);
    SetPyramidContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_LVL_OPEN);
    CreateMon(&gPlayerParty[0], SPECIES_TREECKO, 72, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
    gPlayerPartyCount = 1;
    CreateMon(&gEnemyParty[0], SPECIES_BULBASAUR, 50, 0, FALSE, 0, OT_ID_PLAYER_ID, 0);
    GenerateBattlePyramidWildMon();
    TEST_ASSERT_EQ(SPECIES_PLUSLE, GetMonData(&gEnemyParty[0], MON_DATA_SPECIES));
    TEST_ASSERT_EQ(MOVE_THUNDER_WAVE, GetMonData(&gEnemyParty[0], MON_DATA_MOVE1));
    TEST_ASSERT(GetMonData(&gEnemyParty[0], MON_DATA_LEVEL) >= 52);
    TEST_ASSERT(GetMonData(&gEnemyParty[0], MON_DATA_LEVEL) <= 62);

    TestResetFixture(0x7503);
    SetPyramidContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_50);
    CreateMon(&gPlayerParty[0], SPECIES_TREECKO, 72, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
    gPlayerPartyCount = 1;
    gSpecialVar_0x8004 = SPECIAL_BATTLE_PYRAMID;
    DoSpecialTrainerBattle();
    TEST_ASSERT(gBattleTypeFlags & BATTLE_TYPE_PYRAMID);
    TEST_ASSERT_EQ(50, GetMonData(&gEnemyParty[0], MON_DATA_LEVEL));
}

static void TestTrainerPoolRoundBoundaries(void)
{
    u16 trainer;

    TestResetFixture(0x7510);
    trainer = GetRandomScaledFrontierTrainerId(0, 0);
    TEST_ASSERT(trainer >= FRONTIER_TRAINER_BRADY && trainer <= FRONTIER_TRAINER_JILL);
    trainer = GetRandomScaledFrontierTrainerId(3, 3);
    TEST_ASSERT(trainer >= FRONTIER_TRAINER_NORTON && trainer <= FRONTIER_TRAINER_JAZLYN);
    trainer = GetRandomScaledFrontierTrainerId(0, FRONTIER_STAGES_PER_CHALLENGE - 1);
    TEST_ASSERT(trainer >= FRONTIER_TRAINER_ERIK && trainer <= FRONTIER_TRAINER_CHLOE);
    trainer = GetRandomScaledFrontierTrainerId(8, 0);
    TEST_ASSERT(trainer >= FRONTIER_TRAINER_JAXON && trainer <= FRONTIER_TRAINER_GRETEL);
}

static void TestPartyRestorationAndRunCleanup(void)
{
    u16 hp;
    u16 item = ITEM_LEFTOVERS;

    TestResetFixture(0x7520);
    SetPyramidContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_LVL_50);
    CreateMon(&gPlayerParty[0], SPECIES_TREECKO, 50, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
    CreateMon(&gPlayerParty[1], SPECIES_TORCHIC, 50, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
    CreateMon(&gPlayerParty[2], SPECIES_MUDKIP, 50, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
    SetMonMoveSlot(&gPlayerParty[0], MOVE_TACKLE, 0);
    SetMonMoveSlot(&gPlayerParty[1], MOVE_GROWL, 0);
    SetMonMoveSlot(&gPlayerParty[2], MOVE_WATER_GUN, 0);
    gPlayerPartyCount = FRONTIER_PARTY_SIZE;
    SavePlayerParty();
    gSaveBlock2Ptr->frontier.selectedPartyMons[0] = 1;
    gSaveBlock2Ptr->frontier.selectedPartyMons[1] = 2;
    gSaveBlock2Ptr->frontier.selectedPartyMons[2] = 3;
    SetMonMoveSlot(&gPlayerParty[1], MOVE_THUNDERBOLT, 1);
    CallPyramid(BATTLE_PYRAMID_FUNC_RESTORE_PARTY);
    TEST_ASSERT_EQ(MOVE_TACKLE, GetMonData(&gSaveBlock1Ptr->playerParty[0], MON_DATA_MOVE1));
    TEST_ASSERT_EQ(MOVE_GROWL, GetMonData(&gSaveBlock1Ptr->playerParty[1], MON_DATA_MOVE1));
    TEST_ASSERT_EQ(MOVE_SKETCH, GetMonData(&gSaveBlock1Ptr->playerParty[1], MON_DATA_MOVE2));
    TEST_ASSERT_EQ(MOVE_WATER_GUN, GetMonData(&gSaveBlock1Ptr->playerParty[2], MON_DATA_MOVE1));
    TEST_ASSERT_EQ(1, gSaveBlock2Ptr->frontier.selectedPartyMons[0]);
    TEST_ASSERT_EQ(2, gSaveBlock2Ptr->frontier.selectedPartyMons[1]);
    TEST_ASSERT_EQ(3, gSaveBlock2Ptr->frontier.selectedPartyMons[2]);

    SetPyramidData(PYRAMID_DATA_WIN_STREAK, 9);
    SetPyramidData(PYRAMID_DATA_WIN_STREAK_ACTIVE, TRUE);
    gSaveBlock2Ptr->frontier.pyramidTrainerFlags = 0x12;
    SetMonData(&gPlayerParty[0], MON_DATA_HELD_ITEM, &item);
    hp = 1;
    SetMonData(&gPlayerParty[0], MON_DATA_HP, &hp);
    gBattleOutcome = B_OUTCOME_RAN;
    CallPyramid(BATTLE_PYRAMID_FUNC_RESTORE_PARTY);
    LoadPlayerParty();
    TEST_ASSERT_EQ(9, gSaveBlock2Ptr->frontier.pyramidWinStreaks[FRONTIER_LVL_50]);
    TEST_ASSERT(gSaveBlock2Ptr->frontier.winStreakActiveFlags & STREAK_PYRAMID_50);
    TEST_ASSERT_EQ(0x12, gSaveBlock2Ptr->frontier.pyramidTrainerFlags);

    gBattleOutcome = B_OUTCOME_LOST;
    gSaveBlock2Ptr->frontier.challengeStatus = CHALLENGE_STATUS_LOST;
    SetPyramidData(PYRAMID_DATA_TRAINER_FLAGS, 0xFF);
    CallPyramid(BATTLE_PYRAMID_FUNC_CLEAR_HELD_ITEMS);
    HealPlayerParty();
    SetPyramidData(PYRAMID_DATA_WIN_STREAK_ACTIVE, FALSE);
    gSpecialVar_0x8005 = 0;
    CallPyramid(BATTLE_PYRAMID_FUNC_SAVE);
    TEST_ASSERT_EQ(0, gSaveBlock2Ptr->frontier.challengeStatus);
    TEST_ASSERT_EQ(0xFF, gSaveBlock2Ptr->frontier.pyramidTrainerFlags);
    TEST_ASSERT_EQ(ITEM_NONE, GetMonData(&gPlayerParty[0], MON_DATA_HELD_ITEM));
    TEST_ASSERT_EQ(GetMonData(&gPlayerParty[0], MON_DATA_MAX_HP), GetMonData(&gPlayerParty[0], MON_DATA_HP));
    TEST_ASSERT(!(gSaveBlock2Ptr->frontier.winStreakActiveFlags & STREAK_PYRAMID_50));
}

static void TestBrainBoundariesAndLocation(void)
{
    TestResetFixture(0x7551);
    SetPyramidContext(FRONTIER_CHALLENGE_NORMAL, FRONTIER_LVL_50);
    VarSet(VAR_FRONTIER_BATTLE_MODE, FRONTIER_MODE_SINGLES);
    gSaveBlock2Ptr->frontier.pyramidWinStreaks[FRONTIER_LVL_50] = 20;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_NOT_READY, GetFrontierBrainStatus());
    gSaveBlock2Ptr->frontier.pyramidWinStreaks[FRONTIER_LVL_50] = 21;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_SILVER, GetFrontierBrainStatus());

    TestResetFixture(0x7552);
    SetPyramidContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_OPEN);
    VarSet(VAR_FRONTIER_BATTLE_MODE, FRONTIER_MODE_SINGLES);
    gSaveBlock1Ptr->frontierHardMode.pyramidWinStreaks[FRONTIER_LVL_OPEN] = 13;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_NOT_READY, GetFrontierBrainStatus());
    gSaveBlock1Ptr->frontierHardMode.pyramidWinStreaks[FRONTIER_LVL_OPEN] = 14;
    TEST_ASSERT_EQ(FRONTIER_BRAIN_SILVER, GetFrontierBrainStatus());

    gMapHeader.mapLayoutId = LAYOUT_BATTLE_FRONTIER_BATTLE_PYRAMID_FLOOR;
    TEST_ASSERT_EQ(PYRAMID_LOCATION_FLOOR, CurrentBattlePyramidLocation());
    TEST_ASSERT(InBattlePyramid_());
    gMapHeader.mapLayoutId = LAYOUT_BATTLE_FRONTIER_BATTLE_PYRAMID_TOP;
    TEST_ASSERT_EQ(PYRAMID_LOCATION_TOP, CurrentBattlePyramidLocation());
    gMapHeader.mapLayoutId = 0;
    TEST_ASSERT_EQ(PYRAMID_LOCATION_NONE, CurrentBattlePyramidLocation());
    TEST_ASSERT(!InBattlePyramid_());
}

static void TestPauseResumeAndSummitCompletion(void)
{
    TestResetFixture(0x7601);
    SetPyramidContext(FRONTIER_CHALLENGE_HARD, FRONTIER_LVL_OPEN);
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = FRONTIER_STAGES_PER_CHALLENGE - 1;
    gSpecialVar_0x8005 = CHALLENGE_STATUS_PAUSED;
    CallPyramid(BATTLE_PYRAMID_FUNC_SAVE);
    TEST_ASSERT_EQ(CHALLENGE_STATUS_PAUSED, gSaveBlock2Ptr->frontier.challengeStatus);
    TEST_ASSERT(gSaveBlock2Ptr->frontier.challengePaused);
    TEST_ASSERT_EQ(FRONTIER_STAGES_PER_CHALLENGE - 1, gSaveBlock2Ptr->frontier.curChallengeBattleNum);

    CallPyramid(BATTLE_PYRAMID_FUNC_UPDATE_STREAK);
    gSaveBlock2Ptr->frontier.curChallengeBattleNum++;
    TEST_ASSERT_EQ(FRONTIER_STAGES_PER_CHALLENGE, gSaveBlock2Ptr->frontier.curChallengeBattleNum);
    TEST_ASSERT_EQ(1, gSaveBlock1Ptr->frontierHardMode.pyramidWinStreaks[FRONTIER_LVL_OPEN]);
    SetPyramidData(PYRAMID_DATA_WIN_STREAK_ACTIVE, FALSE);
    TEST_ASSERT(!(gSaveBlock1Ptr->frontierHardMode.pyramidWinStreakActiveFlags & STREAK_PYRAMID_OPEN));
}

void RunTest(void)
{
    TestNormalAndHardInit();
    TestDataProgressionAndCleanup();
    TestFloorSeedTrainerAndItemEvents();
    TestLightRadiusAndHeldItems();
    TestWildBattleAndLevels();
    TestTrainerPoolRoundBoundaries();
    TestPartyRestorationAndRunCleanup();
    TestBrainBoundariesAndLocation();
    TestPauseResumeAndSummitCompletion();
}
