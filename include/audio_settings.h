#ifndef GUARD_AUDIO_SETTINGS_H
#define GUARD_AUDIO_SETTINGS_H

#include "global.h"

enum AudioTrainerMusicMode
{
    AUDIO_TRAINER_MUSIC_DEFAULT,
    AUDIO_TRAINER_MUSIC_FIXED,
    AUDIO_TRAINER_MUSIC_RANDOM,
};

enum AudioMusicPool
{
    AUDIO_MUSIC_POOL_HOENN,
    AUDIO_MUSIC_POOL_HOENN_KANTO,
    AUDIO_MUSIC_POOL_ALL_BATTLES,
};

/* Stable IDs stored in SaveBlock2. Keep these values append-only. */
enum AudioTrainerTrack
{
    AUDIO_TRACK_HOENN_TRAINER = 1,
    AUDIO_TRACK_HOENN_GYM_LEADER,
    AUDIO_TRACK_HOENN_ELITE_FOUR,
    AUDIO_TRACK_HOENN_CHAMPION,
    AUDIO_TRACK_HOENN_RIVAL,
    AUDIO_TRACK_HOENN_AQUA_MAGMA,
    AUDIO_TRACK_HOENN_AQUA_MAGMA_LEADER,
    AUDIO_TRACK_HOENN_FRONTIER_BRAIN,
    AUDIO_TRACK_KANTO_TRAINER,
    AUDIO_TRACK_KANTO_GYM_LEADER,
    AUDIO_TRACK_KANTO_CHAMPION,
    AUDIO_TRACK_HOENN_WILD,
    AUDIO_TRACK_HOENN_REGI,
    AUDIO_TRACK_HOENN_KYOGRE_GROUDON,
    AUDIO_TRACK_HOENN_MEW,
    AUDIO_TRACK_KANTO_WILD,
    AUDIO_TRACK_KANTO_DEOXYS,
    AUDIO_TRACK_KANTO_MEWTWO,
    AUDIO_TRACK_KANTO_LEGEND,
    AUDIO_TRACK_COLOSSEUM_LEGEND_BEAST,
};

#define AUDIO_SETTINGS_MAGIC   0x4155
#define AUDIO_SETTINGS_VERSION 1

bool8 AudioSettings_IsLowHpBeepEnabled(void);
void AudioSettings_SetLowHpBeepEnabled(bool8 enabled);
void AudioSettings_InitDefaults(void);

u8 AudioSettings_GetTrainerMusicMode(void);
void AudioSettings_SetTrainerMusicMode(u8 mode);
u8 AudioSettings_GetMusicPool(void);
void AudioSettings_SetMusicPool(u8 pool);
u8 AudioSettings_GetTrainerTrack(void);
void AudioSettings_SetTrainerTrack(u8 track);

u8 AudioSettings_GetPoolTrackCount(u8 pool);
u8 AudioSettings_GetPoolTrack(u8 pool, u8 index);
u16 AudioSettings_GetTrackSong(u8 track);
bool8 AudioSettings_IsTrackInPool(u8 track, u8 pool);
const u8 *AudioSettings_GetTrackName(u8 track);

/* Battle integration. The caller supplies the vanilla result. */
void AudioSettings_ResetBattleMusic(void);
bool8 AudioSettings_IsTrainerMusicEligible(u16 originalSong);
u16 AudioSettings_GetTrainerMusic(u16 originalSong);

#endif // GUARD_AUDIO_SETTINGS_H
