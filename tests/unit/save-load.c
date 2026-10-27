#include <stddef.h>
#include "global.h"
#include "bike.h"
#include "field_player_avatar.h"
#include "field_specials.h"
#include "item.h"
#include "item_use.h"
#include "load_save.h"
#include "link.h"
#include "event_data.h"
#include "new_game.h"
#include "player_avatar.h"
#include "pokemon.h"
#include "test.h"
#include "constants/event_objects.h"
#include "constants/moves.h"
#include "constants/pokemon.h"
#include "constants/trainers.h"
#include "constants/vars.h"

static void TestAvatarSaveMappings(void)
{
    static const struct
    {
        u8 gender;
        u16 frontPic;
        u8 backPic;
        u8 overworld;
    } expected[] =
    {
        {MALE, TRAINER_PIC_BRENDAN, TRAINER_BACK_PIC_BRENDAN, OBJ_EVENT_GFX_BRENDAN_NORMAL},
        {FEMALE, TRAINER_PIC_MAY, TRAINER_BACK_PIC_MAY, OBJ_EVENT_GFX_MAY_NORMAL},
        {MALE, TRAINER_PIC_RED, TRAINER_BACK_PIC_RED, OBJ_EVENT_GFX_RED},
        {FEMALE, TRAINER_PIC_LEAF, TRAINER_BACK_PIC_LEAF, OBJ_EVENT_GFX_LEAF},
        {MALE, TRAINER_PIC_RS_BRENDAN, TRAINER_BACK_PIC_RUBY_SAPPHIRE_BRENDAN, OBJ_EVENT_GFX_LINK_RS_BRENDAN},
        {FEMALE, TRAINER_PIC_RS_MAY, TRAINER_BACK_PIC_RUBY_SAPPHIRE_MAY, OBJ_EVENT_GFX_LINK_RS_MAY},
        {MALE, TRAINER_PIC_WALLY, TRAINER_BACK_PIC_WALLY, OBJ_EVENT_GFX_WALLY},
        {MALE, TRAINER_PIC_STEVEN, TRAINER_BACK_PIC_STEVEN, OBJ_EVENT_GFX_STEVEN},
    };
    static const u16 invalidIds[] = {8, 255, 256, 257, 0xFFFF};
    u32 i;

    TEST_ASSERT_EQ(8, PLAYER_AVATAR_COUNT);
    for (i = 0; i < ARRAY_COUNT(expected); i++)
    {
        VarSet(VAR_PLAYER_AVATAR_STYLE, i);
        TEST_ASSERT_EQ(i, GetPlayerAvatarId());
        TEST_ASSERT_EQ(expected[i].gender, GetPlayerAvatarStyle());
        TEST_ASSERT_EQ(expected[i].gender, GetPlayerAvatarGender(i));
        TEST_ASSERT_EQ(expected[i].frontPic, GetPlayerAvatarFrontPicId(i));
        TEST_ASSERT_EQ(expected[i].backPic, GetPlayerAvatarBackPicId(i));
        TEST_ASSERT_EQ(expected[i].overworld, GetPlayerAvatarOverworldGraphicsId(i));
        TEST_ASSERT_EQ(expected[i].overworld, GetPlayerAvatarObjectGraphicsId());
    }

    // Both original stored choices must retain their appearance; values with
    // high bits set must be rejected before any narrowing to an 8-bit ID.
    for (i = 0; i < ARRAY_COUNT(invalidIds); i++)
    {
        VarSet(VAR_PLAYER_AVATAR_STYLE, invalidIds[i]);
        TEST_ASSERT_EQ(FEMALE, NormalizePlayerAvatarId(invalidIds[i]));
        TEST_ASSERT_EQ(FEMALE, GetPlayerAvatarId());
        TEST_ASSERT_EQ(FEMALE, GetPlayerAvatarStyle());
    }
    TEST_ASSERT_EQ(TRAINER_PIC_MAY, GetPlayerAvatarFrontPicId(0xFF));
    TEST_ASSERT_EQ(TRAINER_BACK_PIC_MAY, GetPlayerAvatarBackPicId(0xFF));
    TEST_ASSERT_EQ(OBJ_EVENT_GFX_MAY_NORMAL, GetPlayerAvatarOverworldGraphicsId(0xFF));
}

