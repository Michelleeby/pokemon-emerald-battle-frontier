#include "global.h"
#include "data.h"
#include "pokemon.h"
#include "random.h"
#include "team_lab.h"
#include "constants/abilities.h"
#include "constants/moves.h"
#include "constants/pokemon.h"
#include "constants/tms_hms.h"

#define EGG_MOVES_SPECIES_OFFSET 20000
#define EGG_MOVES_TERMINATOR 0xFFFF

#define TMHM_MOVE(name) MOVE_##name,
static const u16 sTmHmMoves[] =
{
    FOREACH_TMHM(TMHM_MOVE)
};
#undef TMHM_MOVE

extern const u16 gEggMoves[];
extern const struct Evolution gEvolutionTable[][EVOS_PER_MON];

#ifdef E2E_TESTING
EWRAM_DATA volatile u8 gE2ETeamLabRolledGender = MON_MALE;
EWRAM_DATA volatile u8 gE2ETeamLabCreatedGender = MON_MALE;
#endif

static bool8 IsEggMove(u16 species, u16 move)
{
    u32 i;
    u16 currentSpecies = SPECIES_NONE;

    for (i = 0; gEggMoves[i] != EGG_MOVES_TERMINATOR; i++)
    {
        if (gEggMoves[i] >= EGG_MOVES_SPECIES_OFFSET)
            currentSpecies = gEggMoves[i] - EGG_MOVES_SPECIES_OFFSET;
        else if (currentSpecies == species && gEggMoves[i] == move)
            return TRUE;
    }

    return FALSE;
}

static bool8 IsLevelUpMove(u16 species, u16 move)
{
    u8 i;

    for (i = 0; i < MAX_LEVEL_UP_MOVES && gLevelUpLearnsets[species][i] != LEVEL_UP_END; i++)
    {
        if ((gLevelUpLearnsets[species][i] & LEVEL_UP_MOVE_ID) == move)
            return TRUE;
    }

    return FALSE;
}

static bool8 IsTmHmMove(u16 species, u16 move)
{
    u8 i;

    for (i = 0; i < ARRAY_COUNT(sTmHmMoves); i++)
    {
        if (sTmHmMoves[i] == move)
            return CanSpeciesLearnTMHM(species, i) != 0;
    }

    return FALSE;
}

static bool8 IsMoveLegalForSpecies(u16 species, u16 move)
{
    return IsLevelUpMove(species, move)
        || IsTmHmMove(species, move)
        || CanSpeciesLearnTutorMove(species, move)
        || IsEggMove(species, move);
}

static u16 GetPreviousEvolution(u16 species)
{
    u16 previousSpecies;
    u8 evolution;

    for (previousSpecies = SPECIES_NONE + 1; previousSpecies < NUM_SPECIES; previousSpecies++)
    {
        for (evolution = 0; evolution < EVOS_PER_MON; evolution++)
        {
            if (gEvolutionTable[previousSpecies][evolution].targetSpecies == species)
                return previousSpecies;
        }
    }

    return SPECIES_NONE;
}

bool8 TeamLab_IsMoveLegal(u16 species, u16 move)
{
    u8 evolution;

    if (move == MOVE_NONE)
        return TRUE;

    if (species == SPECIES_NONE || species >= NUM_SPECIES || move >= MOVES_COUNT)
        return FALSE;

    for (evolution = 0; evolution < EVOS_PER_MON; evolution++)
    {
        if (IsMoveLegalForSpecies(species, move))
            return TRUE;

        species = GetPreviousEvolution(species);
        if (species == SPECIES_NONE)
            break;
    }

    return FALSE;
}

enum TeamLabValidationResult TeamLab_ValidateBuild(const struct TeamLabMonBuild *build)
{
    u8 i;
    u16 evTotal = 0;

    if (build->species == SPECIES_NONE || build->species >= NUM_SPECIES)
        return TEAM_LAB_INVALID_SPECIES;
    if (build->level == 0 || build->level > MAX_LEVEL)
        return TEAM_LAB_INVALID_LEVEL;
    if (build->nature >= NUM_NATURES)
        return TEAM_LAB_INVALID_NATURE;
    if (build->abilityNum > 1
     || (build->abilityNum != 0 && gSpeciesInfo[build->species].abilities[1] == ABILITY_NONE))
        return TEAM_LAB_INVALID_ABILITY;

