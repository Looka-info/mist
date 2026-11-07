#include <gtest/gtest.h>
#include "mist/decode/D3D11VideoDecoder.h"
#include "mist/render/D3D11Renderer.h"
#include "mist/decode/DecodePresentPipeline.h"

using namespace mist::decode;
using namespace mist::render;

TEST(ClientStabilityTest, RepeatedRendererInitTeardown) {
#if defined(_WIN32)
    for (int i = 0; i < 5; ++i) {
        auto createRes = D3D11Renderer::create();
        ASSERT_TRUE(createRes.hasValue());
        auto renderer = std::move(createRes.value());

        RenderConfig config{
            .nativeWindowHandle = nullptr,
            .width = 1280,
            .height = 720,
            .vsync = false,
            .headless = true
        };

        auto initRes = renderer->initialize(config);
        ASSERT_TRUE(initRes.hasValue()) << "Failed at iteration " << i << ": " << initRes.error().format();
        EXPECT_TRUE(renderer->isInitialized());

        auto shutRes = renderer->shutdown();
        EXPECT_TRUE(shutRes.hasValue());
        EXPECT_FALSE(renderer->isInitialized());
    }
#endif
}

TEST(ClientStabilityTest, RepeatedDecoderInitTeardown) {
#if defined(_WIN32)
    auto cap = D3D11VideoDecoder::queryCapabilities();
    if (!cap.supportsH264) {
        GTEST_SKIP() << "Direct3D 11 hardware H.264 video decoding unavailable";
    }

    for (int i = 0; i < 5; ++i) {
        auto decRes = D3D11VideoDecoder::create();
        ASSERT_TRUE(decRes.hasValue()) << "Failed at iteration " << i << ": " << decRes.error().format();
        auto decoder = std::move(decRes.value());

        DecodeConfig config{
            .codec = mist::encode::VideoCodec::H264,
            .preferHardwareDecoding = true,
            .lowLatencyTuning = true
        };

        auto initRes = decoder->initialize(config);
        ASSERT_TRUE(initRes.hasValue()) << "Failed at iteration " << i << ": " << initRes.error().format();
        EXPECT_TRUE(decoder->isInitialized());

        auto shutRes = decoder->shutdown();
        EXPECT_TRUE(shutRes.hasValue());
        EXPECT_FALSE(decoder->isInitialized());
    }
#endif
}

TEST(ClientStabilityTest, RepeatedPipelineInitTeardown) {
#if defined(_WIN32)
    auto cap = D3D11VideoDecoder::queryCapabilities();
    if (!cap.supportsH264) {
        GTEST_SKIP() << "Direct3D 11 hardware H.264 video decoding unavailable";
    }

    for (int i = 0; i < 3; ++i) {
        DecodePresentPipeline pipeline;

        DecodeConfig decConfig{
            .codec = mist::encode::VideoCodec::H264,
            .preferHardwareDecoding = true,
            .lowLatencyTuning = true
        };

        RenderConfig rndConfig{
            .nativeWindowHandle = nullptr,
            .width = 1920,
            .height = 1080,
            .vsync = false,
            .headless = true
        };

        auto initRes = pipeline.initialize(decConfig, rndConfig);
        ASSERT_TRUE(initRes.hasValue()) << "Failed at iteration " << i << ": " << initRes.error().format();

        auto stopRes = pipeline.stop();
        EXPECT_TRUE(stopRes.hasValue());
    }
#endif
}
