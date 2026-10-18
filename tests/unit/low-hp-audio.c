#include "global.h"
#include "audio_settings.h"
#include "battle.h"
#include "battle_gfx_sfx_util.h"
#include "constants/species.h"
#include "pokemon.h"
#include "test.h"

extern u16 gBattleTestLowHpSoundPlayCount;
extern u16 gBattleTestLowHpSoundStopCount;

static void ResetLowHpFixture(bool8 doubleBattle, bool8 enabled)
{
    TestResetFixture(0xA710);
    AudioSettings_InitDefaults();
    AudioSettings_SetLowHpBeepEnabled(enabled);

    gBattleTypeFlags = doubleBattle ? BATTLE_TYPE_DOUBLE : 0;
    gBattlersCount = doubleBattle ? MAX_BATTLERS_COUNT : 2;
    gBattlerPositions[B_BATTLER_0] = B_POSITION_PLAYER_LEFT;
    gBattlerPositions[B_BATTLER_1] = B_POSITION_OPPONENT_LEFT;
    gBattlerPositions[B_BATTLER_2] = B_POSITION_PLAYER_RIGHT;
    gBattlerPositions[B_BATTLER_3] = B_POSITION_OPPONENT_RIGHT;
    AllocateBattleSpritesData();

    gBattleTestLowHpSoundPlayCount = 0;
    gBattleTestLowHpSoundStopCount = 0;
}

static struct Pokemon *CreateLowHpMon(u8 partyId)
{
    u16 hp;

    CreateMon(&gPlayerParty[partyId], SPECIES_TREECKO, 50, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
    hp = GetMonData(&gPlayerParty[partyId], MON_DATA_MAX_HP) / 10;
    SetMonData(&gPlayerParty[partyId], MON_DATA_HP, &hp);
    return &gPlayerParty[partyId];
}

static void TestDisabledClearsStaleStateAndCanReenable(void)
{
    struct Pokemon *mon;

    ResetLowHpFixture(FALSE, FALSE);
    mon = CreateLowHpMon(0);
    gBattleSpritesDataPtr->battlerData[B_BATTLER_0].lowHpSong = 1;
    gBattleSpritesDataPtr->battlerData[B_BATTLER_2].lowHpSong = 1;

    HandleLowHpMusicChange(mon, B_BATTLER_0);
    TEST_ASSERT_EQ(0, gBattleSpritesDataPtr->battlerData[B_BATTLER_0].lowHpSong);
    TEST_ASSERT_EQ(0, gBattleSpritesDataPtr->battlerData[B_BATTLER_2].lowHpSong);
    TEST_ASSERT_EQ(0, gBattleTestLowHpSoundPlayCount);
    TEST_ASSERT_EQ(1, gBattleTestLowHpSoundStopCount);

    AudioSettings_SetLowHpBeepEnabled(TRUE);
    HandleLowHpMusicChange(mon, B_BATTLER_0);
    TEST_ASSERT_EQ(1, gBattleSpritesDataPtr->battlerData[B_BATTLER_0].lowHpSong);
    TEST_ASSERT_EQ(1, gBattleTestLowHpSoundPlayCount);

    FreeBattleSpritesData();
}

static void TestEnabledStartsAndStopsThroughHealing(void)
{
    struct Pokemon *mon;
    u16 hp;

    ResetLowHpFixture(FALSE, TRUE);
    mon = CreateLowHpMon(0);

    HandleLowHpMusicChange(mon, B_BATTLER_0);
    HandleLowHpMusicChange(mon, B_BATTLER_0);
    TEST_ASSERT_EQ(1, gBattleTestLowHpSoundPlayCount);

    hp = GetMonData(mon, MON_DATA_MAX_HP);
    SetMonData(mon, MON_DATA_HP, &hp);
    HandleLowHpMusicChange(mon, B_BATTLER_0);
    TEST_ASSERT_EQ(0, gBattleSpritesDataPtr->battlerData[B_BATTLER_0].lowHpSong);
    TEST_ASSERT_EQ(1, gBattleTestLowHpSoundStopCount);

    FreeBattleSpritesData();
}

static void TestDoubleBattleSharesAlertAndFaintingIsSafe(void)
{
    struct Pokemon *left;
    struct Pokemon *right;
    u16 hp;

    ResetLowHpFixture(TRUE, TRUE);
    left = CreateLowHpMon(0);
    right = CreateLowHpMon(1);

    HandleLowHpMusicChange(left, B_BATTLER_0);
    HandleLowHpMusicChange(right, B_BATTLER_2);
    TEST_ASSERT_EQ(1, gBattleTestLowHpSoundPlayCount);

    hp = GetMonData(left, MON_DATA_MAX_HP);
    SetMonData(left, MON_DATA_HP, &hp);
    HandleLowHpMusicChange(left, B_BATTLER_0);
    TEST_ASSERT_EQ(0, gBattleTestLowHpSoundStopCount);

    hp = 0;
    SetMonData(right, MON_DATA_HP, &hp);
    HandleLowHpMusicChange(right, B_BATTLER_2);
    TEST_ASSERT_EQ(0, gBattleSpritesDataPtr->battlerData[B_BATTLER_2].lowHpSong);
    TEST_ASSERT_EQ(1, gBattleTestLowHpSoundStopCount);
    TEST_ASSERT_EQ(1, gBattleTestLowHpSoundPlayCount);

    FreeBattleSpritesData();
}

void RunTest(void)
{
    TestDisabledClearsStaleStateAndCanReenable();
    TestEnabledStartsAndStopsThroughHealing();
    TestDoubleBattleSharesAlertAndFaintingIsSafe();
}
