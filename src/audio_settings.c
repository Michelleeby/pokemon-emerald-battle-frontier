#include "global.h"
#include "audio_settings.h"
#include "constants/songs.h"

static const u8 sTextAudioHoennTrainer[] = _("HOENN TRAINER");
static const u8 sTextAudioHoennGym[] = _("HOENN GYM");
static const u8 sTextAudioHoennElite[] = _("HOENN ELITE");
static const u8 sTextAudioHoennChampion[] = _("HOENN CHAMPION");
static const u8 sTextAudioHoennRival[] = _("HOENN RIVAL");
static const u8 sTextAudioHoennTeams[] = _("HOENN TEAMS");
static const u8 sTextAudioHoennTeamLeader[] = _("TEAM LEADER");
static const u8 sTextAudioHoennFrontier[] = _("FRONTIER BRAIN");
static const u8 sTextAudioKantoTrainer[] = _("KANTO TRAINER");
static const u8 sTextAudioKantoGym[] = _("KANTO GYM");
static const u8 sTextAudioKantoChampion[] = _("KANTO CHAMPION");
static const u8 sTextAudioHoennWild[] = _("HOENN WILD");
static const u8 sTextAudioHoennRegi[] = _("HOENN REGI");
static const u8 sTextAudioHoennLegend[] = _("HOENN LEGEND");
static const u8 sTextAudioHoennMew[] = _("HOENN MEW");
static const u8 sTextAudioKantoWild[] = _("KANTO WILD");
static const u8 sTextAudioKantoDeoxys[] = _("KANTO DEOXYS");
static const u8 sTextAudioKantoMewtwo[] = _("KANTO MEWTWO");
static const u8 sTextAudioKantoLegend[] = _("KANTO LEGEND");
static const u8 sTextAudioLegendBeasts[] = _("LEGEND BEASTS");

struct AudioTrackInfo
{
    u8 id;
    u16 song;
    const u8 *name;
};

static const struct AudioTrackInfo sAudioTracks[] =
{
    {AUDIO_TRACK_HOENN_TRAINER, MUS_VS_TRAINER, sTextAudioHoennTrainer},
    {AUDIO_TRACK_HOENN_GYM_LEADER, MUS_VS_GYM_LEADER, sTextAudioHoennGym},
    {AUDIO_TRACK_HOENN_ELITE_FOUR, MUS_VS_ELITE_FOUR, sTextAudioHoennElite},
    {AUDIO_TRACK_HOENN_CHAMPION, MUS_VS_CHAMPION, sTextAudioHoennChampion},
    {AUDIO_TRACK_HOENN_RIVAL, MUS_VS_RIVAL, sTextAudioHoennRival},
    {AUDIO_TRACK_HOENN_AQUA_MAGMA, MUS_VS_AQUA_MAGMA, sTextAudioHoennTeams},
    {AUDIO_TRACK_HOENN_AQUA_MAGMA_LEADER, MUS_VS_AQUA_MAGMA_LEADER, sTextAudioHoennTeamLeader},
    {AUDIO_TRACK_HOENN_FRONTIER_BRAIN, MUS_VS_FRONTIER_BRAIN, sTextAudioHoennFrontier},
    {AUDIO_TRACK_KANTO_TRAINER, MUS_RG_VS_TRAINER, sTextAudioKantoTrainer},
    {AUDIO_TRACK_KANTO_GYM_LEADER, MUS_RG_VS_GYM_LEADER, sTextAudioKantoGym},
    {AUDIO_TRACK_KANTO_CHAMPION, MUS_RG_VS_CHAMPION, sTextAudioKantoChampion},
    {AUDIO_TRACK_HOENN_WILD, MUS_VS_WILD, sTextAudioHoennWild},
    {AUDIO_TRACK_HOENN_REGI, MUS_VS_REGI, sTextAudioHoennRegi},
    {AUDIO_TRACK_HOENN_KYOGRE_GROUDON, MUS_VS_KYOGRE_GROUDON, sTextAudioHoennLegend},
    {AUDIO_TRACK_HOENN_MEW, MUS_VS_MEW, sTextAudioHoennMew},
    {AUDIO_TRACK_KANTO_WILD, MUS_RG_VS_WILD, sTextAudioKantoWild},
    {AUDIO_TRACK_KANTO_DEOXYS, MUS_RG_VS_DEOXYS, sTextAudioKantoDeoxys},
    {AUDIO_TRACK_KANTO_MEWTWO, MUS_RG_VS_MEWTWO, sTextAudioKantoMewtwo},
    {AUDIO_TRACK_KANTO_LEGEND, MUS_RG_VS_LEGEND, sTextAudioKantoLegend},
    {AUDIO_TRACK_COLOSSEUM_LEGEND_BEAST, MUS_C_VS_LEGEND_BEAST, sTextAudioLegendBeasts},
};

