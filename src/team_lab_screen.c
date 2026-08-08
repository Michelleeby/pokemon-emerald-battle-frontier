#include "global.h"
#include "bg.h"
#include "battle_main.h"
#include "data.h"
#include "decompress.h"
#include "gpu_regs.h"
#include "graphics.h"
#include "item.h"
#include "list_menu.h"
#include "main.h"
#include "malloc.h"
#include "menu.h"
#include "menu_helpers.h"
#include "palette.h"
#include "party_menu.h"
#include "pokemon.h"
#include "pokemon_icon.h"
#include "pokemon_summary_screen.h"
#include "pokedex.h"
#include "naming_screen.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "strings.h"
#include "task.h"
#include "team_lab.h"
#include "text.h"
#include "text_window.h"
#include "window.h"
#include "constants/abilities.h"
#include "constants/rgb.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/songs.h"

enum
{
    TEAM_LAB_PAGE_BUILD,
    TEAM_LAB_PAGE_STATS,
    TEAM_LAB_PAGE_MOVES,
    TEAM_LAB_PAGE_COUNT,
};

enum
{
    TEAM_LAB_MODE_NORMAL,
    TEAM_LAB_MODE_NATURE_RAISED,
    TEAM_LAB_MODE_NATURE_LOWERED,
    TEAM_LAB_MODE_NATURE_CONFIRM,
    TEAM_LAB_MODE_ABILITY,
    TEAM_LAB_MODE_STAT_PRESET,
    TEAM_LAB_MODE_EV_FIRST,
    TEAM_LAB_MODE_EV_SECOND,
    TEAM_LAB_MODE_EV_THIRD,
};

enum
{
    TEAM_LAB_BUILD_SPECIES,
    TEAM_LAB_BUILD_NATURE,
    TEAM_LAB_BUILD_ABILITY,
    TEAM_LAB_BUILD_LEVEL,
    TEAM_LAB_BUILD_ITEM,
    TEAM_LAB_BUILD_COUNT,
};

enum
{
    WIN_TEAM_LAB_NAME,
    WIN_TEAM_LAB_CONTENT,
    WIN_TEAM_LAB_DETAIL,
    WIN_TEAM_LAB_COUNT,
};

struct TeamLabScreen
{
    MainCallback returnCallback;
    struct Pokemon original;
    struct Pokemon preview;
    struct TeamLabMonBuild draft;
    u16 *bgTilemap;
    u8 partyIndex;
    u8 page;
    u8 monIconSpriteId;
    u8 mode;
    u8 buildFocus;
    u8 statFocus;
    u8 statColumn;
    u8 moveFocus;
    u8 modalCursor;
    u8 natureRaised;
    u8 natureLowered;
    u8 evPresetStats[3];
    u8 statInputHoldFrames;
    u8 moveSearch[MOVE_NAME_LENGTH + 1];
    bool8 dirty;
    bool8 isCreating;
};

static EWRAM_DATA struct TeamLabScreen *sTeamLabScreen = NULL;

static void CB2_InitTeamLabScreen(void);
static void CB2_TeamLabScreen(void);
static void VBlankCB_TeamLabScreen(void);
static void Task_HandleTeamLabScreenInput(u8 taskId);
static void Task_ExitTeamLabScreen(u8 taskId);
static void Task_OpenTeamLabPokedex(u8 taskId);
static void Task_OpenTeamLabItemSelector(u8 taskId);
static void Task_OpenTeamLabMoveSearch(u8 taskId);
static void CB2_ReturnFromTeamLabPokedex(void);
static void CB2_ReturnFromTeamLabItemSelector(void);
static void CB2_ReturnFromTeamLabMoveSearch(void);
static void DrawTeamLabScreen(void);
static void DrawTeamLabBuildPage(void);
static void DrawTeamLabStatsPage(void);
static void DrawTeamLabMovesPage(void);
static void ExtractTeamLabDraft(struct TeamLabMonBuild *draft, struct Pokemon *mon);
static void CreateDefaultTeamLabMon(struct Pokemon *mon);
static void DrawTeamLabModal(void);
static void HandleTeamLabNormalInput(u8 taskId);
static void HandleTeamLabModalInput(void);
static u16 GetAcceleratedStatInput(void);
static void RefreshTeamLabPreview(void);
static void RecreateTeamLabScene(void);
static void FreeTeamLabVisualResources(void);

static const struct BgTemplate sTeamLabBgTemplates[] =
{
    {
        .bg = 0,
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0,
        .baseTile = 0,
    },
    {
        .bg = 1,
        .charBaseIndex = 1,
        .mapBaseIndex = 30,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 1,
        .baseTile = 0,
    },
};

static const struct WindowTemplate sTeamLabWindowTemplates[] =
{
    [WIN_TEAM_LAB_NAME] = {
        .bg = 0,
        .tilemapLeft = 2,
        .tilemapTop = 1,
        .width = 11,
        .height = 2,
        .paletteNum = 15,
        .baseBlock = 1,
    },
    [WIN_TEAM_LAB_CONTENT] = {
        .bg = 0,
        .tilemapLeft = 15,
        .tilemapTop = 1,
        .width = 14,
        .height = 14,
        .paletteNum = 15,
        .baseBlock = 23,
    },
    [WIN_TEAM_LAB_DETAIL] = {
        .bg = 0,
        .tilemapLeft = 2,
        .tilemapTop = 13,
        .width = 12,
        .height = 5,
        .paletteNum = 15,
        .baseBlock = 219,
    },
    DUMMY_WIN_TEMPLATE,
};

static const u8 sTeamLabTextColors[] =
{
    TEXT_COLOR_TRANSPARENT,
    TEXT_COLOR_DARK_GRAY,
    TEXT_COLOR_LIGHT_GRAY,
};

