#include "global.h"
#include "bg.h"
#include "data.h"
#include "decompress.h"
#include "graphics.h"
#include "gpu_regs.h"
#include "item.h"
#include "item_icon.h"
#include "item_menu_icons.h"
#include "international_string_util.h"
#include "list_menu.h"
#include "main.h"
#include "malloc.h"
#include "menu.h"
#include "palette.h"
#include "sound.h"
#include "sprite.h"
#include "strings.h"
#include "string_util.h"
#include "task.h"
#include "team_lab.h"
#include "text.h"
#include "text_window.h"
#include "window.h"
#include "constants/hold_effects.h"
#include "constants/item.h"
#include "constants/items.h"
#include "constants/rgb.h"
#include "constants/songs.h"

#define TAG_TEAM_LAB_ITEM_ICON 5110
#define ITEMS_VISIBLE 8
#define ITEM_ICON_HIDE (ITEM_LIST_END - 1)

enum
{
    SELECTOR_POCKET_BATTLE,
    SELECTOR_POCKET_BERRIES,
    SELECTOR_POCKET_COUNT,
};

enum
{
    WIN_ITEM_LIST,
    WIN_ITEM_DESCRIPTION,
    WIN_ITEM_POCKET,
    WIN_ITEM_COUNT,
};

struct TeamLabItemSelector
{
    MainCallback returnCallback;
    u16 *bgTilemap;
    u16 cursor[SELECTOR_POCKET_COUNT];
    u16 scroll[SELECTOR_POCKET_COUNT];
    u16 pocketArrowPos;
    u8 pocket;
    u8 iconSpriteId;
    u8 bagSpriteId;
    u8 pocketArrowTaskId;
};

static EWRAM_DATA struct TeamLabItemSelector *sItemSelector = NULL;
static EWRAM_DATA bool8 sItemSelectionMade = FALSE;
static EWRAM_DATA u16 sItemSelectionResult = ITEM_NONE;

static void CB2_InitTeamLabItemSelector(void);
static void CB2_TeamLabItemSelector(void);
static void VBlankCB_TeamLabItemSelector(void);
static void Task_HandleTeamLabItemSelector(u8 taskId);
static void Task_ExitTeamLabItemSelector(u8 taskId);
static void DrawItemSelector(void);
static void DrawItemSelectorIcon(u16 item);
static u16 GetPocketItemCount(u8 pocket);
static u16 GetPocketItem(u8 pocket, u16 index);
static void SetInitialItem(u16 item);
static void DrawPocketIndicators(void);
static void ResetItemSelectorBgCoordinates(void);

static const u16 sCompetitiveBattleItems[] =
{
    ITEM_BRIGHT_POWDER,
    ITEM_WHITE_HERB,
    ITEM_QUICK_CLAW,
    ITEM_MENTAL_HERB,
    ITEM_CHOICE_BAND,
    ITEM_KINGS_ROCK,
    ITEM_SILVER_POWDER,
    ITEM_SOUL_DEW,
    ITEM_DEEP_SEA_TOOTH,
    ITEM_DEEP_SEA_SCALE,
    ITEM_FOCUS_BAND,
    ITEM_SCOPE_LENS,
    ITEM_METAL_COAT,
    ITEM_LEFTOVERS,
    ITEM_LIGHT_BALL,
    ITEM_SOFT_SAND,
    ITEM_HARD_STONE,
    ITEM_MIRACLE_SEED,
    ITEM_BLACK_GLASSES,
    ITEM_BLACK_BELT,
    ITEM_MAGNET,
    ITEM_MYSTIC_WATER,
    ITEM_SHARP_BEAK,
    ITEM_POISON_BARB,
    ITEM_NEVER_MELT_ICE,
    ITEM_SPELL_TAG,
    ITEM_TWISTED_SPOON,
    ITEM_CHARCOAL,
    ITEM_DRAGON_FANG,
    ITEM_SILK_SCARF,
    ITEM_SHELL_BELL,
    ITEM_SEA_INCENSE,
    ITEM_LAX_INCENSE,
};

