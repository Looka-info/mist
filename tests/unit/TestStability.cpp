#include <gtest/gtest.h>
#include "mist/capture/DxgiCaptureEngine.h"
#include "mist/encode/NvencVideoEncoder.h"
#include "mist/platform/windows/D3D11Device.h"

using namespace mist::capture;
using namespace mist::encode;
using namespace mist::platform::windows;
using namespace mist::common;

TEST(StabilityTest, RepeatedCaptureInitialization) {
#if defined(_WIN32)
    auto displays = D3D11DeviceManager::enumerateDisplays();
    if (displays.empty()) {
        GTEST_SKIP() << "No displays found for stability test";
    }

    CaptureConfig config{
        .displayIndex = 0,
        .targetFps = 60,
        .format = PixelFormat::B8G8R8A8_UNORM,
        .captureCursor = true
    };

    // Initialize and shutdown 5 times consecutively
    for (int i = 0; i < 5; ++i) {
        DxgiCaptureEngine engine;
        auto initRes = engine.initialize(config);
        if (!initRes) {
            GTEST_SKIP() << "Duplication unavailable in session: " << initRes.error().format();
        }
        EXPECT_TRUE(engine.start().hasValue());
        EXPECT_TRUE(engine.stop().hasValue());
    }
#endif
}

TEST(StabilityTest, RepeatedEncoderInitialization) {
#if defined(_WIN32)
    auto cap = NvencVideoEncoder::queryCapabilities();
    if (!cap.isSupported) {
        GTEST_SKIP() << "NVIDIA NVENC hardware unavailable";
    }

    EncodeConfig config{
        .codec = VideoCodec::H264,
        .width = 1280,
        .height = 720,
        .targetFps = 60,
        .bitrateKbps = 5000,
        .gopLength = 60,
        .lowLatencyTuning = true
    };

    // Initialize and cleanup encoder 5 times consecutively
    for (int i = 0; i < 5; ++i) {
        auto encRes = NvencVideoEncoder::create();
        ASSERT_TRUE(encRes.hasValue());
        auto encoder = std::move(encRes.value());
        EXPECT_TRUE(encoder->initialize(config).hasValue());
        EXPECT_TRUE(encoder->flush().hasValue());
    }
#endif
}