static const u8 sTeamLabSelectedTextColors[] =
{
    TEXT_COLOR_TRANSPARENT,
    TEXT_COLOR_BLUE,
    TEXT_COLOR_LIGHT_BLUE,
};

static const u8 sText_TeamLabTitle[] = _("TEAM LAB");
static const u8 sText_TeamLabBuild[] = _("BUILD");
static const u8 sText_TeamLabStats[] = _("STATS");
static const u8 sText_TeamLabMoves[] = _("MOVES");
static const u8 sText_TeamLabSpecies[] = _("SPECIES");
static const u8 sText_TeamLabNature[] = _("NATURE");
static const u8 sText_TeamLabAbility[] = _("ABILITY");
static const u8 sText_TeamLabLevel[] = _("LEVEL");
static const u8 sText_TeamLabItem[] = _("ITEM");
static const u8 sText_TeamLabIv[] = _("IV");
static const u8 sText_TeamLabEv[] = _("EV");
static const u8 sText_TeamLabStat[] = _("STAT");
static const u8 sText_TeamLabNone[] = _("NONE");
static const u8 sText_TeamLabControls1[] = _("L/R PAGE  START SAVE");
static const u8 sText_TeamLabControls2[] = _("B CANCEL");
static const u8 sText_TeamLabChooseRaised[] = _("CHOOSE RAISED STAT");
static const u8 sText_TeamLabChooseLowered[] = _("CHOOSE LOWERED STAT");
static const u8 sText_TeamLabConfirmNature[] = _("CONFIRM NATURE");
static const u8 sText_TeamLabNoStatChange[] = _("NO STAT CHANGE");
static const u8 sText_TeamLabChooseAbility[] = _("CHOOSE ABILITY");
static const u8 sText_TeamLabStatPreset[] = _("STAT PRESET");
static const u8 sText_TeamLabMaxIvs[] = _("MAX ALL IVS");
static const u8 sText_TeamLabZeroSpeed[] = _("ZERO SPEED IV");
static const u8 sText_TeamLabZeroEvs[] = _("ZERO ALL EVS");
static const u8 sText_TeamLabEvSpread[] = _("252 / 252 / 4");
static const u8 sText_TeamLabChoose252[] = _("CHOOSE 252 STAT");
static const u8 sText_TeamLabChoose4[] = _("CHOOSE 4 EV STAT");
static const u8 sText_TeamLabPlus[] = _("+");
static const u8 sText_TeamLabMinus[] = _("-");
static const u8 sText_TeamLabConfirmChange[] = _("A CONFIRM  B CHANGE");

static const u8 *const sTeamLabStatNames[NUM_STATS] =
{
    gText_HP4,
    gText_Attack3,
    gText_Defense3,
    gText_SpAtk4,
    gText_SpDef4,
    gText_Speed2,
};

static const u8 sTeamLabStatDataIndexes[NUM_STATS] =
{
    STAT_HP,
    STAT_ATK,
    STAT_DEF,
    STAT_SPATK,
    STAT_SPDEF,
    STAT_SPEED,
};

static const u8 *const sTeamLabNatureStatNames[NUM_STATS - 1] =
{
    gText_Attack3,
    gText_Defense3,
    gText_Speed2,
    gText_SpAtk4,
    gText_SpDef4,
};

static void PrintText(u8 windowId, const u8 *text, u8 x, u8 y)
{
    AddTextPrinterParameterized4(windowId, FONT_NORMAL, x, y, 0, 0, sTeamLabTextColors, TEXT_SKIP_DRAW, text);
}

static void PrintSmallText(u8 windowId, const u8 *text, u8 x, u8 y)
{
    AddTextPrinterParameterized4(windowId, FONT_SMALL, x, y, 0, 0, sTeamLabTextColors, TEXT_SKIP_DRAW, text);
}

static void PrintSelectedSmallText(u8 windowId, const u8 *text, u8 x, u8 y)
{
    AddTextPrinterParameterized4(windowId, FONT_SMALL, x, y, 0, 0, sTeamLabSelectedTextColors, TEXT_SKIP_DRAW, text);
}

void ShowTeamLabScreen(u8 partyIndex, void (*returnCallback)(void))
{
    if (partyIndex >= PARTY_SIZE)
    {
        SetMainCallback2(returnCallback);
        return;
    }

    sTeamLabScreen = AllocZeroed(sizeof(*sTeamLabScreen));
    if (sTeamLabScreen == NULL)
    {
        SetMainCallback2(returnCallback);
        return;
    }

    sTeamLabScreen->returnCallback = returnCallback;
    sTeamLabScreen->partyIndex = partyIndex;
    if (GetMonData(&gPlayerParty[partyIndex], MON_DATA_SPECIES) == SPECIES_NONE)
    {
        sTeamLabScreen->isCreating = TRUE;
        CreateDefaultTeamLabMon(&sTeamLabScreen->preview);
    }
    else
    {
        sTeamLabScreen->original = gPlayerParty[partyIndex];
        sTeamLabScreen->preview = sTeamLabScreen->original;
    }
    sTeamLabScreen->monIconSpriteId = SPRITE_NONE;
    ExtractTeamLabDraft(&sTeamLabScreen->draft, &sTeamLabScreen->preview);
    gMain.state = 0;
    SetMainCallback2(CB2_InitTeamLabScreen);
}

static void CreateDefaultTeamLabMon(struct Pokemon *mon)
{
    struct TeamLabMonBuild build = {0};
    u8 i;

    build.species = SPECIES_BULBASAUR;
    build.level = 50;
    build.nature = NATURE_HARDY;
    build.abilityNum = 0;
    build.moves[0] = MOVE_TACKLE;
    for (i = 0; i < NUM_STATS; i++)
        build.ivs[i] = MAX_PER_STAT_IVS;

    TeamLab_CreateMon(mon, &build);
}

