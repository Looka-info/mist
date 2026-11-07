#include <gtest/gtest.h>
#include "mist/platform/windows/PlatformWindows.h"
#include <thread>
#include <chrono>

using namespace mist::platform::windows;
using namespace mist::common;

TEST(PlatformWindowsTest, Win32ErrorMapping) {
    auto ok = win32ErrorToMistError(0, SubsystemId::Platform, "No error");
    EXPECT_TRUE(ok.isSuccess());

    auto notFound = win32ErrorToMistError(2, SubsystemId::Platform, "File test");
    EXPECT_EQ(notFound.code(), ErrorCode::NotFound);
    EXPECT_EQ(notFound.platformCode(), 2);

    auto accessDenied = win32ErrorToMistError(5, SubsystemId::Platform, "Access test");
    EXPECT_EQ(accessDenied.code(), ErrorCode::PermissionDenied);

    auto timeout = win32ErrorToMistError(1460, SubsystemId::Platform, "Timeout test");
    EXPECT_EQ(timeout.code(), ErrorCode::Timeout);
}

TEST(PlatformWindowsTest, HResultMapping) {
    auto ok = hresultToMistError(0, SubsystemId::Platform, "S_OK");
    EXPECT_TRUE(ok.isSuccess());

    auto invalidArg = hresultToMistError(static_cast<long>(0x80070057), SubsystemId::Render, "D3D param");
    EXPECT_EQ(invalidArg.code(), ErrorCode::InvalidArgument);

    auto notImpl = hresultToMistError(static_cast<long>(0x80004001), SubsystemId::Capture, "Capture feature");
    EXPECT_EQ(notImpl.code(), ErrorCode::NotImplemented);
}

TEST(PlatformWindowsTest, HighResolutionTimerMonotonicity) {
    HighResolutionTimer timer;

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    double us1 = timer.elapsedMicroseconds();
    EXPECT_GT(us1, 5000.0); // At least 5ms

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    double us2 = timer.elapsedMicroseconds();
    EXPECT_GT(us2, us1);

    timer.reset();
    double usAfterReset = timer.elapsedMicroseconds();
    EXPECT_LT(usAfterReset, us2);
}
