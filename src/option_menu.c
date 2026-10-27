#include "global.h"
#include "option_menu.h"
#include "audio_settings.h"
#include "bg.h"
#include "decompress.h"
#include "graphics.h"
#include "item_menu_icons.h"
#include "gpu_regs.h"
#include "international_string_util.h"
#include "main.h"
#include "m4a.h"
#include "menu.h"
#include "palette.h"
#include "player_customization.h"
#include "scanline_effect.h"
#include "sprite.h"
#include "strings.h"
#include "task.h"
#include "text.h"
#include "text_window.h"
#include "window.h"
#include "sound.h"
#include "gba/m4a_internal.h"
#include "constants/rgb.h"
#include "constants/songs.h"

#define tMenuSelection data[0]
#define tTextSpeed data[1]
#define tBattleSceneOff data[2]
#define tBattleStyle data[3]
#define tSound data[4]
#define tButtonMode data[5]
#define tWindowFrameType data[6]
#define tAudioMenuSelection data[7]
#define tAudioSound data[8]
#define tAudioMusicMode data[9]
#define tAudioPool data[10]
#define tAudioBeep data[11]
#define tAudioTrack data[12]
#define tPickerSelection data[13]
#define tPickerScroll data[14]
#define tPickerPriorMusic data[15]
#define tPickerPool data[10]

enum
{
    MENUITEM_TEXTSPEED,
    MENUITEM_BATTLESCENE,
    MENUITEM_BATTLESTYLE,
    MENUITEM_AUDIO,
    MENUITEM_BUTTONMODE,
    MENUITEM_APPEARANCE,
    MENUITEM_CANCEL,
    MENUITEM_COUNT,
};

enum
{
    AUDIOMENU_SOUND,
    AUDIOMENU_TRAINER_MUSIC,
    AUDIOMENU_POOL,
    AUDIOMENU_LOW_HP_BEEP,
    AUDIOMENU_BACK,
    AUDIOMENU_COUNT,
};

enum
{
    APPEARANCE_BAG_STYLE,
    APPEARANCE_UI_THEME,
    APPEARANCE_FRAME,
    APPEARANCE_BACK,
    APPEARANCE_COUNT,
};

enum
{
    WIN_HEADER,
    WIN_OPTIONS,
    WIN_APPEARANCE_PREVIEW,
};

#define YPOS_TEXTSPEED    (MENUITEM_TEXTSPEED * 16)
#define YPOS_BATTLESCENE  (MENUITEM_BATTLESCENE * 16)
#define YPOS_BATTLESTYLE  (MENUITEM_BATTLESTYLE * 16)
#define YPOS_AUDIO        (MENUITEM_AUDIO * 16)
#define PICKER_VISIBLE_ROWS 7
#define YPOS_BUTTONMODE   (MENUITEM_BUTTONMODE * 16)
#define YPOS_APPEARANCE   (MENUITEM_APPEARANCE * 16)
#define YPOS_FRAMETYPE    (APPEARANCE_FRAME * 16)

static void Task_OptionMenuFadeIn(u8 taskId);
static void Task_OptionMenuProcessInput(u8 taskId);
static void Task_OptionMenuSave(u8 taskId);
static void Task_OptionMenuFadeOut(u8 taskId);
static void Task_AudioMenuProcessInput(u8 taskId);
static void Task_AppearanceMenuProcessInput(u8 taskId);
static void DrawAppearanceMenu(u8 taskId);
static void UpdateAppearanceBagPreview(void);
static void DestroyAppearanceBagPreview(void);
static void Task_TrainerMusicPickerProcessInput(u8 taskId);
static void HighlightOptionMenuItem(u8 selection);
static u8 TextSpeed_ProcessInput(u8 selection);
static void TextSpeed_DrawChoices(u8 selection);
static u8 BattleScene_ProcessInput(u8 selection);
static void BattleScene_DrawChoices(u8 selection);
static u8 BattleStyle_ProcessInput(u8 selection);
static void BattleStyle_DrawChoices(u8 selection);
static u8 FrameType_ProcessInput(u8 selection);
static void FrameType_DrawChoices(u8 selection);
static u8 ButtonMode_ProcessInput(u8 selection);
static void ButtonMode_DrawChoices(u8 selection);
static void DrawHeaderText(const u8 *title);
static void DrawOptionMenuTexts(void);
static void DrawOptionMenuChoice(const u8 *text, u8 x, u8 y, u8 style);
static void DrawOptionColoredChoice(const u8 *text, u8 x, u8 y, bool8 selected);
static void DrawOptionValue(const u8 *text, u8 y, bool8 selected);
static void DrawOuterOptionChoices(u8 taskId);
static void DrawAudioMenuTexts(void);
static void DrawAudioMenuChoices(u8 taskId);
static void DrawTrainerMusicPicker(u8 taskId);
static void UpdatePickerScroll(u8 taskId, u8 count);
static u16 GetCurrentBgmSong(void);
static void RestorePickerPriorMusic(u8 taskId);
static void DrawBgWindowFrames(void);

EWRAM_DATA static bool8 sArrowPressed = FALSE;
// Keep pending appearance choices separate from the audio picker's task data.
EWRAM_DATA static struct
{
    u8 selection;
    u8 bagStyle;
    u8 uiTheme;
    u8 bagSpriteId;
} sAppearanceMenu = {0};