static const u8 sHoennPool[] =
{
    AUDIO_TRACK_HOENN_TRAINER, AUDIO_TRACK_HOENN_GYM_LEADER,
    AUDIO_TRACK_HOENN_ELITE_FOUR, AUDIO_TRACK_HOENN_CHAMPION,
    AUDIO_TRACK_HOENN_RIVAL, AUDIO_TRACK_HOENN_AQUA_MAGMA,
    AUDIO_TRACK_HOENN_AQUA_MAGMA_LEADER, AUDIO_TRACK_HOENN_FRONTIER_BRAIN,
};

static const u8 sKantoPool[] =
{
    AUDIO_TRACK_KANTO_TRAINER, AUDIO_TRACK_KANTO_GYM_LEADER, AUDIO_TRACK_KANTO_CHAMPION,
};

static const u8 sAllBattlesPool[] =
{
    AUDIO_TRACK_HOENN_WILD, AUDIO_TRACK_HOENN_REGI, AUDIO_TRACK_HOENN_KYOGRE_GROUDON,
    AUDIO_TRACK_HOENN_MEW, AUDIO_TRACK_KANTO_WILD, AUDIO_TRACK_KANTO_DEOXYS,
    AUDIO_TRACK_KANTO_MEWTWO, AUDIO_TRACK_KANTO_LEGEND, AUDIO_TRACK_COLOSSEUM_LEGEND_BEAST,
};

static u32 sAudioRng;
static u8 sPreviousRandomTrack;
static u16 sCachedBattleTrack;
static bool8 sBattleTrackCached;

static void AudioSettings_Normalize(void)
{
    struct AudioSettingsSave *settings = &gSaveBlock2Ptr->audioSettings;

    if (settings->magic != AUDIO_SETTINGS_MAGIC || settings->version != AUDIO_SETTINGS_VERSION)
    {
        AudioSettings_InitDefaults();
        return;
    }

    if (settings->lowHpBeepEnabled > 1)
        settings->lowHpBeepEnabled = FALSE;
    if (settings->trainerMusicMode > AUDIO_TRAINER_MUSIC_RANDOM)
        settings->trainerMusicMode = AUDIO_TRAINER_MUSIC_DEFAULT;
    if (settings->musicPool > AUDIO_MUSIC_POOL_ALL_BATTLES)
        settings->musicPool = AUDIO_MUSIC_POOL_HOENN_KANTO;
    if (settings->trainerTrack == 0 || settings->trainerTrack > ARRAY_COUNT(sAudioTracks))
    {
        settings->trainerTrack = AUDIO_TRACK_HOENN_TRAINER;
        if (settings->trainerMusicMode == AUDIO_TRAINER_MUSIC_FIXED)
            settings->trainerMusicMode = AUDIO_TRAINER_MUSIC_DEFAULT;
    }
    if (settings->trainerMusicMode == AUDIO_TRAINER_MUSIC_FIXED
        && !AudioSettings_IsTrackInPool(settings->trainerTrack, settings->musicPool))
        settings->trainerMusicMode = AUDIO_TRAINER_MUSIC_DEFAULT;
    settings->reserved = 0;
}

