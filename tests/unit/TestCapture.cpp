#include <gtest/gtest.h>
#include "mist/capture/DxgiCaptureEngine.h"
#include "mist/capture/SyntheticFrameGenerator.h"
#include "mist/platform/windows/D3D11Device.h"

using namespace mist::capture;
using namespace mist::platform::windows;
using namespace mist::common;

TEST(CaptureTest, DisplayEnumeration) {
    auto displays = D3D11DeviceManager::enumerateDisplays();
    // In any graphical environment, at least zero or more displays are returned without crashing
    for (const auto& d : displays) {
        EXPECT_FALSE(d.deviceName.empty());
        EXPECT_GT(d.width, 0u);
        EXPECT_GT(d.height, 0u);
    }
}

TEST(CaptureTest, InvalidDisplayIndexRejected) {
    DxgiCaptureEngine engine;
    CaptureConfig config{
        .displayIndex = 9999, // Non-existent display
        .targetFps = 60,
        .format = PixelFormat::B8G8R8A8_UNORM,
        .captureCursor = true
    };

    auto res = engine.initialize(config);
    EXPECT_FALSE(res.hasValue());
    EXPECT_EQ(res.error().code(), ErrorCode::NotFound);
}

TEST(CaptureTest, SyntheticFrameGeneratorLifecycle) {
#if defined(_WIN32)
    auto devRes = D3D11DeviceManager::createDevice(0);
    if (!devRes) {
        GTEST_SKIP() << "D3D11 hardware device unavailable";
    }

    SyntheticFrameGenerator gen;
    auto initRes = gen.initialize(devRes.value().first.Get(), devRes.value().second.Get(), 1280, 720);
    ASSERT_TRUE(initRes.hasValue()) << initRes.error().format();

    auto frameRes = gen.generateFrame(1, 1000000);
    ASSERT_TRUE(frameRes.hasValue()) << frameRes.error().format();

    const auto& frame = frameRes.value();
    EXPECT_EQ(frame.width, 1280u);
    EXPECT_EQ(frame.height, 720u);
    EXPECT_EQ(frame.frameNumber, 1u);
    EXPECT_EQ(frame.timestampNs, 1000000u);
    EXPECT_NE(frame.nativeResource, nullptr);
#endif
}

TEST(CaptureTest, LiveDesktopCapture) {
#if defined(_WIN32)
    auto displays = D3D11DeviceManager::enumerateDisplays();
    if (displays.empty()) {
        GTEST_SKIP() << "No active desktop display found for live capture";
    }

    DxgiCaptureEngine engine;
    CaptureConfig config{
        .displayIndex = 0,
        .targetFps = 60,
        .format = PixelFormat::B8G8R8A8_UNORM,
        .captureCursor = true
    };

    auto initRes = engine.initialize(config);
    if (!initRes) {
        GTEST_SKIP() << "Desktop duplication not available in current session: " << initRes.error().format();
    }

    auto startRes = engine.start();
    ASSERT_TRUE(startRes.hasValue());
    EXPECT_TRUE(engine.isCapturing());

    // Attempt to acquire at least one frame within 500ms
    auto frameRes = engine.acquireNextFrame(500);
    if (frameRes) {
        EXPECT_EQ(frameRes.value().width, displays[0].width);
        EXPECT_EQ(frameRes.value().height, displays[0].height);
        EXPECT_NE(frameRes.value().nativeResource, nullptr);
    } else {
        // Timeout is an acceptable result if the desktop screen is completely static
        EXPECT_EQ(frameRes.error().code(), ErrorCode::Timeout);
    }

    (void)engine.stop();
    EXPECT_FALSE(engine.isCapturing());
#endif
}
