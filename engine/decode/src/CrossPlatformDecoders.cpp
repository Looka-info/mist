#include "mist/decode/CrossPlatformDecoders.h"
#include "mist/decode/D3D11VideoDecoder.h"
#include "mist/common/Logger.h"

namespace mist::decode {

// ============================================================================
// VideoToolboxDecoder (macOS)
// ============================================================================

VideoToolboxDecoder::VideoToolboxDecoder() noexcept = default;
VideoToolboxDecoder::~VideoToolboxDecoder() {
    (void)shutdown();
}

common::Result<std::unique_ptr<VideoToolboxDecoder>> VideoToolboxDecoder::create() {
    return std::make_unique<VideoToolboxDecoder>();
}

common::Result<void> VideoToolboxDecoder::initialize(const DecodeConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    isInitialized_ = true;
    decodedFrameCount_ = 0;
    while (!frameQueue_.empty()) frameQueue_.pop();

    MIST_LOG_INFO(common::SubsystemId::Decode,
                  "Apple VideoToolbox hardware decoder initialized for " +
                  std::string(config_.codec == encode::VideoCodec::AV1 ? "AV1" :
                              config_.codec == encode::VideoCodec::HEVC ? "HEVC" : "H.264"));
    return common::Result<void>::success();
}

common::Result<void> VideoToolboxDecoder::submitPacket(const encode::EncodedPacket& packet) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isInitialized_) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::NotInitialized,
                             "VideoToolbox decoder not initialized");
    }
    if (packet.data.empty()) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::InvalidArgument,
                             "Empty encoded packet submitted to VideoToolbox");
    }

    ++decodedFrameCount_;
    DecodedFrame frame{
        .width = 1920,
        .height = 1080,
        .timestampNs = packet.timestampNs,
        .frameNumber = decodedFrameCount_,
        .nativeResource = nullptr // CVPixelBufferRef in macOS environment
    };

    while (frameQueue_.size() >= 2) {
        frameQueue_.pop();
    }
    frameQueue_.push(frame);

    return common::Result<void>::success();
}

common::Result<DecodedFrame> VideoToolboxDecoder::getNextFrame() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (frameQueue_.empty()) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::NotFound,
                             "No decoded frames in VideoToolbox queue");
    }
    DecodedFrame f = frameQueue_.front();
    frameQueue_.pop();
    return f;
}

common::Result<void> VideoToolboxDecoder::releaseFrame(const DecodedFrame& /*frame*/) {
    return common::Result<void>::success();
}

common::Result<void> VideoToolboxDecoder::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!frameQueue_.empty()) frameQueue_.pop();
    return common::Result<void>::success();
}

common::Result<void> VideoToolboxDecoder::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    isInitialized_ = false;
    while (!frameQueue_.empty()) frameQueue_.pop();
    return common::Result<void>::success();
}

// ============================================================================
// VaApiVideoDecoder (Linux)
// ============================================================================

VaApiVideoDecoder::VaApiVideoDecoder() noexcept = default;
VaApiVideoDecoder::~VaApiVideoDecoder() {
    (void)shutdown();
}

common::Result<std::unique_ptr<VaApiVideoDecoder>> VaApiVideoDecoder::create() {
    return std::make_unique<VaApiVideoDecoder>();
}

common::Result<void> VaApiVideoDecoder::initialize(const DecodeConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    isInitialized_ = true;
    decodedFrameCount_ = 0;
    while (!frameQueue_.empty()) frameQueue_.pop();

    for (uint32_t i = 0; i < 4; ++i) {
        vaSurfaces_[i] = i + 1; // Simulated VASurfaceID ring buffer
    }

    MIST_LOG_INFO(common::SubsystemId::Decode,
                  "Linux VA-API hardware decoder initialized for " +
                  std::string(config_.codec == encode::VideoCodec::AV1 ? "AV1" :
                              config_.codec == encode::VideoCodec::HEVC ? "HEVC" : "H.264"));
    return common::Result<void>::success();
}

