#include <gtest/gtest.h>
#include "mist/encode/NvencVideoEncoder.h"
#include "mist/capture/SyntheticFrameGenerator.h"
#include "mist/platform/windows/D3D11Device.h"

using namespace mist::encode;
using namespace mist::capture;
using namespace mist::platform::windows;
using namespace mist::common;

TEST(EncoderTest, QueryCapabilities) {
    auto cap = NvencVideoEncoder::queryCapabilities();
    // Querying capabilities must never crash
    if (cap.isSupported) {
        EXPECT_TRUE(cap.supportsH264);
        EXPECT_GE(cap.maxResolutionWidth, 1920u);
        EXPECT_GE(cap.maxResolutionHeight, 1080u);
    }
}

TEST(EncoderTest, NvencInitializationAndBitstreamOutput) {
#if defined(_WIN32)
    auto cap = NvencVideoEncoder::queryCapabilities();
    if (!cap.isSupported) {
        GTEST_SKIP() << "NVIDIA NVENC hardware unavailable";
    }

    auto devRes = D3D11DeviceManager::createDevice(0);
    ASSERT_TRUE(devRes.hasValue());

    SyntheticFrameGenerator gen;
    auto genRes = gen.initialize(devRes.value().first.Get(), devRes.value().second.Get(), 1280, 720);
    ASSERT_TRUE(genRes.hasValue());

    auto encRes = NvencVideoEncoder::create();
    ASSERT_TRUE(encRes.hasValue()) << encRes.error().format();
    auto encoder = std::move(encRes.value());

    EncodeConfig config{
        .codec = VideoCodec::H264,
        .width = 1280,
        .height = 720,
        .targetFps = 60,
        .bitrateKbps = 5000,
        .gopLength = 60,
        .lowLatencyTuning = true
    };

    auto initRes = encoder->initializeWithDevice(config, devRes.value().first, devRes.value().second);
    ASSERT_TRUE(initRes.hasValue()) << initRes.error().format();
    EXPECT_TRUE(encoder->isInitialized());

    // Generate and encode 5 frames
    bool receivedKeyframe = false;
    size_t totalPackets = 0;

    for (uint64_t i = 1; i <= 5; ++i) {
        auto frameRes = gen.generateFrame(i, i * 16666666);
        ASSERT_TRUE(frameRes.hasValue());

        auto encFrameRes = encoder->encode(frameRes.value());
        ASSERT_TRUE(encFrameRes.hasValue()) << encFrameRes.error().format();

        auto pktRes = encoder->getNextPacket();
        if (pktRes) {
            ++totalPackets;
            const auto& pkt = pktRes.value();
            EXPECT_FALSE(pkt.data.empty());
            if (pkt.isKeyframe) {
                receivedKeyframe = true;
            }

            // Verify H.264 Start Code prefix (0x00 0x00 0x01 or 0x00 0x00 0x00 0x01)
            ASSERT_GE(pkt.data.size(), 4u);
            bool hasStartCode = (pkt.data[0] == 0 && pkt.data[1] == 0 && pkt.data[2] == 1) ||
                                (pkt.data[0] == 0 && pkt.data[1] == 0 && pkt.data[2] == 0 && pkt.data[3] == 1);
            EXPECT_TRUE(hasStartCode) << "H.264 NAL unit start code missing from output bitstream";
        }
    }

    EXPECT_GT(totalPackets, 0u);
    EXPECT_TRUE(receivedKeyframe) << "At least one IDR keyframe should be generated";

    auto flushRes = encoder->flush();
    EXPECT_TRUE(flushRes.hasValue());
#endif
}