static const u16 sOptionMenuText_Pal[] = INCGFX_U16("graphics/interface/option_menu_text.pal", ".gbapal");
// note: this is only used in the Japanese release
static const u8 sEqualSignGfx[] = INCGFX_U8("graphics/interface/option_menu_equals_sign.png", ".4bpp");
static const u8 sTextAudio[] = _("AUDIO");
static const u8 sTextAppearance[] = _("APPEARANCE");
static const u8 sTextBagStyle[] = _("BAG STYLE");
static const u8 sTextInterfaceColor[] = _("INTERFACE COLOR");
static const u8 sTextBackpack[] = _("BACKPACK");
static const u8 sTextSatchel[] = _("SATCHEL");
static const u8 sTextBlue[] = _("BLUE");
static const u8 sTextPink[] = _("PINK");
static const u8 sTextPreview[] = _("PREVIEW");
static const u8 sPreviewTextColors[] = {0, 1, 8};
static const union AffineAnimCmd sBagPreviewAffineAnim[] =
{
    AFFINEANIMCMD_FRAME(128, 128, 0, 0),
    AFFINEANIMCMD_END
};
static const union AffineAnimCmd *const sBagPreviewAffineAnims[] =
{
    sBagPreviewAffineAnim,
};
static const u8 sTextPressA[] = _("PRESS A");
static const u8 sTextTrainerMusicRow[] = _("TRAINER MUSIC");
static const u8 sTextTrainerMusic[] = _("MUSIC SEL PREVIEW");
static const u8 sTextMusicPool[] = _("MUSIC POOL");
static const u8 sTextLowHpBeep[] = _("LOW HP BEEP");
static const u8 sTextDefault[] = _("DEFAULT");
static const u8 sTextRandom[] = _("RANDOM");
static const u8 sTextPoolHoenn[] = _("HOENN");
static const u8 sTextPoolKanto[] = _("HOENN+KANTO");
static const u8 sTextPoolAll[] = _("ALL BATTLES");
static const u8 sTextOn[] = _("ON");
static const u8 sTextOff[] = _("OFF");
static const u8 sTextBack[] = _("BACK");
static const u8 sOptionTextColors[] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_GREEN, TEXT_COLOR_LIGHT_GREEN};
static const u8 sOptionSelectedTextColors[] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_RED, TEXT_COLOR_LIGHT_RED};

static const u8 *const sOptionMenuItemsNames[MENUITEM_COUNT] =
{
    [MENUITEM_TEXTSPEED]   = gText_TextSpeed,
    [MENUITEM_BATTLESCENE] = gText_BattleScene,
    [MENUITEM_BATTLESTYLE] = gText_BattleStyle,
    [MENUITEM_AUDIO]       = sTextAudio,
    [MENUITEM_BUTTONMODE]  = gText_ButtonMode,
    [MENUITEM_APPEARANCE]  = sTextAppearance,
    [MENUITEM_CANCEL]      = gText_OptionMenuCancel,
};

static const struct WindowTemplate sOptionMenuWinTemplates[] =
{
    [WIN_HEADER] = {
        .bg = 1,
        .tilemapLeft = 2,
        .tilemapTop = 1,
        .width = 26,
        .height = 2,
        .paletteNum = 1,
        .baseBlock = 2
    },
    [WIN_OPTIONS] = {
        .bg = 0,
        .tilemapLeft = 2,
        .tilemapTop = 5,
        .width = 26,
        .height = 14,
        .paletteNum = 1,
        .baseBlock = 0x36
    },
    [WIN_APPEARANCE_PREVIEW] = {
        .bg = 0,
        .tilemapLeft = 3,
        .tilemapTop = 14,
        .width = 24,
        .height = 4,
        .paletteNum = 13,
        .baseBlock = 0x1AB
    },
    DUMMY_WIN_TEMPLATE
};

static const struct BgTemplate sOptionMenuBgTemplates[] =
{
    {
        .bg = 1,
        .charBaseIndex = 1,
        .mapBaseIndex = 30,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0,
        .baseTile = 0
    },
    {
        .bg = 0,
        .charBaseIndex = 1,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 1,
        .baseTile = 0
    }
};

static const u16 sOptionMenuBg_Pal[] = {RGB(17, 18, 31)};

static void MainCB2(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

static void VBlankCB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

void CB2_InitOptionMenu(void)
{
    switch (gMain.state)
    {
    default:
    case 0:
        SetVBlankCallback(NULL);
        gMain.state++;
        break;
    case 1:
        DmaClearLarge16(3, (void *)(VRAM), VRAM_SIZE, 0x1000);
        DmaClear32(3, OAM, OAM_SIZE);
        DmaClear16(3, PLTT, PLTT_SIZE);
        SetGpuReg(REG_OFFSET_DISPCNT, 0);
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sOptionMenuBgTemplates, ARRAY_COUNT(sOptionMenuBgTemplates));
        ChangeBgX(0, 0, BG_COORD_SET);
        ChangeBgY(0, 0, BG_COORD_SET);
        ChangeBgX(1, 0, BG_COORD_SET);
        ChangeBgY(1, 0, BG_COORD_SET);
        ChangeBgX(2, 0, BG_COORD_SET);
        ChangeBgY(2, 0, BG_COORD_SET);
        ChangeBgX(3, 0, BG_COORD_SET);
        ChangeBgY(3, 0, BG_COORD_SET);
        InitWindows(sOptionMenuWinTemplates);
        DeactivateAllTextPrinters();
        SetGpuReg(REG_OFFSET_WIN0H, 0);
        SetGpuReg(REG_OFFSET_WIN0V, 0);
        SetGpuReg(REG_OFFSET_WININ, WININ_WIN0_BG0 | WININ_WIN0_OBJ);
        SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_BG0 | WINOUT_WIN01_BG1 | WINOUT_WIN01_OBJ | WINOUT_WIN01_CLR);
        SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_BG0 | BLDCNT_EFFECT_DARKEN);
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        SetGpuReg(REG_OFFSET_BLDY, 4);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_WIN0_ON | DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
        ShowBg(0);
        ShowBg(1);
        gMain.state++;
        break;
    case 2:
        ResetPaletteFade();
        ScanlineEffect_Stop();
        ResetTasks();
        ResetSpriteData();
        FreeAllSpritePalettes();
        gMain.state++;
        break;
    case 3:
        LoadBgTiles(1, GetWindowFrameTilesPal(gSaveBlock2Ptr->optionsWindowFrameType)->tiles, 0x120, 0x1A2);
        gMain.state++;
        break;
    case 4:
        LoadPalette(sOptionMenuBg_Pal, BG_PLTT_ID(0), sizeof(sOptionMenuBg_Pal));
        LoadPalette(GetWindowFrameTilesPal(gSaveBlock2Ptr->optionsWindowFrameType)->pal, BG_PLTT_ID(7), PLTT_SIZE_4BPP);
        gMain.state++;
        break;
    case 5:
        LoadPalette(sOptionMenuText_Pal, BG_PLTT_ID(1), sizeof(sOptionMenuText_Pal));
        gMain.state++;
        break;
    case 6:
        PutWindowTilemap(WIN_HEADER);
        DrawHeaderText(gText_Option);
        gMain.state++;
        break;
    case 7:
        gMain.state++;
        break;
    case 8:
        PutWindowTilemap(WIN_OPTIONS);
        DrawOptionMenuTexts();
        gMain.state++;
    case 9:
        DrawBgWindowFrames();
        gMain.state++;
        break;
    case 10:
    {
        u8 taskId = CreateTask(Task_OptionMenuFadeIn, 0);

        gTasks[taskId].tMenuSelection = 0;
        gTasks[taskId].tTextSpeed = gSaveBlock2Ptr->optionsTextSpeed;
        gTasks[taskId].tBattleSceneOff = gSaveBlock2Ptr->optionsBattleSceneOff;
        gTasks[taskId].tBattleStyle = gSaveBlock2Ptr->optionsBattleStyle;
        gTasks[taskId].tSound = gSaveBlock2Ptr->optionsSound;
        gTasks[taskId].tButtonMode = gSaveBlock2Ptr->optionsButtonMode;
        gTasks[taskId].tWindowFrameType = gSaveBlock2Ptr->optionsWindowFrameType;
        gTasks[taskId].tAudioSound = gSaveBlock2Ptr->optionsSound;
        gTasks[taskId].tAudioMusicMode = AudioSettings_GetTrainerMusicMode();
        gTasks[taskId].tAudioPool = AudioSettings_GetMusicPool();
        gTasks[taskId].tAudioBeep = AudioSettings_IsLowHpBeepEnabled();
        gTasks[taskId].tAudioTrack = AudioSettings_GetTrainerTrack();
        sAppearanceMenu.selection = APPEARANCE_BAG_STYLE;
        sAppearanceMenu.bagStyle = GetPlayerBagStyle();
        sAppearanceMenu.uiTheme = GetPlayerUiTheme();
        sAppearanceMenu.bagSpriteId = MAX_SPRITES;

        DrawOuterOptionChoices(taskId);
        HighlightOptionMenuItem(gTasks[taskId].tMenuSelection);

        CopyWindowToVram(WIN_OPTIONS, COPYWIN_FULL);
        gMain.state++;
        break;
    }
    case 11:
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        SetVBlankCallback(VBlankCB);
        SetMainCallback2(MainCB2);
        return;
    }
}

