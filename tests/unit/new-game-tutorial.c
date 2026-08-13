#include "global.h"
#include "frontier_intro_tutorial.h"
#include "main.h"
#include "test.h"

void RunTest(void)
{
    u32 i;

    TestResetFixture(0x2222);
    FrontierIntroTutorial_TestStart();
    TEST_ASSERT(FrontierIntroTutorial_TestIsActive());
    TEST_ASSERT_EQ(0, FrontierIntroTutorial_TestGetStep());

    FrontierIntroTutorial_NotifyReady(FRONTIER_INTRO_CHECKPOINT_POKENAV_MAIN);
    for (i = 0; i < 40; i++)
        FrontierIntroTutorial_ApplyInput();
    TEST_ASSERT_EQ(1, FrontierIntroTutorial_TestGetStep());

    gMain.newKeys = B_BUTTON;
    FrontierIntroTutorial_ApplyInput();
    gMain.newKeys = B_BUTTON;
    FrontierIntroTutorial_ApplyInput();
    TEST_ASSERT(FrontierIntroTutorial_IsSkipping());
    FrontierIntroTutorial_CompleteSkip();
    TEST_ASSERT(!FrontierIntroTutorial_TestIsActive());
}