void AudioSettings_InitDefaults(void)
{
    struct AudioSettingsSave *settings = &gSaveBlock2Ptr->audioSettings;

    settings->magic = AUDIO_SETTINGS_MAGIC;
    settings->version = AUDIO_SETTINGS_VERSION;
    settings->lowHpBeepEnabled = FALSE;
    settings->trainerMusicMode = AUDIO_TRAINER_MUSIC_DEFAULT;
    settings->musicPool = AUDIO_MUSIC_POOL_HOENN_KANTO;
    settings->trainerTrack = AUDIO_TRACK_HOENN_TRAINER;
    settings->reserved = 0;
    sPreviousRandomTrack = 0;
    sBattleTrackCached = FALSE;
}

bool8 AudioSettings_IsLowHpBeepEnabled(void)
{
    AudioSettings_Normalize();
    return gSaveBlock2Ptr->audioSettings.lowHpBeepEnabled;
}

void AudioSettings_SetLowHpBeepEnabled(bool8 enabled)
{
    AudioSettings_Normalize();
    gSaveBlock2Ptr->audioSettings.lowHpBeepEnabled = enabled != 0;
}

u8 AudioSettings_GetTrainerMusicMode(void)
{
    AudioSettings_Normalize();
    return gSaveBlock2Ptr->audioSettings.trainerMusicMode;
}

void AudioSettings_SetTrainerMusicMode(u8 mode)
{
    AudioSettings_Normalize();
    gSaveBlock2Ptr->audioSettings.trainerMusicMode = mode <= AUDIO_TRAINER_MUSIC_RANDOM ? mode : AUDIO_TRAINER_MUSIC_DEFAULT;
    if (gSaveBlock2Ptr->audioSettings.trainerMusicMode == AUDIO_TRAINER_MUSIC_FIXED
        && !AudioSettings_IsTrackInPool(gSaveBlock2Ptr->audioSettings.trainerTrack, gSaveBlock2Ptr->audioSettings.musicPool))
        gSaveBlock2Ptr->audioSettings.trainerMusicMode = AUDIO_TRAINER_MUSIC_DEFAULT;
    AudioSettings_ResetBattleMusic();
}

u8 AudioSettings_GetMusicPool(void)
{
    AudioSettings_Normalize();
    return gSaveBlock2Ptr->audioSettings.musicPool;
}

void AudioSettings_SetMusicPool(u8 pool)
{
    AudioSettings_Normalize();
    if (pool > AUDIO_MUSIC_POOL_ALL_BATTLES)
        pool = AUDIO_MUSIC_POOL_HOENN_KANTO;
    gSaveBlock2Ptr->audioSettings.musicPool = pool;
    if (gSaveBlock2Ptr->audioSettings.trainerMusicMode == AUDIO_TRAINER_MUSIC_FIXED
        && !AudioSettings_IsTrackInPool(gSaveBlock2Ptr->audioSettings.trainerTrack, pool))
        gSaveBlock2Ptr->audioSettings.trainerMusicMode = AUDIO_TRAINER_MUSIC_DEFAULT;
    AudioSettings_ResetBattleMusic();
}

u8 AudioSettings_GetTrainerTrack(void)
{
    AudioSettings_Normalize();
    return gSaveBlock2Ptr->audioSettings.trainerTrack;
}

void AudioSettings_SetTrainerTrack(u8 track)
{
    AudioSettings_Normalize();
    if (track == 0 || track > ARRAY_COUNT(sAudioTracks))
        track = AUDIO_TRACK_HOENN_TRAINER;
    gSaveBlock2Ptr->audioSettings.trainerTrack = track;
    if (!AudioSettings_IsTrackInPool(track, gSaveBlock2Ptr->audioSettings.musicPool))
        gSaveBlock2Ptr->audioSettings.trainerMusicMode = AUDIO_TRAINER_MUSIC_DEFAULT;
    AudioSettings_ResetBattleMusic();
}

