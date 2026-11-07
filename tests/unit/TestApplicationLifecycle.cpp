#include <gtest/gtest.h>
#include "HostApplication.h"
#include "ClientApplication.h"

using namespace mist::host;
using namespace mist::client;
using namespace mist::common;

TEST(ApplicationLifecycleTest, HostVersionAndHelp) {
    HostApplication app;

    char arg0[] = "mist_host";
    char argVersion[] = "--version";
    char* argvVersion[] = {arg0, argVersion};
    auto resVersion = app.initialize(2, argvVersion);
    ASSERT_TRUE(resVersion.hasValue());
    EXPECT_EQ(resVersion.value(), std::optional<int>(0));

    char argHelp[] = "--help";
    char* argvHelp[] = {arg0, argHelp};
    auto resHelp = app.initialize(2, argvHelp);
    ASSERT_TRUE(resHelp.hasValue());
    EXPECT_EQ(resHelp.value(), std::optional<int>(0));
}

TEST(ApplicationLifecycleTest, HostSelfTest) {
    HostApplication app;

    char arg0[] = "mist_host";
    char argSelfTest[] = "--self-test";
    char* argvSelfTest[] = {arg0, argSelfTest};

    auto res = app.initialize(2, argvSelfTest);
    ASSERT_TRUE(res.hasValue()) << res.error().format();
    EXPECT_EQ(res.value(), std::optional<int>(0));
}

TEST(ApplicationLifecycleTest, HostRejectsBadArgument) {
    HostApplication app;

    char arg0[] = "mist_host";
    char argBad[] = "--non-existent-flag";
    char* argvBad[] = {arg0, argBad};

    auto res = app.initialize(2, argvBad);
    EXPECT_FALSE(res.hasValue());
    EXPECT_EQ(res.error().code(), ErrorCode::InvalidArgument);
}

TEST(ApplicationLifecycleTest, HostRunAndGracefulShutdown) {
    HostApplication app;

    char arg0[] = "mist_host";
    char* argv[] = {arg0};

    auto initRes = app.initialize(1, argv);
    ASSERT_TRUE(initRes.hasValue());
    EXPECT_FALSE(initRes.value().has_value()); // Should proceed to run

    int runCode = app.run();
    EXPECT_EQ(runCode, 0);

    auto shutdownRes = app.shutdown();
    EXPECT_TRUE(shutdownRes.hasValue());
}

TEST(ApplicationLifecycleTest, ClientVersionAndHelp) {
    ClientApplication app;

    char arg0[] = "mist_client";
    char argVersion[] = "--version";
    char* argvVersion[] = {arg0, argVersion};
    auto resVersion = app.initialize(2, argvVersion);
    ASSERT_TRUE(resVersion.hasValue());
    EXPECT_EQ(resVersion.value(), std::optional<int>(0));

    char argHelp[] = "--help";
    char* argvHelp[] = {arg0, argHelp};
    auto resHelp = app.initialize(2, argvHelp);
    ASSERT_TRUE(resHelp.hasValue());
    EXPECT_EQ(resHelp.value(), std::optional<int>(0));
}

TEST(ApplicationLifecycleTest, ClientSelfTest) {
    ClientApplication app;

    char arg0[] = "mist_client";
    char argSelfTest[] = "--self-test";
    char* argvSelfTest[] = {arg0, argSelfTest};

    auto res = app.initialize(2, argvSelfTest);
    ASSERT_TRUE(res.hasValue()) << res.error().format();
    EXPECT_EQ(res.value(), std::optional<int>(0));
}

TEST(ApplicationLifecycleTest, ClientRejectsBadArgument) {
    ClientApplication app;

    char arg0[] = "mist_client";
    char argBad[] = "--non-existent-flag";
    char* argvBad[] = {arg0, argBad};

    auto res = app.initialize(2, argvBad);
    EXPECT_FALSE(res.hasValue());
    EXPECT_EQ(res.error().code(), ErrorCode::InvalidArgument);
}

TEST(ApplicationLifecycleTest, ClientRunAndGracefulShutdown) {
    ClientApplication app;

    char arg0[] = "mist_client";
    char* argv[] = {arg0};

    auto initRes = app.initialize(1, argv);
    ASSERT_TRUE(initRes.hasValue());
    EXPECT_FALSE(initRes.value().has_value());

    int runCode = app.run();
    EXPECT_EQ(runCode, 0);

    auto shutdownRes = app.shutdown();
    EXPECT_TRUE(shutdownRes.hasValue());
}
