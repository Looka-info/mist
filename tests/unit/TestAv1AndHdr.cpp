#include <gtest/gtest.h>
#include "mist/decode/Av1Parser.h"
#include "mist/encode/SoftwareVideoEncoder.h"
#include "mist/capture/HdrToneMapper.h"
#include "mist/transport/CodecNegotiator.h"
#include <cmath>
#include <vector>

using namespace mist::decode;
using namespace mist::encode;
using namespace mist::capture;
using namespace mist::transport;

namespace {

std::vector<uint8_t> makeAv1Obu(Av1ObuType type, const std::vector<uint8_t>& payload, bool hasSize = true) {
    std::vector<uint8_t> obu;
    uint8_t header = (static_cast<uint8_t>(type) & 0x0F) << 3;
    if (hasSize) {
        header |= 0x02; // obu_has_size_field
    }
    obu.push_back(header);

    if (hasSize) {
        uint8_t lebBuf[8];
        size_t lebLen = Av1Parser::writeLeb128(payload.size(), lebBuf);
        obu.insert(obu.end(), lebBuf, lebBuf + lebLen);
    }

    obu.insert(obu.end(), payload.begin(), payload.end());
    return obu;
}

} // namespace

TEST(Av1ParserTest, Leb128ReadWrite) {
    uint8_t buf[16];
    std::vector<uint64_t> testValues = {0, 1, 127, 128, 255, 16384, 1000000};

    for (uint64_t val : testValues) {
        size_t written = Av1Parser::writeLeb128(val, buf);
        EXPECT_GT(written, 0u);

        size_t offset = 0;
        uint64_t readVal = 0;
        size_t bytesRead = 0;
        bool ok = Av1Parser::readLeb128(buf, written, offset, readVal, bytesRead);
        EXPECT_TRUE(ok);
        EXPECT_EQ(readVal, val);
        EXPECT_EQ(bytesRead, written);
    }
}

TEST(Av1ParserTest, ParseObuUnits) {
    std::vector<uint8_t> bitstream;

    // 1. Temporal Delimiter OBU
    auto td = makeAv1Obu(Av1ObuType::TemporalDelimiter, {});
    // 2. Sequence Header OBU
    auto seq = makeAv1Obu(Av1ObuType::SequenceHeader, {0x01, 0x1F, 0x00, 0x00});
    // 3. Key Frame OBU
    auto frame = makeAv1Obu(Av1ObuType::Frame, {0x00, 0x88, 0x11, 0x22});

    bitstream.insert(bitstream.end(), td.begin(), td.end());
    bitstream.insert(bitstream.end(), seq.begin(), seq.end());
    bitstream.insert(bitstream.end(), frame.begin(), frame.end());

    auto obus = Av1Parser::parseObus(bitstream.data(), bitstream.size());
    ASSERT_EQ(obus.size(), 3u);

    EXPECT_EQ(obus[0].type, Av1ObuType::TemporalDelimiter);
    EXPECT_EQ(obus[0].payloadSize, 0u);

    EXPECT_EQ(obus[1].type, Av1ObuType::SequenceHeader);
    EXPECT_EQ(obus[1].payloadSize, 4u);

    EXPECT_EQ(obus[2].type, Av1ObuType::Frame);
    EXPECT_EQ(obus[2].payloadSize, 4u);
}

TEST(Av1ParserTest, KeyframeDetectionAndMetadata) {
    // Access unit with Sequence Header + Key Frame
    std::vector<uint8_t> keyframeAu;
    auto seq = makeAv1Obu(Av1ObuType::SequenceHeader, {0x01, 0x1F, 0x00, 0x00});
    auto keyFrame = makeAv1Obu(Av1ObuType::Frame, {0x00, 0x88, 0x11});
    keyframeAu.insert(keyframeAu.end(), seq.begin(), seq.end());
    keyframeAu.insert(keyframeAu.end(), keyFrame.begin(), keyFrame.end());

    EXPECT_TRUE(Av1Parser::isKeyframeAccessUnit(keyframeAu.data(), keyframeAu.size()));

    auto meta = Av1Parser::parseSequenceHeader(keyframeAu.data(), keyframeAu.size());
    EXPECT_TRUE(meta.isValid);
    EXPECT_TRUE(meta.is10Bit);
    EXPECT_EQ(meta.bitDepth, 10u);

    // Access unit with only Inter Frame
    std::vector<uint8_t> interAu;
    auto interFrame = makeAv1Obu(Av1ObuType::Frame, {0x20, 0x10, 0x55});
    interAu.insert(interAu.end(), interFrame.begin(), interFrame.end());

    EXPECT_FALSE(Av1Parser::isKeyframeAccessUnit(interAu.data(), interAu.size()));
}

