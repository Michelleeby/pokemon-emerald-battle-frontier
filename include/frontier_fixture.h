#ifndef GUARD_FRONTIER_FIXTURE_H
#define GUARD_FRONTIER_FIXTURE_H

#ifdef E2E_TESTING

void FrontierFixture_Init(const u8 *playerName, bool8 femaleAvatar);
void FrontierFixture_SeedActiveChallenge(u8 facility, u8 challengeMode);
void FrontierFixture_LoadMapAndSave(u16 map, s16 x, s16 y, volatile u32 *status);

#endif

#endif // GUARD_FRONTIER_FIXTURE_H
