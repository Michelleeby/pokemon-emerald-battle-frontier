#include "global.h"
#include "load_save.h"
#include "pokemon.h"
#include "test.h"
#include "constants/moves.h"
#include "constants/pokemon.h"

void RunTest(void)
{
    u8 ppBonuses = 3;
    u8 currentPP = 11;

    TestResetFixture(0x5151);
    gPlayerPartyCount = 1;
    CreateMon(&gPlayerParty[0], SPECIES_TREECKO, 50, 31, FALSE, 0, OT_ID_PLAYER_ID, 7);
    SetMonMoveSlot(&gPlayerParty[0], MOVE_POUND, 0);
    SetMonData(&gPlayerParty[0], MON_DATA_PP_BONUSES, &ppBonuses);
    SetMonData(&gPlayerParty[0], MON_DATA_PP1, &currentPP);
    gSaveBlock2Ptr->frontier.frontierChallengeMode = 1;
    gSaveBlock1Ptr->frontierHardMode.towerWinStreaks[0][0] = 42;
    SavePlayerParty();

    gPlayerPartyCount = 0;
    ZeroPlayerPartyMons();
    LoadPlayerParty();
    TEST_ASSERT_EQ(1, gPlayerPartyCount);
    TEST_ASSERT_EQ(SPECIES_TREECKO, GetMonData(&gPlayerParty[0], MON_DATA_SPECIES));
    TEST_ASSERT_EQ(50, GetMonData(&gPlayerParty[0], MON_DATA_LEVEL));
    TEST_ASSERT_EQ(MOVE_POUND, GetMonData(&gPlayerParty[0], MON_DATA_MOVE1));
    TEST_ASSERT_EQ(ppBonuses, GetMonData(&gPlayerParty[0], MON_DATA_PP_BONUSES));
    TEST_ASSERT_EQ(currentPP, GetMonData(&gPlayerParty[0], MON_DATA_PP1));
    TEST_ASSERT_EQ(1, gSaveBlock2Ptr->frontier.frontierChallengeMode);
    TEST_ASSERT_EQ(42, gSaveBlock1Ptr->frontierHardMode.towerWinStreaks[0][0]);
}