TEST(SoftwareEncoderTest, Av1EncodingAndKeyframes) {
    auto encoderRes = SoftwareVideoEncoder::create();
    ASSERT_TRUE(encoderRes.hasValue());
    auto encoder = std::move(encoderRes.value());

    EncodeConfig config{};
    config.codec = VideoCodec::AV1;
    config.width = 1920;
    config.height = 1080;
    config.targetFps = 60;
    config.bitrateKbps = 15000;
    config.gopLength = 60;

    auto initRes = encoder->initialize(config);
    ASSERT_TRUE(initRes.hasValue());

    CapturedFrame frame{};
    frame.width = 1920;
    frame.height = 1080;
    frame.format = PixelFormat::P010;
    frame.bitDepth = 10;
    frame.colorSpace = ColorSpace::BT2020_PQ;
    frame.timestampNs = 1000000;

    // Encode first frame -> must be keyframe
    auto encRes = encoder->encode(frame);
    ASSERT_TRUE(encRes.hasValue());

    auto pktRes = encoder->getNextPacket();
    ASSERT_TRUE(pktRes.hasValue());
    auto pkt = pktRes.value();
    EXPECT_TRUE(pkt.isKeyframe);
    EXPECT_GT(pkt.data.size(), 0u);
    EXPECT_TRUE(Av1Parser::isKeyframeAccessUnit(pkt.data.data(), pkt.data.size()));

    // Encode second frame -> non-keyframe
    frame.timestampNs = 2000000;
    encRes = encoder->encode(frame);
    ASSERT_TRUE(encRes.hasValue());
    pktRes = encoder->getNextPacket();
    ASSERT_TRUE(pktRes.hasValue());
    EXPECT_FALSE(pktRes.value().isKeyframe);

    // Request keyframe and verify
    EXPECT_TRUE(encoder->requestKeyframe().hasValue());
    frame.timestampNs = 3000000;
    encRes = encoder->encode(frame);
    ASSERT_TRUE(encRes.hasValue());
    pktRes = encoder->getNextPacket();
    ASSERT_TRUE(pktRes.hasValue());
    EXPECT_TRUE(pktRes.value().isKeyframe);
    EXPECT_TRUE(Av1Parser::isKeyframeAccessUnit(pktRes.value().data.data(), pktRes.value().data.size()));
}

TEST(HdrToneMapperTest, P010PackingAndUnpacking) {
    // P010 MSB-alignment test: upper 10 bits of 16-bit word contain data, lower 6 bits 0
    uint16_t sample0 = 0;
    uint16_t packed0 = HdrToneMapper::packP010Sample(sample0);
    EXPECT_EQ(packed0, 0u);
    EXPECT_EQ(HdrToneMapper::unpackP010Sample(packed0), 0u);

    uint16_t sampleMid = 512;
    uint16_t packedMid = HdrToneMapper::packP010Sample(sampleMid);
    EXPECT_EQ(packedMid, 512 << 6);
    EXPECT_EQ(HdrToneMapper::unpackP010Sample(packedMid), 512u);

    uint16_t sampleMax = 1023;
    uint16_t packedMax = HdrToneMapper::packP010Sample(sampleMax);
    EXPECT_EQ(packedMax, 1023 << 6);
    EXPECT_EQ(HdrToneMapper::unpackP010Sample(packedMax), 1023u);
}

TEST(HdrToneMapperTest, SmpteSt2084PqEotfRoundTrip) {
    // 0 normalized PQ code must equal 0 nits
    EXPECT_NEAR(HdrToneMapper::pqToLinearNits(0.0f), 0.0f, 1e-4f);
    EXPECT_NEAR(HdrToneMapper::linearNitsToPq(0.0f), 0.0f, 1e-4f);

    // 1.0 normalized PQ code must equal 10,000 cd/m^2 nits
    float maxNits = HdrToneMapper::pqToLinearNits(1.0f);
    EXPECT_NEAR(maxNits, 10000.0f, 1.0f);
    EXPECT_NEAR(HdrToneMapper::linearNitsToPq(10000.0f), 1.0f, 1e-3f);

    // Round-trip test across multiple PQ code points
    std::vector<float> codePoints = {0.1f, 0.3f, 0.5f, 0.7f, 0.9f};
    for (float pq : codePoints) {
        float nits = HdrToneMapper::pqToLinearNits(pq);
        float roundTripPq = HdrToneMapper::linearNitsToPq(nits);
        EXPECT_NEAR(roundTripPq, pq, 1e-3f);
    }
}

