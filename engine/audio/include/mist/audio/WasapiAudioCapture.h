#pragma once

#include "mist/audio/IAudioEngine.h"
#include "mist/audio/AudioResampler.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <wrl/client.h>
#include <atomic>
#include <memory>
#include <string>

namespace mist::audio {

class WasapiAudioCapture : public IAudioCapture {
public:
    WasapiAudioCapture();
    ~WasapiAudioCapture() override;

    [[nodiscard]] common::Result<void> initialize(const AudioConfig& config) override;
    [[nodiscard]] common::Result<void> start() override;
    [[nodiscard]] common::Result<void> stop() override;
    [[nodiscard]] common::Result<AudioPacket> readPacket(uint32_t timeoutMs) override;

    [[nodiscard]] const AudioConfig& getConfig() const noexcept { return config_; }
    [[nodiscard]] const AudioFormat& getDeviceFormat() const noexcept { return deviceFormat_; }
    [[nodiscard]] bool isCapturing() const noexcept { return isCapturing_; }

private:
    common::Result<void> initEndpoint();
    void releaseEndpoint();

    AudioConfig config_{};
    AudioFormat deviceFormat_{};
    AudioResampler resampler_;

    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator_;
    Microsoft::WRL::ComPtr<IMMDevice> device_;
    Microsoft::WRL::ComPtr<IAudioClient> audioClient_;
    Microsoft::WRL::ComPtr<IAudioCaptureClient> captureClient_;

    HANDLE hCaptureEvent_{nullptr};
    std::atomic<bool> isInitialized_{false};
    std::atomic<bool> isCapturing_{false};
    std::atomic<bool> deviceInvalidated_{false};

    std::vector<float> tempFrameBuffer_;
    std::vector<uint8_t> silentBuffer_;
};

} // namespace mist::audio
