#pragma once

#include "ICaptureEngine.h"
#include "mist/platform/windows/D3D11Device.h"
#include "mist/platform/windows/PlatformWindows.h"
#include <mutex>

#if defined(_WIN32)
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#endif

namespace mist::capture {

struct PointerShape {
    uint32_t type{0};
    uint32_t width{0};
    uint32_t height{0};
    uint32_t pitch{0};
    int32_t xHotSpot{0};
    int32_t yHotSpot{0};
    int32_t xPosition{0};
    int32_t yPosition{0};
    bool isVisible{false};
    std::vector<uint8_t> shapeBuffer;
};

class DxgiCaptureEngine : public ICaptureEngine {
public:
    DxgiCaptureEngine() noexcept;
    ~DxgiCaptureEngine() override;

    [[nodiscard]] common::Result<void> initialize(const CaptureConfig& config) override;
    [[nodiscard]] common::Result<void> start() override;
    [[nodiscard]] common::Result<void> stop() override;
    [[nodiscard]] common::Result<CapturedFrame> acquireNextFrame(uint32_t timeoutMs) override;
    [[nodiscard]] common::Result<void> releaseFrame(const CapturedFrame& frame) override;
    [[nodiscard]] bool isCapturing() const noexcept override;
    [[nodiscard]] CaptureBackend getBackend() const noexcept override { return CaptureBackend::DxgiDuplication; }

    [[nodiscard]] const platform::windows::DisplayInfo& getDisplayInfo() const noexcept { return displayInfo_; }
    [[nodiscard]] const PointerShape& getPointerShape() const noexcept { return pointerShape_; }

#if defined(_WIN32)
    [[nodiscard]] ID3D11Device* getD3D11Device() const noexcept { return device_.Get(); }
    [[nodiscard]] ID3D11DeviceContext* getD3D11Context() const noexcept { return context_.Get(); }
#endif

private:
    [[nodiscard]] common::Result<void> initDuplication();
    [[nodiscard]] common::Result<void> recreateSharedTexture(uint32_t width, uint32_t height);

    mutable std::mutex mutex_;
    CaptureConfig config_;
    platform::windows::DisplayInfo displayInfo_;
    PointerShape pointerShape_;
    uint64_t frameCounter_{0};
    bool isCapturing_{false};
    bool frameAcquired_{false};

    platform::windows::HighResolutionTimer timer_;

#if defined(_WIN32)
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGIOutput1> output_;
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> duplication_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> sharedTexture_;
#endif
};

} // namespace mist::capture
