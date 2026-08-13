#include "global.h"
#include "load_save.h"
#include "malloc.h"
#include "random.h"
#include "test.h"

void TestResetFixture(u16 seed)
{
    gSaveBlock1Ptr = &gSaveblock1.block;
    gSaveBlock2Ptr = &gSaveblock2.block;
    gPokemonStoragePtr = &gPokemonStorage.block;
    memset(gSaveBlock1Ptr, 0, sizeof(*gSaveBlock1Ptr));
    memset(gSaveBlock2Ptr, 0, sizeof(*gSaveBlock2Ptr));
    memset(gPokemonStoragePtr, 0, sizeof(*gPokemonStoragePtr));
    memset(gPlayerParty, 0, sizeof(gPlayerParty));
    memset(gEnemyParty, 0, sizeof(gEnemyParty));
    gPlayerPartyCount = 0;
    InitHeap(gHeap, HEAP_SIZE);
    SeedRng(seed);
    SeedRng2(seed);
}
