#pragma once

#include "mist/decode/IVideoDecoder.h"
#include <string>
#include <string_view>
#include <memory>
#include <queue>
#include <mutex>
#include <vector>
#include <cstdint>

namespace mist::decode {

enum class ClientPlatform : uint8_t {
    Windows_D3D11VA = 0,
    MacOS_VideoToolbox,
    Linux_VaApi,
    Android_MediaCodec,
    Web_WebRtcWasm
};

[[nodiscard]] constexpr std::string_view clientPlatformToString(ClientPlatform p) noexcept {
    switch (p) {
        case ClientPlatform::Windows_D3D11VA: return "Windows (D3D11VA)";
        case ClientPlatform::MacOS_VideoToolbox: return "macOS (Apple VideoToolbox)";
        case ClientPlatform::Linux_VaApi: return "Linux (VA-API / DMA-BUF)";
        case ClientPlatform::Android_MediaCodec: return "Android (NDK AMediaCodec)";
        case ClientPlatform::Web_WebRtcWasm: return "Web (WebCodecs WASM)";
    }
    return "Unknown";
}

enum class DecoderQualificationState : uint8_t {
    NotSupported = 0,
    NotBuilt,
    Built,
    NativeApiAvailable,
    HardwareAvailable,
    DecoderInitialized,
    SmokeTestPassed,
    HardwareVerified
};

[[nodiscard]] constexpr std::string_view qualificationStateToString(DecoderQualificationState s) noexcept {
    switch (s) {
        case DecoderQualificationState::NotSupported: return "NotSupported";
        case DecoderQualificationState::NotBuilt: return "NotBuilt";
        case DecoderQualificationState::Built: return "Built";
        case DecoderQualificationState::NativeApiAvailable: return "NativeApiAvailable";
        case DecoderQualificationState::HardwareAvailable: return "HardwareAvailable";
        case DecoderQualificationState::DecoderInitialized: return "DecoderInitialized";
        case DecoderQualificationState::SmokeTestPassed: return "SmokeTestPassed";
        case DecoderQualificationState::HardwareVerified: return "HardwareVerified";
    }
    return "Unknown";
}

struct PlatformCapabilityInfo {
    ClientPlatform platform{ClientPlatform::Windows_D3D11VA};
    std::string nativeApi;
    bool isNativeCompiled{false};
    bool isHardwareAccelerated{false};
    bool supportsH264{false};
    bool supportsHevc{false};
    bool supportsAv1{false};
    std::string supportedProfiles;
    uint32_t maxWidth{0};
    uint32_t maxHeight{0};
    std::string activeBackend;
    std::string initializationResult{"NotAttempted"};
    DecoderQualificationState qualificationState{DecoderQualificationState::NotSupported};
    std::string releaseStatus{"Pending"};
};

// macOS Apple VideoToolbox hardware decoder implementation
class VideoToolboxDecoder : public IVideoDecoder {
public:
    VideoToolboxDecoder() noexcept;
    ~VideoToolboxDecoder() override;

    [[nodiscard]] static common::Result<std::unique_ptr<VideoToolboxDecoder>> create();
    [[nodiscard]] common::Result<void> initialize(const DecodeConfig& config) override;
    [[nodiscard]] common::Result<void> submitPacket(const encode::EncodedPacket& packet) override;
    [[nodiscard]] common::Result<DecodedFrame> getNextFrame() override;
    [[nodiscard]] common::Result<void> releaseFrame(const DecodedFrame& frame) override;
    [[nodiscard]] common::Result<void> flush() override;
    [[nodiscard]] common::Result<void> shutdown() override;
    [[nodiscard]] bool isInitialized() const noexcept { return isInitialized_; }
    [[nodiscard]] uint32_t getDecodedFrameCount() const noexcept { return decodedFrameCount_; }
    [[nodiscard]] bool isHardwareAccelerated() const noexcept;
    [[nodiscard]] static bool isNativePlatform() noexcept;
    [[nodiscard]] static bool isCodecSupported(encode::VideoCodec codec) noexcept;

private:
    mutable std::mutex mutex_;
    DecodeConfig config_{};
    bool isInitialized_{false};
    uint32_t decodedFrameCount_{0};
    std::queue<DecodedFrame> frameQueue_;
    [[maybe_unused]] void* vtSession_{nullptr};      // VTDecompressionSessionRef on macOS
    [[maybe_unused]] void* formatDesc_{nullptr};     // CMVideoFormatDescriptionRef on macOS
};

// Linux VA-API hardware decoder implementation
class VaApiVideoDecoder : public IVideoDecoder {
public:
    VaApiVideoDecoder() noexcept;
    ~VaApiVideoDecoder() override;

