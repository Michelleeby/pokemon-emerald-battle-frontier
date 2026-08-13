/*
 * Headless mGBA driver for project-owned end-to-end scenarios.
 *
 * The driver owns the emulator core and exposes a small synchronous command
 * protocol to the Python scenario layer. Keep scenario policy and assertions
 * out of this process.
 */

#define _GNU_SOURCE

#include <mgba/core/config.h>
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba-util/vfs.h>

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define E2E_PROTOCOL_VERSION 1
#define E2E_MAX_COMMAND_FRAMES 10000000U
#define E2E_GBA_KEY_MASK 0x3FFU

struct Driver
{
    struct mCore *core;
    struct mLogger logger;
    color_t *videoBuffer;
    const char *romPath;
    const char *savePath;
    bool configInitialized;
    bool romLoaded;
};

static void LogToStderr(struct mLogger *logger, int category, enum mLogLevel level,
                        const char *format, va_list args)
{
    (void)logger;
    (void)level;
    fprintf(stderr, "%s: ", mLogCategoryName(category));
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
}

static void PrintUsage(FILE *stream, const char *program)
{
    fprintf(stream, "Usage: %s --rom PATH [--save PATH]\n", program);
}

static void Reply(const char *status, const char *detail)
{
    printf("%s%s%s\n", status, detail[0] == '\0' ? "" : " ", detail);
    fflush(stdout);
}

static void DestroyCore(struct Driver *driver)
{
    if (driver->core == NULL)
        return;

    if (driver->romLoaded)
        driver->core->unloadROM(driver->core);
    if (driver->configInitialized)
        mCoreConfigDeinit(&driver->core->config);
    driver->core->deinit(driver->core);
    free(driver->videoBuffer);

    driver->core = NULL;
    driver->videoBuffer = NULL;
    driver->configInitialized = false;
    driver->romLoaded = false;
}

static bool CreateCore(struct Driver *driver)
{
    unsigned width;
    unsigned height;

    driver->core = mCoreFind(driver->romPath);
    if (driver->core == NULL)
    {
        fprintf(stderr, "Could not identify ROM: %s\n", driver->romPath);
        return false;
    }
    if (!driver->core->init(driver->core))
    {
        fputs("Could not initialize mGBA core\n", stderr);
        driver->core->deinit(driver->core);
        driver->core = NULL;
        return false;
    }

    mCoreInitConfig(driver->core, "pokeemerald-e2e");
    driver->configInitialized = true;
    mCoreConfigSetDefaultValue(&driver->core->config, "idleOptimization", "remove");

    driver->core->desiredVideoDimensions(driver->core, &width, &height);
    driver->videoBuffer = calloc((size_t)width * height, sizeof(*driver->videoBuffer));
    if (driver->videoBuffer == NULL)
    {
        fputs("Could not allocate video buffer\n", stderr);
        DestroyCore(driver);
        return false;
    }
    driver->core->setVideoBuffer(driver->core, driver->videoBuffer, width);

    if (!mCoreLoadFile(driver->core, driver->romPath))
    {
        fprintf(stderr, "Could not load ROM: %s\n", driver->romPath);
        DestroyCore(driver);
        return false;
    }
    driver->romLoaded = true;

    if (driver->savePath != NULL && !mCoreLoadSaveFile(driver->core, driver->savePath, false))
    {
        fprintf(stderr, "Could not load save: %s\n", driver->savePath);
        DestroyCore(driver);
        return false;
    }

    driver->core->reset(driver->core);
    return true;
}

static bool ParseU32(const char *text, uint32_t *value)
{
    char *end;
    unsigned long parsed;

    if (text == NULL || text[0] == '\0' || text[0] == '-')
        return false;

    errno = 0;
    parsed = strtoul(text, &end, 0);
    if (errno != 0 || end[0] != '\0' || parsed > UINT32_MAX)
        return false;

    *value = (uint32_t)parsed;
    return true;
}

static uint32_t ReadMemory(struct Driver *driver, unsigned width, uint32_t address)
{
    switch (width)
    {
    case 1:
        return driver->core->busRead8(driver->core, address);
    case 2:
        return driver->core->busRead16(driver->core, address);
    default:
        return driver->core->busRead32(driver->core, address);
    }
}

