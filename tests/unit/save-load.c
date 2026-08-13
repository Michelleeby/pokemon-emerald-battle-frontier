#include "global.h"
#include "load_save.h"
#include "test.h"
#include "constants/pokemon.h"

void RunTest(void)
{
    TestResetFixture(0x5151);
    gPlayerPartyCount = 1;
    CreateMon(&gPlayerParty[0], SPECIES_TREECKO, 50, 31, FALSE, 0, OT_ID_PLAYER_ID, 7);
    gSaveBlock2Ptr->frontier.frontierChallengeMode = 1;
    gSaveBlock1Ptr->frontierHardMode.towerWinStreaks[0][0] = 42;
    SavePlayerParty();

    gPlayerPartyCount = 0;
    ZeroPlayerPartyMons();
    LoadPlayerParty();
    TEST_ASSERT_EQ(1, gPlayerPartyCount);
    TEST_ASSERT_EQ(SPECIES_TREECKO, GetMonData(&gPlayerParty[0], MON_DATA_SPECIES));
    TEST_ASSERT_EQ(50, GetMonData(&gPlayerParty[0], MON_DATA_LEVEL));
    TEST_ASSERT_EQ(1, gSaveBlock2Ptr->frontier.frontierChallengeMode);
    TEST_ASSERT_EQ(42, gSaveBlock1Ptr->frontierHardMode.towerWinStreaks[0][0]);
}
