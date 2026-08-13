#include "global.h"
#include "load_save.h"
#include "main.h"
#include "new_game.h"
#include "overworld.h"
#include "field_player_avatar.h"
#include "pokemon.h"
#include "random.h"
#include "save.h"
#include "script.h"
#include "string_util.h"
#include "constants/maps.h"
#include "constants/moves.h"
#include "constants/pokemon.h"
#include "constants/species.h"

#ifdef E2E_TESTING

#define E2E_FIXTURE_RUNNING 0x45324531
#define E2E_FIXTURE_SAVED   0x45324532
#define E2E_FIXTURE_FAILED  0x453245FF

// Read by the host harness to stop fixture generation deterministically.
EWRAM_DATA volatile u32 gE2EFixtureStatus = E2E_FIXTURE_RUNNING;

static void E2E_SaveTowerLobbyFixture(void)
{
    PlayerFaceDirection(DIR_NORTH);
    if (TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK)
        gE2EFixtureStatus = E2E_FIXTURE_SAVED;
    else
        gE2EFixtureStatus = E2E_FIXTURE_FAILED;

    gFieldCallback = NULL;
    SetMainCallback2(NULL);
}

void E2E_CreateTowerLobbyFixture(void)
{
    static const u8 sPlayerName[] = _("E2ETEST");

    SetSaveBlocksPointers(0);
    Save_ResetSaveCounters();
    Sav2_ClearSetDefault();
    NewGameInitData();

    StringCopy(gSaveBlock2Ptr->playerName, sPlayerName);
    SetTrainerId(0x12345678, gSaveBlock2Ptr->playerTrainerId);
    SeedRng(0x1234);
    SeedRng2(0x5678);

    CreateMon(&gPlayerParty[0], SPECIES_TREECKO, 50, 31, TRUE,
              0x11111111, OT_ID_PLAYER_ID, 0);
    CreateMon(&gPlayerParty[1], SPECIES_TORCHIC, 50, 31, TRUE,
              0x22222222, OT_ID_PLAYER_ID, 0);
    CreateMon(&gPlayerParty[2], SPECIES_MUDKIP, 50, 31, TRUE,
              0x33333333, OT_ID_PLAYER_ID, 0);
    SetMonMoveSlot(&gPlayerParty[0], MOVE_LEAF_BLADE, 0);
    SetMonMoveSlot(&gPlayerParty[1], MOVE_FLAMETHROWER, 0);
    SetMonMoveSlot(&gPlayerParty[2], MOVE_SURF, 0);
    gPlayerParty[0].hp = gPlayerParty[0].maxHP = 999;
    gPlayerParty[0].attack = gPlayerParty[0].defense = 999;
    gPlayerParty[0].speed = gPlayerParty[0].spAttack = gPlayerParty[0].spDefense = 999;
    gPlayerPartyCount = 3;

    SetWarpDestination(MAP_GROUP(MAP_BATTLE_FRONTIER_BATTLE_TOWER_LOBBY),
                       MAP_NUM(MAP_BATTLE_FRONTIER_BATTLE_TOWER_LOBBY),
                       WARP_ID_NONE, 6, 6);
    WarpIntoMap();
    ResetInitialPlayerAvatarState();
    ScriptContext_Init();
    UnlockPlayerFieldControls();
    gFieldCallback = E2E_SaveTowerLobbyFixture;
    SetMainCallback2(CB2_LoadMap);
}

#endif