TEST(HdrToneMapperTest, ColorSpaceAndToneMapping) {
    ToneMappingConfig config{};
    config.op = ToneMappingOperator::AcesFilmic;
    config.targetSdrWhiteLuminanceNits = 100.0f;

    // Test tone mapping bright HDR highlight (e.g. 1000 nits)
    float rHdr = HdrToneMapper::linearNitsToPq(1000.0f);
    float gHdr = HdrToneMapper::linearNitsToPq(1000.0f);
    float bHdr = HdrToneMapper::linearNitsToPq(1000.0f);

    float sdrR = 0.0f, sdrG = 0.0f, sdrB = 0.0f;
    HdrToneMapper::toneMapHdrRgbToSdrRgb(rHdr, gHdr, bHdr, config, sdrR, sdrG, sdrB);

    // Output SDR values must be strictly clamped and within displayable [0..1] range
    EXPECT_GE(sdrR, 0.0f);
    EXPECT_LE(sdrR, 1.0f);
    EXPECT_GE(sdrG, 0.0f);
    EXPECT_LE(sdrG, 1.0f);
    EXPECT_GE(sdrB, 0.0f);
    EXPECT_LE(sdrB, 1.0f);

    // Test Reinhard tone mapper
    config.op = ToneMappingOperator::Reinhard;
    float mappedReinhard = HdrToneMapper::toneMapScalar(10.0f, config);
    EXPECT_NEAR(mappedReinhard, 10.0f / 11.0f, 1e-3f);

    // Test BT.2020 to BT.709 conversion on white
    float r709, g709, b709;
    HdrToneMapper::transformBt2020ToBt709(1.0f, 1.0f, 1.0f, r709, g709, b709);
    EXPECT_NEAR(r709, 1.0f, 1e-3f);
    EXPECT_NEAR(g709, 1.0f, 1e-3f);
    EXPECT_NEAR(b709, 1.0f, 1e-3f);
}

TEST(HdrToneMapperTest, ConvertR10G10B10A2AndToneMapP010ToNv12) {
    constexpr uint32_t width = 4;
    constexpr uint32_t height = 4;

    // Create synthetic R10G10B10A2 frame (all mid-grey: 512 in 10-bit)
    uint32_t midPixel = 512 | (512 << 10) | (512 << 20) | (3 << 30);
    std::vector<uint32_t> srcR10(width * height, midPixel);

    std::vector<uint16_t> dstY(width * height, 0);
    std::vector<uint16_t> dstUv(width * (height / 2), 0);

    auto res = HdrToneMapper::convertR10G10B10A2ToP010(srcR10.data(), dstY.data(), dstUv.data(), width, height);
    ASSERT_TRUE(res.hasValue());

    // Verify P010 packing: samples should be near 512 unpacked
    uint16_t unpackedY = HdrToneMapper::unpackP010Sample(dstY[0]);
    EXPECT_NEAR(unpackedY, 512, 5);

    // Now tone-map P010 buffer to 8-bit NV12
    std::vector<uint8_t> sdrY(width * height, 0);
    std::vector<uint8_t> sdrUv(width * (height / 2), 0);

    ToneMappingConfig config{};
    config.op = ToneMappingOperator::AcesFilmic;
    auto tmRes = HdrToneMapper::toneMapP010ToNv12(dstY.data(), dstUv.data(), sdrY.data(), sdrUv.data(), width, height, config);
    ASSERT_TRUE(tmRes.hasValue());

    // Verify SDR outputs are valid
    EXPECT_GT(sdrY[0], 0);
    EXPECT_LT(sdrY[0], 255);
}

TEST(CodecNegotiationTest, Av1AndHdrCapabilities) {
    CodecCapabilities hostCaps{};
    hostCaps.supportsH264 = true;
    hostCaps.supportsHevc = true;
    hostCaps.supportsAv1 = true;
    hostCaps.supports10Bit = true;
    hostCaps.supportsHdr = true;

    CodecCapabilities clientCaps{};
    clientCaps.supportsH264 = true;
    clientCaps.supportsHevc = true;
    clientCaps.supportsAv1 = true;
    clientCaps.supports10Bit = true;
    clientCaps.supportsHdr = true;

    // Negotiation should choose AV1 as default top priority
    auto result = CodecNegotiator::negotiate(hostCaps, clientCaps);
    EXPECT_EQ(result.selectedCodec, VideoCodec::AV1);
    EXPECT_EQ(result.codecName, "AV1");
    EXPECT_EQ(result.rtpPayloadType, 97);
    EXPECT_TRUE(result.is10Bit);
    EXPECT_TRUE(result.isHdr);

    // If client does not support HDR, negotiated result should indicate HDR disabled
    clientCaps.supportsHdr = false;
    result = CodecNegotiator::negotiate(hostCaps, clientCaps);
    EXPECT_EQ(result.selectedCodec, VideoCodec::AV1);
    EXPECT_FALSE(result.isHdr);
}
