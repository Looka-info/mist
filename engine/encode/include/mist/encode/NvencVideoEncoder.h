#pragma once

#include "IVideoEncoder.h"
#include "mist/platform/windows/D3D11Device.h"
#include "mist/platform/windows/PlatformWindows.h"
#include <mutex>
#include <vector>
#include <queue>
#include <unordered_map>

#if defined(_WIN32)
#include <d3d11.h>
#include <wrl/client.h>
#endif

namespace mist::encode {

struct NvencCapability {
    bool isSupported{false};
    uint32_t maxResolutionWidth{4096};
    uint32_t maxResolutionHeight{4096};
    bool supportsH264{false};
    bool supportsHevc{false};
    bool supportsBgraInput{false};
    bool supportsNv12Input{false};
    std::string driverVersion;
};

struct NvencRuntimeAudit {
    bool isNvidiaGpu{false};
    std::string gpuName;
    std::string driverVersion;
    std::string dllPath;
    std::string dllFileVersion;
    uint32_t compileTimeMajor{0};
    uint32_t compileTimeMinor{0};
    uint32_t runtimeMaxMajor{0};
    uint32_t runtimeMaxMinor{0};
    bool isCompatible{false};
    std::string sessionInitStatus;
    bool supportsH264{false};
    bool supportsHevc{false};
    std::string selectedCodec;
    std::string selectedInputFormat;
    std::string details;
};

class NvencVideoEncoder : public IVideoEncoder {
public:
    NvencVideoEncoder() noexcept;
    ~NvencVideoEncoder() override;

    [[nodiscard]] static common::Result<std::unique_ptr<NvencVideoEncoder>> create();
    [[nodiscard]] static NvencCapability queryCapabilities(ID3D11Device* device = nullptr) noexcept;
    [[nodiscard]] static NvencRuntimeAudit queryRuntimeAudit(ID3D11Device* device = nullptr) noexcept;

    [[nodiscard]] common::Result<void> initialize(const EncodeConfig& config) override;
#if defined(_WIN32)
    [[nodiscard]] common::Result<void> initializeWithDevice(
        const EncodeConfig& config,
        Microsoft::WRL::ComPtr<ID3D11Device> device,
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context = nullptr
    ) override;
    void setDevice(
        Microsoft::WRL::ComPtr<ID3D11Device> device,
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context = nullptr
    ) noexcept;
    [[nodiscard]] ID3D11Device* getD3D11Device() const noexcept { return device_.Get(); }
#endif
    [[nodiscard]] common::Result<void> encode(const capture::CapturedFrame& frame) override;
    [[nodiscard]] common::Result<EncodedPacket> getNextPacket() override;
    [[nodiscard]] common::Result<void> requestKeyframe() override;
    [[nodiscard]] common::Result<void> setBitrate(uint32_t bitrateKbps) override;
    [[nodiscard]] common::Result<void> flush() override;

    [[nodiscard]] bool isInitialized() const noexcept { return isInitialized_; }
    [[nodiscard]] const EncodeConfig& getConfig() const noexcept { return config_; }

private:
    [[nodiscard]] common::Result<void> loadNvencLibrary();
    [[nodiscard]] common::Result<void> initializeSession(ID3D11Device* device);
    [[nodiscard]] common::Result<void> allocateBuffers();
    void cleanup() noexcept;

    mutable std::mutex mutex_;
    EncodeConfig config_;
    bool isInitialized_{false};
    bool forceKeyframeNext_{false};
    uint64_t frameCounter_{0};

    platform::windows::HighResolutionTimer timer_;

    std::queue<EncodedPacket> outputQueue_;

#if defined(_WIN32)
    HMODULE hNvencLib_{nullptr};
    void* encoderSession_{nullptr}; // void* to hold NV_ENCODE_API session handle
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;

    struct BufferSlot {
        void* outputBitstream{nullptr};
    };
    std::vector<BufferSlot> bufferSlots_;
    size_t currentSlot_{0};

    struct RegisteredResource {
        void* handle{nullptr};
        uint32_t bufferFormat{0};
    };
    std::unordered_map<void*, RegisteredResource> registeredResources_;
#endif
};

} // namespace mist::encode