static void Task_OptionMenuFadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
        gTasks[taskId].func = Task_OptionMenuProcessInput;
}

static void Task_OptionMenuProcessInput(u8 taskId)
{
    if (JOY_NEW(A_BUTTON))
    {
        if (gTasks[taskId].tMenuSelection == MENUITEM_CANCEL)
            gTasks[taskId].func = Task_OptionMenuSave;
        else if (gTasks[taskId].tMenuSelection == MENUITEM_AUDIO)
        {
            gTasks[taskId].tAudioMenuSelection = AUDIOMENU_SOUND;
            DrawAudioMenuTexts();
            DrawAudioMenuChoices(taskId);
            DrawHeaderText(sTextAudio);
            HighlightOptionMenuItem(gTasks[taskId].tAudioMenuSelection);
            gTasks[taskId].func = Task_AudioMenuProcessInput;
            CopyWindowToVram(WIN_OPTIONS, COPYWIN_FULL);
        }
        else if (gTasks[taskId].tMenuSelection == MENUITEM_APPEARANCE)
        {
            sAppearanceMenu.selection = APPEARANCE_BAG_STYLE;
            DrawHeaderText(sTextAppearance);
            UpdateAppearanceBagPreview();
            DrawAppearanceMenu(taskId);
            HighlightOptionMenuItem(sAppearanceMenu.selection);
            gTasks[taskId].func = Task_AppearanceMenuProcessInput;
        }
    }
    else if (JOY_NEW(B_BUTTON))
    {
        gTasks[taskId].func = Task_OptionMenuSave;
    }
    else if (JOY_NEW(DPAD_UP))
    {
        if (gTasks[taskId].tMenuSelection > 0)
            gTasks[taskId].tMenuSelection--;
        else
            gTasks[taskId].tMenuSelection = MENUITEM_CANCEL;
        HighlightOptionMenuItem(gTasks[taskId].tMenuSelection);
    }
    else if (JOY_NEW(DPAD_DOWN))
    {
        if (gTasks[taskId].tMenuSelection < MENUITEM_CANCEL)
            gTasks[taskId].tMenuSelection++;
        else
            gTasks[taskId].tMenuSelection = 0;
        HighlightOptionMenuItem(gTasks[taskId].tMenuSelection);
    }
    else
    {
        u8 previousOption;

        switch (gTasks[taskId].tMenuSelection)
        {
        case MENUITEM_TEXTSPEED:
            previousOption = gTasks[taskId].tTextSpeed;
            gTasks[taskId].tTextSpeed = TextSpeed_ProcessInput(gTasks[taskId].tTextSpeed);

            if (previousOption != gTasks[taskId].tTextSpeed)
                TextSpeed_DrawChoices(gTasks[taskId].tTextSpeed);
            break;
        case MENUITEM_BATTLESCENE:
            previousOption = gTasks[taskId].tBattleSceneOff;
            gTasks[taskId].tBattleSceneOff = BattleScene_ProcessInput(gTasks[taskId].tBattleSceneOff);

            if (previousOption != gTasks[taskId].tBattleSceneOff)
                BattleScene_DrawChoices(gTasks[taskId].tBattleSceneOff);
            break;
        case MENUITEM_BATTLESTYLE:
            previousOption = gTasks[taskId].tBattleStyle;
            gTasks[taskId].tBattleStyle = BattleStyle_ProcessInput(gTasks[taskId].tBattleStyle);

            if (previousOption != gTasks[taskId].tBattleStyle)
                BattleStyle_DrawChoices(gTasks[taskId].tBattleStyle);
            break;
        case MENUITEM_AUDIO:
            return;
        case MENUITEM_BUTTONMODE:
            previousOption = gTasks[taskId].tButtonMode;
            gTasks[taskId].tButtonMode = ButtonMode_ProcessInput(gTasks[taskId].tButtonMode);

            if (previousOption != gTasks[taskId].tButtonMode)
                ButtonMode_DrawChoices(gTasks[taskId].tButtonMode);
            break;
        case MENUITEM_APPEARANCE:
            return;
        default:
            return;
        }

        if (sArrowPressed)
        {
            sArrowPressed = FALSE;
            CopyWindowToVram(WIN_OPTIONS, COPYWIN_GFX);
        }
    }
}

