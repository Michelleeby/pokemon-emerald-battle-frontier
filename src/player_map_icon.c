#include "global.h"
#include "player_map_icon.h"
#include "player_avatar.h"
#include "event_object_movement.h"

const u8 *GetPlayerMapIconTiles(u8 avatarId)
{
    const struct ObjectEventGraphicsInfo *graphics = GetObjectEventGraphicsInfo(GetPlayerAvatarOverworldGraphicsId(avatarId));

    // The first two tile rows of the south-facing 16x32 frame form a 16x16 head.
    // Reuse the actual avatar art so markers cannot drift from the selection.
    return graphics->images[0].data;
}

const u16 *GetPlayerMapIconPalette(u8 avatarId)
{
    return GetObjectEventPaletteByGraphicsId(GetPlayerAvatarOverworldGraphicsId(avatarId));
}
