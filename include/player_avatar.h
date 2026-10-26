#ifndef GUARD_PLAYER_AVATAR_H
#define GUARD_PLAYER_AVATAR_H

#define PLAYER_AVATAR_COUNT 8

struct LinkPlayer;

// IDs match TRAINER_BACK_PIC_*; the saved 0/1 choices remain Brendan/May.
u8 NormalizePlayerAvatarId(u16 avatarId);
u8 GetPlayerAvatarId(void);
u8 GetPlayerAvatarGender(u8 avatarId);
u16 GetPlayerAvatarFrontPicId(u8 avatarId);
u8 GetPlayerAvatarBackPicId(u8 avatarId);
u8 GetPlayerAvatarOverworldGraphicsId(u8 avatarId);
u8 EncodePlayerAvatarId(u8 avatarId);
bool8 IsEncodedPlayerAvatarId(u8 value);
u8 GetLinkPlayerAvatarId(const struct LinkPlayer *player);
u16 GetLinkPlayerFrontPicId(const struct LinkPlayer *player);
u8 GetLinkPlayerBackPicId(const struct LinkPlayer *player);

#endif // GUARD_PLAYER_AVATAR_H