static void ExtractTeamLabDraft(struct TeamLabMonBuild *draft, struct Pokemon *mon)
{
    u8 i;

    draft->species = GetMonData(mon, MON_DATA_SPECIES);
    draft->heldItem = GetMonData(mon, MON_DATA_HELD_ITEM);
    draft->level = GetLevelFromMonExp(mon);
    draft->nature = GetNature(mon);
    draft->abilityNum = GetMonData(mon, MON_DATA_ABILITY_NUM);
    for (i = 0; i < NUM_STATS; i++)
    {
        draft->ivs[i] = GetMonData(mon, MON_DATA_HP_IV + i);
        draft->evs[i] = GetMonData(mon, MON_DATA_HP_EV + i);
    }
    for (i = 0; i < MAX_MON_MOVES; i++)
        draft->moves[i] = GetMonData(mon, MON_DATA_MOVE1 + i);
}

static void CB2_InitTeamLabScreen(void)
{
    switch (gMain.state)
    {
    case 0:
        if (sTeamLabScreen->bgTilemap == NULL)
        {
            sTeamLabScreen->bgTilemap = AllocZeroed(BG_SCREEN_SIZE);
            if (sTeamLabScreen->bgTilemap == NULL)
            {
                SetMainCallback2(sTeamLabScreen->returnCallback);
                return;
            }
        }
        SetVBlankCallback(NULL);
        ResetTasks();
        ResetSpriteData();
        FreeAllSpritePalettes();
        ClearScheduledBgCopiesToVram();
        CpuFill32(0, (void *)VRAM, VRAM_SIZE);
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sTeamLabBgTemplates, ARRAY_COUNT(sTeamLabBgTemplates));
        SetBgTilemapBuffer(1, sTeamLabScreen->bgTilemap);
        InitWindows(sTeamLabWindowTemplates);
        DeactivateAllTextPrinters();
        LoadPalette(gStandardMenuPalette, BG_PLTT_ID(15), PLTT_SIZE_4BPP);
        ResetTempTileDataBuffers();
        DecompressAndCopyTileDataToVram(1, gMenuPokeblock_Gfx, 0, 0, 0);
        gMain.state++;
        break;
    case 1:
        if (FreeTempTileDataBuffersIfPossible() != TRUE)
        {
            LZDecompressWram(gMenuPokeblock_Tilemap, sTeamLabScreen->bgTilemap);
            // Remove the Pokeblock Case's FEEL inset from the left detail panel.
            FillBgTilemapBufferRect(1, 0xF, 6, 17, 7, 2, 0);
            FillBgTilemapBufferRect(1, 0x10, 13, 17, 1, 2, 0);
            LoadCompressedPalette(gMenuPokeblock_Pal, BG_PLTT_ID(0), 6 * PLTT_SIZE_4BPP);
            CopyBgTilemapBufferToVram(1);
            gMain.state++;
        }
        break;
    case 2:
        ResetPaletteFade();
        DrawTeamLabScreen();
        if (sTeamLabScreen->draft.species > SPECIES_NONE && sTeamLabScreen->draft.species < NUM_SPECIES)
        {
            LoadMonIconPalette(sTeamLabScreen->draft.species);
            sTeamLabScreen->monIconSpriteId = CreateMonIcon(sTeamLabScreen->draft.species, SpriteCallbackDummy, 56, 72, 0, GetMonData(&sTeamLabScreen->preview, MON_DATA_PERSONALITY), FALSE);
        }
        CreateTask(Task_HandleTeamLabScreenInput, 0);
        ShowBg(0);
        ShowBg(1);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        SetVBlankCallback(VBlankCB_TeamLabScreen);
        SetMainCallback2(CB2_TeamLabScreen);
        break;
    }
}