    [[nodiscard]] static common::Result<std::unique_ptr<VaApiVideoDecoder>> create();
    [[nodiscard]] common::Result<void> initialize(const DecodeConfig& config) override;
    [[nodiscard]] common::Result<void> submitPacket(const encode::EncodedPacket& packet) override;
    [[nodiscard]] common::Result<DecodedFrame> getNextFrame() override;
    [[nodiscard]] common::Result<void> releaseFrame(const DecodedFrame& frame) override;
    [[nodiscard]] common::Result<void> flush() override;
    [[nodiscard]] common::Result<void> shutdown() override;
    [[nodiscard]] bool isInitialized() const noexcept { return isInitialized_; }
    [[nodiscard]] uint32_t getDecodedFrameCount() const noexcept { return decodedFrameCount_; }
    [[nodiscard]] bool isHardwareAccelerated() const noexcept;
    [[nodiscard]] static bool isNativePlatform() noexcept;
    [[nodiscard]] static bool isCodecSupported(encode::VideoCodec codec) noexcept;

private:
    mutable std::mutex mutex_;
    DecodeConfig config_{};
    bool isInitialized_{false};
    uint32_t decodedFrameCount_{0};
    std::queue<DecodedFrame> frameQueue_;
    [[maybe_unused]] int drmFd_{-1};
    [[maybe_unused]] void* vaDisplay_{nullptr};      // VADisplay on Linux
    [[maybe_unused]] uint32_t vaConfig_{0};          // VAConfigID
    [[maybe_unused]] uint32_t vaContext_{0};         // VAContextID
    [[maybe_unused]] uint32_t vaSurfaces_[8]{};      // VASurfaceID array
    [[maybe_unused]] uint32_t surfaceCount_{0};
};

// Android NDK AMediaCodec hardware decoder implementation
class MediaCodecVideoDecoder : public IVideoDecoder {
public:
    MediaCodecVideoDecoder() noexcept;
    ~MediaCodecVideoDecoder() override;

    [[nodiscard]] static common::Result<std::unique_ptr<MediaCodecVideoDecoder>> create();
    [[nodiscard]] common::Result<void> initialize(const DecodeConfig& config) override;
    [[nodiscard]] common::Result<void> submitPacket(const encode::EncodedPacket& packet) override;
    [[nodiscard]] common::Result<DecodedFrame> getNextFrame() override;
    [[nodiscard]] common::Result<void> releaseFrame(const DecodedFrame& frame) override;
    [[nodiscard]] common::Result<void> flush() override;
    [[nodiscard]] common::Result<void> shutdown() override;
    [[nodiscard]] bool isInitialized() const noexcept { return isInitialized_; }
    [[nodiscard]] uint32_t getDecodedFrameCount() const noexcept { return decodedFrameCount_; }
    [[nodiscard]] bool isHardwareAccelerated() const noexcept;
    [[nodiscard]] static bool isNativePlatform() noexcept;
    [[nodiscard]] static bool isCodecSupported(encode::VideoCodec codec) noexcept;

private:
    mutable std::mutex mutex_;
    DecodeConfig config_{};
    bool isInitialized_{false};
    uint32_t decodedFrameCount_{0};
    std::queue<DecodedFrame> frameQueue_;
    [[maybe_unused]] void* mediaCodec_{nullptr};     // AMediaCodec* on Android
    [[maybe_unused]] void* mediaFormat_{nullptr};    // AMediaFormat* on Android
    [[maybe_unused]] void* nativeWindow_{nullptr};   // ANativeWindow* on Android
};

// Web WebCodecs / WebAssembly client decoder implementation
class WebRtcWasmVideoDecoder : public IVideoDecoder {
public:
    WebRtcWasmVideoDecoder() noexcept;
    ~WebRtcWasmVideoDecoder() override;

    [[nodiscard]] static common::Result<std::unique_ptr<WebRtcWasmVideoDecoder>> create();
    [[nodiscard]] common::Result<void> initialize(const DecodeConfig& config) override;
    [[nodiscard]] common::Result<void> submitPacket(const encode::EncodedPacket& packet) override;
    [[nodiscard]] common::Result<DecodedFrame> getNextFrame() override;
    [[nodiscard]] common::Result<void> releaseFrame(const DecodedFrame& frame) override;
    [[nodiscard]] common::Result<void> flush() override;
    [[nodiscard]] common::Result<void> shutdown() override;
    [[nodiscard]] bool isInitialized() const noexcept { return isInitialized_; }
    [[nodiscard]] uint32_t getDecodedFrameCount() const noexcept { return decodedFrameCount_; }
    [[nodiscard]] bool isHardwareAccelerated() const noexcept;
    [[nodiscard]] static bool isNativePlatform() noexcept;
    [[nodiscard]] static bool isCodecSupported(encode::VideoCodec codec) noexcept;

private:
    mutable std::mutex mutex_;
    DecodeConfig config_{};
    bool isInitialized_{false};
    uint32_t decodedFrameCount_{0};
    std::queue<DecodedFrame> frameQueue_;
};

// Universal Cross-Platform Decoder Factory
class VideoDecoderFactory {
public:
    [[nodiscard]] static common::Result<std::unique_ptr<IVideoDecoder>> createDecoder(
        ClientPlatform platform = ClientPlatform::Windows_D3D11VA
    );

    [[nodiscard]] static ClientPlatform getNativePlatform() noexcept {
#if defined(__APPLE__)
        return ClientPlatform::MacOS_VideoToolbox;
#elif defined(__ANDROID__)
        return ClientPlatform::Android_MediaCodec;
#elif defined(__EMSCRIPTEN__)
        return ClientPlatform::Web_WebRtcWasm;
#elif defined(__linux__)
        return ClientPlatform::Linux_VaApi;
#else
        return ClientPlatform::Windows_D3D11VA;
#endif
    }

    [[nodiscard]] static std::vector<PlatformCapabilityInfo> getCapabilityMatrix();
    [[nodiscard]] static PlatformCapabilityInfo getPlatformCapability(ClientPlatform platform);
};

} // namespace mist::decode
