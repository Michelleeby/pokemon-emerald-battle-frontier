#ifndef GUARD_TEAM_LAB_H
#define GUARD_TEAM_LAB_H

#include "global.h"

#define TEAM_LAB_MAX_PER_STAT_EVS 252

enum TeamLabValidationResult
{
    TEAM_LAB_VALID,
    TEAM_LAB_INVALID_SPECIES,
    TEAM_LAB_INVALID_LEVEL,
    TEAM_LAB_INVALID_NATURE,
    TEAM_LAB_INVALID_ABILITY,
    TEAM_LAB_INVALID_IV,
    TEAM_LAB_INVALID_EV,
    TEAM_LAB_INVALID_EV_TOTAL,
    TEAM_LAB_INVALID_MOVE,
};

struct TeamLabMonBuild
{
    u16 species;
    u16 heldItem;
    u16 moves[MAX_MON_MOVES];
    u8 level;
    u8 nature;
    u8 abilityNum;
    u8 ivs[NUM_STATS];
    u8 evs[NUM_STATS];
    // Keep PP state after the existing fields: E2E tooling reads this build
    // directly from memory using the original offsets.
    u8 ppBonuses;
    u8 pp[MAX_MON_MOVES];
    bool8 hasPpData;
};

bool8 TeamLab_IsMoveLegal(u16 species, u16 move);
bool8 TeamLab_IsPpUpEligible(u16 move);
u8 TeamLab_GetPpBonusCount(u8 ppBonuses, u8 moveIndex);
u8 TeamLab_GetMoveMaxPp(u16 move, u8 ppBonuses, u8 moveIndex);
bool8 TeamLab_AdjustMovePp(u16 move, u8 *ppBonuses, u8 *currentPp, u8 moveIndex, s8 delta);
void TeamLab_MaxMovePp(u16 move, u8 *ppBonuses, u8 *currentPp, u8 moveIndex);
enum TeamLabValidationResult TeamLab_ValidateBuild(const struct TeamLabMonBuild *build);
void TeamLab_CreateMon(struct Pokemon *mon, const struct TeamLabMonBuild *build);
void TeamLab_MaxMonIvs(struct Pokemon *mon);
void TeamLab_ZeroMonEvs(struct Pokemon *mon);
void TeamLab_SetNature(struct Pokemon *mon, u8 nature);
void TeamLab_SetAbilityNum(struct Pokemon *mon, u8 abilityNum);
void TeamLab_SetLevel(struct Pokemon *mon, u8 level);
void ShowTeamLabScreen(u8 partyIndex, void (*returnCallback)(void));
void OpenTeamLabItemSelector(u16 initialItem, void (*returnCallback)(void));
bool8 GetTeamLabItemSelectionResult(u16 *item);

#endif // GUARD_TEAM_LAB_H
