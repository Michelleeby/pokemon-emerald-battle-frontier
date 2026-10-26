#include "global.h"
#include "player_avatar.h"
#include "event_data.h"
#include "link.h"
#include "constants/event_objects.h"
#include "constants/trainers.h"
#include "constants/vars.h"

struct PlayerAvatarAppearance
{
    u16 frontPic;
    u8 overworldGraphics;
    u8 gender;
};

static const struct PlayerAvatarAppearance sPlayerAvatars[PLAYER_AVATAR_COUNT] =
{
    [TRAINER_BACK_PIC_BRENDAN] = {TRAINER_PIC_BRENDAN, OBJ_EVENT_GFX_BRENDAN_NORMAL, MALE},
    [TRAINER_BACK_PIC_MAY] = {TRAINER_PIC_MAY, OBJ_EVENT_GFX_MAY_NORMAL, FEMALE},
    [TRAINER_BACK_PIC_RED] = {TRAINER_PIC_RED, OBJ_EVENT_GFX_RED, MALE},
    [TRAINER_BACK_PIC_LEAF] = {TRAINER_PIC_LEAF, OBJ_EVENT_GFX_LEAF, FEMALE},
    [TRAINER_BACK_PIC_RUBY_SAPPHIRE_BRENDAN] = {TRAINER_PIC_RS_BRENDAN, OBJ_EVENT_GFX_LINK_RS_BRENDAN, MALE},
    [TRAINER_BACK_PIC_RUBY_SAPPHIRE_MAY] = {TRAINER_PIC_RS_MAY, OBJ_EVENT_GFX_LINK_RS_MAY, FEMALE},
    [TRAINER_BACK_PIC_WALLY] = {TRAINER_PIC_WALLY, OBJ_EVENT_GFX_WALLY, MALE},
    [TRAINER_BACK_PIC_STEVEN] = {TRAINER_PIC_STEVEN, OBJ_EVENT_GFX_STEVEN, MALE},
};

// An opt-in tag in an unused link byte leaves the legacy binary gender intact.
// The same byte encoding fits the recorded battle's existing appearance slot.
#define PLAYER_AVATAR_TAG 0xA0

u8 NormalizePlayerAvatarId(u16 avatarId)
{
    return avatarId < PLAYER_AVATAR_COUNT ? avatarId : TRAINER_BACK_PIC_MAY;
}

u8 GetPlayerAvatarId(void)
{
    return NormalizePlayerAvatarId(VarGet(VAR_PLAYER_AVATAR_STYLE));
}

u8 GetPlayerAvatarGender(u8 avatarId)
{
    return sPlayerAvatars[NormalizePlayerAvatarId(avatarId)].gender;
}

u16 GetPlayerAvatarFrontPicId(u8 avatarId)
{
    return sPlayerAvatars[NormalizePlayerAvatarId(avatarId)].frontPic;
}

u8 GetPlayerAvatarBackPicId(u8 avatarId)
{
    return NormalizePlayerAvatarId(avatarId);
}

u8 GetPlayerAvatarOverworldGraphicsId(u8 avatarId)
{
    return sPlayerAvatars[NormalizePlayerAvatarId(avatarId)].overworldGraphics;
}

u8 EncodePlayerAvatarId(u8 avatarId)
{
    return PLAYER_AVATAR_TAG | NormalizePlayerAvatarId(avatarId);
}

bool8 IsEncodedPlayerAvatarId(u8 value)
{
    return value >= PLAYER_AVATAR_TAG && value < PLAYER_AVATAR_TAG + PLAYER_AVATAR_COUNT;
}

u8 GetLinkPlayerAvatarId(const struct LinkPlayer *player)
{
    u8 gender = player->gender == MALE ? MALE : FEMALE;

    if ((player->version & 0xFF) == VERSION_EMERALD && IsEncodedPlayerAvatarId(player->neverRead))
        return player->neverRead - PLAYER_AVATAR_TAG;

    switch (player->version & 0xFF)
    {
    case VERSION_FIRE_RED:
    case VERSION_LEAF_GREEN:
        return TRAINER_BACK_PIC_RED + gender;
    case VERSION_RUBY:
    case VERSION_SAPPHIRE:
        return TRAINER_BACK_PIC_RUBY_SAPPHIRE_BRENDAN + gender;
    default:
        return TRAINER_BACK_PIC_BRENDAN + gender;
    }
}

u16 GetLinkPlayerFrontPicId(const struct LinkPlayer *player)
{
    return GetPlayerAvatarFrontPicId(GetLinkPlayerAvatarId(player));
}

u8 GetLinkPlayerBackPicId(const struct LinkPlayer *player)
{
    return GetPlayerAvatarBackPicId(GetLinkPlayerAvatarId(player));
}
