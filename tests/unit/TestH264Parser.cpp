#include <gtest/gtest.h>
#include "mist/decode/H264Parser.h"
#include <vector>

using namespace mist::decode;

TEST(H264ParserTest, ParseEmptyAndTruncated) {
    auto nalsEmpty = H264Parser::parseNalUnits(nullptr, 0);
    EXPECT_TRUE(nalsEmpty.empty());

    const uint8_t shortData[] = { 0x00, 0x00, 0x01 };
    auto nalsShort = H264Parser::parseNalUnits(shortData, sizeof(shortData));
    EXPECT_TRUE(nalsShort.empty());

    const uint8_t minNal[] = { 0x00, 0x00, 0x01, 0x09 };
    auto nalsMin = H264Parser::parseNalUnits(minNal, sizeof(minNal));
    EXPECT_EQ(nalsMin.size(), 1u);

    auto spsMeta = H264Parser::parseSps(nullptr, 0);
    EXPECT_FALSE(spsMeta.isValid);
}

TEST(H264ParserTest, ParseSyntheticNalUnits) {
    // Construct synthetic Annex-B stream:
    // [00 00 00 01] AUD(0x09)
    // [00 00 00 01] SPS(0x67 ...)
    // [00 00 00 01] PPS(0x68 ...)
    // [00 00 01] IDR(0x65 ...)
    std::vector<uint8_t> stream = {
        0x00, 0x00, 0x00, 0x01, 0x09, 0x10,
        0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x1E, 0xAB,
        0x00, 0x00, 0x00, 0x01, 0x68, 0xCE, 0x3C, 0x80,
        0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x00
    };

    auto nals = H264Parser::parseNalUnits(stream.data(), stream.size());
    ASSERT_EQ(nals.size(), 4u);

    EXPECT_EQ(nals[0].type, NalType::Aud);
    EXPECT_EQ(nals[0].startCodeSize, 4u);

    EXPECT_EQ(nals[1].type, NalType::Sps);
    EXPECT_EQ(nals[1].startCodeSize, 4u);

    EXPECT_EQ(nals[2].type, NalType::Pps);
    EXPECT_EQ(nals[2].startCodeSize, 4u);

    EXPECT_EQ(nals[3].type, NalType::IdrSlice);
    EXPECT_EQ(nals[3].startCodeSize, 3u);

    EXPECT_TRUE(H264Parser::isKeyframeAccessUnit(stream.data(), stream.size()));
}

TEST(H264ParserTest, ExtractAccessUnits) {
    std::vector<uint8_t> stream = {
        // Frame 1: AUD + IDR
        0x00, 0x00, 0x00, 0x01, 0x09, 0x10,
        0x00, 0x00, 0x01, 0x65, 0x88, 0x84,
        // Frame 2: AUD + Non-IDR
        0x00, 0x00, 0x00, 0x01, 0x09, 0x30,
        0x00, 0x00, 0x01, 0x41, 0x9A
    };

    auto aus = H264Parser::extractAccessUnits(stream.data(), stream.size(), 60);
    ASSERT_EQ(aus.size(), 2u);

    EXPECT_TRUE(aus[0].isKeyframe);
    EXPECT_EQ(aus[0].sequenceNumber, 1u);

    EXPECT_FALSE(aus[1].isKeyframe);
    EXPECT_EQ(aus[1].sequenceNumber, 2u);
}