static void TestLinkAvatarBounds(void)
{
    struct LinkPlayer player = {0};
    u32 i;

    player.version = VERSION_EMERALD;
    for (i = 0; i < PLAYER_AVATAR_COUNT; i++)
    {
        player.neverRead = EncodePlayerAvatarId(i);
        TEST_ASSERT(IsEncodedPlayerAvatarId(player.neverRead));
        TEST_ASSERT_EQ(i, GetLinkPlayerAvatarId(&player));
        TEST_ASSERT_EQ(GetPlayerAvatarFrontPicId(i), GetLinkPlayerFrontPicId(&player));
        TEST_ASSERT_EQ(GetPlayerAvatarBackPicId(i), GetLinkPlayerBackPicId(&player));
    }
    TEST_ASSERT_EQ(EncodePlayerAvatarId(FEMALE), EncodePlayerAvatarId(0xFF));

    // Exhaust the untrusted metadata byte: only this ROM's eight tagged values
    // may change appearance, and all other values must retain vanilla gender.
    for (i = 0; i <= 0xFF; i++)
    {
        player.neverRead = i;
        player.gender = MALE;
        if (!IsEncodedPlayerAvatarId(i))
        {
            TEST_ASSERT_EQ(TRAINER_BACK_PIC_BRENDAN, GetLinkPlayerAvatarId(&player));
            player.gender = 0xFF;
            TEST_ASSERT_EQ(TRAINER_BACK_PIC_MAY, GetLinkPlayerAvatarId(&player));
        }
    }

    // A foreign game's unrelated byte must never opt into Emerald's encoding.
    player.neverRead = EncodePlayerAvatarId(TRAINER_BACK_PIC_STEVEN);
    player.version = VERSION_FIRE_RED;
    player.gender = MALE;
    TEST_ASSERT_EQ(TRAINER_BACK_PIC_RED, GetLinkPlayerAvatarId(&player));
    player.version = VERSION_LEAF_GREEN;
    player.gender = FEMALE;
    TEST_ASSERT_EQ(TRAINER_BACK_PIC_LEAF, GetLinkPlayerAvatarId(&player));
    player.version = VERSION_RUBY;
    player.gender = MALE;
    TEST_ASSERT_EQ(TRAINER_BACK_PIC_RUBY_SAPPHIRE_BRENDAN, GetLinkPlayerAvatarId(&player));
    player.version = VERSION_SAPPHIRE;
    player.gender = 0xFF;
    TEST_ASSERT_EQ(TRAINER_BACK_PIC_RUBY_SAPPHIRE_MAY, GetLinkPlayerAvatarId(&player));
}

extern void SetBattleTowerLinkPlayerGfx(void);

static void TestTowerStandInMappings(void)
{
    u32 i;
    for (i = 0; i < PLAYER_AVATAR_COUNT; i++)
    {
        gLinkPlayers[0].version = VERSION_EMERALD;
        gLinkPlayers[0].neverRead = EncodePlayerAvatarId(i);
        gLinkPlayers[0].gender = GetPlayerAvatarGender(i);
        gLinkPlayers[1].version = VERSION_EMERALD;
        gLinkPlayers[1].neverRead = EncodePlayerAvatarId(PLAYER_AVATAR_COUNT - i - 1);
        gLinkPlayers[1].gender = GetPlayerAvatarGender(PLAYER_AVATAR_COUNT - i - 1);
        SetBattleTowerLinkPlayerGfx();
        TEST_ASSERT_EQ(GetPlayerAvatarOverworldGraphicsId(i), VarGet(VAR_OBJ_GFX_ID_F));
        TEST_ASSERT_EQ(GetPlayerAvatarOverworldGraphicsId(PLAYER_AVATAR_COUNT - i - 1), VarGet(VAR_OBJ_GFX_ID_E));
    }

    // Legacy peers still use the shared game's normal avatar mapping.
    gLinkPlayers[0].neverRead = 0;
    gLinkPlayers[0].gender = FEMALE;
    gLinkPlayers[1].neverRead = 0;
    gLinkPlayers[1].gender = MALE;
    SetBattleTowerLinkPlayerGfx();
    TEST_ASSERT_EQ(OBJ_EVENT_GFX_MAY_NORMAL, VarGet(VAR_OBJ_GFX_ID_F));
    TEST_ASSERT_EQ(OBJ_EVENT_GFX_BRENDAN_NORMAL, VarGet(VAR_OBJ_GFX_ID_E));
}

