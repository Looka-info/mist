#include <gtest/gtest.h>
#include "mist/encode/VideoEncoderFactory.h"
#include "mist/encode/SoftwareVideoEncoder.h"
#include "mist/encode/AmfVideoEncoder.h"
#include "mist/encode/QsvVideoEncoder.h"
#include "mist/decode/HevcParser.h"
#include "mist/decode/H264Parser.h"

using namespace mist::encode;

TEST(EncoderFallbacksTest, SystemCapabilitiesDetection) {
    auto sysCaps = VideoEncoderFactory::querySystemCapabilities();
    EXPECT_TRUE(sysCaps.hasSoftware);
    EXPECT_FALSE(sysCaps.availableBackends.empty());

    // Software must always be in available backends list
    auto it = std::find(sysCaps.availableBackends.begin(), sysCaps.availableBackends.end(), "Software");
    EXPECT_NE(it, sysCaps.availableBackends.end());

    // Preferred backend must be valid
    EXPECT_TRUE(sysCaps.preferredBackend == EncoderBackend::Nvenc ||
                sysCaps.preferredBackend == EncoderBackend::Amf ||
                sysCaps.preferredBackend == EncoderBackend::Qsv ||
                sysCaps.preferredBackend == EncoderBackend::Software);
}

TEST(EncoderFallbacksTest, SoftwareEncoderH264Lifecycle) {
    auto encRes = SoftwareVideoEncoder::create();
    ASSERT_TRUE(encRes.hasValue());
    auto encoder = std::move(encRes.value());

    // Invalid config should fail
    EncodeConfig invalidConfig{};
    invalidConfig.width = 0;
    auto failRes = encoder->initialize(invalidConfig);
    EXPECT_FALSE(failRes.hasValue());

    // Valid H.264 config
    EncodeConfig config{
        .codec = VideoCodec::H264,
        .width = 1920,
        .height = 1080,
        .targetFps = 60,
        .bitrateKbps = 15000,
        .gopLength = 60,
        .lowLatencyTuning = true
    };

    auto initRes = encoder->initialize(config);
    ASSERT_TRUE(initRes.hasValue());
    EXPECT_TRUE(encoder->isInitialized());

    // Encode frame 0 (keyframe expected)
    mist::capture::CapturedFrame frame0{};
    frame0.timestampNs = 1000000;
    frame0.width = 1920;
    frame0.height = 1080;

    auto enc0Res = encoder->encode(frame0);
    ASSERT_TRUE(enc0Res.hasValue());

    auto pkt0Res = encoder->getNextPacket();
    ASSERT_TRUE(pkt0Res.hasValue());
    const auto& pkt0 = pkt0Res.value();
    EXPECT_TRUE(pkt0.isKeyframe);
    EXPECT_EQ(pkt0.frameNumber, 0u);
    EXPECT_GT(pkt0.data.size(), 64u);

    // Verify Annex-B NAL start code (0x00 00 00 01)
    ASSERT_GE(pkt0.data.size(), 4u);
    EXPECT_EQ(pkt0.data[0], 0x00);
    EXPECT_EQ(pkt0.data[1], 0x00);
    EXPECT_EQ(pkt0.data[2], 0x00);
    EXPECT_EQ(pkt0.data[3], 0x01);

    // Encode frame 1 (delta frame expected)
    mist::capture::CapturedFrame frame1{};
    frame1.timestampNs = 2000000;
    frame1.width = 1920;
    frame1.height = 1080;

    auto enc1Res = encoder->encode(frame1);
    ASSERT_TRUE(enc1Res.hasValue());

    auto pkt1Res = encoder->getNextPacket();
    ASSERT_TRUE(pkt1Res.hasValue());
    const auto& pkt1 = pkt1Res.value();
    EXPECT_FALSE(pkt1.isKeyframe);
    EXPECT_EQ(pkt1.frameNumber, 1u);

    // Dynamic bitrate change
    EXPECT_TRUE(encoder->setBitrate(25000).hasValue());
    EXPECT_EQ(encoder->getConfig().bitrateKbps, 25000u);

    // Force keyframe request
    EXPECT_TRUE(encoder->requestKeyframe().hasValue());

    mist::capture::CapturedFrame frame2{};
    frame2.timestampNs = 3000000;
    EXPECT_TRUE(encoder->encode(frame2).hasValue());

    auto pkt2Res = encoder->getNextPacket();
    ASSERT_TRUE(pkt2Res.hasValue());
    EXPECT_TRUE(pkt2Res.value().isKeyframe);

    EXPECT_TRUE(encoder->flush().hasValue());
}

