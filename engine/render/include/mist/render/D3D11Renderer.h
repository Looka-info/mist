#pragma once

#include "IRenderer.h"
#include "mist/platform/windows/D3D11Device.h"
#include "mist/platform/windows/PlatformWindows.h"
#include "mist/telemetry/Metrics.h"
#include <mutex>
#include <atomic>
#include <memory>
#include <string>

#if defined(_WIN32)
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dxgi1_5.h>
#include <wrl/client.h>
#endif

namespace mist::render {

class D3D11Renderer : public IRenderer {
public:
    D3D11Renderer() noexcept;
    ~D3D11Renderer() override;

    [[nodiscard]] static common::Result<std::unique_ptr<D3D11Renderer>> create();

    [[nodiscard]] common::Result<void> initialize(const RenderConfig& config) override;
#if defined(_WIN32)
    [[nodiscard]] common::Result<void> initializeWithDevice(
        const RenderConfig& config,
        Microsoft::WRL::ComPtr<ID3D11Device> device,
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context = nullptr
    );
#endif
    [[nodiscard]] common::Result<void> present(const decode::DecodedFrame& frame) override;
    [[nodiscard]] common::Result<void> resize(uint32_t width, uint32_t height) override;
    [[nodiscard]] common::Result<void> setVsync(bool enabled) override;
    [[nodiscard]] common::Result<void> shutdown() override;

    [[nodiscard]] bool isInitialized() const noexcept { return isInitialized_.load(); }
    [[nodiscard]] const RenderConfig& getConfig() const noexcept { return config_; }
    [[nodiscard]] uint64_t getFramesPresented() const noexcept { return framesPresented_.load(); }
    [[nodiscard]] uint64_t getFramesDropped() const noexcept { return framesDropped_.load(); }
    [[nodiscard]] double getPresentLatencyUs() const noexcept { return lastPresentLatencyUs_; }

#if defined(_WIN32)
    [[nodiscard]] HWND getHwnd() const noexcept { return hwnd_; }
    [[nodiscard]] ID3D11Device* getD3D11Device() const noexcept { return device_.Get(); }
#endif

private:
#if defined(_WIN32)
    [[nodiscard]] common::Result<void> createRendererWindow();
    [[nodiscard]] common::Result<void> createSwapChain();
    [[nodiscard]] common::Result<void> initVideoProcessor();
    void renderLetterboxed(ID3D11Texture2D* sourceTexture, uint32_t srcWidth, uint32_t srcHeight);
#endif
    void cleanup() noexcept;

    mutable std::mutex mutex_;
    RenderConfig config_;
    std::atomic<bool> isInitialized_{false};
    std::atomic<uint64_t> framesPresented_{0};
    std::atomic<uint64_t> framesDropped_{0};
    double lastPresentLatencyUs_{0.0};

    platform::windows::HighResolutionTimer timer_;

#if defined(_WIN32)
    HWND hwnd_{nullptr};
    bool ownsHwnd_{false};
    bool tearingSupported_{false};

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv_;

    // D3D11 Video Processor for hardware NV12 -> RGB blit and scaling
    Microsoft::WRL::ComPtr<ID3D11VideoDevice> videoDevice_;
    Microsoft::WRL::ComPtr<ID3D11VideoContext> videoContext_;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> vpEnum_;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessor> videoProcessor_;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> vpOutputView_;
    bool useVideoProcessor_{false};
#endif
};

} // namespace mist::render
