#include <gtest/gtest.h>
#include "mist/decode/HevcParser.h"
#include "mist/transport/CodecNegotiator.h"
#include "mist/decode/D3D11VideoDecoder.h"

using namespace mist::decode;
using namespace mist::transport;

namespace {

// Helper to construct a synthetic Annex-B HEVC NAL unit with 4-byte start code
std::vector<uint8_t> makeHevcNal(HevcNalType type, const std::vector<uint8_t>& payload = {}) {
    std::vector<uint8_t> nal;
    // 4-byte start code: 0x00, 0x00, 0x00, 0x01
    nal.push_back(0x00);
    nal.push_back(0x00);
    nal.push_back(0x00);
    nal.push_back(0x01);

    // HEVC NAL unit header (2 bytes)
    // forbidden_zero_bit (1) = 0
    // nal_unit_type (6)
    // nuh_layer_id (6) = 0
    // nuh_temporal_id_plus1 (3) = 1
    uint8_t byte0 = static_cast<uint8_t>((static_cast<uint8_t>(type) & 0x3F) << 1);
    uint8_t byte1 = 0x01; // nuh_layer_id = 0, temporal_id_plus1 = 1

    nal.push_back(byte0);
    nal.push_back(byte1);
    nal.insert(nal.end(), payload.begin(), payload.end());
    return nal;
}

} // namespace

TEST(HevcCodecTest, ParseNalUnits) {
    // Construct synthetic HEVC Annex-B bitstream with VPS, SPS, PPS, and IDR
    std::vector<uint8_t> bitstream;

    auto vps = makeHevcNal(HevcNalType::Vps, {0x10, 0x20});
    auto sps = makeHevcNal(HevcNalType::Sps, {0x01, 0x02, 0x03});
    auto pps = makeHevcNal(HevcNalType::Pps, {0xAA, 0xBB});
    auto idr = makeHevcNal(HevcNalType::IdrWRadl, {0x11, 0x22, 0x33, 0x44});

    bitstream.insert(bitstream.end(), vps.begin(), vps.end());
    bitstream.insert(bitstream.end(), sps.begin(), sps.end());
    bitstream.insert(bitstream.end(), pps.begin(), pps.end());
    bitstream.insert(bitstream.end(), idr.begin(), idr.end());

    auto nals = HevcParser::parseNalUnits(bitstream.data(), bitstream.size());
    ASSERT_EQ(nals.size(), 4u);

    EXPECT_EQ(nals[0].type, HevcNalType::Vps);
    EXPECT_EQ(nals[0].startCodeSize, 4u);

    EXPECT_EQ(nals[1].type, HevcNalType::Sps);
    EXPECT_EQ(nals[1].startCodeSize, 4u);

    EXPECT_EQ(nals[2].type, HevcNalType::Pps);
    EXPECT_EQ(nals[2].startCodeSize, 4u);

    EXPECT_EQ(nals[3].type, HevcNalType::IdrWRadl);
    EXPECT_EQ(nals[3].startCodeSize, 4u);
}

TEST(HevcCodecTest, KeyframeDetection) {
    // IDR frame should be recognized as keyframe
    auto idrNal = makeHevcNal(HevcNalType::IdrWRadl, {0x01, 0x02});
    EXPECT_TRUE(HevcParser::isKeyframeAccessUnit(idrNal.data(), idrNal.size()));

    // CRA frame is also an IRAP keyframe
    auto craNal = makeHevcNal(HevcNalType::CraNut, {0x05, 0x06});
    EXPECT_TRUE(HevcParser::isKeyframeAccessUnit(craNal.data(), craNal.size()));

    // Non-keyframe TrailR should not be marked as keyframe
    auto trailNal = makeHevcNal(HevcNalType::TrailR, {0x07, 0x08});
    EXPECT_FALSE(HevcParser::isKeyframeAccessUnit(trailNal.data(), trailNal.size()));
}

TEST(HevcCodecTest, CodecNegotiationBothSupportHevc) {
    CodecCapabilities host;
    host.supportsH264 = true;
    host.supportsHevc = true;

    CodecCapabilities client;
    client.supportsH264 = true;
    client.supportsHevc = true;

    auto res = CodecNegotiator::negotiate(host, client);
    EXPECT_EQ(res.selectedCodec, mist::encode::VideoCodec::HEVC);
    EXPECT_EQ(res.codecName, "H265");
    EXPECT_EQ(res.rtpPayloadType, 98);
    EXPECT_TRUE(res.isHardwareAccelerated);
}

TEST(HevcCodecTest, CodecNegotiationFallbackToH264) {
    CodecCapabilities host;
    host.supportsH264 = true;
    host.supportsHevc = true;

    CodecCapabilities client;
    client.supportsH264 = true;
    client.supportsHevc = false; // Client does not support HEVC

    auto res = CodecNegotiator::negotiate(host, client);
    EXPECT_EQ(res.selectedCodec, mist::encode::VideoCodec::H264);
    EXPECT_EQ(res.codecName, "H264");
    EXPECT_EQ(res.rtpPayloadType, 96);
}

TEST(HevcCodecTest, CodecNegotiationExplicitH264Override) {
    CodecCapabilities host;
    host.supportsH264 = true;
    host.supportsHevc = true;

    CodecCapabilities client;
    client.supportsH264 = true;
    client.supportsHevc = true;

    // Force H264 via preference
    auto res = CodecNegotiator::negotiate(host, client, "h264");
    EXPECT_EQ(res.selectedCodec, mist::encode::VideoCodec::H264);
    EXPECT_EQ(res.codecName, "H264");
    EXPECT_EQ(res.rtpPayloadType, 96);
}

TEST(HevcCodecTest, CodecStringConversion) {
    EXPECT_EQ(CodecNegotiator::codecToString(mist::encode::VideoCodec::H264), "H264");
    EXPECT_EQ(CodecNegotiator::codecToString(mist::encode::VideoCodec::HEVC), "H265");
    EXPECT_EQ(CodecNegotiator::codecToString(mist::encode::VideoCodec::AV1), "AV1");

    EXPECT_EQ(CodecNegotiator::stringToCodec("hevc"), mist::encode::VideoCodec::HEVC);
    EXPECT_EQ(CodecNegotiator::stringToCodec("H265"), mist::encode::VideoCodec::HEVC);
    EXPECT_EQ(CodecNegotiator::stringToCodec("h.265"), mist::encode::VideoCodec::HEVC);
    EXPECT_EQ(CodecNegotiator::stringToCodec("h264"), mist::encode::VideoCodec::H264);
    EXPECT_EQ(CodecNegotiator::stringToCodec("av1"), mist::encode::VideoCodec::AV1);
}