    for (i = 0; i < NUM_STATS; i++)
    {
        if (build->ivs[i] > MAX_PER_STAT_IVS)
            return TEAM_LAB_INVALID_IV;
        if (build->evs[i] > TEAM_LAB_MAX_PER_STAT_EVS)
            return TEAM_LAB_INVALID_EV;
        evTotal += build->evs[i];
    }

    for (i = 0; i < MAX_MON_MOVES; i++)
        if (!TeamLab_IsMoveLegal(build->species, build->moves[i]))
            return TEAM_LAB_INVALID_MOVE;

    if (evTotal > MAX_TOTAL_EVS)
        return TEAM_LAB_INVALID_EV_TOTAL;

    return TEAM_LAB_VALID;
}

void TeamLab_MaxMonIvs(struct Pokemon *mon)
{
    u8 stat;
    u8 max = MAX_PER_STAT_IVS;

    for (stat = 0; stat < NUM_STATS; stat++)
        SetMonData(mon, MON_DATA_HP_IV + stat, &max);
}

void TeamLab_ZeroMonEvs(struct Pokemon *mon)
{
    u8 stat;
    u8 zero = 0;

    for (stat = 0; stat < NUM_STATS; stat++)
        SetMonData(mon, MON_DATA_HP_EV + stat, &zero);
}

static u32 GetTeamLabPersonality(struct Pokemon *mon, u8 nature, u8 abilityNum)
{
    u16 species = GetMonData(mon, MON_DATA_SPECIES);
    u32 otId = GetMonData(mon, MON_DATA_OT_ID);
    u8 gender = GetMonGender(mon);
    bool8 isShiny = IsMonShiny(mon);
    u32 personality;

    do
    {
        personality = Random32();
    } while (GetNatureFromPersonality(personality) != nature
          || (gSpeciesInfo[species].abilities[1] != ABILITY_NONE && (personality & 1) != abilityNum)
          || GetGenderFromSpeciesAndPersonality(species, personality) != gender
          || (IsShinyOtIdPersonality(otId, personality) != isShiny));

    return personality;
}

void TeamLab_SetNature(struct Pokemon *mon, u8 nature)
{
    u8 abilityNum = GetMonData(mon, MON_DATA_ABILITY_NUM);
    u32 personality = GetTeamLabPersonality(mon, nature, abilityNum);

    SetMonPersonality(mon, personality);
    CalculateMonStats(mon);
}

void TeamLab_SetAbilityNum(struct Pokemon *mon, u8 abilityNum)
{
    u8 nature = GetNature(mon);
    u32 personality = GetTeamLabPersonality(mon, nature, abilityNum);

    SetMonPersonality(mon, personality);
    SetMonData(mon, MON_DATA_ABILITY_NUM, &abilityNum);
    CalculateMonStats(mon);
}

void TeamLab_SetLevel(struct Pokemon *mon, u8 level)
{
    u16 species = GetMonData(mon, MON_DATA_SPECIES);
    u32 exp;

    if (level == 0)
        level = 1;
    else if (level > MAX_LEVEL)
        level = MAX_LEVEL;

    exp = gExperienceTables[gSpeciesInfo[species].growthRate][level];
    SetMonData(mon, MON_DATA_EXP, &exp);
    CalculateMonStats(mon);
}

void TeamLab_CreateMon(struct Pokemon *mon, const struct TeamLabMonBuild *build)
{
    u8 gender = GetGenderFromSpeciesAndPersonality(build->species, Random32());
    u32 personality;
    u8 i;

    do
    {
        personality = Random32();
    } while (GetNatureFromPersonality(personality) != build->nature
          || ((personality & 1) != build->abilityNum && gSpeciesInfo[build->species].abilities[1] != ABILITY_NONE)
          || GetGenderFromSpeciesAndPersonality(build->species, personality) != gender);

    CreateMon(mon, build->species, build->level, 0, TRUE, personality, OT_ID_PLAYER_ID, 0);
#ifdef E2E_TESTING
    gE2ETeamLabRolledGender = gender;
    gE2ETeamLabCreatedGender = GetMonGender(mon);
#endif
    SetMonData(mon, MON_DATA_HELD_ITEM, &build->heldItem);

    for (i = 0; i < NUM_STATS; i++)
    {
        SetMonData(mon, MON_DATA_HP_IV + i, &build->ivs[i]);
        SetMonData(mon, MON_DATA_HP_EV + i, &build->evs[i]);
    }
    for (i = 0; i < MAX_MON_MOVES; i++)
        SetMonMoveSlot(mon, build->moves[i], i);

    CalculateMonStats(mon);
}
