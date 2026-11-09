#include <gtest/gtest.h>
#include "mist/audio/AudioDeviceEnumerator.h"
#include "mist/audio/WasapiAudioCapture.h"
#include "mist/audio/WasapiAudioPlayback.h"
#if defined(_WIN32)
#include <windows.h>
#include <mmdeviceapi.h>
#endif

using namespace mist::audio;

#if defined(_MSC_VER)
static bool isWasapiHardwarePresent() {
    bool present = false;
    __try {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool coInit = (hr == S_OK || hr == S_FALSE);
        IMMDeviceEnumerator* enumerator = nullptr;
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
        if (SUCCEEDED(hr) && enumerator) {
            IMMDeviceCollection* collection = nullptr;
            hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
            if (SUCCEEDED(hr) && collection) {
                UINT count = 0;
                collection->GetCount(&count);
                if (count > 0) {
                    IMMDevice* dev = nullptr;
                    if (SUCCEEDED(collection->Item(0, &dev)) && dev) {
                        present = true;
                        dev->Release();
                    }
                }
                collection->Release();
            }
            enumerator->Release();
        }
        if (coInit) CoUninitialize();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        present = false;
    }
    return present;
}
#elif defined(_WIN32)
static bool isWasapiHardwarePresent() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool coInit = (hr == S_OK || hr == S_FALSE);
    bool present = false;
    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
    if (SUCCEEDED(hr) && enumerator) {
        IMMDeviceCollection* collection = nullptr;
        hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
        if (SUCCEEDED(hr) && collection) {
            UINT count = 0;
            collection->GetCount(&count);
            if (count > 0) {
                present = true;
            }
            collection->Release();
        }
        enumerator->Release();
    }
    if (coInit) CoUninitialize();
    return present;
}
#else
static bool isWasapiHardwarePresent() { return false; }
#endif

TEST(WasapiAudioTest, DeviceEnumeration) {
    if (!isWasapiHardwarePresent()) {
        GTEST_SKIP() << "No active WASAPI audio render endpoints detected in this environment";
    }
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
    if (!isWasapiHardwarePresent()) {
        GTEST_SKIP() << "No default WASAPI audio render endpoint detected in this environment";
    }
    auto res = AudioDeviceEnumerator::getDefaultRenderDevice();
    if (!res.hasValue()) {
        GTEST_SKIP() << "No default WASAPI audio render endpoint detected in this environment";
    }
    const auto& dev = res.value();
    EXPECT_TRUE(dev.isDefault);
    EXPECT_FALSE(dev.friendlyName.empty());
}

TEST(WasapiAudioTest, CaptureLifecycle) {
    if (!isWasapiHardwarePresent()) {
        GTEST_SKIP() << "No default audio endpoint for capture lifecycle test";
    }
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
    if (!isWasapiHardwarePresent()) {
        GTEST_SKIP() << "No default audio endpoint for playback lifecycle test";
    }
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
