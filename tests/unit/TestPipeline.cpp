#include <gtest/gtest.h>
#include "mist/capture/CaptureEncodePipeline.h"
#include "mist/encode/NvencVideoEncoder.h"

using namespace mist::pipeline;
using namespace mist::capture;
using namespace mist::encode;
using namespace mist::common;

TEST(PipelineTest, ReportJsonFormatting) {
    PipelineReport report{
        .hardwareGpu = "NVIDIA GeForce GTX 1660 SUPER",
        .driverVersion = "32.0.16.1714",
        .width = 1920,
        .height = 1080,
        .targetFps = 60,
        .actualFps = 59.98,
        .averageBitrateKbps = 14950.0,
        .framesAcquired = 600,
        .framesEncoded = 600,
        .framesDropped = 0,
        .captureTimeouts = 5,
        .captureErrors = 0,
        .encodeErrors = 0,
        .captureLatencyUs = {},
        .conversionLatencyUs = {},
        .encodeLatencyUs = {},
        .captureToBitstreamLatencyUs = {}
    };

    std::string json = report.toJson();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("\"gpu\": \"NVIDIA GeForce GTX 1660 SUPER\""), std::string::npos);
    EXPECT_NE(json.find("\"width\": 1920"), std::string::npos);
    EXPECT_NE(json.find("\"frames_encoded\": 600"), std::string::npos);
    EXPECT_NE(json.find("\"latencies_us\""), std::string::npos);
}

TEST(PipelineTest, SyntheticPipelineBenchmark) {
#if defined(_WIN32)
    auto cap = NvencVideoEncoder::queryCapabilities();
    if (!cap.isSupported) {
        GTEST_SKIP() << "NVIDIA NVENC hardware unavailable";
    }

    CaptureConfig capConfig{
        .displayIndex = 0,
        .targetFps = 60,
        .format = PixelFormat::B8G8R8A8_UNORM,
        .captureCursor = false
    };

    EncodeConfig encConfig{
        .codec = VideoCodec::H264,
        .width = 1280,
        .height = 720,
        .targetFps = 60,
        .bitrateKbps = 5000,
        .gopLength = 60,
        .lowLatencyTuning = true
    };

    CaptureEncodePipeline pipeline;
    auto initRes = pipeline.initialize(capConfig, encConfig, /*useSyntheticSource=*/true);
    ASSERT_TRUE(initRes.hasValue()) << initRes.error().format();

    auto runRes = pipeline.runForDuration(1); // 1-second synthetic run
    ASSERT_TRUE(runRes.hasValue()) << runRes.error().format();

    const auto& report = runRes.value();
    EXPECT_GT(report.framesAcquired, 0u);
    EXPECT_GT(report.framesEncoded, 0u);
    EXPECT_EQ(report.encodeErrors, 0u);
#endif
}
