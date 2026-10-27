#include "global.h"
#include "player_customization.h"
#include "player_avatar.h"
#include "event_data.h"
#include "trainer_card.h"
#include "constants/vars.h"

// Versioned values occupy existing unused event variables, leaving save offsets
// intact. Old saves cannot accidentally interpret an ordinary 0/1 as a choice.
#define PLAYER_BAG_SAVE_TAG   0xB100
#define PLAYER_THEME_SAVE_TAG 0xB200
#define PLAYER_THEME_LINK_TAG 0xB0

EWRAM_DATA u8 gNewGameBagStyle = PLAYER_BAG_BACKPACK;
EWRAM_DATA u8 gNewGameUiTheme = PLAYER_THEME_BLUE;

void PlayerCustomization_Init(u8 bagStyle, u8 uiTheme)
{
    VarSet(VAR_PLAYER_BAG_STYLE, PLAYER_BAG_SAVE_TAG | (bagStyle < PLAYER_BAG_COUNT ? bagStyle : PLAYER_BAG_BACKPACK));
    VarSet(VAR_PLAYER_UI_THEME, PLAYER_THEME_SAVE_TAG | (uiTheme < PLAYER_THEME_COUNT ? uiTheme : PLAYER_THEME_BLUE));
}

static void NormalizePlayerCustomization(void)
{
    u16 bag = VarGet(VAR_PLAYER_BAG_STYLE);
    u16 theme = VarGet(VAR_PLAYER_UI_THEME);
    u8 legacyStyle;

    if (bag >= PLAYER_BAG_SAVE_TAG && bag < PLAYER_BAG_SAVE_TAG + PLAYER_BAG_COUNT
     && theme >= PLAYER_THEME_SAVE_TAG && theme < PLAYER_THEME_SAVE_TAG + PLAYER_THEME_COUNT)
        return;

    legacyStyle = GetPlayerAvatarGender(GetPlayerAvatarId());
    // Migrate the old appearance once, just as other versioned save settings
    // normalize on access. Later avatar changes never change the stored choices.
    if (bag < PLAYER_BAG_SAVE_TAG || bag >= PLAYER_BAG_SAVE_TAG + PLAYER_BAG_COUNT)
        VarSet(VAR_PLAYER_BAG_STYLE, PLAYER_BAG_SAVE_TAG | legacyStyle);
    if (theme < PLAYER_THEME_SAVE_TAG || theme >= PLAYER_THEME_SAVE_TAG + PLAYER_THEME_COUNT)
        VarSet(VAR_PLAYER_UI_THEME, PLAYER_THEME_SAVE_TAG | legacyStyle);
}

u8 GetPlayerBagStyle(void)
{
    NormalizePlayerCustomization();
    return VarGet(VAR_PLAYER_BAG_STYLE) - PLAYER_BAG_SAVE_TAG;
}

u8 GetPlayerUiTheme(void)
{
    NormalizePlayerCustomization();
    return VarGet(VAR_PLAYER_UI_THEME) - PLAYER_THEME_SAVE_TAG;
}

u8 EncodePlayerUiTheme(u8 uiTheme)
{
    return PLAYER_THEME_LINK_TAG | (uiTheme < PLAYER_THEME_COUNT ? uiTheme : PLAYER_THEME_BLUE);
}

u8 GetTrainerCardUiTheme(const struct TrainerCard *card)
{
    u8 theme = card->filler[1];

    if (card->version == VERSION_EMERALD && theme >= PLAYER_THEME_LINK_TAG && theme < PLAYER_THEME_LINK_TAG + PLAYER_THEME_COUNT)
        return theme - PLAYER_THEME_LINK_TAG;
    return card->gender == FEMALE ? PLAYER_THEME_PINK : PLAYER_THEME_BLUE;
}