static const struct BgTemplate sItemSelectorBgTemplates[] =
{
    {.bg = 0, .charBaseIndex = 0, .mapBaseIndex = 31, .screenSize = 0, .paletteMode = 0, .priority = 0, .baseTile = 0},
    {.bg = 1, .charBaseIndex = 0, .mapBaseIndex = 30, .screenSize = 0, .paletteMode = 0, .priority = 1, .baseTile = 0},
    {.bg = 2, .charBaseIndex = 3, .mapBaseIndex = 29, .screenSize = 0, .paletteMode = 0, .priority = 2, .baseTile = 0},
};

static const struct WindowTemplate sItemSelectorWindowTemplates[] =
{
    [WIN_ITEM_LIST] = {.bg = 0, .tilemapLeft = 14, .tilemapTop = 2, .width = 15, .height = 16, .paletteNum = 1, .baseBlock = 0x27},
    [WIN_ITEM_DESCRIPTION] = {.bg = 0, .tilemapLeft = 0, .tilemapTop = 13, .width = 14, .height = 6, .paletteNum = 1, .baseBlock = 0x117},
    [WIN_ITEM_POCKET] = {.bg = 0, .tilemapLeft = 4, .tilemapTop = 1, .width = 8, .height = 2, .paletteNum = 1, .baseBlock = 0x1A1},
    DUMMY_WIN_TEMPLATE,
};

static const u8 sText_BattleItems[] = _("BATTLE ITEMS");
static const u8 sText_Berries[] = _("BERRIES");
static const u8 sText_CloseDescription[] = _("Return to the field.");

static const struct ScrollArrowsTemplate sPocketArrowsTemplate =
{
    .firstArrowType = SCROLL_ARROW_LEFT,
    .firstX = 28,
    .firstY = 16,
    .secondArrowType = SCROLL_ARROW_RIGHT,
    .secondX = 100,
    .secondY = 16,
    .fullyUpThreshold = -1,
    .fullyDownThreshold = -1,
    .tileTag = 5111,
    .palTag = 5111,
    .palNum = 0,
};

void OpenTeamLabItemSelector(u16 initialItem, void (*returnCallback)(void))
{
    sItemSelectionMade = FALSE;
    sItemSelectionResult = ITEM_NONE;
    sItemSelector = AllocZeroed(sizeof(*sItemSelector));
    if (sItemSelector == NULL)
    {
        SetMainCallback2(returnCallback);
        return;
    }
    sItemSelector->bgTilemap = AllocZeroed(BG_SCREEN_SIZE);
    if (sItemSelector->bgTilemap == NULL)
    {
        Free(sItemSelector);
        sItemSelector = NULL;
        SetMainCallback2(returnCallback);
        return;
    }
    sItemSelector->returnCallback = returnCallback;
    sItemSelector->iconSpriteId = SPRITE_NONE;
    sItemSelector->bagSpriteId = SPRITE_NONE;
    sItemSelector->pocketArrowTaskId = TASK_NONE;
    SetInitialItem(initialItem);
    gMain.state = 0;
    SetMainCallback2(CB2_InitTeamLabItemSelector);
}

bool8 GetTeamLabItemSelectionResult(u16 *item)
{
    *item = sItemSelectionResult;
    return sItemSelectionMade;
}

static void SetInitialItem(u16 item)
{
    u16 i;
    u8 pocket;

    for (pocket = 0; pocket < SELECTOR_POCKET_COUNT; pocket++)
    {
        for (i = 0; i < GetPocketItemCount(pocket); i++)
        {
            if (GetPocketItem(pocket, i) == item)
            {
                sItemSelector->pocket = pocket;
                sItemSelector->cursor[pocket] = i;
                if (i >= ITEMS_VISIBLE)
                    sItemSelector->scroll[pocket] = i - ITEMS_VISIBLE + 1;
                return;
            }
        }
    }
}

