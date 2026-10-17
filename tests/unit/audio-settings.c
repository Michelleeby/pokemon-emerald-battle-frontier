#include <stddef.h>
#include "global.h"
#include "audio_settings.h"
#include "test.h"
#include "random.h"
#include "battle.h"
#include "battle_setup.h"
#include "pokemon.h"
#include "constants/songs.h"
#include "constants/opponents.h"

void RunTest(void)
{
    u8 i;

    TestResetFixture(0xA0D1);
    AudioSettings_InitDefaults();
    TEST_ASSERT_EQ(8, sizeof(struct AudioSettingsSave));
    TEST_ASSERT_EQ(0x90, offsetof(struct SaveBlock2, audioSettings));
    TEST_ASSERT_EQ(FALSE, AudioSettings_IsLowHpBeepEnabled());
    TEST_ASSERT_EQ(AUDIO_TRAINER_MUSIC_DEFAULT, AudioSettings_GetTrainerMusicMode());
    TEST_ASSERT_EQ(AUDIO_MUSIC_POOL_HOENN_KANTO, AudioSettings_GetMusicPool());
    TEST_ASSERT_EQ(AUDIO_TRACK_HOENN_TRAINER, AudioSettings_GetTrainerTrack());
    TEST_ASSERT_EQ(8, AudioSettings_GetPoolTrackCount(AUDIO_MUSIC_POOL_HOENN));
    TEST_ASSERT_EQ(11, AudioSettings_GetPoolTrackCount(AUDIO_MUSIC_POOL_HOENN_KANTO));
    TEST_ASSERT_EQ(20, AudioSettings_GetPoolTrackCount(AUDIO_MUSIC_POOL_ALL_BATTLES));

    for (i = 0; i < AudioSettings_GetPoolTrackCount(AUDIO_MUSIC_POOL_ALL_BATTLES); i++)
        TEST_ASSERT(AudioSettings_GetTrackSong(AudioSettings_GetPoolTrack(AUDIO_MUSIC_POOL_ALL_BATTLES, i)) != MUS_NONE);

    AudioSettings_SetLowHpBeepEnabled(TRUE);
    TEST_ASSERT(AudioSettings_IsLowHpBeepEnabled());
    AudioSettings_SetTrainerTrack(AUDIO_TRACK_KANTO_TRAINER);
    AudioSettings_SetTrainerMusicMode(AUDIO_TRAINER_MUSIC_FIXED);
    TEST_ASSERT_EQ(MUS_RG_VS_TRAINER, AudioSettings_GetTrainerMusic(MUS_VS_TRAINER));
    TEST_ASSERT_EQ(MUS_RG_VS_TRAINER, AudioSettings_GetTrainerMusic(MUS_VS_TRAINER));

    AudioSettings_SetMusicPool(AUDIO_MUSIC_POOL_HOENN);
    TEST_ASSERT_EQ(AUDIO_TRAINER_MUSIC_DEFAULT, AudioSettings_GetTrainerMusicMode());
    TEST_ASSERT_EQ(MUS_VS_TRAINER, AudioSettings_GetTrainerMusic(MUS_VS_TRAINER));

    gSaveBlock2Ptr->audioSettings.magic = 0;
    gSaveBlock2Ptr->encryptionKey = 0xCAFEBABE;
    TEST_ASSERT_EQ(FALSE, AudioSettings_IsLowHpBeepEnabled());
    TEST_ASSERT_EQ(AUDIO_MUSIC_POOL_HOENN_KANTO, AudioSettings_GetMusicPool());
    TEST_ASSERT_EQ(0xCAFEBABE, gSaveBlock2Ptr->encryptionKey);

    gSaveBlock2Ptr->audioSettings.magic = AUDIO_SETTINGS_MAGIC;
    gSaveBlock2Ptr->audioSettings.version = AUDIO_SETTINGS_VERSION;
    gSaveBlock2Ptr->audioSettings.lowHpBeepEnabled = 2;
    gSaveBlock2Ptr->audioSettings.trainerMusicMode = AUDIO_TRAINER_MUSIC_FIXED;
    gSaveBlock2Ptr->audioSettings.trainerTrack = 0xFF;
    TEST_ASSERT_EQ(FALSE, AudioSettings_IsLowHpBeepEnabled());
    TEST_ASSERT_EQ(AUDIO_TRAINER_MUSIC_DEFAULT, AudioSettings_GetTrainerMusicMode());

    gSaveBlock2Ptr->audioSettings.trainerMusicMode = AUDIO_TRAINER_MUSIC_RANDOM;
    gSaveBlock2Ptr->audioSettings.musicPool = AUDIO_MUSIC_POOL_ALL_BATTLES;
    AudioSettings_ResetBattleMusic();
    gRngValue = 0x12345678;
    gRng2Value = 0x87654321;
    AudioSettings_GetTrainerMusic(MUS_VS_TRAINER);
    TEST_ASSERT_EQ(0x12345678, gRngValue);
    TEST_ASSERT_EQ(0x87654321, gRng2Value);

    AudioSettings_SetTrainerTrack(AUDIO_TRACK_KANTO_TRAINER);
    AudioSettings_SetTrainerMusicMode(AUDIO_TRAINER_MUSIC_FIXED);
    gTrainerBattleOpponent_A = TRAINER_NONE;
    gBattleTypeFlags = BATTLE_TYPE_TRAINER;
    AudioSettings_ResetBattleMusic();
    TEST_ASSERT_EQ(MUS_RG_VS_TRAINER, GetBattleBGM());
    gBattleTypeFlags |= BATTLE_TYPE_FIRST_BATTLE;
    AudioSettings_ResetBattleMusic();
    TEST_ASSERT_EQ(MUS_VS_TRAINER, GetBattleBGM());

    gBattleTypeFlags = BATTLE_TYPE_TRAINER;
    gTrainerBattleOpponent_A = TRAINER_ANABEL;
    AudioSettings_ResetBattleMusic();
    TEST_ASSERT_EQ(MUS_VS_FRONTIER_BRAIN, GetBattleBGM());

    AudioSettings_SetMusicPool(AUDIO_MUSIC_POOL_HOENN);
    AudioSettings_SetTrainerMusicMode(AUDIO_TRAINER_MUSIC_RANDOM);
    {
        u16 previousSong = MUS_NONE;
        for (i = 0; i < 8; i++)
        {
            u8 j;
            u16 song;
            bool8 inPool = FALSE;

            gBattleTypeFlags = BATTLE_TYPE_TRAINER;
            gTrainerBattleOpponent_A = TRAINER_NONE;
            AudioSettings_ResetBattleMusic();
            song = GetBattleBGM();
            for (j = 0; j < AudioSettings_GetPoolTrackCount(AUDIO_MUSIC_POOL_HOENN); j++)
                if (AudioSettings_GetTrackSong(AudioSettings_GetPoolTrack(AUDIO_MUSIC_POOL_HOENN, j)) == song)
                    inPool = TRUE;
            TEST_ASSERT(inPool);
            TEST_ASSERT(song != previousSong);
            previousSong = song;
        }
    }
}