static void TestAvatarTravelRestrictions(void)
{
    static const u16 unavailableItems[] = {
        ITEM_MACH_BIKE, ITEM_ACRO_BIKE, ITEM_OLD_ROD, ITEM_GOOD_ROD,
        ITEM_SUPER_ROD, ITEM_WAILMER_PAIL,
    };
    static const u16 unavailableFieldMoves[] = {
        MOVE_CUT, MOVE_FLASH, MOVE_ROCK_SMASH, MOVE_STRENGTH, MOVE_SURF,
        MOVE_FLY, MOVE_DIVE, MOVE_WATERFALL, MOVE_TELEPORT, MOVE_DIG,
        MOVE_SECRET_POWER, MOVE_SWEET_SCENT,
    };
    u32 i, state;

    for (i = 0; i < ARRAY_COUNT(unavailableItems); i++)
        TEST_ASSERT(GetItemFieldFunc(unavailableItems[i]) == ItemUseOutOfBattle_CannotUse);
    for (i = 0; i < ARRAY_COUNT(unavailableFieldMoves); i++)
        TEST_ASSERT(!IsPlayerFieldMoveAllowed(unavailableFieldMoves[i]));
    TEST_ASSERT(IsPlayerFieldMoveAllowed(MOVE_MILK_DRINK));
    TEST_ASSERT(IsPlayerFieldMoveAllowed(MOVE_SOFT_BOILED));
    TEST_ASSERT(!IsPlayerFieldMoveAllowed(MOVE_NONE));
    TEST_ASSERT(!IsPlayerFieldMoveAllowed(0xFFFF));

    for (i = 0; i < PLAYER_AVATAR_COUNT; i++)
    {
        VarSet(VAR_PLAYER_AVATAR_STYLE, i);
        gPlayerAvatar.gender = GetPlayerAvatarGender(i);
        for (state = PLAYER_AVATAR_STATE_NORMAL; state <= PLAYER_AVATAR_STATE_WATERING; state++)
            TEST_ASSERT_EQ(GetPlayerAvatarOverworldGraphicsId(i), GetPlayerAvatarGraphicsIdByStateId(state));
        gPlayerAvatar.flags = PLAYER_AVATAR_FLAG_CONTROLLABLE;
        SetPlayerAvatarStateMask(PLAYER_AVATAR_FLAG_MACH_BIKE | PLAYER_AVATAR_FLAG_ACRO_BIKE
                                | PLAYER_AVATAR_FLAG_SURFING | PLAYER_AVATAR_FLAG_UNDERWATER);
        TEST_ASSERT_EQ(PLAYER_AVATAR_FLAG_CONTROLLABLE | PLAYER_AVATAR_FLAG_ON_FOOT, gPlayerAvatar.flags);
    }
    gPlayerAvatar.flags = PLAYER_AVATAR_FLAG_ON_FOOT | PLAYER_AVATAR_FLAG_DASH;
    gPlayerAvatar.bikeSpeed = 0;
    TEST_ASSERT_EQ(PLAYER_SPEED_NORMAL, GetPlayerSpeed());
    gPlayerAvatar.bikeSpeed = 1;
    TEST_ASSERT_EQ(PLAYER_SPEED_FAST, GetPlayerSpeed());
    gPlayerAvatar.bikeSpeed = 3;
    TEST_ASSERT_EQ(PLAYER_SPEED_FASTEST, GetPlayerSpeed());
    gPlayerAvatar.flags = PLAYER_AVATAR_FLAG_ON_FOOT;
    TEST_ASSERT_EQ(PLAYER_SPEED_NORMAL, GetPlayerSpeed());
}

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
    TestAvatarSaveMappings();
    TestLinkAvatarBounds();
    TestAvatarTravelRestrictions();
    TestTowerStandInMappings();
}