static void ReturnToOptionMenu(u8 taskId, u8 selection)
{
    DrawHeaderText(gText_Option);
    DrawOptionMenuTexts();
    DrawOuterOptionChoices(taskId);
    PutWindowTilemap(WIN_OPTIONS);
    HighlightOptionMenuItem(selection);
    gTasks[taskId].tMenuSelection = selection;
    gTasks[taskId].func = Task_OptionMenuProcessInput;
    CopyWindowToVram(WIN_OPTIONS, COPYWIN_FULL);
}

static void DestroyAppearanceBagPreview(void)
{
    if (sAppearanceMenu.bagSpriteId < MAX_SPRITES)
        DestroySpriteAndFreeResources(&gSprites[sAppearanceMenu.bagSpriteId]);
    else
    {
        // Loading can succeed even when no sprite slot remains.
        FreeSpriteTilesByTag(gBagMaleSpriteSheet.tag);
        FreeSpritePaletteByTag(gBagPaletteTable.tag);
    }
    sAppearanceMenu.bagSpriteId = MAX_SPRITES;
}

static void UpdateAppearanceBagPreview(void)
{
    struct Sprite *sprite;

    DestroyAppearanceBagPreview();
    LoadCompressedSpriteSheet(sAppearanceMenu.bagStyle == PLAYER_BAG_BACKPACK ? &gBagMaleSpriteSheet : &gBagFemaleSpriteSheet);
    LoadCompressedSpritePalette(&gBagPaletteTable);
    if (GetSpriteTileStartByTag(gBagMaleSpriteSheet.tag) == 0xFFFF
        || IndexOfSpritePaletteTag(gBagPaletteTable.tag) == 0xFF)
    {
        DestroyAppearanceBagPreview();
        return;
    }

    sAppearanceMenu.bagSpriteId = CreateIndependentBagVisualSprite(0);
    if (sAppearanceMenu.bagSpriteId >= MAX_SPRITES)
    {
        DestroyAppearanceBagPreview();
        return;
    }

    sprite = &gSprites[sAppearanceMenu.bagSpriteId];
    sprite->x = 184;
    sprite->y = 128;
    sprite->oam.priority = 0;
    sprite->affineAnims = sBagPreviewAffineAnims;
    StartSpriteAffineAnim(sprite, 0);
}

static void DrawAppearanceMenu(u8 taskId)
{
    FillWindowPixelBuffer(WIN_OPTIONS, PIXEL_FILL(1));
    AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, sTextBagStyle, 8, 1, TEXT_SKIP_DRAW, NULL);
    AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, sTextInterfaceColor, 8, 17, TEXT_SKIP_DRAW, NULL);
    AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, gText_Frame, 8, 33, TEXT_SKIP_DRAW, NULL);
    AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, sTextBack, 8, 49, TEXT_SKIP_DRAW, NULL);
    DrawOptionValue(sAppearanceMenu.bagStyle == PLAYER_BAG_BACKPACK ? sTextBackpack : sTextSatchel,
                   0, sAppearanceMenu.selection == APPEARANCE_BAG_STYLE);
    DrawOptionValue(sAppearanceMenu.uiTheme == PLAYER_THEME_BLUE ? sTextBlue : sTextPink,
                   16, sAppearanceMenu.selection == APPEARANCE_UI_THEME);
    FrameType_DrawChoices(gTasks[taskId].tWindowFrameType);
    CopyWindowToVram(WIN_OPTIONS, COPYWIN_FULL);

    LoadCompressedPalette(sAppearanceMenu.uiTheme == PLAYER_THEME_BLUE ? gBagScreenMale_Pal : gBagScreenFemale_Pal,
                          BG_PLTT_ID(13), PLTT_SIZE_4BPP);
    FillWindowPixelBuffer(WIN_APPEARANCE_PREVIEW, PIXEL_FILL(7));
    FillWindowPixelRect(WIN_APPEARANCE_PREVIEW, 8, 0, 0, 192, 2);
    FillWindowPixelRect(WIN_APPEARANCE_PREVIEW, 8, 0, 30, 192, 2);
    FillWindowPixelRect(WIN_APPEARANCE_PREVIEW, 8, 0, 0, 2, 32);
    FillWindowPixelRect(WIN_APPEARANCE_PREVIEW, 8, 190, 0, 2, 32);
    AddTextPrinterParameterized3(WIN_APPEARANCE_PREVIEW, FONT_NORMAL, 8, 9,
                                sPreviewTextColors, TEXT_SKIP_DRAW, sTextPreview);
    PutWindowTilemap(WIN_APPEARANCE_PREVIEW);
    CopyWindowToVram(WIN_APPEARANCE_PREVIEW, COPYWIN_FULL);
}

static void Task_AppearanceMenuProcessInput(u8 taskId)
{
    bool8 redraw = FALSE;

    if (JOY_NEW(B_BUTTON)
        || (JOY_NEW(A_BUTTON) && sAppearanceMenu.selection == APPEARANCE_BACK))
    {
        DestroyAppearanceBagPreview();
        ClearWindowTilemap(WIN_APPEARANCE_PREVIEW);
        ReturnToOptionMenu(taskId, MENUITEM_APPEARANCE);
        return;
    }
    else if (JOY_NEW(DPAD_UP))
    {
        if (sAppearanceMenu.selection == 0)
            sAppearanceMenu.selection = APPEARANCE_BACK;
        else
            sAppearanceMenu.selection--;
        redraw = TRUE;
    }
    else if (JOY_NEW(DPAD_DOWN))
    {
        sAppearanceMenu.selection = (sAppearanceMenu.selection + 1) % APPEARANCE_COUNT;
        redraw = TRUE;
    }
    else if (JOY_NEW(DPAD_LEFT | DPAD_RIGHT))
    {
        switch (sAppearanceMenu.selection)
        {
        case APPEARANCE_BAG_STYLE:
            sAppearanceMenu.bagStyle ^= 1;
            UpdateAppearanceBagPreview();
            redraw = TRUE;
            break;
        case APPEARANCE_UI_THEME:
            sAppearanceMenu.uiTheme ^= 1;
            redraw = TRUE;
            break;
        case APPEARANCE_FRAME:
            gTasks[taskId].tWindowFrameType = FrameType_ProcessInput(gTasks[taskId].tWindowFrameType);
            sArrowPressed = FALSE;
            redraw = TRUE;
            break;
        }
    }

    if (redraw)
    {
        DrawAppearanceMenu(taskId);
        HighlightOptionMenuItem(sAppearanceMenu.selection);
    }
}