static bool ParseReadWidth(const char *command, unsigned *width)
{
    if (strcmp(command, "READ8") == 0 || strcmp(command, "WAIT8") == 0)
        *width = 1;
    else if (strcmp(command, "READ16") == 0 || strcmp(command, "WAIT16") == 0)
        *width = 2;
    else if (strcmp(command, "READ32") == 0 || strcmp(command, "WAIT32") == 0)
        *width = 4;
    else
        return false;
    return true;
}

static bool RequireEnd(char *next)
{
    return next == NULL;
}

static bool RunCommand(struct Driver *driver, char *line)
{
    char *context = NULL;
    char *command = strtok_r(line, " \t\r\n", &context);
    char detail[160];
    uint32_t first;
    uint32_t second;
    uint32_t third;
    uint32_t fourth;
    unsigned width;
    unsigned i;

    if (command == NULL)
        return true;

    if (strcmp(command, "PING") == 0 && RequireEnd(strtok_r(NULL, " \t\r\n", &context)))
    {
        Reply("OK", "pong");
        return true;
    }

    if (strcmp(command, "QUIT") == 0 && RequireEnd(strtok_r(NULL, " \t\r\n", &context)))
    {
        Reply("OK", "bye");
        return false;
    }

    if (strcmp(command, "RESTART") == 0 && RequireEnd(strtok_r(NULL, " \t\r\n", &context)))
    {
        DestroyCore(driver);
        if (!CreateCore(driver))
        {
            Reply("ERR", "restart-failed");
            return false;
        }
        snprintf(detail, sizeof(detail), "frame=%" PRIu32, driver->core->frameCounter(driver->core));
        Reply("OK", detail);
        return true;
    }

    if (strcmp(command, "FRAME") == 0)
    {
        char *count = strtok_r(NULL, " \t\r\n", &context);
        if (!ParseU32(count, &first) || first > E2E_MAX_COMMAND_FRAMES
         || !RequireEnd(strtok_r(NULL, " \t\r\n", &context)))
        {
            Reply("ERR", "usage FRAME count");
            return true;
        }
        for (i = 0; i < first; i++)
            driver->core->runFrame(driver->core);
        snprintf(detail, sizeof(detail), "frame=%" PRIu32, driver->core->frameCounter(driver->core));
        Reply("OK", detail);
        return true;
    }

    if (strcmp(command, "KEYS") == 0)
    {
        char *keys = strtok_r(NULL, " \t\r\n", &context);
        if (!ParseU32(keys, &first) || (first & ~E2E_GBA_KEY_MASK) != 0
         || !RequireEnd(strtok_r(NULL, " \t\r\n", &context)))
        {
            Reply("ERR", "usage KEYS mask");
            return true;
        }
        driver->core->setKeys(driver->core, first);
        snprintf(detail, sizeof(detail), "keys=0x%03" PRIX32, first);
        Reply("OK", detail);
        return true;
    }

    if (strcmp(command, "PRESS") == 0)
    {
        char *keys = strtok_r(NULL, " \t\r\n", &context);
        char *heldFrames = strtok_r(NULL, " \t\r\n", &context);
        char *releasedFrames = strtok_r(NULL, " \t\r\n", &context);
        if (!ParseU32(keys, &first) || (first & ~E2E_GBA_KEY_MASK) != 0
         || !ParseU32(heldFrames, &second) || second > E2E_MAX_COMMAND_FRAMES
         || !ParseU32(releasedFrames, &third) || third > E2E_MAX_COMMAND_FRAMES
         || !RequireEnd(strtok_r(NULL, " \t\r\n", &context)))
        {
            Reply("ERR", "usage PRESS mask held_frames released_frames");
            return true;
        }
        driver->core->setKeys(driver->core, first);
        for (i = 0; i < second; i++)
            driver->core->runFrame(driver->core);
        driver->core->setKeys(driver->core, 0);
        for (i = 0; i < third; i++)
            driver->core->runFrame(driver->core);
        snprintf(detail, sizeof(detail), "frame=%" PRIu32, driver->core->frameCounter(driver->core));
        Reply("OK", detail);
        return true;
    }

    if (ParseReadWidth(command, &width) && strncmp(command, "READ", 4) == 0)
    {
        char *address = strtok_r(NULL, " \t\r\n", &context);
        if (!ParseU32(address, &first)
         || !RequireEnd(strtok_r(NULL, " \t\r\n", &context)))
        {
            Reply("ERR", "usage READ8|READ16|READ32 address");
            return true;
        }
        second = ReadMemory(driver, width, first);
        snprintf(detail, sizeof(detail), "value=0x%08" PRIX32, second);
        Reply("OK", detail);
        return true;
    }

    if (ParseReadWidth(command, &width) && strncmp(command, "WAIT", 4) == 0)
    {
        char *address = strtok_r(NULL, " \t\r\n", &context);
        char *mask = strtok_r(NULL, " \t\r\n", &context);
        char *expected = strtok_r(NULL, " \t\r\n", &context);
        char *maxFrames = strtok_r(NULL, " \t\r\n", &context);
        if (!ParseU32(address, &first) || !ParseU32(mask, &second)
         || !ParseU32(expected, &third) || !ParseU32(maxFrames, &fourth)
         || fourth > E2E_MAX_COMMAND_FRAMES
         || !RequireEnd(strtok_r(NULL, " \t\r\n", &context)))
        {
            Reply("ERR", "usage WAIT8|WAIT16|WAIT32 address mask expected max_frames");
            return true;
        }
        for (i = 0; ; i++)
        {
            uint32_t value = ReadMemory(driver, width, first);
            if ((value & second) == (third & second))
            {
                snprintf(detail, sizeof(detail), "frames=%u value=0x%08" PRIX32, i, value);
                Reply("OK", detail);
                return true;
            }
            if (i == fourth)
            {
                snprintf(detail, sizeof(detail), "wait-timeout frames=%u value=0x%08" PRIX32, i, value);
                Reply("ERR", detail);
                return true;
            }
            driver->core->runFrame(driver->core);
        }
    }

    if (strcmp(command, "SCREENSHOT") == 0)
    {
        char *path = strtok_r(NULL, " \t\r\n", &context);
        struct VFile *file;
        bool success;

        if (path == NULL || !RequireEnd(strtok_r(NULL, " \t\r\n", &context)))
        {
            Reply("ERR", "usage SCREENSHOT path");
            return true;
        }
        file = VFileOpen(path, O_CREAT | O_TRUNC | O_WRONLY);
        if (file == NULL)
        {
            Reply("ERR", "screenshot-open-failed");
            return true;
        }
        success = mCoreTakeScreenshotVF(driver->core, file);
        file->close(file);
        Reply(success ? "OK" : "ERR", success ? "screenshot-written" : "screenshot-write-failed");
        return true;
    }

    Reply("ERR", "unknown-command");
    return true;
}

