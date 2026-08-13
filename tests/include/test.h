#ifndef GUARD_TEST_H
#define GUARD_TEST_H

#ifdef TEST_GAME
#include "global.h"
#else
typedef unsigned short u16;
typedef unsigned int u32;
#endif

void TestLog(const char *message);
void TestFail(const char *file, u32 line, const char *expression);
void TestFinish(void);
void TestMain(void);
void RunTest(void);
#ifdef TEST_GAME
void TestResetFixture(u16 seed);
#endif

#define TEST_ASSERT(expression) \
    do \
    { \
        if (!(expression)) \
            TestFail(__FILE__, __LINE__, #expression); \
    } while (0)

#define TEST_ASSERT_EQ(expected, actual) TEST_ASSERT((expected) == (actual))

#endif // GUARD_TEST_H