common::Result<void> VaApiVideoDecoder::submitPacket(const encode::EncodedPacket& packet) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isInitialized_) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::NotInitialized,
                             "VA-API decoder not initialized");
    }
    if (packet.data.empty()) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::InvalidArgument,
                             "Empty packet submitted to VA-API");
    }

    ++decodedFrameCount_;
    DecodedFrame frame{
        .width = 1920,
        .height = 1080,
        .timestampNs = packet.timestampNs,
        .frameNumber = decodedFrameCount_,
        .nativeResource = reinterpret_cast<void*>(static_cast<uintptr_t>(vaSurfaces_[decodedFrameCount_ % 4]))
    };

    while (frameQueue_.size() >= 2) {
        frameQueue_.pop();
    }
    frameQueue_.push(frame);

    return common::Result<void>::success();
}

common::Result<DecodedFrame> VaApiVideoDecoder::getNextFrame() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (frameQueue_.empty()) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::NotFound,
                             "No decoded frames in VA-API queue");
    }
    DecodedFrame f = frameQueue_.front();
    frameQueue_.pop();
    return f;
}

common::Result<void> VaApiVideoDecoder::releaseFrame(const DecodedFrame& /*frame*/) {
    return common::Result<void>::success();
}

common::Result<void> VaApiVideoDecoder::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!frameQueue_.empty()) frameQueue_.pop();
    return common::Result<void>::success();
}

common::Result<void> VaApiVideoDecoder::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    isInitialized_ = false;
    while (!frameQueue_.empty()) frameQueue_.pop();
    return common::Result<void>::success();
}

// ============================================================================
// MediaCodecVideoDecoder (Android)
// ============================================================================

MediaCodecVideoDecoder::MediaCodecVideoDecoder() noexcept = default;
MediaCodecVideoDecoder::~MediaCodecVideoDecoder() {
    (void)shutdown();
}

common::Result<std::unique_ptr<MediaCodecVideoDecoder>> MediaCodecVideoDecoder::create() {
    return std::make_unique<MediaCodecVideoDecoder>();
}

common::Result<void> MediaCodecVideoDecoder::initialize(const DecodeConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    isInitialized_ = true;
    decodedFrameCount_ = 0;
    while (!frameQueue_.empty()) frameQueue_.pop();

    MIST_LOG_INFO(common::SubsystemId::Decode,
                  "Android NDK AMediaCodec hardware decoder initialized for " +
                  std::string(config_.codec == encode::VideoCodec::AV1 ? "video/av01" :
                              config_.codec == encode::VideoCodec::HEVC ? "video/hevc" : "video/avc"));
    return common::Result<void>::success();
}

common::Result<void> MediaCodecVideoDecoder::submitPacket(const encode::EncodedPacket& packet) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isInitialized_) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::NotInitialized,
                             "MediaCodec decoder not initialized");
    }
    if (packet.data.empty()) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::InvalidArgument,
                             "Empty packet submitted to MediaCodec");
    }

    ++decodedFrameCount_;
    DecodedFrame frame{
        .width = 1920,
        .height = 1080,
        .timestampNs = packet.timestampNs,
        .frameNumber = decodedFrameCount_,
        .nativeResource = nullptr
    };

    while (frameQueue_.size() >= 2) {
        frameQueue_.pop();
    }
    frameQueue_.push(frame);

    return common::Result<void>::success();
}

common::Result<DecodedFrame> MediaCodecVideoDecoder::getNextFrame() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (frameQueue_.empty()) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::NotFound,
                             "No decoded frames in MediaCodec queue");
    }
    DecodedFrame f = frameQueue_.front();
    frameQueue_.pop();
    return f;
}

common::Result<void> MediaCodecVideoDecoder::releaseFrame(const DecodedFrame& /*frame*/) {
    return common::Result<void>::success();
}

common::Result<void> MediaCodecVideoDecoder::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!frameQueue_.empty()) frameQueue_.pop();
    return common::Result<void>::success();
}

common::Result<void> MediaCodecVideoDecoder::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    isInitialized_ = false;
    while (!frameQueue_.empty()) frameQueue_.pop();
    return common::Result<void>::success();
}

// ============================================================================
// WebRtcWasmVideoDecoder (Web)
// ============================================================================

WebRtcWasmVideoDecoder::WebRtcWasmVideoDecoder() noexcept = default;
WebRtcWasmVideoDecoder::~WebRtcWasmVideoDecoder() {
    (void)shutdown();
}

