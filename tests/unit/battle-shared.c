#include "global.h"
#include "battle.h"
#include "battle_controllers.h"
#include "test.h"

void RunTest(void)
{
    TestResetFixture(0xB477);
    memset(gBattleBufferA, 0, sizeof(gBattleBufferA));
    gActiveBattler = 0;
    BtlController_EmitOneReturnValue(0, 0x4567);
    TEST_ASSERT_EQ(CONTROLLER_ONERETURNVALUE, gBattleBufferA[0][0]);
    TEST_ASSERT_EQ(0x67, gBattleBufferA[0][1]);
    TEST_ASSERT_EQ(0x45, gBattleBufferA[0][2]);

    gBattleTypeFlags = BATTLE_TYPE_FIRST_BATTLE;
    SetUpBattleVarsAndBirchZigzagoon();
    TEST_ASSERT_EQ(SPECIES_ZIGZAGOON, GetMonData(&gEnemyParty[0], MON_DATA_SPECIES));
    TEST_ASSERT_EQ(2, GetMonData(&gEnemyParty[0], MON_DATA_LEVEL));
}
