#pragma once

#include "IVideoEncoder.h"
#include <mutex>
#include <vector>
#include <queue>
#include <string>

namespace mist::encode {

struct SoftwareEncoderCapability {
    bool isSupported{true};
    bool supportsH264{true};
    bool supportsHevc{true};
    uint32_t maxResolutionWidth{4096};
    uint32_t maxResolutionHeight{4096};
    std::string encoderName{"Software CPU Fallback Encoder (Annex-B)"};
};

class SoftwareVideoEncoder : public IVideoEncoder {
public:
    SoftwareVideoEncoder() noexcept;
    ~SoftwareVideoEncoder() override;

    [[nodiscard]] static common::Result<std::unique_ptr<SoftwareVideoEncoder>> create();
    [[nodiscard]] static SoftwareEncoderCapability queryCapabilities() noexcept;

    [[nodiscard]] common::Result<void> initialize(const EncodeConfig& config) override;
#if defined(_WIN32)
    [[nodiscard]] common::Result<void> initializeWithDevice(
        const EncodeConfig& config,
        Microsoft::WRL::ComPtr<ID3D11Device> device,
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context = nullptr
    ) override;
#endif
    [[nodiscard]] common::Result<void> encode(const capture::CapturedFrame& frame) override;
    [[nodiscard]] common::Result<EncodedPacket> getNextPacket() override;
    [[nodiscard]] common::Result<void> requestKeyframe() override;
    [[nodiscard]] common::Result<void> setBitrate(uint32_t bitrateKbps) override;
    [[nodiscard]] common::Result<void> flush() override;

    [[nodiscard]] bool isInitialized() const noexcept { return isInitialized_; }
    [[nodiscard]] const EncodeConfig& getConfig() const noexcept { return config_; }

private:
    void cleanup() noexcept;

    mutable std::mutex mutex_;
    EncodeConfig config_{};
    bool isInitialized_{false};
    bool keyframeRequested_{true};
    uint64_t frameCounter_{0};
    std::queue<EncodedPacket> outputQueue_;
};

} // namespace mist::encode
