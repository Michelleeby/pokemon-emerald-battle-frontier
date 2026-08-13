#include "test.h"

#define REG_DEBUG_STRING ((volatile char *)0x04FFF600)
#define REG_DEBUG_FLAGS  (*(volatile unsigned short *)0x04FFF700)
#define REG_DEBUG_ENABLE (*(volatile unsigned short *)0x04FFF780)
#ifndef MGBA_LOG_ERROR
#define MGBA_LOG_ERROR 1
#define MGBA_LOG_INFO  3
#endif
#define MGBA_LOG_SEND  0x100

extern void TestExit(u32 status);

static void CopyString(volatile char *destination, const char *source, u32 *offset)
{
    while (*source != '\0' && *offset < 255)
        destination[(*offset)++] = *source++;
}

static void AppendNumber(volatile char *destination, u32 value, u32 *offset)
{
    const u32 divisors[] = {10000, 1000, 100, 10, 1};
    u32 i;
    unsigned int started = 0;

    for (i = 0; i < sizeof(divisors) / sizeof(divisors[0]); i++)
    {
        unsigned int digit = 0;

        while (value >= divisors[i])
        {
            value -= divisors[i];
            digit++;
        }
        if (digit != 0 || started || divisors[i] == 1)
        {
            destination[(*offset)++] = '0' + digit;
            started = 1;
        }
    }
}

static void SendLog(unsigned short level)
{
    REG_DEBUG_FLAGS = level | MGBA_LOG_SEND;
}

void TestLog(const char *message)
{
    u32 offset = 0;

    CopyString(REG_DEBUG_STRING, message, &offset);
    REG_DEBUG_STRING[offset] = '\0';
    SendLog(MGBA_LOG_INFO);
}

void TestFail(const char *file, u32 line, const char *expression)
{
    u32 offset = 0;

    CopyString(REG_DEBUG_STRING, "TEST_FAIL ", &offset);
    CopyString(REG_DEBUG_STRING, file, &offset);
    if (offset < 255)
        REG_DEBUG_STRING[offset++] = ':';
    AppendNumber(REG_DEBUG_STRING, line, &offset);
    CopyString(REG_DEBUG_STRING, ": assertion `", &offset);
    CopyString(REG_DEBUG_STRING, expression, &offset);
    CopyString(REG_DEBUG_STRING, "` failed", &offset);
    REG_DEBUG_STRING[offset] = '\0';
    SendLog(MGBA_LOG_ERROR);
    TestExit(1);
}

void TestFinish(void)
{
    TestLog("TEST_PASS");
    TestExit(0);
}

#ifdef TEST_GAME
void TestMain(void)
#else
void AgbMain(void)
#endif
{
    REG_DEBUG_ENABLE = 0xC0DE;
    TestLog("TEST_BEGIN");
    RunTest();
    TestFinish();
}
