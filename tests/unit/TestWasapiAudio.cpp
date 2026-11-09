#include <gtest/gtest.h>
#include "mist/audio/AudioDeviceEnumerator.h"
#include "mist/audio/WasapiAudioCapture.h"
#include "mist/audio/WasapiAudioPlayback.h"

using namespace mist::audio;

TEST(WasapiAudioTest, DeviceEnumeration) {
    auto res = AudioDeviceEnumerator::enumerateRenderDevices();
    if (!res.hasValue() || res.value().empty()) {
        GTEST_SKIP() << "No active WASAPI audio render endpoints detected in this environment";
    }
    const auto& devices = res.value();
    EXPECT_GE(devices.size(), 1u);
    bool foundDefault = false;
    for (const auto& dev : devices) {
        EXPECT_FALSE(dev.friendlyName.empty());
        EXPECT_GT(dev.sampleRate, 0u);
        EXPECT_GT(dev.channels, 0u);
        if (dev.isDefault) {
            foundDefault = true;
        }
    }
    EXPECT_TRUE(foundDefault);
}

TEST(WasapiAudioTest, GetDefaultRenderDevice) {
    auto res = AudioDeviceEnumerator::getDefaultRenderDevice();
    if (!res.hasValue()) {
        GTEST_SKIP() << "No default WASAPI audio render endpoint detected in this environment";
    }
    const auto& dev = res.value();
    EXPECT_TRUE(dev.isDefault);
    EXPECT_FALSE(dev.friendlyName.empty());
}

TEST(WasapiAudioTest, CaptureLifecycle) {
    auto devRes = AudioDeviceEnumerator::getDefaultRenderDevice();
    if (!devRes.hasValue()) {
        GTEST_SKIP() << "No default audio endpoint for capture lifecycle test";
    }

    WasapiAudioCapture capture;
    AudioConfig config{
        .sampleRate = 48000,
        .channels = 2,
        .format = AudioSampleFormat::Float32,
        .bufferFrameSize = 480
    };

    auto initRes = capture.initialize(config);
    if (!initRes.hasValue()) {
        GTEST_SKIP() << "WASAPI audio capture initialization not supported: " << initRes.error().format();
    }

    auto startRes = capture.start();
    ASSERT_TRUE(startRes.hasValue());
    EXPECT_TRUE(capture.isCapturing());

    auto pktRes = capture.readPacket(50);
    ASSERT_TRUE(pktRes.hasValue());
    EXPECT_EQ(pktRes.value().frames, 480u);

    auto stopRes = capture.stop();
    ASSERT_TRUE(stopRes.hasValue());
    EXPECT_FALSE(capture.isCapturing());
}

TEST(WasapiAudioTest, PlaybackLifecycleAndStats) {
    auto devRes = AudioDeviceEnumerator::getDefaultRenderDevice();
    if (!devRes.hasValue()) {
        GTEST_SKIP() << "No default audio endpoint for playback lifecycle test";
    }

    WasapiAudioPlayback playback;
    AudioConfig config{
        .sampleRate = 48000,
        .channels = 2,
        .format = AudioSampleFormat::Float32,
        .bufferFrameSize = 480
    };

    auto initRes = playback.initialize(config);
    if (!initRes.hasValue()) {
        GTEST_SKIP() << "WASAPI audio playback initialization not supported: " << initRes.error().format();
    }

    auto startRes = playback.start();
    ASSERT_TRUE(startRes.hasValue());
    EXPECT_TRUE(playback.isPlaying());

    // Write a silent packet
    AudioPacket pkt;
    pkt.frames = 480;
    pkt.data.resize(480 * 2 * sizeof(float), 0);
    auto writeRes = playback.writePacket(pkt);
    EXPECT_TRUE(writeRes.hasValue());

    auto stopRes = playback.stop();
    ASSERT_TRUE(stopRes.hasValue());
    EXPECT_FALSE(playback.isPlaying());

    auto stats = playback.getStats();
    EXPECT_GE(stats.framesRendered, 0u);
}
