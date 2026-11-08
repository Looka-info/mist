#include "mist/audio/WasapiAudioPlayback.h"
#include <algorithm>
#include <cstring>
#include <avrt.h>

using Microsoft::WRL::ComPtr;

namespace mist::audio {

namespace {

static constexpr GUID GUID_IEEE_FLOAT = {
    0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }
};

} // namespace

WasapiAudioPlayback::WasapiAudioPlayback() {
    ringBuffer_.resize(48000 * 2); // 1-second capacity
}

WasapiAudioPlayback::~WasapiAudioPlayback() {
    (void)stop();
    releaseEndpoint();
}

common::Result<void> WasapiAudioPlayback::initialize(const AudioConfig& config) {
    if (isPlaying_) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::AlreadyExists, "Cannot reinitialize while playing");
    }

    releaseEndpoint();
    config_ = config;

    auto res = initEndpoint();
    if (res.hasError()) {
        return res;
    }

    isInitialized_ = true;
    return common::Result<void>::success();
}

common::Result<void> WasapiAudioPlayback::initEndpoint() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    (void)hr;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), &enumerator_);
    if (FAILED(hr) || !enumerator_) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Failed to create MMDeviceEnumerator");
    }

    hr = enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &device_);
    if (FAILED(hr) || !device_) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::HardwareUnavailable, "No active audio render endpoint found");
    }

    hr = device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &audioClient_);
    if (FAILED(hr) || !audioClient_) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Failed to activate IAudioClient");
    }

    WAVEFORMATEX* mixFormat = nullptr;
    hr = audioClient_->GetMixFormat(&mixFormat);
    if (FAILED(hr) || !mixFormat) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Failed to get mix format");
    }

    endpointChannels_ = mixFormat->nChannels;
    endpointSampleRate_ = mixFormat->nSamplesPerSec;

    if (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        auto* ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mixFormat);
        endpointIsFloat_ = (ext->SubFormat == GUID_IEEE_FLOAT);
    } else {
        endpointIsFloat_ = (mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
    }

    REFERENCE_TIME bufferDuration = 200000; // 20 ms in 100ns units
    hr = audioClient_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        bufferDuration,
        0,
        mixFormat,
        nullptr
    );

    CoTaskMemFree(mixFormat);

    if (FAILED(hr)) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Failed to initialize IAudioClient for playback: hr=" + std::to_string(hr));
    }

    hr = audioClient_->GetBufferSize(&endpointBufferFrames_);
    if (FAILED(hr)) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Failed to get audio buffer size");
    }

    hRenderEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!hRenderEvent_) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Failed to create render event handle");
    }

    hr = audioClient_->SetEventHandle(hRenderEvent_);
    if (FAILED(hr)) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Failed to set audio client event handle");
    }

    hr = audioClient_->GetService(__uuidof(IAudioRenderClient), &renderClient_);
    if (FAILED(hr) || !renderClient_) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Failed to get IAudioRenderClient");
    }

    return common::Result<void>::success();
}

void WasapiAudioPlayback::releaseEndpoint() {
    if (renderClient_) renderClient_.Reset();
    if (audioClient_) audioClient_.Reset();
    if (device_) device_.Reset();
    if (enumerator_) enumerator_.Reset();

    if (hRenderEvent_) {
        CloseHandle(hRenderEvent_);
        hRenderEvent_ = nullptr;
    }
}

common::Result<void> WasapiAudioPlayback::start() {
    if (!isInitialized_) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::NotInitialized, "WasapiAudioPlayback not initialized");
    }
    if (isPlaying_) {
        return common::Result<void>::success();
    }

    // Pre-fill buffer with silence before starting playback to avoid immediate underrun
    BYTE* pData = nullptr;
    if (SUCCEEDED(renderClient_->GetBuffer(endpointBufferFrames_, &pData)) && pData) {
        size_t bytes = endpointBufferFrames_ * endpointChannels_ * (endpointIsFloat_ ? sizeof(float) : sizeof(int16_t));
        std::memset(pData, 0, bytes);
        renderClient_->ReleaseBuffer(endpointBufferFrames_, 0);
    }

    HRESULT hr = audioClient_->Start();
    if (FAILED(hr)) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Failed to start IAudioClient");
    }

    isRunning_ = true;
    isPlaying_ = true;
    renderThread_ = std::thread(&WasapiAudioPlayback::renderLoop, this);

    return common::Result<void>::success();
}

common::Result<void> WasapiAudioPlayback::stop() {
    if (!isPlaying_) {
        return common::Result<void>::success();
    }

    isRunning_ = false;
    if (hRenderEvent_) {
        SetEvent(hRenderEvent_);
    }

    if (renderThread_.joinable()) {
        renderThread_.join();
    }

    if (audioClient_) {
        audioClient_->Stop();
    }
    isPlaying_ = false;

    return common::Result<void>::success();
}

