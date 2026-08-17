#ifndef GUARD_BATTLE_DOME_H
#define GUARD_BATTLE_DOME_H

extern u32 gPlayerPartyLostHP;

int GetDomeTrainerSelectedMons(u16 tournamentTrainerId);
int TrainerIdToDomeTournamentId(u16 trainerId);

#ifdef E2E_TESTING
void E2E_InitDomeTournament(void);
#endif

#endif // GUARD_BATTLE_DOME_H
