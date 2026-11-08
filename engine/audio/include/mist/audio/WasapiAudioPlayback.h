#pragma once

#include "mist/audio/IAudioEngine.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <wrl/client.h>
#include <atomic>
#include <memory>
#include <thread>
#include <mutex>

namespace mist::audio {

struct AudioPlaybackStats {
    uint64_t framesRendered{0};
    uint64_t underrunCount{0};
    uint64_t overflowCount{0};
    uint32_t currentBufferFrames{0};
};

class WasapiAudioPlayback : public IAudioPlayback {
public:
    WasapiAudioPlayback();
    ~WasapiAudioPlayback() override;

    [[nodiscard]] common::Result<void> initialize(const AudioConfig& config) override;
    [[nodiscard]] common::Result<void> start() override;
    [[nodiscard]] common::Result<void> stop() override;
    [[nodiscard]] common::Result<void> writePacket(const AudioPacket& packet) override;

    [[nodiscard]] AudioPlaybackStats getStats() const noexcept;
    [[nodiscard]] bool isPlaying() const noexcept { return isPlaying_; }

private:
    common::Result<void> initEndpoint();
    void releaseEndpoint();
    void renderLoop();

    AudioConfig config_{};
    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator_;
    Microsoft::WRL::ComPtr<IMMDevice> device_;
    Microsoft::WRL::ComPtr<IAudioClient> audioClient_;
    Microsoft::WRL::ComPtr<IAudioRenderClient> renderClient_;

    HANDLE hRenderEvent_{nullptr};
    std::thread renderThread_;
    std::atomic<bool> isRunning_{false};
    std::atomic<bool> isPlaying_{false};
    std::atomic<bool> isInitialized_{false};

    // Jitter ring buffer
    mutable std::mutex ringMutex_;
    std::vector<float> ringBuffer_;
    size_t ringReadPos_{0};
    size_t ringWritePos_{0};
    size_t ringCount_{0};

    // Playout metrics
    std::atomic<uint64_t> framesRendered_{0};
    std::atomic<uint64_t> underrunCount_{0};
    std::atomic<uint64_t> overflowCount_{0};

    UINT32 endpointBufferFrames_{0};
    bool endpointIsFloat_{true};
    uint32_t endpointChannels_{2};
    uint32_t endpointSampleRate_{48000};
};

} // namespace mist::audio
