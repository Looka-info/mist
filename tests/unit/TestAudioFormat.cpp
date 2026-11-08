#include <gtest/gtest.h>
#include "mist/audio/AudioResampler.h"
#include <cmath>
#include <vector>

using namespace mist::audio;

TEST(AudioFormatTest, ConfigureValidFormats) {
    AudioResampler resampler;
    AudioFormat fmt48k{
        .sampleRate = 48000,
        .channels = 2,
        .sampleFormat = AudioSampleFormat::Float32,
        .bitsPerSample = 32
    };
    auto res = resampler.configure(fmt48k);
    EXPECT_TRUE(res.hasValue());

    AudioFormat fmt44k{
        .sampleRate = 44100,
        .channels = 2,
        .sampleFormat = AudioSampleFormat::Int16,
        .bitsPerSample = 16
    };
    EXPECT_TRUE(resampler.configure(fmt44k).hasValue());

    AudioFormat fmtInvalid{
        .sampleRate = 0,
        .channels = 0,
        .sampleFormat = AudioSampleFormat::Float32,
        .bitsPerSample = 32
    };
    EXPECT_TRUE(resampler.configure(fmtInvalid).hasError());
}

TEST(AudioFormatTest, DownmixMonoToStereo) {
    AudioResampler resampler;
    AudioFormat monoFmt{
        .sampleRate = 48000,
        .channels = 1,
        .sampleFormat = AudioSampleFormat::Float32,
        .bitsPerSample = 32
    };
    ASSERT_TRUE(resampler.configure(monoFmt).hasValue());

    std::vector<float> monoData(480, 0.75f);
    ASSERT_TRUE(resampler.pushInput(monoData.data(), monoData.size()).hasValue());

    EXPECT_TRUE(resampler.hasFrame(480));
    std::vector<float> stereoOut(480 * 2);
    EXPECT_TRUE(resampler.popFrame(stereoOut, 480));

    for (size_t i = 0; i < 480; ++i) {
        EXPECT_NEAR(stereoOut[i * 2 + 0], 0.75f, 1e-4f);
        EXPECT_NEAR(stereoOut[i * 2 + 1], 0.75f, 1e-4f);
    }
}

TEST(AudioFormatTest, Downmix5Point1SurroundToStereo) {
    AudioResampler resampler;
    AudioFormat surroundFmt{
        .sampleRate = 48000,
        .channels = 6, // FL, FR, FC, LFE, BL, BR
        .sampleFormat = AudioSampleFormat::Float32,
        .bitsPerSample = 32
    };
    ASSERT_TRUE(resampler.configure(surroundFmt).hasValue());

    // 480 frames of 6 channels
    std::vector<float> input(480 * 6, 0.0f);
    for (size_t i = 0; i < 480; ++i) {
        input[i * 6 + 0] = 1.0f; // Left only
        input[i * 6 + 1] = 0.0f; // Right
    }

    ASSERT_TRUE(resampler.pushInput(input.data(), 480).hasValue());

    std::vector<float> stereoOut(480 * 2);
    EXPECT_TRUE(resampler.popFrame(stereoOut, 480));

    // Left should have signal, Right should be near zero
    EXPECT_GT(stereoOut[0], 0.3f);
    EXPECT_NEAR(stereoOut[1], 0.0f, 1e-4f);
}

TEST(AudioFormatTest, Resample44100To48000) {
    AudioResampler resampler;
    AudioFormat fmt44k{
        .sampleRate = 44100,
        .channels = 2,
        .sampleFormat = AudioSampleFormat::Float32,
        .bitsPerSample = 32
    };
    ASSERT_TRUE(resampler.configure(fmt44k, 48000).hasValue());

    // Push 44100 samples (1 second at 44.1kHz)
    std::vector<float> inData(44100 * 2, 0.5f);
    ASSERT_TRUE(resampler.pushInput(inData.data(), 44100).hasValue());

    // Resampled output should be approximately 48000 frames
    size_t available = resampler.availableFrames();
    EXPECT_GE(available, 47990u);
    EXPECT_LE(available, 48010u);

    std::vector<float> frame(480 * 2);
    EXPECT_TRUE(resampler.popFrame(frame, 480));
    EXPECT_NEAR(frame[0], 0.5f, 0.05f);
}

TEST(AudioFormatTest, PopFrameInt16) {
    AudioResampler resampler;
    AudioFormat fmt{
        .sampleRate = 48000,
        .channels = 2,
        .sampleFormat = AudioSampleFormat::Float32,
        .bitsPerSample = 32
    };
    ASSERT_TRUE(resampler.configure(fmt).hasValue());

    std::vector<float> inData(480 * 2, 0.5f);
    ASSERT_TRUE(resampler.pushInput(inData.data(), 480).hasValue());

    std::vector<int16_t> outInt16(480 * 2);
    EXPECT_TRUE(resampler.popFrame(outInt16, 480));

    // 0.5f * 32767 = ~16383
    EXPECT_NEAR(outInt16[0], 16383, 10);
    EXPECT_NEAR(outInt16[1], 16383, 10);
}