static void Task_AudioMenuProcessInput(u8 taskId)
{
    u8 previous;
    bool8 redraw = FALSE;

    if (JOY_NEW(A_BUTTON))
    {
        if (gTasks[taskId].tAudioMenuSelection == AUDIOMENU_TRAINER_MUSIC)
        {
            gTasks[taskId].tPickerPool = gTasks[taskId].tAudioPool;
            if (IsBGMPausedOrStopped())
                gTasks[taskId].tPickerPriorMusic = MUS_NONE;
            else
                gTasks[taskId].tPickerPriorMusic = GetCurrentBgmSong();
            if (gTasks[taskId].tAudioMusicMode == AUDIO_TRAINER_MUSIC_FIXED)
            {
                gTasks[taskId].tPickerSelection = 2;
                for (previous = 0; previous < AudioSettings_GetPoolTrackCount(gTasks[taskId].tPickerPool); previous++)
                    if (AudioSettings_GetPoolTrack(gTasks[taskId].tPickerPool, previous) == gTasks[taskId].tAudioTrack)
                        gTasks[taskId].tPickerSelection = previous + 2;
            }
            else if (gTasks[taskId].tAudioMusicMode == AUDIO_TRAINER_MUSIC_RANDOM)
                gTasks[taskId].tPickerSelection = 1;
            else
                gTasks[taskId].tPickerSelection = 0;
            gTasks[taskId].tPickerScroll = 0;
            UpdatePickerScroll(taskId, AudioSettings_GetPoolTrackCount(gTasks[taskId].tPickerPool) + 2);
            DrawTrainerMusicPicker(taskId);
            DrawHeaderText(sTextTrainerMusic);
            HighlightOptionMenuItem(gTasks[taskId].tPickerSelection - gTasks[taskId].tPickerScroll);
            gTasks[taskId].func = Task_TrainerMusicPickerProcessInput;
            redraw = TRUE;
        }
        else if (gTasks[taskId].tAudioMenuSelection == AUDIOMENU_BACK)
            ReturnToOptionMenu(taskId, MENUITEM_AUDIO);
    }
    else if (JOY_NEW(B_BUTTON))
    {
        ReturnToOptionMenu(taskId, MENUITEM_AUDIO);
    }
    else if (JOY_NEW(DPAD_UP))
    {
        if (gTasks[taskId].tAudioMenuSelection == 0)
            gTasks[taskId].tAudioMenuSelection = AUDIOMENU_BACK;
        else
            gTasks[taskId].tAudioMenuSelection--;
        HighlightOptionMenuItem(gTasks[taskId].tAudioMenuSelection);
        DrawAudioMenuChoices(taskId);
        redraw = TRUE;
    }
    else if (JOY_NEW(DPAD_DOWN))
    {
        if (gTasks[taskId].tAudioMenuSelection == AUDIOMENU_BACK)
            gTasks[taskId].tAudioMenuSelection = 0;
        else
            gTasks[taskId].tAudioMenuSelection++;
        HighlightOptionMenuItem(gTasks[taskId].tAudioMenuSelection);
        DrawAudioMenuChoices(taskId);
        redraw = TRUE;
    }
    else if (JOY_NEW(DPAD_LEFT | DPAD_RIGHT))
    {
        previous = 0xFF;
        switch (gTasks[taskId].tAudioMenuSelection)
        {
        case AUDIOMENU_SOUND:
            previous = gTasks[taskId].tAudioSound;
            gTasks[taskId].tAudioSound ^= 1;
            SetPokemonCryStereo(gTasks[taskId].tAudioSound);
            break;
        case AUDIOMENU_POOL:
            previous = gTasks[taskId].tAudioPool;
            if (JOY_NEW(DPAD_RIGHT))
                gTasks[taskId].tAudioPool = (gTasks[taskId].tAudioPool + 1) % 3;
            else if (JOY_NEW(DPAD_LEFT))
                gTasks[taskId].tAudioPool = gTasks[taskId].tAudioPool == 0 ? 2 : gTasks[taskId].tAudioPool - 1;
            if (gTasks[taskId].tAudioMusicMode == AUDIO_TRAINER_MUSIC_FIXED
                && !AudioSettings_IsTrackInPool(gTasks[taskId].tAudioTrack, gTasks[taskId].tAudioPool))
                gTasks[taskId].tAudioMusicMode = AUDIO_TRAINER_MUSIC_DEFAULT;
            break;
        case AUDIOMENU_LOW_HP_BEEP:
            previous = gTasks[taskId].tAudioBeep;
            gTasks[taskId].tAudioBeep ^= 1;
            break;
        }
        if (previous != 0xFF)
        {
            DrawAudioMenuChoices(taskId);
            redraw = TRUE;
        }
    }
    if (redraw)
        CopyWindowToVram(WIN_OPTIONS, COPYWIN_GFX);
}

