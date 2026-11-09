#pragma once

#include "mist/decode/IVideoDecoder.h"
#include <string>
#include <string_view>
#include <memory>
#include <queue>
#include <mutex>
#include <vector>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#include <VideoToolbox/VideoToolbox.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#endif

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

struct PlatformCapabilityInfo {
    ClientPlatform platform{ClientPlatform::Windows_D3D11VA};
    std::string nativeApi;
    bool isNativeCompiled{false};
    bool isHardwareAccelerated{false};
    bool supportsH264{false};
    bool supportsHevc{false};
    bool supportsAv1{false};
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
#if defined(__APPLE__)
    VTDecompressionSessionRef vtSession_{nullptr};
    CMVideoFormatDescriptionRef formatDesc_{nullptr};
    bool formatCreated_{false};

    static void decompressionOutputCallback(
        void* decompressionOutputRefCon,
        void* sourceFrameRefCon,
        OSStatus status,
        VTDecodeInfoFlags infoFlags,
        CVImageBufferRef imageBuffer,
        CMTime presentationTimeStamp,
        CMTime presentationDuration);

    bool extractH264Parameters(const uint8_t* data, size_t size);
    bool extractHevcParameters(const uint8_t* data, size_t size);
    bool createDecompressionSession();
#else
    [[maybe_unused]] void* vtSession_{nullptr};
    [[maybe_unused]] void* formatDesc_{nullptr};
#endif
};

enum class VaSurfaceState : uint8_t {
    Available = 0,
    Decoding,
    Decoded,
    InUse,
    Released
};

struct VaSurfaceSlot {
    uint32_t surfaceId{0};
    VaSurfaceState state{VaSurfaceState::Available};
    uint64_t frameNumber{0};
    uint64_t timestampNs{0};
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
#if defined(__linux__) && !defined(__ANDROID__)
    int drmFd_{-1};
    void* vaDisplay_{nullptr};
    uint32_t vaConfig_{0};
    uint32_t vaContext_{0};
    std::vector<VaSurfaceSlot> surfacePool_;
    static constexpr size_t kMaxVaSurfaces = 8;
    bool initVaDisplay();
    bool createVaContext(uint32_t width, uint32_t height);
    void destroyVaResources();
#else
    [[maybe_unused]] void* vaDisplay_{nullptr};
    [[maybe_unused]] uint32_t vaContext_{0};
#endif
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
#if defined(__ANDROID__)
    AMediaCodec* mediaCodec_{nullptr};
    ANativeWindow* nativeWindow_{nullptr};
#else
    [[maybe_unused]] void* mediaCodec_{nullptr};
    [[maybe_unused]] void* nativeWindow_{nullptr};
#endif
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