int main(int argc, char **argv)
{
    struct Driver driver = {0};
    char line[4096];
    int i;

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--help") == 0)
        {
            PrintUsage(stdout, argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--protocol-version") == 0)
        {
            printf("%d\n", E2E_PROTOCOL_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--rom") == 0 && i + 1 < argc)
        {
            driver.romPath = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--save") == 0 && i + 1 < argc)
        {
            driver.savePath = argv[++i];
            continue;
        }

        fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]);
        PrintUsage(stderr, argv[0]);
        return 2;
    }

    if (driver.romPath == NULL)
    {
        fputs("Missing required --rom option\n", stderr);
        PrintUsage(stderr, argv[0]);
        return 2;
    }

    driver.logger.log = LogToStderr;
    driver.logger.filter = NULL;
    mLogSetDefaultLogger(&driver.logger);
    if (!CreateCore(&driver))
    {
        mLogSetDefaultLogger(NULL);
        return 1;
    }

    printf("READY protocol=%d frame=%" PRIu32 "\n",
           E2E_PROTOCOL_VERSION, driver.core->frameCounter(driver.core));
    fflush(stdout);

    while (fgets(line, sizeof(line), stdin) != NULL)
    {
        if (!RunCommand(&driver, line))
            break;
    }

    DestroyCore(&driver);
    mLogSetDefaultLogger(NULL);
    return 0;
}
