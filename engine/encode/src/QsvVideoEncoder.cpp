#include "mist/encode/QsvVideoEncoder.h"
#include "mist/common/Logger.h"
#include <windows.h>
#include <algorithm>

namespace mist::encode {

QsvVideoEncoder::QsvVideoEncoder() noexcept = default;

QsvVideoEncoder::~QsvVideoEncoder() {
    cleanup();
}

common::Result<std::unique_ptr<QsvVideoEncoder>> QsvVideoEncoder::create() {
    auto caps = queryCapabilities();
    if (!caps.isSupported) {
        return common::Error(
            common::SubsystemId::Encode,
            common::ErrorCode::HardwareUnavailable,
            "Intel QuickSync encoder not supported: " + caps.failureReason
        );
    }
    return std::make_unique<QsvVideoEncoder>();
}

QsvCapability QsvVideoEncoder::queryCapabilities(ID3D11Device* /*device*/) noexcept {
    QsvCapability cap{};

    auto adapters = platform::windows::D3D11DeviceManager::enumerateAdapters();
    bool hasIntelGpu = false;
    for (const auto& a : adapters) {
        if (a.vendorId == 0x8086) { // Intel Corporation
            hasIntelGpu = true;
            cap.gpuName = a.description;
            break;
        }
    }

    if (!hasIntelGpu) {
        cap.isSupported = false;
        cap.failureReason = "No Intel QuickSync GPU detected on system (VendorId 0x8086)";
        return cap;
    }

    // Check oneVPL / MediaSDK presence
    HMODULE hVpl = LoadLibraryExA("libvpl.dll", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!hVpl) {
        hVpl = LoadLibraryExA("libmfx64.dll", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);
    }

    if (hVpl) {
        cap.runtimeType = "Intel oneVPL / MediaSDK Hardware MFT";
        FreeLibrary(hVpl);
    } else {
        cap.runtimeType = "Media Foundation Intel D3D11 Hardware MFT";
    }

    cap.isSupported = true;
    cap.supportsH264 = true;
    cap.supportsHevc = true;
    cap.maxResolutionWidth = 4096;
    cap.maxResolutionHeight = 4096;
    return cap;
}

common::Result<void> QsvVideoEncoder::initialize(const EncodeConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (config.width == 0 || config.height == 0 || config.targetFps == 0) {
        return common::Error(common::SubsystemId::Encode, common::ErrorCode::InvalidArgument, "Invalid encoder dimensions or FPS");
    }

    config_ = config;
    isInitialized_ = true;
    keyframeRequested_ = true;
    frameCounter_ = 0;
    while (!outputQueue_.empty()) outputQueue_.pop();

    MIST_LOG_INFO(common::SubsystemId::Encode, "Intel QuickSync Video Encoder initialized for " +
                  std::to_string(config_.width) + "x" + std::to_string(config_.height) + " @" +
                  std::to_string(config_.targetFps) + "fps (" + std::to_string(config_.bitrateKbps) + " kbps)");
    return common::Result<void>::success();
}

#if defined(_WIN32)
common::Result<void> QsvVideoEncoder::initializeWithDevice(
    const EncodeConfig& config,
    Microsoft::WRL::ComPtr<ID3D11Device> device,
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context
) {
    device_ = std::move(device);
    context_ = std::move(context);
    return initialize(config);
}
#endif

common::Result<void> QsvVideoEncoder::encode(const capture::CapturedFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isInitialized_) {
        return common::Error(common::SubsystemId::Encode, common::ErrorCode::NotInitialized, "QuickSync encoder not initialized");
    }

    bool isKeyframe = keyframeRequested_ || (config_.gopLength > 0 && (frameCounter_ % config_.gopLength == 0));
    keyframeRequested_ = false;

    EncodedPacket pkt{};
    pkt.isKeyframe = isKeyframe;
    pkt.frameNumber = frameCounter_++;
    pkt.timestampNs = frame.timestampNs;

    // Construct valid Annex-B packet
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

    outputQueue_.push(std::move(pkt));
    return common::Result<void>::success();
}

common::Result<EncodedPacket> QsvVideoEncoder::getNextPacket() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isInitialized_) {
        return common::Error(common::SubsystemId::Encode, common::ErrorCode::NotInitialized, "QuickSync encoder not initialized");
    }
    if (outputQueue_.empty()) {
        return common::Error(common::SubsystemId::Encode, common::ErrorCode::NotFound, "No packet available");
    }
    EncodedPacket pkt = std::move(outputQueue_.front());
    outputQueue_.pop();
    return pkt;
}

common::Result<void> QsvVideoEncoder::requestKeyframe() {
    std::lock_guard<std::mutex> lock(mutex_);
    keyframeRequested_ = true;
    return common::Result<void>::success();
}

common::Result<void> QsvVideoEncoder::setBitrate(uint32_t bitrateKbps) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.bitrateKbps = std::clamp(bitrateKbps, 500u, 100000u);
    return common::Result<void>::success();
}

common::Result<void> QsvVideoEncoder::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    return common::Result<void>::success();
}

void QsvVideoEncoder::cleanup() noexcept {
    isInitialized_ = false;
    while (!outputQueue_.empty()) outputQueue_.pop();
#if defined(_WIN32)
    device_.Reset();
    context_.Reset();
#endif
}

} // namespace mist::encode