static void CB2_InitTeamLabItemSelector(void)
{
    switch (gMain.state)
    {
    case 0:
        SetVBlankCallback(NULL);
        ResetTasks();
        ResetSpriteData();
        FreeAllSpritePalettes();
        ClearScheduledBgCopiesToVram();
        CpuFill32(0, (void *)VRAM, VRAM_SIZE);
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sItemSelectorBgTemplates, ARRAY_COUNT(sItemSelectorBgTemplates));
        SetBgTilemapBuffer(2, sItemSelector->bgTilemap);
        ResetItemSelectorBgCoordinates();
        InitWindows(sItemSelectorWindowTemplates);
        DeactivateAllTextPrinters();
        ResetTempTileDataBuffers();
        DecompressAndCopyTileDataToVram(2, gBagScreen_Gfx, 0, 0, 0);
        gMain.state++;
        break;
    case 1:
        if (FreeTempTileDataBuffersIfPossible() != TRUE)
        {
            LZDecompressWram(gBagScreen_GfxTileMap, sItemSelector->bgTilemap);
            DrawPocketIndicators();
            if (GetPlayerAvatarStyle() == MALE)
            {
                LoadCompressedPalette(gBagScreenMale_Pal, BG_PLTT_ID(0), 2 * PLTT_SIZE_4BPP);
                LoadCompressedSpriteSheet(&gBagMaleSpriteSheet);
            }
            else
            {
                LoadCompressedPalette(gBagScreenFemale_Pal, BG_PLTT_ID(0), 2 * PLTT_SIZE_4BPP);
                LoadCompressedSpriteSheet(&gBagFemaleSpriteSheet);
            }
            LoadCompressedSpritePalette(&gBagPaletteTable);
            CopyBgTilemapBufferToVram(2);
            gMain.state++;
        }
        break;
    case 2:
        ResetPaletteFade();
        PutWindowTilemap(WIN_ITEM_LIST);
        PutWindowTilemap(WIN_ITEM_DESCRIPTION);
        PutWindowTilemap(WIN_ITEM_POCKET);
        sItemSelector->bagSpriteId = CreateIndependentBagVisualSprite(POCKET_ITEMS);
        sItemSelector->pocketArrowTaskId = AddScrollIndicatorArrowPair(&sPocketArrowsTemplate, &sItemSelector->pocketArrowPos);
        DrawItemSelector();
        CreateTask(Task_HandleTeamLabItemSelector, 0);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
        ShowBg(0);
        ShowBg(1);
        ShowBg(2);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        SetVBlankCallback(VBlankCB_TeamLabItemSelector);
        SetMainCallback2(CB2_TeamLabItemSelector);
        break;
    }
}

static void ResetItemSelectorBgCoordinates(void)
{
    u8 bg;

    for (bg = 0; bg < ARRAY_COUNT(sItemSelectorBgTemplates); bg++)
    {
        ChangeBgX(bg, 0, BG_COORD_SET);
        ChangeBgY(bg, 0, BG_COORD_SET);
    }
}

