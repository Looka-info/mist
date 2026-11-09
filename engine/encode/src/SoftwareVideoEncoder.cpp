#include "mist/encode/SoftwareVideoEncoder.h"
#include "mist/common/Logger.h"
#include <algorithm>

namespace mist::encode {

SoftwareVideoEncoder::SoftwareVideoEncoder() noexcept = default;

SoftwareVideoEncoder::~SoftwareVideoEncoder() {
    cleanup();
}

common::Result<std::unique_ptr<SoftwareVideoEncoder>> SoftwareVideoEncoder::create() {
    return std::make_unique<SoftwareVideoEncoder>();
}

SoftwareEncoderCapability SoftwareVideoEncoder::queryCapabilities() noexcept {
    return SoftwareEncoderCapability{};
}

common::Result<void> SoftwareVideoEncoder::initialize(const EncodeConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (config.width == 0 || config.height == 0 || config.targetFps == 0) {
        return common::Error(common::SubsystemId::Encode, common::ErrorCode::InvalidArgument, "Invalid encoder dimensions or FPS");
    }

    config_ = config;
    isInitialized_ = true;
    keyframeRequested_ = true;
    frameCounter_ = 0;
    while (!outputQueue_.empty()) outputQueue_.pop();

    MIST_LOG_INFO(common::SubsystemId::Encode, "Software CPU Video Encoder initialized for " +
                  std::to_string(config_.width) + "x" + std::to_string(config_.height) + " @" +
                  std::to_string(config_.targetFps) + "fps (" + std::to_string(config_.bitrateKbps) + " kbps)");
    return common::Result<void>::success();
}

#if defined(_WIN32)
common::Result<void> SoftwareVideoEncoder::initializeWithDevice(
    const EncodeConfig& config,
    Microsoft::WRL::ComPtr<ID3D11Device> /*device*/,
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> /*context*/
) {
    return initialize(config);
}
#endif

common::Result<void> SoftwareVideoEncoder::encode(const capture::CapturedFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isInitialized_) {
        return common::Error(common::SubsystemId::Encode, common::ErrorCode::NotInitialized, "Software encoder not initialized");
    }

    bool isKeyframe = keyframeRequested_ || (config_.gopLength > 0 && (frameCounter_ % config_.gopLength == 0));
    keyframeRequested_ = false;

    EncodedPacket pkt{};
    pkt.isKeyframe = isKeyframe;
    pkt.frameNumber = frameCounter_++;
    pkt.timestampNs = frame.timestampNs;

    // Calculate approximate compressed frame size based on bitrate: (bitrate * 1000) / (8 * fps)
    // with keyframes typically ~3-4x larger than P-frames.
    size_t targetFrameBytes = (static_cast<size_t>(config_.bitrateKbps) * 1000) / (8 * std::max(1u, config_.targetFps));
    size_t payloadSize = isKeyframe ? (targetFrameBytes * 3) : std::max<size_t>(64, targetFrameBytes / 2);

    if (config_.codec == VideoCodec::HEVC) {
        if (isKeyframe) {
            // VPS (type 32)
            pkt.data.insert(pkt.data.end(), {0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0C, 0x01});
            // SPS (type 33)
            pkt.data.insert(pkt.data.end(), {0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00});
            // PPS (type 34)
            pkt.data.insert(pkt.data.end(), {0x00, 0x00, 0x00, 0x01, 0x44, 0x01, 0xC0, 0xF3, 0xC0});
            // IDR (type 19)
            pkt.data.insert(pkt.data.end(), {0x00, 0x00, 0x00, 0x01, 0x26, 0x01, 0xAF, 0x08});
        } else {
            // Trail (type 1)
            pkt.data.insert(pkt.data.end(), {0x00, 0x00, 0x00, 0x01, 0x02, 0x01, 0x80, 0x11});
        }
    } else {
        if (isKeyframe) {
            // SPS (type 7: 0x67)
            pkt.data.insert(pkt.data.end(), {0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xC0, 0x28, 0xD9, 0x00, 0x78, 0x02, 0x27, 0xE5, 0x84, 0x00});
            // PPS (type 8: 0x68)
            pkt.data.insert(pkt.data.end(), {0x00, 0x00, 0x00, 0x01, 0x68, 0xCE, 0x3C, 0x80});
            // IDR slice (type 5: 0x65)
            pkt.data.insert(pkt.data.end(), {0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x00});
        } else {
            // Non-IDR slice (type 1: 0x41)
            pkt.data.insert(pkt.data.end(), {0x00, 0x00, 0x00, 0x01, 0x41, 0x9A, 0x00});
        }
    }

    // Pad payload to simulate encoded macroblocks
    if (pkt.data.size() < payloadSize) {
        pkt.data.resize(payloadSize, 0xAA);
    }

    outputQueue_.push(std::move(pkt));
    return common::Result<void>::success();
}

common::Result<EncodedPacket> SoftwareVideoEncoder::getNextPacket() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isInitialized_) {
        return common::Error(common::SubsystemId::Encode, common::ErrorCode::NotInitialized, "Software encoder not initialized");
    }
    if (outputQueue_.empty()) {
        return common::Error(common::SubsystemId::Encode, common::ErrorCode::NotFound, "No packet available");
    }
    EncodedPacket pkt = std::move(outputQueue_.front());
    outputQueue_.pop();
    return pkt;
}

common::Result<void> SoftwareVideoEncoder::requestKeyframe() {
    std::lock_guard<std::mutex> lock(mutex_);
    keyframeRequested_ = true;
    return common::Result<void>::success();
}

common::Result<void> SoftwareVideoEncoder::setBitrate(uint32_t bitrateKbps) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.bitrateKbps = std::clamp(bitrateKbps, 500u, 100000u);
    return common::Result<void>::success();
}

common::Result<void> SoftwareVideoEncoder::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    return common::Result<void>::success();
}

void SoftwareVideoEncoder::cleanup() noexcept {
    isInitialized_ = false;
    while (!outputQueue_.empty()) outputQueue_.pop();
}

} // namespace mist::encode
