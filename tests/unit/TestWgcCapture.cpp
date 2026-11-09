#include <gtest/gtest.h>
#include "mist/capture/WgcCaptureEngine.h"
#include "mist/capture/CaptureEngineFactory.h"
#include "mist/capture/DxgiCaptureEngine.h"

using namespace mist;
using namespace mist::capture;

TEST(WgcCaptureTest, PlatformSupportDetection) {
#if defined(_WIN32)
    // Windows 10 build 17134+ is required for WGC. Windows 11 host always satisfies this.
    EXPECT_TRUE(WgcCaptureEngine::isSupported());
    EXPECT_TRUE(WgcCaptureEngine::supportsHybridAdapterCapture());
#endif
}

TEST(WgcCaptureTest, FactoryQueryAvailableBackends) {
    auto backends = CaptureEngineFactory::queryAvailableBackends();
    EXPECT_FALSE(backends.empty());

#if defined(_WIN32)
    bool hasDxgi = false;
    bool hasWgc = false;
    bool hasSynthetic = false;

    for (auto b : backends) {
        if (b == CaptureBackend::DxgiDuplication) hasDxgi = true;
        if (b == CaptureBackend::WindowsGraphicsCapture) hasWgc = true;
        if (b == CaptureBackend::Synthetic) hasSynthetic = true;
    }

    EXPECT_TRUE(hasDxgi);
    EXPECT_TRUE(hasWgc);
    EXPECT_TRUE(hasSynthetic);
    EXPECT_TRUE(CaptureEngineFactory::isBackendSupported(CaptureBackend::WindowsGraphicsCapture));
    EXPECT_TRUE(CaptureEngineFactory::isBackendSupported(CaptureBackend::DxgiDuplication));
#endif
}

TEST(WgcCaptureTest, WgcEngineLifecycleAndFrameAcquisition) {
    WgcCaptureEngine engine;
    EXPECT_FALSE(engine.isCapturing());
    EXPECT_EQ(engine.getBackend(), CaptureBackend::WindowsGraphicsCapture);

    CaptureConfig config{};
    config.displayIndex = 0;
    config.targetFps = 60;
    config.format = PixelFormat::B8G8R8A8_UNORM;
    config.captureCursor = true;
    config.disableBorder = true;
    config.simulationOnly = true;

    auto initRes = engine.initialize(config);
    ASSERT_TRUE(initRes.hasValue());
    EXPECT_TRUE(engine.isBorderSuppressed());
    EXPECT_TRUE(engine.isCursorCaptureEnabled());
    EXPECT_EQ(engine.getWidth(), 1920u);
    EXPECT_EQ(engine.getHeight(), 1080u);

    auto startRes = engine.start();
    ASSERT_TRUE(startRes.hasValue());
    EXPECT_TRUE(engine.isCapturing());

    auto frameRes1 = engine.acquireNextFrame(100);
    ASSERT_TRUE(frameRes1.hasValue());
    const auto& frame1 = frameRes1.value();
    EXPECT_EQ(frame1.width, 1920u);
    EXPECT_EQ(frame1.height, 1080u);
    EXPECT_EQ(frame1.frameNumber, 1u);
    EXPECT_GT(frame1.timestampNs, 0u);

    auto frameRes2 = engine.acquireNextFrame(100);
    ASSERT_TRUE(frameRes2.hasValue());
    EXPECT_EQ(frameRes2.value().frameNumber, 2u);

    EXPECT_TRUE(engine.releaseFrame(frame1).hasValue());

    auto stopRes = engine.stop();
    EXPECT_TRUE(stopRes.hasValue());
    EXPECT_FALSE(engine.isCapturing());

    // Acquire after stop fails with InvalidState
    auto failFrame = engine.acquireNextFrame(100);
    EXPECT_FALSE(failFrame.hasValue());
    EXPECT_EQ(failFrame.error().code(), common::ErrorCode::InvalidState);
}

TEST(WgcCaptureTest, FactoryCreateAndFallback) {
    CaptureConfig config{};
    config.displayIndex = 0;
    config.targetFps = 60;
    config.format = PixelFormat::B8G8R8A8_UNORM;
    config.disableBorder = true;
    config.simulationOnly = true;

    // Direct creation of WGC backend
    auto [engine, res] = CaptureEngineFactory::createAndInitialize(
        config, CaptureBackend::WindowsGraphicsCapture, /*allowFallback=*/false
    );
    ASSERT_TRUE(res.hasValue());
    ASSERT_NE(engine, nullptr);
    EXPECT_EQ(engine->getBackend(), CaptureBackend::WindowsGraphicsCapture);

    // Backend string formatting
    EXPECT_EQ(captureBackendToString(CaptureBackend::WindowsGraphicsCapture), "Windows.Graphics.Capture");
    EXPECT_EQ(captureBackendToString(CaptureBackend::DxgiDuplication), "DXGI Desktop Duplication");
    EXPECT_EQ(captureBackendToString(CaptureBackend::Synthetic), "Synthetic Generator");
}