static void Task_TrainerMusicPickerProcessInput(u8 taskId)
{
    u8 count = AudioSettings_GetPoolTrackCount(gTasks[taskId].tPickerPool) + 2;
    u8 selected;
    bool8 redraw = FALSE;

    if (JOY_NEW(SELECT_BUTTON) && gTasks[taskId].tPickerSelection >= 2)
    {
        selected = AudioSettings_GetPoolTrack(gTasks[taskId].tPickerPool, gTasks[taskId].tPickerSelection - 2);
        PlayBGM(AudioSettings_GetTrackSong(selected));
    }
    else if (JOY_NEW(A_BUTTON))
    {
        selected = gTasks[taskId].tPickerSelection;
        RestorePickerPriorMusic(taskId);
        if (selected == 0)
            gTasks[taskId].tAudioMusicMode = AUDIO_TRAINER_MUSIC_DEFAULT;
        else if (selected == 1)
            gTasks[taskId].tAudioMusicMode = AUDIO_TRAINER_MUSIC_RANDOM;
        else
        {
            gTasks[taskId].tAudioMusicMode = AUDIO_TRAINER_MUSIC_FIXED;
            gTasks[taskId].tAudioTrack = AudioSettings_GetPoolTrack(gTasks[taskId].tPickerPool, selected - 2);
        }
        DrawAudioMenuTexts();
        DrawAudioMenuChoices(taskId);
        DrawHeaderText(sTextAudio);
        HighlightOptionMenuItem(gTasks[taskId].tAudioMenuSelection);
        gTasks[taskId].func = Task_AudioMenuProcessInput;
        redraw = TRUE;
    }
    else if (JOY_NEW(B_BUTTON))
    {
        RestorePickerPriorMusic(taskId);
        DrawAudioMenuTexts();
        DrawAudioMenuChoices(taskId);
        DrawHeaderText(sTextAudio);
        HighlightOptionMenuItem(gTasks[taskId].tAudioMenuSelection);
        gTasks[taskId].func = Task_AudioMenuProcessInput;
        redraw = TRUE;
    }
    else if (JOY_NEW(DPAD_UP))
    {
        if (gTasks[taskId].tPickerSelection == 0)
            gTasks[taskId].tPickerSelection = count - 1;
        else
            gTasks[taskId].tPickerSelection--;
        UpdatePickerScroll(taskId, count);
        DrawTrainerMusicPicker(taskId);
        HighlightOptionMenuItem(gTasks[taskId].tPickerSelection - gTasks[taskId].tPickerScroll);
        redraw = TRUE;
    }
    else if (JOY_NEW(DPAD_DOWN))
    {
        if (gTasks[taskId].tPickerSelection + 1 >= count)
            gTasks[taskId].tPickerSelection = 0;
        else
            gTasks[taskId].tPickerSelection++;
        UpdatePickerScroll(taskId, count);
        DrawTrainerMusicPicker(taskId);
        HighlightOptionMenuItem(gTasks[taskId].tPickerSelection - gTasks[taskId].tPickerScroll);
        redraw = TRUE;
    }
    if (redraw)
        CopyWindowToVram(WIN_OPTIONS, COPYWIN_GFX);
}

static void Task_OptionMenuSave(u8 taskId)
{
    gSaveBlock2Ptr->optionsTextSpeed = gTasks[taskId].tTextSpeed;
    gSaveBlock2Ptr->optionsBattleSceneOff = gTasks[taskId].tBattleSceneOff;
    gSaveBlock2Ptr->optionsBattleStyle = gTasks[taskId].tBattleStyle;
    gSaveBlock2Ptr->optionsSound = gTasks[taskId].tAudioSound;
    gSaveBlock2Ptr->optionsButtonMode = gTasks[taskId].tButtonMode;
    gSaveBlock2Ptr->optionsWindowFrameType = gTasks[taskId].tWindowFrameType;

    SetPlayerBagStyle(sAppearanceMenu.bagStyle);
    SetPlayerUiTheme(sAppearanceMenu.uiTheme);

    AudioSettings_SetMusicPool(gTasks[taskId].tAudioPool);
    AudioSettings_SetTrainerTrack(gTasks[taskId].tAudioTrack);
    AudioSettings_SetTrainerMusicMode(gTasks[taskId].tAudioMusicMode);
    AudioSettings_SetLowHpBeepEnabled(gTasks[taskId].tAudioBeep);

    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    gTasks[taskId].func = Task_OptionMenuFadeOut;
}

static void Task_OptionMenuFadeOut(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        DestroyTask(taskId);
        FreeAllWindowBuffers();
        SetMainCallback2(gMain.savedCallback);
    }
}

static void HighlightOptionMenuItem(u8 index)
{
    SetGpuReg(REG_OFFSET_WIN0H, WIN_RANGE(16, DISPLAY_WIDTH - 16));
    SetGpuReg(REG_OFFSET_WIN0V, WIN_RANGE(index * 16 + 40, index * 16 + 56));
}

static void DrawOptionMenuChoice(const u8 *text, u8 x, u8 y, u8 style)
{
    u8 dst[16];
    u16 i;

    for (i = 0; *text != EOS && i < ARRAY_COUNT(dst) - 1; i++)
        dst[i] = *(text++);

    if (style != 0)
    {
        dst[2] = TEXT_COLOR_RED;
        dst[5] = TEXT_COLOR_LIGHT_RED;
    }

    dst[i] = EOS;
    AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, dst, x, y + 1, TEXT_SKIP_DRAW, NULL);
}

static u8 TextSpeed_ProcessInput(u8 selection)
{
    if (JOY_NEW(DPAD_RIGHT))
    {
        if (selection <= 1)
            selection++;
        else
            selection = 0;

        sArrowPressed = TRUE;
    }
    if (JOY_NEW(DPAD_LEFT))
    {
        if (selection != 0)
            selection--;
        else
            selection = 2;

        sArrowPressed = TRUE;
    }
    return selection;
}

static void TextSpeed_DrawChoices(u8 selection)
{
    u8 styles[3];
    s32 widthSlow, widthMid, widthFast, xMid;

    styles[0] = 0;
    styles[1] = 0;
    styles[2] = 0;
    styles[selection] = 1;

    DrawOptionMenuChoice(gText_TextSpeedSlow, 104, YPOS_TEXTSPEED, styles[0]);

    widthSlow = GetStringWidth(FONT_NORMAL, gText_TextSpeedSlow, 0);
    widthMid = GetStringWidth(FONT_NORMAL, gText_TextSpeedMid, 0);
    widthFast = GetStringWidth(FONT_NORMAL, gText_TextSpeedFast, 0);

    widthMid -= 94;
    xMid = (widthSlow - widthMid - widthFast) / 2 + 104;
    DrawOptionMenuChoice(gText_TextSpeedMid, xMid, YPOS_TEXTSPEED, styles[1]);

    DrawOptionMenuChoice(gText_TextSpeedFast, GetStringRightAlignXOffset(FONT_NORMAL, gText_TextSpeedFast, 198), YPOS_TEXTSPEED, styles[2]);
}

static u8 BattleScene_ProcessInput(u8 selection)
{
    if (JOY_NEW(DPAD_LEFT | DPAD_RIGHT))
    {
        selection ^= 1;
        sArrowPressed = TRUE;
    }

    return selection;
}

static void BattleScene_DrawChoices(u8 selection)
{
    u8 styles[2];

    styles[0] = 0;
    styles[1] = 0;
    styles[selection] = 1;

    DrawOptionMenuChoice(gText_BattleSceneOn, 104, YPOS_BATTLESCENE, styles[0]);
    DrawOptionMenuChoice(gText_BattleSceneOff, GetStringRightAlignXOffset(FONT_NORMAL, gText_BattleSceneOff, 198), YPOS_BATTLESCENE, styles[1]);
}