static void VBlankCB_TeamLabScreen(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void CB2_TeamLabScreen(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    RunTextPrinters();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

static void ClearTeamLabWindows(void)
{
    u8 i;

    for (i = 0; i < WIN_TEAM_LAB_COUNT; i++)
        FillWindowPixelBuffer(i, PIXEL_FILL(0));
}

static void CopyTeamLabWindows(void)
{
    u8 i;

    for (i = 0; i < WIN_TEAM_LAB_COUNT; i++)
    {
        PutWindowTilemap(i);
        CopyWindowToVram(i, COPYWIN_FULL);
    }
}

static void DrawTeamLabScreen(void)
{
    ClearTeamLabWindows();
    PrintText(WIN_TEAM_LAB_NAME, sText_TeamLabTitle, 0, 1);
    PrintText(WIN_TEAM_LAB_DETAIL, gSpeciesNames[sTeamLabScreen->draft.species], 0, 1);
    PrintSmallText(WIN_TEAM_LAB_DETAIL, sText_TeamLabControls1, 0, 18);
    PrintSmallText(WIN_TEAM_LAB_DETAIL, sText_TeamLabControls2, 0, 26);

    if (sTeamLabScreen->mode != TEAM_LAB_MODE_NORMAL)
        DrawTeamLabModal();
    else switch (sTeamLabScreen->page)
    {
    case TEAM_LAB_PAGE_BUILD:
        DrawTeamLabBuildPage();
        break;
    case TEAM_LAB_PAGE_STATS:
        DrawTeamLabStatsPage();
        break;
    case TEAM_LAB_PAGE_MOVES:
        DrawTeamLabMovesPage();
        break;
    }

    CopyTeamLabWindows();
}

static void DrawTeamLabBuildPage(void)
{
    u8 ability = GetAbilityBySpecies(sTeamLabScreen->draft.species, sTeamLabScreen->draft.abilityNum);
    const u8 *values[TEAM_LAB_BUILD_COUNT];
    const u8 *labels[TEAM_LAB_BUILD_COUNT] =
    {
        sText_TeamLabSpecies,
        sText_TeamLabNature,
        sText_TeamLabAbility,
        sText_TeamLabLevel,
        sText_TeamLabItem,
    };
    u8 i;

    PrintText(WIN_TEAM_LAB_CONTENT, sText_TeamLabBuild, 0, 1);
    ConvertIntToDecimalStringN(gStringVar1, sTeamLabScreen->draft.level, STR_CONV_MODE_LEFT_ALIGN, 3);
    values[0] = gSpeciesNames[sTeamLabScreen->draft.species];
    values[1] = gNatureNamePointers[sTeamLabScreen->draft.nature];
    values[2] = gAbilityNames[ability];
    values[3] = gStringVar1;
    values[4] = sTeamLabScreen->draft.heldItem == ITEM_NONE ? sText_TeamLabNone : GetItemName(sTeamLabScreen->draft.heldItem);
    for (i = 0; i < TEAM_LAB_BUILD_COUNT; i++)
    {
        u8 y = 17 + i * 16;

        if (i == sTeamLabScreen->buildFocus)
            PrintSelectedSmallText(WIN_TEAM_LAB_CONTENT, labels[i], 0, y);
        else
            PrintSmallText(WIN_TEAM_LAB_CONTENT, labels[i], 0, y);
        PrintSmallText(WIN_TEAM_LAB_CONTENT, values[i], 58, y);
    }
}

static void DrawTeamLabStatsPage(void)
{
    static const u8 sStatDataIds[NUM_STATS] =
    {
        MON_DATA_MAX_HP,
        MON_DATA_ATK,
        MON_DATA_DEF,
        MON_DATA_SPATK,
        MON_DATA_SPDEF,
        MON_DATA_SPEED,
    };
    u8 i;

    PrintText(WIN_TEAM_LAB_CONTENT, sText_TeamLabStats, 0, 1);
    PrintSmallText(WIN_TEAM_LAB_CONTENT, sText_TeamLabIv, 46, 18);
    PrintSmallText(WIN_TEAM_LAB_CONTENT, sText_TeamLabEv, 70, 18);
    PrintSmallText(WIN_TEAM_LAB_CONTENT, sText_TeamLabStat, 90, 18);
    for (i = 0; i < NUM_STATS; i++)
    {
        u8 stat = sTeamLabStatDataIndexes[i];
        u8 y = 31 + i * 13;

        PrintSmallText(WIN_TEAM_LAB_CONTENT, sTeamLabStatNames[i], 0, y);
        ConvertIntToDecimalStringN(gStringVar1, sTeamLabScreen->draft.ivs[stat], STR_CONV_MODE_RIGHT_ALIGN, 2);
        if (sTeamLabScreen->statFocus == i && sTeamLabScreen->statColumn == 0)
            PrintSelectedSmallText(WIN_TEAM_LAB_CONTENT, gStringVar1, 46, y);
        else
            PrintSmallText(WIN_TEAM_LAB_CONTENT, gStringVar1, 46, y);
        ConvertIntToDecimalStringN(gStringVar1, sTeamLabScreen->draft.evs[stat], STR_CONV_MODE_RIGHT_ALIGN, 3);
        if (sTeamLabScreen->statFocus == i && sTeamLabScreen->statColumn == 1)
            PrintSelectedSmallText(WIN_TEAM_LAB_CONTENT, gStringVar1, 66, y);
        else
            PrintSmallText(WIN_TEAM_LAB_CONTENT, gStringVar1, 66, y);
        ConvertIntToDecimalStringN(gStringVar1, GetMonData(&sTeamLabScreen->preview, sStatDataIds[i]), STR_CONV_MODE_RIGHT_ALIGN, 3);
        PrintSmallText(WIN_TEAM_LAB_CONTENT, gStringVar1, 90, y);
    }
}

static void DrawTeamLabMovesPage(void)
{
    u8 i;

    PrintText(WIN_TEAM_LAB_CONTENT, sText_TeamLabMoves, 0, 1);
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        const u8 *moveName = sTeamLabScreen->draft.moves[i] == MOVE_NONE ? sText_TeamLabNone : gMoveNames[sTeamLabScreen->draft.moves[i]];

        ConvertIntToDecimalStringN(gStringVar1, i + 1, STR_CONV_MODE_LEFT_ALIGN, 1);
        if (i == sTeamLabScreen->moveFocus)
            PrintSelectedSmallText(WIN_TEAM_LAB_CONTENT, gStringVar1, 0, 25 + i * 20);
        else
            PrintSmallText(WIN_TEAM_LAB_CONTENT, gStringVar1, 0, 25 + i * 20);
        PrintSmallText(WIN_TEAM_LAB_CONTENT, moveName, 16, 25 + i * 20);
    }
}

static void DrawModalChoice(const u8 *text, u8 choice, u8 y)
{
    if (choice == sTeamLabScreen->modalCursor)
        PrintSelectedSmallText(WIN_TEAM_LAB_CONTENT, text, 8, y);
    else
        PrintSmallText(WIN_TEAM_LAB_CONTENT, text, 8, y);
}

static void DrawTeamLabModal(void)
{
    u8 i;
    const u8 *heading;

    switch (sTeamLabScreen->mode)
    {
    case TEAM_LAB_MODE_NATURE_RAISED:
    case TEAM_LAB_MODE_NATURE_LOWERED:
        heading = sTeamLabScreen->mode == TEAM_LAB_MODE_NATURE_RAISED ? sText_TeamLabChooseRaised : sText_TeamLabChooseLowered;
        PrintSmallText(WIN_TEAM_LAB_CONTENT, heading, 0, 1);
        for (i = 0; i < NUM_STATS - 1; i++)
            DrawModalChoice(sTeamLabNatureStatNames[i], i, 19 + i * 16);
        break;
    case TEAM_LAB_MODE_NATURE_CONFIRM:
        PrintSmallText(WIN_TEAM_LAB_CONTENT, sText_TeamLabConfirmNature, 0, 1);
        PrintText(WIN_TEAM_LAB_CONTENT, gNatureNamePointers[sTeamLabScreen->natureRaised * 5 + sTeamLabScreen->natureLowered], 8, 25);
        if (sTeamLabScreen->natureRaised == sTeamLabScreen->natureLowered)
            PrintSmallText(WIN_TEAM_LAB_CONTENT, sText_TeamLabNoStatChange, 8, 49);
        else
        {
            StringCopy(gStringVar1, sText_TeamLabPlus);
            StringAppend(gStringVar1, sTeamLabNatureStatNames[sTeamLabScreen->natureRaised]);
            PrintSmallText(WIN_TEAM_LAB_CONTENT, gStringVar1, 8, 49);
            StringCopy(gStringVar1, sText_TeamLabMinus);
            StringAppend(gStringVar1, sTeamLabNatureStatNames[sTeamLabScreen->natureLowered]);
            PrintSmallText(WIN_TEAM_LAB_CONTENT, gStringVar1, 8, 65);
        }
        PrintSmallText(WIN_TEAM_LAB_CONTENT, sText_TeamLabConfirmChange, 8, 89);
        break;
    case TEAM_LAB_MODE_ABILITY:
    {
        u8 abilityCount = gSpeciesInfo[sTeamLabScreen->draft.species].abilities[1] == ABILITY_NONE ? 1 : 2;

        PrintSmallText(WIN_TEAM_LAB_CONTENT, sText_TeamLabChooseAbility, 0, 1);
        for (i = 0; i < abilityCount; i++)
            DrawModalChoice(gAbilityNames[GetAbilityBySpecies(sTeamLabScreen->draft.species, i)], i, 21 + i * 18);
        PrintSmallText(WIN_TEAM_LAB_CONTENT, gAbilityDescriptionPointers[GetAbilityBySpecies(sTeamLabScreen->draft.species, sTeamLabScreen->modalCursor)], 0, 65);
        break;
    }
    case TEAM_LAB_MODE_STAT_PRESET:
    {
        static const u8 *const presetNames[] =
        {
            sText_TeamLabMaxIvs,
            sText_TeamLabZeroSpeed,
            sText_TeamLabZeroEvs,
            sText_TeamLabEvSpread,
        };

        PrintSmallText(WIN_TEAM_LAB_CONTENT, sText_TeamLabStatPreset, 0, 1);
        for (i = 0; i < ARRAY_COUNT(presetNames); i++)
            DrawModalChoice(presetNames[i], i, 21 + i * 18);
        break;
    }
    case TEAM_LAB_MODE_EV_FIRST:
    case TEAM_LAB_MODE_EV_SECOND:
    case TEAM_LAB_MODE_EV_THIRD:
        PrintSmallText(WIN_TEAM_LAB_CONTENT, sTeamLabScreen->mode == TEAM_LAB_MODE_EV_THIRD ? sText_TeamLabChoose4 : sText_TeamLabChoose252, 0, 1);
        for (i = 0; i < NUM_STATS; i++)
            DrawModalChoice(sTeamLabStatNames[i], i, 19 + i * 14);
        break;
    }
}

static void Task_HandleTeamLabScreenInput(u8 taskId)
{
    if (gPaletteFade.active)
        return;

    if (sTeamLabScreen->mode != TEAM_LAB_MODE_NORMAL)
    {
        HandleTeamLabModalInput();
        return;
    }

    HandleTeamLabNormalInput(taskId);
}

static void ChangeWrappedValue(u8 *value, s8 delta, u8 count)
{
    if (delta < 0)
        *value = (*value + count - 1) % count;
    else
        *value = (*value + 1) % count;
}

static u16 GetAcceleratedStatInput(void)
{
    u16 heldKeys = JOY_HELD(DPAD_LEFT | DPAD_RIGHT);

    if (sTeamLabScreen->page != TEAM_LAB_PAGE_STATS
     || (heldKeys != DPAD_LEFT && heldKeys != DPAD_RIGHT))
    {
        sTeamLabScreen->statInputHoldFrames = 0;
        return 0;
    }

    if (JOY_NEW(DPAD_LEFT | DPAD_RIGHT))
    {
        sTeamLabScreen->statInputHoldFrames = 0;
        return heldKeys;
    }

    if (sTeamLabScreen->statInputHoldFrames < 255)
        sTeamLabScreen->statInputHoldFrames++;

    if (sTeamLabScreen->statInputHoldFrames < 20)
        return 0;
    if (sTeamLabScreen->statInputHoldFrames < 50
     && (sTeamLabScreen->statInputHoldFrames - 20) % 3 != 0)
        return 0;

    return heldKeys;
}

static void HandleTeamLabNormalInput(u8 taskId)
{
    s8 delta = 0;
    u16 horizontalInput = sTeamLabScreen->page == TEAM_LAB_PAGE_STATS
                        ? GetAcceleratedStatInput()
                        : JOY_NEW(DPAD_LEFT | DPAD_RIGHT);

    if (GetLRKeysPressed() == MENU_L_PRESSED)
    {
        sTeamLabScreen->page = (sTeamLabScreen->page + TEAM_LAB_PAGE_COUNT - 1) % TEAM_LAB_PAGE_COUNT;
        PlaySE(SE_SELECT);
        DrawTeamLabScreen();
    }
    else if (GetLRKeysPressed() == MENU_R_PRESSED)
    {
        sTeamLabScreen->page = (sTeamLabScreen->page + 1) % TEAM_LAB_PAGE_COUNT;
        PlaySE(SE_SELECT);
        DrawTeamLabScreen();
    }
    else if (JOY_NEW(DPAD_UP | DPAD_DOWN))
    {
        delta = JOY_NEW(DPAD_UP) ? -1 : 1;
        if (sTeamLabScreen->page == TEAM_LAB_PAGE_BUILD)
            ChangeWrappedValue(&sTeamLabScreen->buildFocus, delta, TEAM_LAB_BUILD_COUNT);
        else if (sTeamLabScreen->page == TEAM_LAB_PAGE_STATS)
            ChangeWrappedValue(&sTeamLabScreen->statFocus, delta, NUM_STATS);
        else
            ChangeWrappedValue(&sTeamLabScreen->moveFocus, delta, MAX_MON_MOVES);
        PlaySE(SE_SELECT);
        DrawTeamLabScreen();
    }
    else if (JOY_NEW(A_BUTTON))
    {
        if (sTeamLabScreen->page == TEAM_LAB_PAGE_BUILD)
        {
            if (sTeamLabScreen->buildFocus == TEAM_LAB_BUILD_SPECIES)
            {
                BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
                gTasks[taskId].func = Task_OpenTeamLabPokedex;
            }
            else if (sTeamLabScreen->buildFocus == TEAM_LAB_BUILD_NATURE)
            {
                sTeamLabScreen->mode = TEAM_LAB_MODE_NATURE_RAISED;
                sTeamLabScreen->modalCursor = 0;
                DrawTeamLabScreen();
            }
            else if (sTeamLabScreen->buildFocus == TEAM_LAB_BUILD_ABILITY)
            {
                sTeamLabScreen->mode = TEAM_LAB_MODE_ABILITY;
                sTeamLabScreen->modalCursor = sTeamLabScreen->draft.abilityNum;
                DrawTeamLabScreen();
            }
            else if (sTeamLabScreen->buildFocus == TEAM_LAB_BUILD_ITEM)
            {
                BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
                gTasks[taskId].func = Task_OpenTeamLabItemSelector;
            }
        }
        else if (sTeamLabScreen->page == TEAM_LAB_PAGE_STATS)
        {
            sTeamLabScreen->statColumn ^= 1;
            DrawTeamLabScreen();
        }
        else
        {
            sTeamLabScreen->moveSearch[0] = EOS;
            BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
            gTasks[taskId].func = Task_OpenTeamLabMoveSearch;
        }
        PlaySE(SE_SELECT);
    }
    else if (horizontalInput)
    {
        delta = horizontalInput & DPAD_LEFT ? -1 : 1;
        if (sTeamLabScreen->page == TEAM_LAB_PAGE_BUILD && sTeamLabScreen->buildFocus == TEAM_LAB_BUILD_LEVEL)
        {
            if ((delta < 0 && sTeamLabScreen->draft.level > 1) || (delta > 0 && sTeamLabScreen->draft.level < MAX_LEVEL))
            {
                sTeamLabScreen->draft.level += delta;
                TeamLab_SetLevel(&sTeamLabScreen->preview, sTeamLabScreen->draft.level);
                sTeamLabScreen->dirty = TRUE;
                PlaySE(SE_SELECT);
                DrawTeamLabScreen();
            }
            else
                PlaySE(SE_FAILURE);
        }
        else if (sTeamLabScreen->page == TEAM_LAB_PAGE_STATS)
        {
            u8 stat = sTeamLabStatDataIndexes[sTeamLabScreen->statFocus];
            u8 *value = sTeamLabScreen->statColumn == 0 ? &sTeamLabScreen->draft.ivs[stat] : &sTeamLabScreen->draft.evs[stat];
            u8 max = sTeamLabScreen->statColumn == 0 ? MAX_PER_STAT_IVS : TEAM_LAB_MAX_PER_STAT_EVS;
            u16 evTotal = 0;
            u8 i;

            for (i = 0; i < NUM_STATS; i++)
                evTotal += sTeamLabScreen->draft.evs[i];
            if ((delta < 0 && *value == 0) || (delta > 0 && (*value == max || (sTeamLabScreen->statColumn == 1 && evTotal >= MAX_TOTAL_EVS))))
                PlaySE(SE_FAILURE);
            else
            {
                *value += delta;
                RefreshTeamLabPreview();
                sTeamLabScreen->dirty = TRUE;
                PlaySE(SE_SELECT);
                DrawTeamLabScreen();
            }
        }
    }
    else if (JOY_NEW(SELECT_BUTTON))
    {
        if (sTeamLabScreen->page == TEAM_LAB_PAGE_STATS)
        {
            sTeamLabScreen->mode = TEAM_LAB_MODE_STAT_PRESET;
            sTeamLabScreen->modalCursor = 0;
            DrawTeamLabScreen();
            PlaySE(SE_SELECT);
        }
        else if (sTeamLabScreen->page == TEAM_LAB_PAGE_MOVES)
        {
            sTeamLabScreen->draft.moves[sTeamLabScreen->moveFocus] = MOVE_NONE;
            SetMonMoveSlot(&sTeamLabScreen->preview, MOVE_NONE, sTeamLabScreen->moveFocus);
            sTeamLabScreen->dirty = TRUE;
            DrawTeamLabScreen();
            PlaySE(SE_SELECT);
        }
    }
    else if (JOY_NEW(START_BUTTON))
    {
        if (TeamLab_ValidateBuild(&sTeamLabScreen->draft) != TEAM_LAB_VALID)
        {
            PlaySE(SE_FAILURE);
            return;
        }
        if (sTeamLabScreen->isCreating)
        {
            CalculatePlayerPartyCount();
            if (gPlayerPartyCount >= PARTY_SIZE)
            {
                PlaySE(SE_FAILURE);
                return;
            }
            sTeamLabScreen->partyIndex = gPlayerPartyCount;
        }
        gPlayerParty[sTeamLabScreen->partyIndex] = sTeamLabScreen->preview;
        CalculatePlayerPartyCount();
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_ExitTeamLabScreen;
        PlaySE(SE_SELECT);
    }
    else if (JOY_NEW(B_BUTTON))
    {
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_ExitTeamLabScreen;
        PlaySE(SE_SELECT);
    }
}

static void HandleTeamLabModalInput(void)
{
    s8 delta;
    u8 count;

    if (sTeamLabScreen->mode == TEAM_LAB_MODE_NATURE_CONFIRM)
    {
        if (JOY_NEW(A_BUTTON))
        {
            sTeamLabScreen->draft.nature = sTeamLabScreen->natureRaised * 5 + sTeamLabScreen->natureLowered;
            TeamLab_SetNature(&sTeamLabScreen->preview, sTeamLabScreen->draft.nature);
            sTeamLabScreen->dirty = TRUE;
            sTeamLabScreen->mode = TEAM_LAB_MODE_NORMAL;
            DrawTeamLabScreen();
            PlaySE(SE_SELECT);
        }
        else if (JOY_NEW(B_BUTTON))
        {
            sTeamLabScreen->mode = TEAM_LAB_MODE_NATURE_LOWERED;
            sTeamLabScreen->modalCursor = sTeamLabScreen->natureLowered;
            DrawTeamLabScreen();
            PlaySE(SE_SELECT);
        }
        return;
    }

    if (sTeamLabScreen->mode == TEAM_LAB_MODE_ABILITY)
        count = gSpeciesInfo[sTeamLabScreen->draft.species].abilities[1] == ABILITY_NONE ? 1 : 2;
    else if (sTeamLabScreen->mode == TEAM_LAB_MODE_STAT_PRESET)
        count = 4;
    else if (sTeamLabScreen->mode == TEAM_LAB_MODE_NATURE_RAISED || sTeamLabScreen->mode == TEAM_LAB_MODE_NATURE_LOWERED)
        count = NUM_STATS - 1;
    else
        count = NUM_STATS;

    if (JOY_NEW(DPAD_UP | DPAD_DOWN))
    {
        delta = JOY_NEW(DPAD_UP) ? -1 : 1;
        ChangeWrappedValue(&sTeamLabScreen->modalCursor, delta, count);
        DrawTeamLabScreen();
        PlaySE(SE_SELECT);
    }
    else if (JOY_NEW(B_BUTTON))
    {
        if (sTeamLabScreen->mode == TEAM_LAB_MODE_NATURE_LOWERED)
        {
            sTeamLabScreen->mode = TEAM_LAB_MODE_NATURE_RAISED;
            sTeamLabScreen->modalCursor = sTeamLabScreen->natureRaised;
        }
        else
            sTeamLabScreen->mode = TEAM_LAB_MODE_NORMAL;
        DrawTeamLabScreen();
        PlaySE(SE_SELECT);
    }
    else if (JOY_NEW(A_BUTTON))
    {
        if (sTeamLabScreen->mode == TEAM_LAB_MODE_NATURE_RAISED)
        {
            sTeamLabScreen->natureRaised = sTeamLabScreen->modalCursor;
            sTeamLabScreen->mode = TEAM_LAB_MODE_NATURE_LOWERED;
            sTeamLabScreen->modalCursor = 0;
        }
        else if (sTeamLabScreen->mode == TEAM_LAB_MODE_NATURE_LOWERED)
        {
            sTeamLabScreen->natureLowered = sTeamLabScreen->modalCursor;
            sTeamLabScreen->mode = TEAM_LAB_MODE_NATURE_CONFIRM;
        }
        else if (sTeamLabScreen->mode == TEAM_LAB_MODE_ABILITY)
        {
            sTeamLabScreen->draft.abilityNum = sTeamLabScreen->modalCursor;
            TeamLab_SetAbilityNum(&sTeamLabScreen->preview, sTeamLabScreen->draft.abilityNum);
            sTeamLabScreen->dirty = TRUE;
            sTeamLabScreen->mode = TEAM_LAB_MODE_NORMAL;
        }
        else if (sTeamLabScreen->mode == TEAM_LAB_MODE_STAT_PRESET)
        {
            if (sTeamLabScreen->modalCursor == 0)
                TeamLab_MaxMonIvs(&sTeamLabScreen->preview);
            else if (sTeamLabScreen->modalCursor == 1)
            {
                u8 zero = 0;
                SetMonData(&sTeamLabScreen->preview, MON_DATA_SPEED_IV, &zero);
            }
            else if (sTeamLabScreen->modalCursor == 2)
                TeamLab_ZeroMonEvs(&sTeamLabScreen->preview);
            else
            {
                sTeamLabScreen->mode = TEAM_LAB_MODE_EV_FIRST;
                sTeamLabScreen->modalCursor = 0;
                DrawTeamLabScreen();
                PlaySE(SE_SELECT);
                return;
            }
            ExtractTeamLabDraft(&sTeamLabScreen->draft, &sTeamLabScreen->preview);
            CalculateMonStats(&sTeamLabScreen->preview);
            sTeamLabScreen->dirty = TRUE;
            sTeamLabScreen->mode = TEAM_LAB_MODE_NORMAL;
        }
        else
        {
            u8 step = sTeamLabScreen->mode - TEAM_LAB_MODE_EV_FIRST;

            if ((step >= 1 && sTeamLabScreen->modalCursor == sTeamLabScreen->evPresetStats[0])
             || (step == 2 && sTeamLabScreen->modalCursor == sTeamLabScreen->evPresetStats[1]))
            {
                PlaySE(SE_FAILURE);
                return;
            }
            sTeamLabScreen->evPresetStats[step] = sTeamLabScreen->modalCursor;
            if (sTeamLabScreen->mode != TEAM_LAB_MODE_EV_THIRD)
            {
                sTeamLabScreen->mode++;
                sTeamLabScreen->modalCursor = 0;
                DrawTeamLabScreen();
                PlaySE(SE_SELECT);
                return;
            }
            TeamLab_ZeroMonEvs(&sTeamLabScreen->preview);
            for (step = 0; step < 3; step++)
            {
                u8 value = step < 2 ? TEAM_LAB_MAX_PER_STAT_EVS : 4;
                u8 stat = sTeamLabStatDataIndexes[sTeamLabScreen->evPresetStats[step]];
                SetMonData(&sTeamLabScreen->preview, MON_DATA_HP_EV + stat, &value);
            }
            ExtractTeamLabDraft(&sTeamLabScreen->draft, &sTeamLabScreen->preview);
            CalculateMonStats(&sTeamLabScreen->preview);
            sTeamLabScreen->dirty = TRUE;
            sTeamLabScreen->mode = TEAM_LAB_MODE_NORMAL;
        }
        DrawTeamLabScreen();
        PlaySE(SE_SELECT);
    }
}

static void RefreshTeamLabPreview(void)
{
    u8 i;

    for (i = 0; i < NUM_STATS; i++)
    {
        SetMonData(&sTeamLabScreen->preview, MON_DATA_HP_IV + i, &sTeamLabScreen->draft.ivs[i]);
        SetMonData(&sTeamLabScreen->preview, MON_DATA_HP_EV + i, &sTeamLabScreen->draft.evs[i]);
    }
    CalculateMonStats(&sTeamLabScreen->preview);
}

static void FreeTeamLabVisualResources(void)
{
    SetVBlankCallback(NULL);
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    FreeAllWindowBuffers();
    Free(sTeamLabScreen->bgTilemap);
    sTeamLabScreen->bgTilemap = NULL;
}

static void RecreateTeamLabScene(void)
{
    sTeamLabScreen->mode = TEAM_LAB_MODE_NORMAL;
    sTeamLabScreen->monIconSpriteId = SPRITE_NONE;
    gMain.state = 0;
    SetMainCallback2(CB2_InitTeamLabScreen);
}

static void Task_OpenTeamLabPokedex(u8 taskId)
{
    u16 species;

    if (gPaletteFade.active)
        return;

    species = sTeamLabScreen->draft.species;
    FreeTeamLabVisualResources();
    OpenPokedexForSelection(species, CB2_ReturnFromTeamLabPokedex);
}

static void Task_OpenTeamLabItemSelector(u8 taskId)
{
    u16 item;

    if (gPaletteFade.active)
        return;
    item = sTeamLabScreen->draft.heldItem;
    FreeTeamLabVisualResources();
    OpenTeamLabItemSelector(item, CB2_ReturnFromTeamLabItemSelector);
}

static void CB2_ReturnFromTeamLabItemSelector(void)
{
    u16 item;

    if (GetTeamLabItemSelectionResult(&item))
    {
        sTeamLabScreen->draft.heldItem = item;
        SetMonData(&sTeamLabScreen->preview, MON_DATA_HELD_ITEM, &item);
        sTeamLabScreen->dirty = TRUE;
    }
    RecreateTeamLabScene();
}

static void CB2_ReturnFromTeamLabPokedex(void)
{
    u16 species;

    if (GetPokedexSelectionResult(&species) && species > SPECIES_NONE && species < NUM_SPECIES)
    {
        u8 i;

        sTeamLabScreen->draft.species = species;
        if (gSpeciesInfo[species].abilities[1] == ABILITY_NONE)
            sTeamLabScreen->draft.abilityNum = 0;
        for (i = 0; i < MAX_MON_MOVES; i++)
            if (!TeamLab_IsMoveLegal(species, sTeamLabScreen->draft.moves[i]))
                sTeamLabScreen->draft.moves[i] = MOVE_NONE;
        TeamLab_CreateMon(&sTeamLabScreen->preview, &sTeamLabScreen->draft);
        sTeamLabScreen->dirty = TRUE;
    }
    RecreateTeamLabScene();
}

static void Task_OpenTeamLabMoveSearch(u8 taskId)
{
    if (gPaletteFade.active)
        return;

    FreeTeamLabVisualResources();
    DoNamingScreen(NAMING_SCREEN_MOVE_SEARCH, sTeamLabScreen->moveSearch, 0, 0, 0, CB2_ReturnFromTeamLabMoveSearch);
}

static void CB2_ReturnFromTeamLabMoveSearch(void)
{
    u16 move;

    if (sTeamLabScreen->moveSearch[0] != EOS)
    {
        for (move = MOVE_NONE + 1; move < MOVES_COUNT; move++)
        {
            if (StringCompare(sTeamLabScreen->moveSearch, gMoveNames[move]) == 0
             && TeamLab_IsMoveLegal(sTeamLabScreen->draft.species, move))
            {
                sTeamLabScreen->draft.moves[sTeamLabScreen->moveFocus] = move;
                SetMonMoveSlot(&sTeamLabScreen->preview, move, sTeamLabScreen->moveFocus);
                sTeamLabScreen->dirty = TRUE;
                break;
            }
        }
    }
    RecreateTeamLabScene();
}

static void Task_ExitTeamLabScreen(u8 taskId)
{
    MainCallback returnCallback;

    if (gPaletteFade.active)
        return;

    returnCallback = sTeamLabScreen->returnCallback;
    gLastViewedMonIndex = sTeamLabScreen->partyIndex;
    FreeTeamLabVisualResources();
    Free(sTeamLabScreen);
    sTeamLabScreen = NULL;
    SetMainCallback2(returnCallback);
}
