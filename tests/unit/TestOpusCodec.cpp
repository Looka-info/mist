#include <gtest/gtest.h>
#include "mist/audio/OpusAudioEncoder.h"
#include "mist/audio/OpusAudioDecoder.h"
#include <cmath>
#include <vector>

using namespace mist::audio;

TEST(OpusCodecTest, EncoderLifecycleAndConfiguration) {
    OpusAudioEncoder encoder;
    EXPECT_FALSE(encoder.isInitialized());

    OpusEncoderConfig config{
        .sampleRate = 48000,
        .channels = 2,
        .bitrateBps = 128000,
        .frameSizeSamples = 480,
        .application = OpusApplication::RestrictedLowDelay,
        .enableFec = true,
        .expectedLossPercent = 5,
        .complexity = 8
    };

    auto initRes = encoder.initialize(config);
    EXPECT_TRUE(initRes.hasValue());
    EXPECT_TRUE(encoder.isInitialized());

    EXPECT_TRUE(encoder.setBitrate(96000).hasValue());
    EXPECT_TRUE(encoder.setExpectedLossPercent(10).hasValue());

    // Invalid configurations
    OpusEncoderConfig badRate = config;
    badRate.sampleRate = 44100; // Not a native Opus rate
    EXPECT_TRUE(encoder.initialize(badRate).hasError());
}

TEST(OpusCodecTest, EncodeDecodeRoundTrip) {
    OpusAudioEncoder encoder;
    OpusEncoderConfig encConfig{
        .sampleRate = 48000,
        .channels = 2,
        .bitrateBps = 128000,
        .frameSizeSamples = 480,
        .application = OpusApplication::RestrictedLowDelay
    };
    ASSERT_TRUE(encoder.initialize(encConfig).hasValue());

    OpusAudioDecoder decoder;
    ASSERT_TRUE(decoder.initialize({.sampleRate = 48000, .channels = 2}).hasValue());

    // Generate 440 Hz test sine wave (480 samples, stereo = 960 floats)
    std::vector<float> inputPcm(480 * 2);
    constexpr double freq = 440.0;
    constexpr double twoPi = 6.283185307179586;
    for (size_t i = 0; i < 480; ++i) {
        double t = static_cast<double>(i) / 48000.0;
        float s = static_cast<float>(0.5 * std::sin(twoPi * freq * t));
        inputPcm[i * 2 + 0] = s;
        inputPcm[i * 2 + 1] = s;
    }

    auto encRes = encoder.encode(inputPcm);
    ASSERT_TRUE(encRes.hasValue());
    const auto& bitstream = encRes.value();
    EXPECT_GT(bitstream.size(), 20u);
    EXPECT_LT(bitstream.size(), 1000u);

    std::vector<float> outputPcm(480 * 2, 0.0f);
    auto decRes = decoder.decode(bitstream, outputPcm);
    ASSERT_TRUE(decRes.hasValue());
    EXPECT_EQ(decRes.value(), 480u);

    // Verify signal energy was reconstructed
    double inEnergy = 0.0;
    double outEnergy = 0.0;
    for (size_t i = 0; i < 480 * 2; ++i) {
        inEnergy += inputPcm[i] * inputPcm[i];
        outEnergy += outputPcm[i] * outputPcm[i];
    }
    EXPECT_GT(outEnergy, 0.5 * inEnergy);
}

TEST(OpusCodecTest, PacketLossConcealment) {
    OpusAudioDecoder decoder;
    ASSERT_TRUE(decoder.initialize({.sampleRate = 48000, .channels = 2}).hasValue());

    // Invoke PLC on lost frame
    std::vector<float> plcOut(480 * 2, 0.0f);
    auto plcRes = decoder.decodeLostFrame(plcOut, 480);
    ASSERT_TRUE(plcRes.hasValue());
    EXPECT_EQ(plcRes.value(), 480u);
}