static u8 BattleStyle_ProcessInput(u8 selection)
{
    if (JOY_NEW(DPAD_LEFT | DPAD_RIGHT))
    {
        selection ^= 1;
        sArrowPressed = TRUE;
    }

    return selection;
}

static void BattleStyle_DrawChoices(u8 selection)
{
    u8 styles[2];

    styles[0] = 0;
    styles[1] = 0;
    styles[selection] = 1;

    DrawOptionMenuChoice(gText_BattleStyleShift, 104, YPOS_BATTLESTYLE, styles[0]);
    DrawOptionMenuChoice(gText_BattleStyleSet, GetStringRightAlignXOffset(FONT_NORMAL, gText_BattleStyleSet, 198), YPOS_BATTLESTYLE, styles[1]);
}

static const u8 *GetAudioModeText(u8 mode, u8 track)
{
    if (mode == AUDIO_TRAINER_MUSIC_RANDOM)
        return sTextRandom;
    if (mode == AUDIO_TRAINER_MUSIC_FIXED)
        return AudioSettings_GetTrackName(track);
    return sTextDefault;
}

static const u8 *GetAudioPoolText(u8 pool)
{
    if (pool == AUDIO_MUSIC_POOL_HOENN)
        return sTextPoolHoenn;
    if (pool == AUDIO_MUSIC_POOL_ALL_BATTLES)
        return sTextPoolAll;
    return sTextPoolKanto;
}

static void DrawOptionValue(const u8 *text, u8 y, bool8 selected)
{
    DrawOptionColoredChoice(text, GetStringRightAlignXOffset(FONT_NORMAL, text, 198), y, selected);
}

static void DrawOptionColoredChoice(const u8 *text, u8 x, u8 y, bool8 selected)
{
    AddTextPrinterParameterized3(WIN_OPTIONS, FONT_NORMAL, x, y + 1,
                                 selected ? sOptionSelectedTextColors : sOptionTextColors,
                                 TEXT_SKIP_DRAW, text);
}

static void DrawAudioMenuTexts(void)
{
    FillWindowPixelBuffer(WIN_OPTIONS, PIXEL_FILL(1));
    AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, gText_Sound, 8, 1, TEXT_SKIP_DRAW, NULL);
    AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, sTextTrainerMusicRow, 8, 17, TEXT_SKIP_DRAW, NULL);
    AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, sTextMusicPool, 8, 33, TEXT_SKIP_DRAW, NULL);
    AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, sTextLowHpBeep, 8, 49, TEXT_SKIP_DRAW, NULL);
    AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, sTextBack, 8, 65, TEXT_SKIP_DRAW, NULL);
}

static void DrawAudioMenuChoices(u8 taskId)
{
    u8 style;

    DrawAudioMenuTexts();
    style = gTasks[taskId].tAudioMenuSelection == AUDIOMENU_SOUND;
    DrawOptionValue(gTasks[taskId].tAudioSound ? gText_SoundStereo : gText_SoundMono, 0, style);
    style = gTasks[taskId].tAudioMenuSelection == AUDIOMENU_TRAINER_MUSIC;
    DrawOptionValue(GetAudioModeText(gTasks[taskId].tAudioMusicMode, gTasks[taskId].tAudioTrack), 16, style);
    style = gTasks[taskId].tAudioMenuSelection == AUDIOMENU_POOL;
    DrawOptionValue(GetAudioPoolText(gTasks[taskId].tAudioPool), 32, style);
    style = gTasks[taskId].tAudioMenuSelection == AUDIOMENU_LOW_HP_BEEP;
    DrawOptionValue(gTasks[taskId].tAudioBeep ? sTextOn : sTextOff, 48, style);
}

static void DrawOuterOptionChoices(u8 taskId)
{
    TextSpeed_DrawChoices(gTasks[taskId].tTextSpeed);
    BattleScene_DrawChoices(gTasks[taskId].tBattleSceneOff);
    BattleStyle_DrawChoices(gTasks[taskId].tBattleStyle);
    DrawOptionMenuChoice(sTextPressA, GetStringRightAlignXOffset(FONT_NORMAL, sTextPressA, 198), YPOS_AUDIO, 0);
    ButtonMode_DrawChoices(gTasks[taskId].tButtonMode);
    DrawOptionMenuChoice(sTextPressA, GetStringRightAlignXOffset(FONT_NORMAL, sTextPressA, 198), YPOS_APPEARANCE, 0);
}

static void DrawTrainerMusicPicker(u8 taskId)
{
    u8 i;
    u8 count = AudioSettings_GetPoolTrackCount(gTasks[taskId].tPickerPool) + 2;
    u8 index;
    const u8 *text;

    FillWindowPixelBuffer(WIN_OPTIONS, PIXEL_FILL(1));
    for (i = 0; i < PICKER_VISIBLE_ROWS; i++)
    {
        index = gTasks[taskId].tPickerScroll + i;
        if (index >= count)
            break;
        if (index == 0)
            text = sTextDefault;
        else if (index == 1)
            text = sTextRandom;
        else
            text = AudioSettings_GetTrackName(AudioSettings_GetPoolTrack(gTasks[taskId].tPickerPool, index - 2));
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, text, 8, i * 16 + 1, TEXT_SKIP_DRAW, NULL);
        if (index == gTasks[taskId].tPickerSelection)
            DrawOptionColoredChoice(text, 8, i * 16, TRUE);
    }
}

static u16 GetCurrentBgmSong(void)
{
    const struct SongHeader *songHeader = gMPlayInfo_BGM.songHeader;
    u16 songId;

    if (songHeader == NULL)
        return GetCurrentMapMusic();
    for (songId = 0; songId <= LAST_PHONEME_SONG; songId++)
    {
        if (gSongTable[songId].header == songHeader)
            return songId;
    }
    return GetCurrentMapMusic();
}

static void RestorePickerPriorMusic(u8 taskId)
{
    if (gTasks[taskId].tPickerPriorMusic == -1)
        m4aMPlayStop(&gMPlayInfo_BGM);
    else
        PlayBGM(gTasks[taskId].tPickerPriorMusic);
}

