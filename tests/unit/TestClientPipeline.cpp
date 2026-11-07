#include <gtest/gtest.h>
#include "mist/decode/DecodePresentPipeline.h"
#include <fstream>

using namespace mist::decode;
using namespace mist::render;

static std::string findTestH264() {
    for (const auto& p : { "artifacts/test.h264", "../../artifacts/test.h264", "../../../artifacts/test.h264" }) {
        std::ifstream f(p, std::ios::binary);
        if (f.is_open()) return p;
    }
    return "";
}

TEST(ClientPipelineTest, RunBitstreamHeadless) {
#if defined(_WIN32)
    std::string path = findTestH264();
    if (path.empty()) {
        GTEST_SKIP() << "artifacts/test.h264 not present on disk, skipping pipeline test.";
    }

    auto cap = D3D11VideoDecoder::queryCapabilities();
    if (!cap.supportsH264) {
        GTEST_SKIP() << "Direct3D 11 hardware H.264 video decoding unavailable";
    }

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
    ASSERT_TRUE(initRes.hasValue()) << initRes.error().format();

    auto runRes = pipeline.runBitstream(path, 60, 1);
    ASSERT_TRUE(runRes.hasValue()) << runRes.error().format();

    const auto& report = runRes.value();
    EXPECT_GT(report.accessUnitsIngested, 0u);
    EXPECT_GT(report.framesDecoded, 0u);
    EXPECT_GT(report.framesPresented, 0u);
    EXPECT_GT(report.actualFps, 0.0);

    std::string jsonStr = report.toJson();
    EXPECT_FALSE(jsonStr.empty());
    EXPECT_NE(jsonStr.find("\"framesDecoded\""), std::string::npos);
    EXPECT_NE(jsonStr.find("\"actualFps\""), std::string::npos);
    EXPECT_NE(jsonStr.find("\"hardwareGpu\""), std::string::npos);
#endif
}