static const u8 *GetPool(u8 pool, u8 *count)
{
    if (pool == AUDIO_MUSIC_POOL_HOENN)
    {
        *count = ARRAY_COUNT(sHoennPool);
        return sHoennPool;
    }
    if (pool == AUDIO_MUSIC_POOL_ALL_BATTLES)
    {
        *count = ARRAY_COUNT(sHoennPool) + ARRAY_COUNT(sKantoPool) + ARRAY_COUNT(sAllBattlesPool);
        return NULL;
    }
    *count = ARRAY_COUNT(sHoennPool) + ARRAY_COUNT(sKantoPool);
    return NULL;
}

u8 AudioSettings_GetPoolTrackCount(u8 pool)
{
    u8 count;
    GetPool(pool, &count);
    return count;
}

u8 AudioSettings_GetPoolTrack(u8 pool, u8 index)
{
    u8 count;
    const u8 *list = GetPool(pool, &count);
    if (index >= count)
        return 0;
    if (list != NULL)
        return list[index];
    if (index < ARRAY_COUNT(sHoennPool))
        return sHoennPool[index];
    index -= ARRAY_COUNT(sHoennPool);
    if (pool == AUDIO_MUSIC_POOL_ALL_BATTLES)
    {
        if (index < ARRAY_COUNT(sKantoPool))
            return sKantoPool[index];
        return sAllBattlesPool[index - ARRAY_COUNT(sKantoPool)];
    }
    return sKantoPool[index];
}

bool8 AudioSettings_IsTrackInPool(u8 track, u8 pool)
{
    u8 i;
    for (i = 0; i < AudioSettings_GetPoolTrackCount(pool); i++)
    {
        if (AudioSettings_GetPoolTrack(pool, i) == track)
            return TRUE;
    }
    return FALSE;
}

u16 AudioSettings_GetTrackSong(u8 track)
{
    u8 i;
    for (i = 0; i < ARRAY_COUNT(sAudioTracks); i++)
        if (sAudioTracks[i].id == track)
            return sAudioTracks[i].song;
    return MUS_VS_TRAINER;
}

const u8 *AudioSettings_GetTrackName(u8 track)
{
    u8 i;
    for (i = 0; i < ARRAY_COUNT(sAudioTracks); i++)
        if (sAudioTracks[i].id == track)
            return sAudioTracks[i].name;
    return sTextAudioHoennTrainer;
}

void AudioSettings_ResetBattleMusic(void)
{
    sBattleTrackCached = FALSE;
}

bool8 AudioSettings_IsTrainerMusicEligible(u16 originalSong)
{
    return originalSong == MUS_VS_TRAINER;
}

static u8 AudioRandom(u8 limit)
{
    if (sAudioRng == 0)
        sAudioRng = 0xA0155EED;
    sAudioRng ^= sAudioRng << 13;
    sAudioRng ^= sAudioRng >> 17;
    sAudioRng ^= sAudioRng << 5;
    return sAudioRng % limit;
}

u16 AudioSettings_GetTrainerMusic(u16 originalSong)
{
    u8 mode, pool, track, count, index;

    AudioSettings_Normalize();
    mode = gSaveBlock2Ptr->audioSettings.trainerMusicMode;
    if (mode == AUDIO_TRAINER_MUSIC_DEFAULT || !AudioSettings_IsTrainerMusicEligible(originalSong))
        return originalSong;
    if (sBattleTrackCached)
        return sCachedBattleTrack;

    pool = gSaveBlock2Ptr->audioSettings.musicPool;
    if (mode == AUDIO_TRAINER_MUSIC_FIXED)
    {
        track = gSaveBlock2Ptr->audioSettings.trainerTrack;
        if (!AudioSettings_IsTrackInPool(track, pool))
            return originalSong;
    }
    else
    {
        count = AudioSettings_GetPoolTrackCount(pool);
        if (count == 0)
            return originalSong;
        index = AudioRandom(count);
        track = AudioSettings_GetPoolTrack(pool, index);
        if (count > 1 && track == sPreviousRandomTrack)
            track = AudioSettings_GetPoolTrack(pool, (index + 1) % count);
        sPreviousRandomTrack = track;
    }
    sCachedBattleTrack = AudioSettings_GetTrackSong(track);
    sBattleTrackCached = TRUE;
    return sCachedBattleTrack;
}
