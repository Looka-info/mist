#include <gtest/gtest.h>
#include "DesktopApplication.h"

using namespace mist::desktop;

TEST(DesktopAppTest, HelpAndVersionArgs) {
    DesktopApplication app;
    char* argv1[] = {const_cast<char*>("mist_desktop.exe"), const_cast<char*>("--help")};
    auto res1 = app.initialize(2, argv1);
    ASSERT_TRUE(res1.hasValue());
    EXPECT_TRUE(res1.value().has_value());
    EXPECT_EQ(res1.value().value(), 0);

    char* argv2[] = {const_cast<char*>("mist_desktop.exe"), const_cast<char*>("--version")};
    auto res2 = app.initialize(2, argv2);
    ASSERT_TRUE(res2.hasValue());
    EXPECT_TRUE(res2.value().has_value());
    EXPECT_EQ(res2.value().value(), 0);
}

TEST(DesktopAppTest, SelfTestAndProfileConfig) {
    DesktopApplication app;
    char* argv[] = {
        const_cast<char*>("mist_desktop.exe"),
        const_cast<char*>("--self-test"),
        const_cast<char*>("--profile"),
        const_cast<char*>("Performance")
    };

    auto initRes = app.initialize(4, argv);
    ASSERT_TRUE(initRes.hasValue());
    EXPECT_FALSE(initRes.value().has_value()); // Continues to run()

    EXPECT_EQ(app.getQualityManager().getProfile(), mist::transport::qos::QualityProfile::Performance);

    int runCode = app.run();
    EXPECT_EQ(runCode, 0);

    auto shutRes = app.shutdown();
    EXPECT_TRUE(shutRes.hasValue());
}
