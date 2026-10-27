#ifndef GUARD_PLAYER_CUSTOMIZATION_H
#define GUARD_PLAYER_CUSTOMIZATION_H

#define PLAYER_BAG_BACKPACK 0
#define PLAYER_BAG_SATCHEL  1
#define PLAYER_BAG_COUNT    2

#define PLAYER_THEME_BLUE  0
#define PLAYER_THEME_PINK  1
#define PLAYER_THEME_COUNT 2

struct TrainerCard;

extern u8 gNewGameBagStyle;
extern u8 gNewGameUiTheme;

void PlayerCustomization_Init(u8 bagStyle, u8 uiTheme);
u8 GetPlayerBagStyle(void);
u8 GetPlayerUiTheme(void);
u8 EncodePlayerUiTheme(u8 uiTheme);
u8 GetTrainerCardUiTheme(const struct TrainerCard *card);

#endif // GUARD_PLAYER_CUSTOMIZATION_H
