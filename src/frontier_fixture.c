#include "global.h"
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
#include "script.h"
#include "string_util.h"
#include "constants/battle_frontier.h"

#ifdef E2E_TESTING

#define FIXTURE_SAVED  0x45324532
#define FIXTURE_FAILED 0x453245FF

static volatile u32 *sFixtureStatus;

static void SaveFrontierFixture(void)
{
    PlayerFaceDirection(DIR_NORTH);
    if (TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK)
        *sFixtureStatus = FIXTURE_SAVED;
    else
        *sFixtureStatus = FIXTURE_FAILED;

    gFieldCallback = NULL;
    SetMainCallback2(NULL);
}

void FrontierFixture_Init(const u8 *playerName, bool8 femaleAvatar)
{
    SetSaveBlocksPointers(0);
    Save_ResetSaveCounters();
    Sav2_ClearSetDefault();
    NewGameInitData();

    if (femaleAvatar)
        VarSet(VAR_PLAYER_AVATAR_STYLE, FEMALE);
    StringCopy(gSaveBlock2Ptr->playerName, playerName);
    SetTrainerId(0x12345678, gSaveBlock2Ptr->playerTrainerId);
    SeedRng(0x1234);
    SeedRng2(0x5678);
}

void FrontierFixture_SeedActiveChallenge(u8 facility, u8 challengeMode)
{
    u32 i;

    VarSet(VAR_FRONTIER_FACILITY, facility);
    VarSet(VAR_FRONTIER_BATTLE_MODE, FRONTIER_MODE_SINGLES);
    gSaveBlock2Ptr->frontier.challengeStatus = CHALLENGE_STATUS_SAVING;
    gSaveBlock2Ptr->frontier.lvlMode = FRONTIER_LVL_50;
    gSaveBlock2Ptr->frontier.challengePaused = FALSE;
    gSaveBlock2Ptr->frontier.disableRecordBattle = FALSE;
    gSaveBlock2Ptr->frontier.curChallengeBattleNum = 0;
    gSaveBlock2Ptr->frontier.frontierChallengeMode = challengeMode;
    for (i = 0; i < FRONTIER_PARTY_SIZE; i++)
        gSaveBlock2Ptr->frontier.selectedPartyMons[i] = i + 1;
    SavePlayerParty();
}

void FrontierFixture_LoadMapAndSave(u16 map, s16 x, s16 y, volatile u32 *status)
{
    sFixtureStatus = status;
    SetWarpDestination(MAP_GROUP(map), MAP_NUM(map), WARP_ID_NONE, x, y);
    WarpIntoMap();
    ResetInitialPlayerAvatarState();
    ScriptContext_Init();
    UnlockPlayerFieldControls();
    gFieldCallback = SaveFrontierFixture;
    SetMainCallback2(CB2_LoadMap);
}

#endif