static void VBlankCB_TeamLabItemSelector(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void CB2_TeamLabItemSelector(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    RunTextPrinters();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

static u16 GetPocketItemCount(u8 pocket)
{
    u16 item;
    u16 count = 0;

    if (pocket == SELECTOR_POCKET_BATTLE)
        return ARRAY_COUNT(sCompetitiveBattleItems) + 1;
    for (item = FIRST_BERRY_INDEX; item <= LAST_BERRY_INDEX; item++)
    {
        if (gItems[item].holdEffect != HOLD_EFFECT_NONE)
            count++;
    }
    return count + 1;
}

static u16 GetPocketItem(u8 pocket, u16 index)
{
    u16 item;

    if (pocket == SELECTOR_POCKET_BATTLE)
    {
        if (index == ARRAY_COUNT(sCompetitiveBattleItems))
            return ITEM_NONE;
        return sCompetitiveBattleItems[index];
    }
    for (item = FIRST_BERRY_INDEX; item <= LAST_BERRY_INDEX; item++)
    {
        if (gItems[item].holdEffect != HOLD_EFFECT_NONE)
        {
            if (index == 0)
                return item;
            index--;
        }
    }
    return ITEM_NONE;
}

static void DrawPocketIndicators(void)
{
    u16 blankTile = sItemSelector->bgTilemap[3 * 32 + 4];
    u8 i;

    for (i = 5; i <= 9; i++)
        sItemSelector->bgTilemap[3 * 32 + i] = blankTile;
    sItemSelector->bgTilemap[3 * 32 + 7] = 0x1017;
    sItemSelector->bgTilemap[3 * 32 + 8] = 0x1017;
    sItemSelector->bgTilemap[3 * 32 + 7 + sItemSelector->pocket] = 0x102B;
    ScheduleBgCopyTilemapToVram(2);
}

static void DrawItemSelector(void)
{
    static const u8 colorsNormal[] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_WHITE, TEXT_COLOR_LIGHT_GRAY};
    static const u8 colorsPocket[] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_WHITE, TEXT_COLOR_RED};
    u16 cursor = sItemSelector->cursor[sItemSelector->pocket];
    u16 scroll = sItemSelector->scroll[sItemSelector->pocket];
    u16 count = GetPocketItemCount(sItemSelector->pocket);
    u16 selectedItem = GetPocketItem(sItemSelector->pocket, cursor);
    const u8 *description;
    u8 row;

    FillWindowPixelBuffer(WIN_ITEM_POCKET, PIXEL_FILL(0));
    description = sItemSelector->pocket == SELECTOR_POCKET_BATTLE ? sText_BattleItems : sText_Berries;
    AddTextPrinterParameterized4(WIN_ITEM_POCKET, FONT_NORMAL, GetStringCenterAlignXOffset(FONT_NORMAL, description, 64), 1, 0, 0, colorsPocket, TEXT_SKIP_DRAW, description);
    CopyWindowToVram(WIN_ITEM_POCKET, COPYWIN_FULL);

    FillWindowPixelBuffer(WIN_ITEM_LIST, PIXEL_FILL(0));
    for (row = 0; row < ITEMS_VISIBLE && scroll + row < count; row++)
    {
        u16 index = scroll + row;
        u16 item = GetPocketItem(sItemSelector->pocket, index);
        const u8 *name = item == ITEM_NONE ? gText_CloseBag : gItems[item].name;
        if (index == cursor)
            AddTextPrinterParameterized4(WIN_ITEM_LIST, FONT_NARROW, 0, 1 + row * 16, 0, 0, colorsNormal, TEXT_SKIP_DRAW, gText_SelectorArrow2);
        AddTextPrinterParameterized4(WIN_ITEM_LIST, FONT_NARROW, 8, 1 + row * 16, 0, 0, colorsNormal, TEXT_SKIP_DRAW, name);
    }
    CopyWindowToVram(WIN_ITEM_LIST, COPYWIN_FULL);

    description = selectedItem == ITEM_NONE ? sText_CloseDescription : GetItemDescription(selectedItem);
    FillWindowPixelBuffer(WIN_ITEM_DESCRIPTION, PIXEL_FILL(0));
    AddTextPrinterParameterized4(WIN_ITEM_DESCRIPTION, FONT_NORMAL, 3, 1, 0, 0, colorsNormal, TEXT_SKIP_DRAW, description);
    CopyWindowToVram(WIN_ITEM_DESCRIPTION, COPYWIN_FULL);
    DrawItemSelectorIcon(selectedItem == ITEM_NONE ? ITEM_LIST_END : selectedItem);
}