TEST(EncoderFallbacksTest, SoftwareEncoderHevcBitstream) {
    auto encRes = SoftwareVideoEncoder::create();
    ASSERT_TRUE(encRes.hasValue());
    auto encoder = std::move(encRes.value());

    EncodeConfig config{
        .codec = VideoCodec::HEVC,
        .width = 1280,
        .height = 720,
        .targetFps = 60,
        .bitrateKbps = 8000,
        .gopLength = 60,
        .lowLatencyTuning = true
    };

    ASSERT_TRUE(encoder->initialize(config).hasValue());

    mist::capture::CapturedFrame frame{};
    frame.timestampNs = 16666666;
    ASSERT_TRUE(encoder->encode(frame).hasValue());

    auto pktRes = encoder->getNextPacket();
    ASSERT_TRUE(pktRes.hasValue());
    const auto& pkt = pktRes.value();
    EXPECT_TRUE(pkt.isKeyframe);

    // Parse with HevcParser
    auto nals = mist::decode::HevcParser::parseNalUnits(pkt.data.data(), pkt.data.size());
    EXPECT_GE(nals.size(), 3u); // VPS, SPS, PPS, IDR

    bool hasVps = false;
    bool hasSps = false;
    bool hasPps = false;
    bool hasIdr = false;
    for (const auto& nal : nals) {
        if (nal.type == mist::decode::HevcNalType::Vps) hasVps = true;
        if (nal.type == mist::decode::HevcNalType::Sps) hasSps = true;
        if (nal.type == mist::decode::HevcNalType::Pps) hasPps = true;
        if (nal.type == mist::decode::HevcNalType::IdrWRadl) hasIdr = true;
    }
    EXPECT_TRUE(hasVps);
    EXPECT_TRUE(hasSps);
    EXPECT_TRUE(hasPps);
    EXPECT_TRUE(hasIdr);

    EXPECT_TRUE(mist::decode::HevcParser::isKeyframeAccessUnit(pkt.data.data(), pkt.data.size()));
}

TEST(EncoderFallbacksTest, FactoryCreationAndFallback) {
    // Explicit software encoder creation
    auto softRes = VideoEncoderFactory::createEncoder(EncoderBackend::Software);
    ASSERT_TRUE(softRes.hasValue());
    EXPECT_NE(softRes.value(), nullptr);

    // Auto encoder creation must always succeed
    auto autoRes = VideoEncoderFactory::createEncoder(EncoderBackend::Auto);
    ASSERT_TRUE(autoRes.hasValue());
    EXPECT_NE(autoRes.value(), nullptr);

    // AMF capability query
    auto amfCap = AmfVideoEncoder::queryCapabilities();
    if (!amfCap.isSupported) {
        auto amfRes = AmfVideoEncoder::create();
        EXPECT_FALSE(amfRes.hasValue());
    }

    // QSV capability query
    auto qsvCap = QsvVideoEncoder::queryCapabilities();
    if (!qsvCap.isSupported) {
        auto qsvRes = QsvVideoEncoder::create();
        EXPECT_FALSE(qsvRes.hasValue());
    }
}

TEST(EncoderFallbacksTest, BackendStringConversions) {
    EXPECT_EQ(VideoEncoderFactory::backendToString(EncoderBackend::Nvenc), "NVENC");
    EXPECT_EQ(VideoEncoderFactory::backendToString(EncoderBackend::Amf), "AMF");
    EXPECT_EQ(VideoEncoderFactory::backendToString(EncoderBackend::Qsv), "QuickSync");
    EXPECT_EQ(VideoEncoderFactory::backendToString(EncoderBackend::Software), "Software");
    EXPECT_EQ(VideoEncoderFactory::backendToString(EncoderBackend::Auto), "Auto");

    EXPECT_EQ(VideoEncoderFactory::stringToBackend("NVENC"), EncoderBackend::Nvenc);
    EXPECT_EQ(VideoEncoderFactory::stringToBackend("amf"), EncoderBackend::Amf);
    EXPECT_EQ(VideoEncoderFactory::stringToBackend("quicksync"), EncoderBackend::Qsv);
    EXPECT_EQ(VideoEncoderFactory::stringToBackend("software"), EncoderBackend::Software);
    EXPECT_EQ(VideoEncoderFactory::stringToBackend("x264"), EncoderBackend::Software);
    EXPECT_EQ(VideoEncoderFactory::stringToBackend("auto"), EncoderBackend::Auto);
}
