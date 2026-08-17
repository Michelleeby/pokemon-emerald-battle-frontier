#include "global.h"
#include "battle_dome.h"
#include "event_data.h"
#include "field_player_avatar.h"
#include "frontier_fixture.h"
#include "load_save.h"
#include "main.h"
#include "new_game.h"
#include "overworld.h"
#include "pokemon.h"
#include "random.h"
#include "save.h"
#include "string_util.h"
#include "constants/battle_frontier.h"
#include "constants/frontier_util.h"
#include "constants/items.h"
#include "constants/maps.h"
#include "constants/moves.h"
#include "constants/pokemon.h"
#include "constants/species.h"

#ifdef CAPTURE_FIXTURE

#define CAPTURE_FIXTURE_RUNNING 0x43415031
EWRAM_DATA volatile u32 gCaptureFixtureStatus = CAPTURE_FIXTURE_RUNNING;

static void SetCaptureMon(
    struct Pokemon *mon,
    u16 species,
    u8 nature,
    u16 item,
    const u8 evs[NUM_STATS],
    const u16 moves[MAX_MON_MOVES])
{
    u32 i;
    u8 abilityNum = 0;

    CreateMonWithNature(mon, species, 50, 31, nature);
    SetMonData(mon, MON_DATA_HELD_ITEM, &item);
    SetMonData(mon, MON_DATA_ABILITY_NUM, &abilityNum);
    for (i = 0; i < NUM_STATS; i++)
        SetMonData(mon, MON_DATA_HP_EV + i, &evs[i]);
    for (i = 0; i < MAX_MON_MOVES; i++)
        SetMonMoveSlot(mon, moves[i], i);
    CalculateMonStats(mon);
}

void Capture_CreateBattleDomeTuckerFixture(void)
{
    static const u8 sPlayerName[] = _("MAY");
    // Internal stat order is HP, Attack, Defense, Speed, Sp. Attack, Sp. Defense.
    static const u8 sBlazikenEvs[NUM_STATS] = {0, 4, 0, 252, 252, 0};
    static const u8 sLatiosEvs[NUM_STATS] = {4, 0, 0, 252, 252, 0};
    static const u8 sSwampertEvs[NUM_STATS] = {252, 0, 252, 0, 0, 4};
    static const u16 sBlazikenMoves[MAX_MON_MOVES] = {
        MOVE_OVERHEAT, MOVE_BRICK_BREAK, MOVE_ROCK_SLIDE, MOVE_QUICK_ATTACK
    };
    static const u16 sLatiosMoves[MAX_MON_MOVES] = {
        MOVE_PSYCHIC, MOVE_DRAGON_CLAW, MOVE_THUNDER, MOVE_CALM_MIND
    };
    static const u16 sSwampertMoves[MAX_MON_MOVES] = {
        MOVE_EARTHQUAKE, MOVE_HYDRO_PUMP, MOVE_TOXIC, MOVE_PROTECT
    };

    FrontierFixture_Init(sPlayerName, TRUE);

    ZeroPlayerPartyMons();
    SetCaptureMon(&gPlayerParty[0], SPECIES_BLAZIKEN, NATURE_NAIVE,
                  ITEM_LUM_BERRY, sBlazikenEvs, sBlazikenMoves);
    SetCaptureMon(&gPlayerParty[1], SPECIES_LATIOS, NATURE_TIMID,
                  ITEM_SOUL_DEW, sLatiosEvs, sLatiosMoves);
    SetCaptureMon(&gPlayerParty[2], SPECIES_SWAMPERT, NATURE_RELAXED,
                  ITEM_LEFTOVERS, sSwampertEvs, sSwampertMoves);
    gPlayerPartyCount = 3;

    // LEFT reserves a capture-only namespace for clean Frontier exterior
    // stills. Additional held buttons select the facility without changing
    // any of the fixture routes used by the animated captures below.
    if (JOY_HELD(DPAD_LEFT))
    {
        u16 map;
        s16 x;
        s16 y;

        if (JOY_HELD(A_BUTTON))
        {
            map = MAP_BATTLE_FRONTIER_OUTSIDE_WEST;
            x = 19;
            y = 18;
        }
        else if (JOY_HELD(B_BUTTON))
        {
            map = MAP_BATTLE_FRONTIER_OUTSIDE_WEST;
            x = 11;
            y = 39;
        }
        else if (JOY_HELD(L_BUTTON))
        {
            map = MAP_BATTLE_FRONTIER_OUTSIDE_EAST;
            x = 45;
            y = 57;
        }
        else if (JOY_HELD(R_BUTTON))
        {
            map = MAP_BATTLE_FRONTIER_OUTSIDE_EAST;
            x = 39;
            y = 30;
        }
        else if (JOY_HELD(START_BUTTON))
        {
            map = MAP_BATTLE_FRONTIER_OUTSIDE_WEST;
            x = 42;
            y = 28;
        }
        else if (JOY_HELD(SELECT_BUTTON))
        {
            map = MAP_BATTLE_FRONTIER_OUTSIDE_EAST;
            x = 58;
            y = 15;
        }
        else
        {
            map = MAP_BATTLE_FRONTIER_OUTSIDE_EAST;
            x = 16;
            y = 15;
        }
        FrontierFixture_LoadMapAndSave(map, x, y, &gCaptureFixtureStatus);
    }
    else if (JOY_HELD(L_BUTTON))
    {
        FrontierFixture_SeedActiveChallenge(
            FRONTIER_FACILITY_DOME, FRONTIER_CHALLENGE_NORMAL);
        gSaveBlock2Ptr->frontier.winStreakActiveFlags |= STREAK_DOME_SINGLES_50;
        gSaveBlock2Ptr->frontier
            .domeWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 4;
        gSaveBlock2Ptr->frontier.selectedPartyMons[3] = 0x0302;
        E2E_InitDomeTournament();
        FrontierFixture_LoadMapAndSave(
            MAP_BATTLE_FRONTIER_BATTLE_DOME_PRE_BATTLE_ROOM,
            5,
            7,
            &gCaptureFixtureStatus);
    }
    else if (JOY_HELD(START_BUTTON))
    {
        FrontierFixture_SeedActiveChallenge(
            FRONTIER_FACILITY_TOWER, FRONTIER_CHALLENGE_NORMAL);
        gSaveBlock2Ptr->frontier.winStreakActiveFlags |= STREAK_TOWER_SINGLES_50;
        gSaveBlock2Ptr->frontier
            .towerWinStreaks[FRONTIER_MODE_SINGLES][FRONTIER_LVL_50] = 33;
        FrontierFixture_LoadMapAndSave(
            MAP_BATTLE_FRONTIER_BATTLE_TOWER_BATTLE_ROOM,
            5,
            8,
            &gCaptureFixtureStatus);
    }
    else
    {
        FrontierFixture_LoadMapAndSave(
            MAP_BATTLE_FRONTIER_BATTLE_TOWER_LOBBY,
            6,
            6,
            &gCaptureFixtureStatus);
    }
}

#endif