static void UpdatePickerScroll(u8 taskId, u8 count)
{
    u8 maxScroll = count > PICKER_VISIBLE_ROWS ? count - PICKER_VISIBLE_ROWS : 0;

    if (gTasks[taskId].tPickerSelection < gTasks[taskId].tPickerScroll)
        gTasks[taskId].tPickerScroll = gTasks[taskId].tPickerSelection;
    else if (gTasks[taskId].tPickerSelection >= gTasks[taskId].tPickerScroll + PICKER_VISIBLE_ROWS)
        gTasks[taskId].tPickerScroll = gTasks[taskId].tPickerSelection - PICKER_VISIBLE_ROWS + 1;
    if (gTasks[taskId].tPickerScroll > maxScroll)
        gTasks[taskId].tPickerScroll = maxScroll;
}

static u8 FrameType_ProcessInput(u8 selection)
{
    if (JOY_NEW(DPAD_RIGHT))
    {
        if (selection < WINDOW_FRAMES_COUNT - 1)
            selection++;
        else
            selection = 0;

        LoadBgTiles(1, GetWindowFrameTilesPal(selection)->tiles, 0x120, 0x1A2);
        LoadPalette(GetWindowFrameTilesPal(selection)->pal, BG_PLTT_ID(7), PLTT_SIZE_4BPP);
        sArrowPressed = TRUE;
    }
    if (JOY_NEW(DPAD_LEFT))
    {
        if (selection != 0)
            selection--;
        else
            selection = WINDOW_FRAMES_COUNT - 1;

        LoadBgTiles(1, GetWindowFrameTilesPal(selection)->tiles, 0x120, 0x1A2);
        LoadPalette(GetWindowFrameTilesPal(selection)->pal, BG_PLTT_ID(7), PLTT_SIZE_4BPP);
        sArrowPressed = TRUE;
    }
    return selection;
}

static void FrameType_DrawChoices(u8 selection)
{
    u8 text[16];
    u8 n = selection + 1;
    u16 i;

    for (i = 0; gText_FrameTypeNumber[i] != EOS && i <= 5; i++)
        text[i] = gText_FrameTypeNumber[i];

    // Convert a number to decimal string
    if (n / 10 != 0)
    {
        text[i] = n / 10 + CHAR_0;
        i++;
        text[i] = n % 10 + CHAR_0;
        i++;
    }
    else
    {
        text[i] = n % 10 + CHAR_0;
        i++;
        text[i] = CHAR_SPACER;
        i++;
    }

    text[i] = EOS;

    DrawOptionMenuChoice(gText_FrameType, 104, YPOS_FRAMETYPE, 0);
    DrawOptionMenuChoice(text, 128, YPOS_FRAMETYPE, 1);
}

static u8 ButtonMode_ProcessInput(u8 selection)
{
    if (JOY_NEW(DPAD_RIGHT))
    {
        if (selection <= 1)
            selection++;
        else
            selection = 0;

        sArrowPressed = TRUE;
    }
    if (JOY_NEW(DPAD_LEFT))
    {
        if (selection != 0)
            selection--;
        else
            selection = 2;

        sArrowPressed = TRUE;
    }
    return selection;
}

static void ButtonMode_DrawChoices(u8 selection)
{
    s32 widthNormal, widthLR, widthLA, xLR;
    u8 styles[3];

    styles[0] = 0;
    styles[1] = 0;
    styles[2] = 0;
    styles[selection] = 1;

    DrawOptionMenuChoice(gText_ButtonTypeNormal, 104, YPOS_BUTTONMODE, styles[0]);

    widthNormal = GetStringWidth(FONT_NORMAL, gText_ButtonTypeNormal, 0);
    widthLR = GetStringWidth(FONT_NORMAL, gText_ButtonTypeLR, 0);
    widthLA = GetStringWidth(FONT_NORMAL, gText_ButtonTypeLEqualsA, 0);

    widthLR -= 94;
    xLR = (widthNormal - widthLR - widthLA) / 2 + 104;
    DrawOptionMenuChoice(gText_ButtonTypeLR, xLR, YPOS_BUTTONMODE, styles[1]);

    DrawOptionMenuChoice(gText_ButtonTypeLEqualsA, GetStringRightAlignXOffset(FONT_NORMAL, gText_ButtonTypeLEqualsA, 198), YPOS_BUTTONMODE, styles[2]);
}

static void DrawHeaderText(const u8 *title)
{
    FillWindowPixelBuffer(WIN_HEADER, PIXEL_FILL(1));
    AddTextPrinterParameterized(WIN_HEADER, FONT_NORMAL, title, 8, 1, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(WIN_HEADER, COPYWIN_FULL);
}

static void DrawOptionMenuTexts(void)
{
    u8 i;

    FillWindowPixelBuffer(WIN_OPTIONS, PIXEL_FILL(1));
    for (i = 0; i < MENUITEM_COUNT; i++)
        AddTextPrinterParameterized(WIN_OPTIONS, FONT_NORMAL, sOptionMenuItemsNames[i], 8, (i * 16) + 1, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(WIN_OPTIONS, COPYWIN_FULL);
}

#define TILE_TOP_CORNER_L 0x1A2
#define TILE_TOP_EDGE     0x1A3
#define TILE_TOP_CORNER_R 0x1A4
#define TILE_LEFT_EDGE    0x1A5
#define TILE_RIGHT_EDGE   0x1A7
#define TILE_BOT_CORNER_L 0x1A8
#define TILE_BOT_EDGE     0x1A9
#define TILE_BOT_CORNER_R 0x1AA

static void DrawBgWindowFrames(void)
{
    //                     bg, tile,              x, y, width, height, palNum
    // Draw title window frame
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_L,  1,  0,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_EDGE,      2,  0, 27,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_R, 28,  0,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_LEFT_EDGE,     1,  1,  1,  2,  7);
    FillBgTilemapBufferRect(1, TILE_RIGHT_EDGE,   28,  1,  1,  2,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_L,  1,  3,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_EDGE,      2,  3, 27,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_R, 28,  3,  1,  1,  7);

    // Draw options list window frame
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_L,  1,  4,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_EDGE,      2,  4, 26,  1,  7);
    FillBgTilemapBufferRect(1, TILE_TOP_CORNER_R, 28,  4,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_LEFT_EDGE,     1,  5,  1, 18,  7);
    FillBgTilemapBufferRect(1, TILE_RIGHT_EDGE,   28,  5,  1, 18,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_L,  1, 19,  1,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_EDGE,      2, 19, 26,  1,  7);
    FillBgTilemapBufferRect(1, TILE_BOT_CORNER_R, 28, 19,  1,  1,  7);

    CopyBgTilemapBufferToVram(1);
}
