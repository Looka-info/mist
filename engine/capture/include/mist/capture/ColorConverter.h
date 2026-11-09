#pragma once

#include "mist/common/Result.h"
#include "mist/platform/windows/PlatformWindows.h"
#include <cstdint>

#if defined(_WIN32)
#include <d3d11.h>
#include <wrl/client.h>
#endif

namespace mist::capture {

class ColorConverter {
public:
    ColorConverter() noexcept;
    ~ColorConverter() = default;

#if defined(_WIN32)
    [[nodiscard]] common::Result<void> initialize(ID3D11Device* device, ID3D11DeviceContext* context, uint32_t width, uint32_t height);
    [[nodiscard]] common::Result<ID3D11Texture2D*> convertBgraToNv12(ID3D11Texture2D* bgraTexture);
    [[nodiscard]] common::Result<ID3D11Texture2D*> convertHdrToP010(ID3D11Texture2D* hdrTexture);
    [[nodiscard]] common::Result<ID3D11Texture2D*> convertHdrToSdrNv12(ID3D11Texture2D* hdrTexture);
    [[nodiscard]] double getLastConversionTimeMs() const noexcept { return lastConversionTimeMs_; }
    [[nodiscard]] bool isHdrSupported() const noexcept { return supportsR10Input_ && supportsP010Output_; }
    [[nodiscard]] bool isP010Supported() const noexcept { return supportsP010Output_; }
#endif

private:
    uint32_t width_{0};
    uint32_t height_{0};
    double lastConversionTimeMs_{0.0};
    platform::windows::HighResolutionTimer timer_;

#if defined(_WIN32)
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D11VideoDevice> videoDevice_;
    Microsoft::WRL::ComPtr<ID3D11VideoContext> videoContext_;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> vpEnum_;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessor> videoProcessor_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> nv12Textures_[2];
    Microsoft::WRL::ComPtr<ID3D11Texture2D> p010Textures_[2];
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> inputViews_[2];
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> outputViews_[2];
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> p010OutputViews_[2];
    size_t currentSlot_{0};
    bool useVideoProcessor_{false};
    bool supportsR10Input_{false};
    bool supportsP010Output_{false};
#endif
};

} // namespace mist::capture