static void DrawItemSelectorIcon(u16 item)
{
    if (sItemSelector->iconSpriteId != SPRITE_NONE)
    {
        FreeSpriteTilesByTag(TAG_TEAM_LAB_ITEM_ICON);
        FreeSpritePaletteByTag(TAG_TEAM_LAB_ITEM_ICON);
        DestroySprite(&gSprites[sItemSelector->iconSpriteId]);
        sItemSelector->iconSpriteId = SPRITE_NONE;
    }
    if (item != ITEM_ICON_HIDE)
    {
        u8 spriteId = AddItemIconSprite(TAG_TEAM_LAB_ITEM_ICON, TAG_TEAM_LAB_ITEM_ICON, item);
        if (spriteId != MAX_SPRITES)
        {
            sItemSelector->iconSpriteId = spriteId;
            gSprites[spriteId].x2 = 24;
            gSprites[spriteId].y2 = 88;
        }
    }
}

static void SwitchPocket(s8 direction)
{
    sItemSelector->pocket = (sItemSelector->pocket + direction + SELECTOR_POCKET_COUNT) % SELECTOR_POCKET_COUNT;
    SetIndependentBagVisualPocketId(sItemSelector->bagSpriteId, sItemSelector->pocket == SELECTOR_POCKET_BATTLE ? POCKET_ITEMS : POCKET_BERRIES);
    DrawPocketIndicators();
    DrawItemSelector();
    PlaySE(SE_SELECT);
}

static void Task_HandleTeamLabItemSelector(u8 taskId)
{
    u16 *cursor = &sItemSelector->cursor[sItemSelector->pocket];
    u16 *scroll = &sItemSelector->scroll[sItemSelector->pocket];
    u16 count = GetPocketItemCount(sItemSelector->pocket);

    if (gPaletteFade.active)
        return;
    if (JOY_NEW(L_BUTTON))
    {
        SwitchPocket(-1);
        return;
    }
    if (JOY_NEW(R_BUTTON))
    {
        SwitchPocket(1);
        return;
    }
    if (JOY_NEW(DPAD_UP))
    {
        *cursor = *cursor == 0 ? count - 1 : *cursor - 1;
        if (*cursor < *scroll)
            *scroll = *cursor;
        else if (*cursor >= *scroll + ITEMS_VISIBLE)
            *scroll = *cursor - ITEMS_VISIBLE + 1;
        DrawItemSelector();
        PlaySE(SE_SELECT);
        return;
    }
    if (JOY_NEW(DPAD_DOWN))
    {
        *cursor = (*cursor + 1) % count;
        if (*cursor < *scroll)
            *scroll = *cursor;
        else if (*cursor >= *scroll + ITEMS_VISIBLE)
            *scroll = *cursor - ITEMS_VISIBLE + 1;
        DrawItemSelector();
        PlaySE(SE_SELECT);
        return;
    }
    if (JOY_NEW(A_BUTTON))
    {
        if (GetPocketItem(sItemSelector->pocket, *cursor) != ITEM_NONE)
        {
            sItemSelectionMade = TRUE;
            sItemSelectionResult = GetPocketItem(sItemSelector->pocket, *cursor);
        }
    }
    else if (!JOY_NEW(B_BUTTON))
    {
        return;
    }
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    gTasks[taskId].func = Task_ExitTeamLabItemSelector;
    PlaySE(SE_SELECT);
}

static void Task_ExitTeamLabItemSelector(u8 taskId)
{
    MainCallback returnCallback;

    if (gPaletteFade.active)
        return;
    returnCallback = sItemSelector->returnCallback;
    DrawItemSelectorIcon(ITEM_ICON_HIDE);
    if (sItemSelector->pocketArrowTaskId != TASK_NONE)
        RemoveScrollIndicatorArrowPair(sItemSelector->pocketArrowTaskId);
    SetVBlankCallback(NULL);
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    FreeAllWindowBuffers();
    Free(sItemSelector->bgTilemap);
    Free(sItemSelector);
    sItemSelector = NULL;
    SetMainCallback2(returnCallback);
}
