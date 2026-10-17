#include <stddef.h>
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
    // Pin legacy save boundaries independently of the new field definitions.
    TEST_ASSERT_EQ(0x3D88, sizeof(struct SaveBlock1));
    TEST_ASSERT_EQ(0xF44, sizeof(struct SaveBlock2));
    TEST_ASSERT_EQ(0x18, offsetof(struct SaveBlock2, pokedex));
    TEST_ASSERT_EQ(0x98, offsetof(struct SaveBlock2, localTimeOffset));
    TEST_ASSERT_EQ(0xAC, offsetof(struct SaveBlock2, encryptionKey));
    TEST_ASSERT_EQ(0x64C, offsetof(struct SaveBlock2, frontier));
    TEST_ASSERT_EQ(0x3598, offsetof(struct SaveBlock1, frontierHardMode));
    // Public fixture anonymization also depends on these shipped boundaries.
    TEST_ASSERT_EQ(0xEF9, offsetof(struct SaveBlock2, frontier.opponentNames));
    TEST_ASSERT_EQ(0xF09, offsetof(struct SaveBlock2, frontier.opponentTrainerIds));
    TEST_ASSERT_EQ(0xBD8, offsetof(struct SaveBlock2, frontier.towerInterview.opponentName));
    TEST_ASSERT_EQ(0x2E28, offsetof(struct SaveBlock1, oldMan));
    TEST_ASSERT_EQ(0x40, sizeof(OldMan));
    TEST_ASSERT_EQ(0x2E68, offsetof(struct SaveBlock1, dewfordTrends));
    TEST_ASSERT_EQ(0x3030, offsetof(struct SaveBlock1, daycare));
    TEST_ASSERT_EQ(0x8C, sizeof(struct DaycareMon));
    TEST_ASSERT_EQ(0x24, sizeof(struct Mail));
    TEST_ASSERT_EQ(0x50, sizeof(struct BoxPokemon));
    TEST_ASSERT_EQ(0x14, offsetof(struct BoxPokemon, otName));
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
