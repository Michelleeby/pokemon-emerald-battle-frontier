#include "global.h"
#include "pokemon.h"
#include "team_lab.h"
#include "test.h"
#include "constants/moves.h"
#include "constants/pokemon.h"

static struct TeamLabMonBuild ValidBuild(void)
{
    struct TeamLabMonBuild build = {0};
    u8 i;

    build.species = SPECIES_BULBASAUR;
    build.level = 50;
    build.nature = NATURE_MODEST;
    build.abilityNum = 0;
    build.moves[0] = MOVE_TACKLE;
    for (i = 0; i < NUM_STATS; i++)
        build.ivs[i] = 31;
    build.evs[STAT_HP] = 4;
    build.evs[STAT_SPATK] = 252;
    build.evs[STAT_SPEED] = 252;
    return build;
}

void RunTest(void)
{
    struct TeamLabMonBuild build;
    struct Pokemon mon;
    struct Pokemon repeat;
    u32 personality;
    u8 i;

    TestResetFixture(0xCAFE);
    build = ValidBuild();
    TEST_ASSERT_EQ(TEAM_LAB_VALID, TeamLab_ValidateBuild(&build));

    build.species = SPECIES_NONE;
    TEST_ASSERT_EQ(TEAM_LAB_INVALID_SPECIES, TeamLab_ValidateBuild(&build));
    build = ValidBuild();
    build.level = 0;
    TEST_ASSERT_EQ(TEAM_LAB_INVALID_LEVEL, TeamLab_ValidateBuild(&build));
    build = ValidBuild();
    build.nature = NUM_NATURES;
    TEST_ASSERT_EQ(TEAM_LAB_INVALID_NATURE, TeamLab_ValidateBuild(&build));
    build = ValidBuild();
    build.ivs[STAT_HP] = 32;
    TEST_ASSERT_EQ(TEAM_LAB_INVALID_IV, TeamLab_ValidateBuild(&build));
    build = ValidBuild();
    build.evs[STAT_HP] = 253;
    TEST_ASSERT_EQ(TEAM_LAB_INVALID_EV, TeamLab_ValidateBuild(&build));
    build = ValidBuild();
    build.evs[STAT_ATK] = 4;
    TEST_ASSERT_EQ(TEAM_LAB_INVALID_EV_TOTAL, TeamLab_ValidateBuild(&build));
    build = ValidBuild();
    build.moves[0] = MOVES_COUNT;
    TEST_ASSERT_EQ(TEAM_LAB_INVALID_MOVE, TeamLab_ValidateBuild(&build));

    TEST_ASSERT(TeamLab_IsMoveLegal(SPECIES_IVYSAUR, MOVE_TACKLE));
    TEST_ASSERT(!TeamLab_IsMoveLegal(SPECIES_NONE, MOVE_TACKLE));

    build = ValidBuild();
    TeamLab_CreateMon(&mon, &build);
    TEST_ASSERT_EQ(SPECIES_BULBASAUR, GetMonData(&mon, MON_DATA_SPECIES));
    TEST_ASSERT_EQ(50, GetMonData(&mon, MON_DATA_LEVEL));
    TEST_ASSERT_EQ(NATURE_MODEST, GetNature(&mon));
    personality = GetMonData(&mon, MON_DATA_PERSONALITY);
    TestResetFixture(0xCAFE);
    TeamLab_CreateMon(&repeat, &build);
    TEST_ASSERT_EQ(personality, GetMonData(&repeat, MON_DATA_PERSONALITY));
    for (i = 0; i < NUM_STATS; i++)
        TEST_ASSERT_EQ(build.ivs[i], GetMonData(&mon, MON_DATA_HP_IV + i));

    TeamLab_ZeroMonEvs(&mon);
    for (i = 0; i < NUM_STATS; i++)
        TEST_ASSERT_EQ(0, GetMonData(&mon, MON_DATA_HP_EV + i));
    TeamLab_MaxMonIvs(&mon);
    for (i = 0; i < NUM_STATS; i++)
        TEST_ASSERT_EQ(31, GetMonData(&mon, MON_DATA_HP_IV + i));
}
