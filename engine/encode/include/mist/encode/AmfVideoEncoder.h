#pragma once

#include "IVideoEncoder.h"
#include "mist/platform/windows/D3D11Device.h"
#include <mutex>
#include <vector>
#include <queue>
#include <string>

#if defined(_WIN32)
#include <d3d11.h>
#include <wrl/client.h>
#endif

namespace mist::encode {

struct AmfCapability {
    bool isSupported{false};
    bool supportsH264{false};
    bool supportsHevc{false};
    uint32_t maxResolutionWidth{4096};
    uint32_t maxResolutionHeight{4096};
    std::string gpuName;
    std::string driverVersion;
    std::string dllPath;
    std::string failureReason;
};

class AmfVideoEncoder : public IVideoEncoder {
public:
    AmfVideoEncoder() noexcept;
    ~AmfVideoEncoder() override;

    [[nodiscard]] static common::Result<std::unique_ptr<AmfVideoEncoder>> create();
    [[nodiscard]] static AmfCapability queryCapabilities(ID3D11Device* device = nullptr) noexcept;

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

#if defined(_WIN32)
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
#endif
};

} // namespace mist::encode