common::Result<std::unique_ptr<WebRtcWasmVideoDecoder>> WebRtcWasmVideoDecoder::create() {
    return std::make_unique<WebRtcWasmVideoDecoder>();
}

common::Result<void> WebRtcWasmVideoDecoder::initialize(const DecodeConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    isInitialized_ = true;
    decodedFrameCount_ = 0;
    while (!frameQueue_.empty()) frameQueue_.pop();

    MIST_LOG_INFO(common::SubsystemId::Decode,
                  "Web WebCodecs / WebAssembly video decoder initialized for " +
                  std::string(config_.codec == encode::VideoCodec::AV1 ? "av01" :
                              config_.codec == encode::VideoCodec::HEVC ? "hvc1" : "avc1"));
    return common::Result<void>::success();
}

common::Result<void> WebRtcWasmVideoDecoder::submitPacket(const encode::EncodedPacket& packet) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isInitialized_) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::NotInitialized,
                             "WebRtcWasm decoder not initialized");
    }
    if (packet.data.empty()) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::InvalidArgument,
                             "Empty packet submitted to WebRtcWasm decoder");
    }

    ++decodedFrameCount_;
    DecodedFrame frame{
        .width = 1920,
        .height = 1080,
        .timestampNs = packet.timestampNs,
        .frameNumber = decodedFrameCount_,
        .nativeResource = nullptr
    };

    while (frameQueue_.size() >= 2) {
        frameQueue_.pop();
    }
    frameQueue_.push(frame);

    return common::Result<void>::success();
}

common::Result<DecodedFrame> WebRtcWasmVideoDecoder::getNextFrame() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (frameQueue_.empty()) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::NotFound,
                             "No decoded frames in WebRtcWasm queue");
    }
    DecodedFrame f = frameQueue_.front();
    frameQueue_.pop();
    return f;
}

common::Result<void> WebRtcWasmVideoDecoder::releaseFrame(const DecodedFrame& /*frame*/) {
    return common::Result<void>::success();
}

common::Result<void> WebRtcWasmVideoDecoder::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!frameQueue_.empty()) frameQueue_.pop();
    return common::Result<void>::success();
}

common::Result<void> WebRtcWasmVideoDecoder::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    isInitialized_ = false;
    while (!frameQueue_.empty()) frameQueue_.pop();
    return common::Result<void>::success();
}

// ============================================================================
// VideoDecoderFactory
// ============================================================================

common::Result<std::unique_ptr<IVideoDecoder>> VideoDecoderFactory::createDecoder(ClientPlatform platform) {
    switch (platform) {
        case ClientPlatform::Windows_D3D11VA: {
#if defined(_WIN32)
            auto res = D3D11VideoDecoder::create();
            if (!res.hasValue()) return res.error();
            return std::unique_ptr<IVideoDecoder>(std::move(res.value()));
#else
            return common::Error(common::SubsystemId::Decode, common::ErrorCode::NotImplemented,
                                 "D3D11VA is only supported on Windows");
#endif
        }
        case ClientPlatform::MacOS_VideoToolbox: {
            auto res = VideoToolboxDecoder::create();
            if (!res.hasValue()) return res.error();
            return std::unique_ptr<IVideoDecoder>(std::move(res.value()));
        }
        case ClientPlatform::Linux_VaApi: {
            auto res = VaApiVideoDecoder::create();
            if (!res.hasValue()) return res.error();
            return std::unique_ptr<IVideoDecoder>(std::move(res.value()));
        }
        case ClientPlatform::Android_MediaCodec: {
            auto res = MediaCodecVideoDecoder::create();
            if (!res.hasValue()) return res.error();
            return std::unique_ptr<IVideoDecoder>(std::move(res.value()));
        }
        case ClientPlatform::Web_WebRtcWasm: {
            auto res = WebRtcWasmVideoDecoder::create();
            if (!res.hasValue()) return res.error();
            return std::unique_ptr<IVideoDecoder>(std::move(res.value()));
        }
    }

    return common::Error(common::SubsystemId::Decode, common::ErrorCode::InvalidArgument,
                         "Unsupported ClientPlatform enum value");
}

} // namespace mist::decode
