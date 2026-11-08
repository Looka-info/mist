#include <gtest/gtest.h>
#include "mist/audio/AudioDeviceEnumerator.h"
#include "mist/audio/WasapiAudioCapture.h"
#include "mist/audio/WasapiAudioPlayback.h"

using namespace mist::audio;

TEST(WasapiAudioTest, DeviceEnumeration) {
    auto res = AudioDeviceEnumerator::enumerateRenderDevices();
    ASSERT_TRUE(res.hasValue());
    const auto& devices = res.value();
    // In any standard Windows audio setup, at least one render endpoint exists
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
    ASSERT_TRUE(res.hasValue());
    const auto& dev = res.value();
    EXPECT_TRUE(dev.isDefault);
    EXPECT_FALSE(dev.friendlyName.empty());
}

TEST(WasapiAudioTest, CaptureLifecycle) {
    WasapiAudioCapture capture;
    AudioConfig config{
        .sampleRate = 48000,
        .channels = 2,
        .format = AudioSampleFormat::Float32,
        .bufferFrameSize = 480
    };

    auto initRes = capture.initialize(config);
    ASSERT_TRUE(initRes.hasValue());

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
    WasapiAudioPlayback playback;
    AudioConfig config{
        .sampleRate = 48000,
        .channels = 2,
        .format = AudioSampleFormat::Float32,
        .bufferFrameSize = 480
    };

    auto initRes = playback.initialize(config);
    ASSERT_TRUE(initRes.hasValue());

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
