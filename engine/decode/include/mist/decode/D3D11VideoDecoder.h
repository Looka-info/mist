#pragma once

#include "IVideoDecoder.h"
#include "H264Parser.h"
#include "mist/platform/windows/D3D11Device.h"
#include "mist/platform/windows/PlatformWindows.h"
#include <mutex>
#include <queue>
#include <memory>
#include <string>

#if defined(_WIN32)
#include <d3d11.h>
#include <mfapi.h>
#include <mftransform.h>
#include <mfidl.h>
#include <mferror.h>
#include <wrl/client.h>
#endif

namespace mist::decode {

struct DecoderCapability {
    bool isHardwareAccelerated{false};
    bool supportsH264{false};
    uint32_t maxResolutionWidth{4096};
    uint32_t maxResolutionHeight{4096};
    std::string deviceName;
    std::string driverVersion;
};

class D3D11VideoDecoder : public IVideoDecoder {
public:
    D3D11VideoDecoder() noexcept;
    ~D3D11VideoDecoder() override;

    [[nodiscard]] static common::Result<std::unique_ptr<D3D11VideoDecoder>> create();
    [[nodiscard]] static DecoderCapability queryCapabilities(ID3D11Device* device = nullptr) noexcept;

    [[nodiscard]] common::Result<void> initialize(const DecodeConfig& config) override;
#if defined(_WIN32)
    [[nodiscard]] common::Result<void> initializeWithDevice(
        const DecodeConfig& config,
        Microsoft::WRL::ComPtr<ID3D11Device> device,
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context = nullptr
    );
    void setDevice(
        Microsoft::WRL::ComPtr<ID3D11Device> device,
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context = nullptr
    ) noexcept;
    [[nodiscard]] ID3D11Device* getD3D11Device() const noexcept { return device_.Get(); }
#endif

    [[nodiscard]] common::Result<void> submitPacket(const encode::EncodedPacket& packet) override;
    [[nodiscard]] common::Result<DecodedFrame> getNextFrame() override;
    [[nodiscard]] common::Result<void> releaseFrame(const DecodedFrame& frame) override;
    [[nodiscard]] common::Result<void> flush() override;
    [[nodiscard]] common::Result<void> shutdown() override;

    [[nodiscard]] bool isInitialized() const noexcept { return isInitialized_; }
    [[nodiscard]] const DecodeConfig& getConfig() const noexcept { return config_; }
    [[nodiscard]] uint32_t getWidth() const noexcept { return width_; }
    [[nodiscard]] uint32_t getHeight() const noexcept { return height_; }

    struct DecoderStats {
        uint64_t packetsSubmitted{0};
        uint64_t framesDecoded{0};
        uint64_t framesQueueDiscarded{0};
        uint64_t decoderErrors{0};
    };
    [[nodiscard]] DecoderStats getDecoderStats() const noexcept;

private:
    [[nodiscard]] common::Result<void> initializeMft();
    [[nodiscard]] common::Result<void> drainOutput();
    void cleanup() noexcept;

    mutable std::mutex mutex_;
    DecoderStats stats_{};
    DecodeConfig config_;
    bool isInitialized_{false};
    uint32_t width_{1920};
    uint32_t height_{1080};
    uint64_t frameCounter_{0};

    std::queue<DecodedFrame> decodedQueue_;

#if defined(_WIN32)
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> dxgiManager_;
    UINT dxgiResetToken_{0};
    Microsoft::WRL::ComPtr<IMFTransform> decoderTransform_;
    bool mfStarted_{false};
#endif
};

} // namespace mist::decode
