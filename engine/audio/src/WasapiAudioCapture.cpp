#include "mist/audio/WasapiAudioCapture.h"
#include <chrono>
#include <cstring>
#include <avrt.h>

using Microsoft::WRL::ComPtr;

namespace mist::audio {

namespace {

static constexpr GUID GUID_IEEE_FLOAT = {
    0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }
};

uint64_t getNowNs() {
    static LARGE_INTEGER freq;
    static bool init = []() {
        QueryPerformanceFrequency(&freq);
        return true;
    }();
    (void)init;
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (static_cast<uint64_t>(counter.QuadPart) * 1000000000ULL) / static_cast<uint64_t>(freq.QuadPart);
}

} // namespace

WasapiAudioCapture::WasapiAudioCapture() = default;

WasapiAudioCapture::~WasapiAudioCapture() {
    (void)stop();
    releaseEndpoint();
}

common::Result<void> WasapiAudioCapture::initialize(const AudioConfig& config) {
    if (isCapturing_) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::AlreadyExists, "Cannot reinitialize while capturing");
    }

    releaseEndpoint();

    config_ = config;
    tempFrameBuffer_.resize(config_.bufferFrameSize * 2);

    auto res = initEndpoint();
    if (res.hasError()) {
        return res;
    }

    isInitialized_ = true;
    return common::Result<void>::success();
}

common::Result<void> WasapiAudioCapture::initEndpoint() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    (void)hr;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), &enumerator_);
    if (FAILED(hr) || !enumerator_) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Failed to create MMDeviceEnumerator");
    }

    hr = enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &device_);
    if (FAILED(hr) || !device_) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::HardwareUnavailable, "No active audio render endpoint found for loopback capture");
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

    deviceFormat_.sampleRate = mixFormat->nSamplesPerSec;
    deviceFormat_.channels = mixFormat->nChannels;
    deviceFormat_.bitsPerSample = mixFormat->wBitsPerSample;

    if (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        auto* ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mixFormat);
        if (ext->SubFormat == GUID_IEEE_FLOAT) {
            deviceFormat_.sampleFormat = AudioSampleFormat::Float32;
        } else {
            deviceFormat_.sampleFormat = AudioSampleFormat::Int16;
        }
    } else if (mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        deviceFormat_.sampleFormat = AudioSampleFormat::Float32;
    } else {
        deviceFormat_.sampleFormat = AudioSampleFormat::Int16;
    }

    // Configure resampler from deviceFormat to target sample rate
    auto resConfig = resampler_.configure(deviceFormat_, config_.sampleRate);
    if (resConfig.hasError()) {
        CoTaskMemFree(mixFormat);
        return resConfig;
    }

    // Initialize in loopback shared event-driven mode with 20ms buffer
    REFERENCE_TIME bufferDuration = 200000; // 20 ms in 100ns units
    hr = audioClient_->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        bufferDuration,
        0,
        mixFormat,
        nullptr
    );

    CoTaskMemFree(mixFormat);

    if (FAILED(hr)) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Failed to initialize IAudioClient in loopback mode: hr=" + std::to_string(hr));
    }

    hCaptureEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!hCaptureEvent_) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Failed to create capture event handle");
    }

    hr = audioClient_->SetEventHandle(hCaptureEvent_);
    if (FAILED(hr)) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Failed to set audio client event handle");
    }

    hr = audioClient_->GetService(__uuidof(IAudioCaptureClient), &captureClient_);
    if (FAILED(hr) || !captureClient_) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Failed to get IAudioCaptureClient");
    }

    deviceInvalidated_ = false;
    return common::Result<void>::success();
}

void WasapiAudioCapture::releaseEndpoint() {
    if (captureClient_) captureClient_.Reset();
    if (audioClient_) audioClient_.Reset();
    if (device_) device_.Reset();
    if (enumerator_) enumerator_.Reset();

    if (hCaptureEvent_) {
        CloseHandle(hCaptureEvent_);
        hCaptureEvent_ = nullptr;
    }
}

