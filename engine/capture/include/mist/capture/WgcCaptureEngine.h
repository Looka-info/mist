#pragma once

#include "ICaptureEngine.h"
#include "mist/platform/windows/D3D11Device.h"
#include "mist/platform/windows/PlatformWindows.h"
#include <mutex>
#include <vector>
#include <atomic>

#if defined(_WIN32)
#include <d3d11.h>
#include <wrl/client.h>
#endif

namespace mist::capture {

class WgcCaptureEngine : public ICaptureEngine {
public:
    WgcCaptureEngine() noexcept;
    ~WgcCaptureEngine() override;

    [[nodiscard]] common::Result<void> initialize(const CaptureConfig& config) override;
    [[nodiscard]] common::Result<void> start() override;
    [[nodiscard]] common::Result<void> stop() override;
    [[nodiscard]] common::Result<CapturedFrame> acquireNextFrame(uint32_t timeoutMs) override;
    [[nodiscard]] common::Result<void> releaseFrame(const CapturedFrame& frame) override;
    [[nodiscard]] bool isCapturing() const noexcept override;

    [[nodiscard]] CaptureBackend getBackend() const noexcept override {
        return CaptureBackend::WindowsGraphicsCapture;
    }

    // Windows 10 build 1803+ / 19041+ capability check
    [[nodiscard]] static bool isSupported() noexcept;

    // Multi-adapter / hybrid laptop capability: returns true if WGC can capture cross-adapter
    [[nodiscard]] static bool supportsHybridAdapterCapture() noexcept;

    // Feature status
    [[nodiscard]] bool isBorderSuppressed() const noexcept { return borderSuppressed_; }
    [[nodiscard]] bool isCursorCaptureEnabled() const noexcept { return cursorCaptureEnabled_; }
    [[nodiscard]] uint64_t getFrameCounter() const noexcept { return frameCounter_; }
    [[nodiscard]] uint32_t getWidth() const noexcept { return width_; }
    [[nodiscard]] uint32_t getHeight() const noexcept { return height_; }

#if defined(_WIN32)
    [[nodiscard]] ID3D11Device* getD3D11Device() const noexcept { return device_.Get(); }
    [[nodiscard]] ID3D11DeviceContext* getD3D11Context() const noexcept { return context_.Get(); }
#endif

private:
    [[nodiscard]] common::Result<void> recreateSharedTexture(uint32_t width, uint32_t height);

    mutable std::mutex mutex_;
    CaptureConfig config_;
    uint64_t frameCounter_{0};
    bool isCapturing_{false};
    bool borderSuppressed_{false};
    bool cursorCaptureEnabled_{true};
    uint32_t width_{1920};
    uint32_t height_{1080};
    platform::windows::HighResolutionTimer timer_;

#if defined(_WIN32)
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> sharedTexture_;
#endif
};

} // namespace mist::capture