common::Result<void> WasapiAudioPlayback::writePacket(const AudioPacket& packet) {
    if (packet.data.empty()) {
        return common::Result<void>::success();
    }

    const auto* src = reinterpret_cast<const float*>(packet.data.data());
    size_t samples = packet.data.size() / sizeof(float);

    std::lock_guard<std::mutex> lock(ringMutex_);

    // Bounded jitter buffer: max 4800 samples (100 ms at 48kHz stereo = 4800 floats)
    constexpr size_t maxBufferSamples = 48000 * 2 / 10; // 100 ms
    if (ringCount_ + samples > maxBufferSamples) {
        // Discard oldest to control latency bloat
        size_t discard = (ringCount_ + samples) - maxBufferSamples;
        ringReadPos_ = (ringReadPos_ + discard) % ringBuffer_.size();
        ringCount_ -= discard;
        overflowCount_.fetch_add(1, std::memory_order_relaxed);
    }

    for (size_t i = 0; i < samples; ++i) {
        ringBuffer_[ringWritePos_] = src[i];
        ringWritePos_ = (ringWritePos_ + 1) % ringBuffer_.size();
    }
    ringCount_ += samples;

    return common::Result<void>::success();
}

void WasapiAudioPlayback::renderLoop() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    DWORD taskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
    (void)hTask;

    while (isRunning_) {
        DWORD waitRes = WaitForSingleObject(hRenderEvent_, 20);
        if (!isRunning_) break;

        if (waitRes == WAIT_OBJECT_0 || waitRes == WAIT_TIMEOUT) {
            UINT32 paddingFrames = 0;
            HRESULT hr = audioClient_->GetCurrentPadding(&paddingFrames);
            if (FAILED(hr)) continue;

            if (endpointBufferFrames_ <= paddingFrames) continue;
            UINT32 framesToFill = endpointBufferFrames_ - paddingFrames;

            BYTE* pData = nullptr;
            hr = renderClient_->GetBuffer(framesToFill, &pData);
            if (FAILED(hr) || !pData) continue;

            auto* outFloat = reinterpret_cast<float*>(pData);
            auto* outInt16 = reinterpret_cast<int16_t*>(pData);

            size_t framesRead = 0;
            {
                std::lock_guard<std::mutex> lock(ringMutex_);
                size_t availableFrames = ringCount_ / 2;
                framesRead = std::min<size_t>(availableFrames, framesToFill);

                for (size_t i = 0; i < framesRead; ++i) {
                    float l = ringBuffer_[ringReadPos_];
                    float r = ringBuffer_[(ringReadPos_ + 1) % ringBuffer_.size()];
                    ringReadPos_ = (ringReadPos_ + 2) % ringBuffer_.size();

                    if (endpointIsFloat_) {
                        outFloat[i * endpointChannels_ + 0] = l;
                        outFloat[i * endpointChannels_ + 1] = r;
                        for (uint32_t c = 2; c < endpointChannels_; ++c) {
                            outFloat[i * endpointChannels_ + c] = 0.0f;
                        }
                    } else {
                        outInt16[i * endpointChannels_ + 0] = static_cast<int16_t>(std::clamp(l, -1.0f, 1.0f) * 32767.0f);
                        outInt16[i * endpointChannels_ + 1] = static_cast<int16_t>(std::clamp(r, -1.0f, 1.0f) * 32767.0f);
                        for (uint32_t c = 2; c < endpointChannels_; ++c) {
                            outInt16[i * endpointChannels_ + c] = 0;
                        }
                    }
                }
                ringCount_ -= framesRead * 2;
            }

            if (framesRead < framesToFill) {
                // Buffer underrun: fill remaining with silence
                underrunCount_.fetch_add(1, std::memory_order_relaxed);
                for (size_t i = framesRead; i < framesToFill; ++i) {
                    if (endpointIsFloat_) {
                        for (uint32_t c = 0; c < endpointChannels_; ++c) {
                            outFloat[i * endpointChannels_ + c] = 0.0f;
                        }
                    } else {
                        for (uint32_t c = 0; c < endpointChannels_; ++c) {
                            outInt16[i * endpointChannels_ + c] = 0;
                        }
                    }
                }
            }

            renderClient_->ReleaseBuffer(framesToFill, 0);
            framesRendered_.fetch_add(framesToFill, std::memory_order_relaxed);
        }
    }

    if (hTask) {
        AvRevertMmThreadCharacteristics(hTask);
    }
    CoUninitialize();
}

AudioPlaybackStats WasapiAudioPlayback::getStats() const noexcept {
    AudioPlaybackStats stats;
    stats.framesRendered = framesRendered_.load(std::memory_order_relaxed);
    stats.underrunCount = underrunCount_.load(std::memory_order_relaxed);
    stats.overflowCount = overflowCount_.load(std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(ringMutex_);
        stats.currentBufferFrames = static_cast<uint32_t>(ringCount_ / 2);
    }
    return stats;
}

} // namespace mist::audio