common::Result<void> WasapiAudioCapture::start() {
    if (!isInitialized_) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::NotInitialized, "WasapiAudioCapture not initialized");
    }
    if (isCapturing_) {
        return common::Result<void>::success();
    }

    HRESULT hr = audioClient_->Start();
    if (FAILED(hr)) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Failed to start IAudioClient");
    }

    isCapturing_ = true;
    resampler_.reset();
    return common::Result<void>::success();
}

common::Result<void> WasapiAudioCapture::stop() {
    if (!isCapturing_) {
        return common::Result<void>::success();
    }

    if (audioClient_) {
        audioClient_->Stop();
    }
    isCapturing_ = false;
    return common::Result<void>::success();
}

common::Result<AudioPacket> WasapiAudioCapture::readPacket(uint32_t timeoutMs) {
    if (!isCapturing_) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::NotInitialized, "Capture is not running");
    }

    // Check if resampler already has enough frames
    if (resampler_.hasFrame(config_.bufferFrameSize)) {
        (void)resampler_.popFrame(std::span<float>(tempFrameBuffer_.data(), tempFrameBuffer_.size()), config_.bufferFrameSize);
        AudioPacket pkt;
        pkt.timestampNs = getNowNs();
        pkt.frames = config_.bufferFrameSize;
        pkt.data.resize(tempFrameBuffer_.size() * sizeof(float));
        std::memcpy(pkt.data.data(), tempFrameBuffer_.data(), pkt.data.size());
        return pkt;
    }

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    while (std::chrono::steady_clock::now() < deadline) {
        DWORD waitRes = WaitForSingleObject(hCaptureEvent_, 10);
        if (waitRes == WAIT_OBJECT_0) {
            UINT32 packetLength = 0;
            HRESULT hr = captureClient_->GetNextPacketSize(&packetLength);
            while (SUCCEEDED(hr) && packetLength > 0) {
                BYTE* pData = nullptr;
                UINT32 numFramesRead = 0;
                DWORD flags = 0;
                UINT64 devicePosition = 0;
                UINT64 qpcPosition = 0;

                hr = captureClient_->GetBuffer(&pData, &numFramesRead, &flags, &devicePosition, &qpcPosition);
                if (SUCCEEDED(hr)) {
                    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                        // Silent buffer: fill with zeros
                        size_t sampleBytes = (deviceFormat_.bitsPerSample / 8) * deviceFormat_.channels;
                        size_t totalBytes = numFramesRead * sampleBytes;
                        if (silentBuffer_.size() < totalBytes) {
                            silentBuffer_.resize(totalBytes, 0);
                        } else {
                            std::fill(silentBuffer_.begin(), silentBuffer_.begin() + static_cast<std::ptrdiff_t>(totalBytes), 0);
                        }
                        (void)resampler_.pushInput(silentBuffer_.data(), numFramesRead);
                    } else if (pData && numFramesRead > 0) {
                        (void)resampler_.pushInput(pData, numFramesRead);
                    }

                    captureClient_->ReleaseBuffer(numFramesRead);
                }

                hr = captureClient_->GetNextPacketSize(&packetLength);
            }

            if (resampler_.hasFrame(config_.bufferFrameSize)) {
                (void)resampler_.popFrame(std::span<float>(tempFrameBuffer_.data(), tempFrameBuffer_.size()), config_.bufferFrameSize);
                AudioPacket pkt;
                pkt.timestampNs = getNowNs();
                pkt.frames = config_.bufferFrameSize;
                pkt.data.resize(tempFrameBuffer_.size() * sizeof(float));
                std::memcpy(pkt.data.data(), tempFrameBuffer_.data(), pkt.data.size());
                return pkt;
            }
        }
    }

    // Timeout: if no audio is actively playing on the host, loopback produces silence.
    // Emit a silent frame so the audio stream maintains its 100 or 50 pps cadence without freezing.
    std::fill(tempFrameBuffer_.begin(), tempFrameBuffer_.end(), 0.0f);
    AudioPacket pkt;
    pkt.timestampNs = getNowNs();
    pkt.frames = config_.bufferFrameSize;
    pkt.data.resize(tempFrameBuffer_.size() * sizeof(float));
    std::memcpy(pkt.data.data(), tempFrameBuffer_.data(), pkt.data.size());
    return pkt;
}

} // namespace mist::audio
