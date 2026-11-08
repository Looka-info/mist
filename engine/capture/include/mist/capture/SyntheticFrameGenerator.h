#pragma once

#include "ICaptureEngine.h"
#include "mist/common/Result.h"
#include "mist/platform/windows/PlatformWindows.h"
#include <cstdint>

#if defined(_WIN32)
#include <d3d11.h>
#include <wrl/client.h>
#endif

namespace mist::capture {

class SyntheticFrameGenerator {
public:
    SyntheticFrameGenerator() noexcept;
    ~SyntheticFrameGenerator() = default;

#if defined(_WIN32)
    [[nodiscard]] common::Result<void> initialize(ID3D11Device* device, ID3D11DeviceContext* context, uint32_t width, uint32_t height);
    [[nodiscard]] common::Result<CapturedFrame> generateFrame(uint64_t frameNumber, uint64_t timestampNs);
#endif

private:
    uint32_t width_{1920};
    uint32_t height_{1080};
    platform::windows::HighResolutionTimer timer_;

#if defined(_WIN32)
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> patternTexture_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv_;
#endif
};

} // namespace mist::capture
