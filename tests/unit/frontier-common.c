#include "global.h"
#include "frontier_util.h"
#include "test.h"
#include "constants/battle_frontier.h"
#include "constants/frontier_util.h"

void RunTest(void)
{
    u16 *normal;
    u16 *hard;

    TestResetFixture(0x1234);
    gSaveBlock2Ptr->frontier.frontierChallengeMode = FRONTIER_CHALLENGE_NORMAL;
    normal = FrontierTest_GetTowerWinStreak(FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    *normal = 17;
    gSaveBlock2Ptr->frontier.frontierChallengeMode = FRONTIER_CHALLENGE_HARD;
    hard = FrontierTest_GetTowerWinStreak(FRONTIER_MODE_SINGLES, FRONTIER_LVL_50);
    TEST_ASSERT(normal != hard);
    TEST_ASSERT_EQ(0, *hard);
    *hard = 29;
    TEST_ASSERT_EQ(17, *normal);

    TEST_ASSERT(FrontierTest_GetTowerRecordWinStreak(FRONTIER_MODE_DOUBLES, FRONTIER_LVL_OPEN)
             == &gSaveBlock1Ptr->frontierHardMode.towerRecordWinStreaks[FRONTIER_MODE_DOUBLES][FRONTIER_LVL_OPEN]);
    TEST_ASSERT(FrontierTest_GetTowerWinStreak(FRONTIER_MODE_MULTIS, FRONTIER_LVL_50)
             == &gSaveBlock2Ptr->frontier.towerWinStreaks[FRONTIER_MODE_MULTIS][FRONTIER_LVL_50]);

    gSaveBlock2Ptr->frontier.frontierChallengeMode = FRONTIER_CHALLENGE_NORMAL;
    TEST_ASSERT_EQ(70, FrontierTest_GetBrainStreakAppearance(FRONTIER_FACILITY_TOWER, 1));
    gSaveBlock2Ptr->frontier.frontierChallengeMode = FRONTIER_CHALLENGE_HARD;
    TEST_ASSERT_EQ(35, FrontierTest_GetBrainStreakAppearance(FRONTIER_FACILITY_TOWER, 1));
    TEST_ASSERT_EQ(14, FrontierTest_GetBrainStreakAppearance(FRONTIER_FACILITY_ARENA, 0));
}
