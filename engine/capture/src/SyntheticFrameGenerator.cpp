#include "mist/capture/SyntheticFrameGenerator.h"
#include "mist/common/Logger.h"
#include <cmath>

namespace mist::capture {

SyntheticFrameGenerator::SyntheticFrameGenerator() noexcept = default;

#if defined(_WIN32)
common::Result<void> SyntheticFrameGenerator::initialize(ID3D11Device* device, ID3D11DeviceContext* context, uint32_t width, uint32_t height) {
    if (!device || !context) {
        return common::Error(common::SubsystemId::Capture, common::ErrorCode::InvalidArgument, "Device or context is null");
    }

    device_ = device;
    context_ = context;
    width_ = width;
    height_ = height;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width_;
    desc.Height = height_;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    HRESULT hr = device_->CreateTexture2D(&desc, nullptr, texture_.GetAddressOf());
    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Capture, "CreateTexture2D for synthetic frame failed");
    }

    hr = device_->CreateRenderTargetView(reinterpret_cast<ID3D11Resource*>(texture_.Get()), nullptr, rtv_.GetAddressOf());
    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Capture, "CreateRenderTargetView failed");
    }

    timer_.reset();
    MIST_LOG_INFO(common::SubsystemId::Capture, "Synthetic GPU frame generator initialized (" +
                  std::to_string(width_) + "x" + std::to_string(height_) + ")");
    return common::Result<void>::success();
}

common::Result<CapturedFrame> SyntheticFrameGenerator::generateFrame(uint64_t frameNumber, uint64_t timestampNs) {
    if (!context_ || !rtv_ || !texture_) {
        return common::Error(common::SubsystemId::Capture, common::ErrorCode::NotInitialized, "SyntheticFrameGenerator not initialized");
    }

    // Generate dynamic color wave based on frame index
    float t = static_cast<float>(frameNumber % 120) / 120.0f;
    float r = 0.5f + 0.5f * std::sin(t * 6.2831853f);
    float g = 0.5f + 0.5f * std::sin(t * 6.2831853f + 2.0943951f);
    float b = 0.5f + 0.5f * std::sin(t * 6.2831853f + 4.1887902f);

    const float clearColor[4] = { r, g, b, 1.0f };
    context_->ClearRenderTargetView(rtv_.Get(), clearColor);

    CapturedFrame frame{
        .width = width_,
        .height = height_,
        .format = PixelFormat::B8G8R8A8_UNORM,
        .timestampNs = timestampNs > 0 ? timestampNs : timer_.elapsedNanoseconds(),
        .frameNumber = frameNumber,
        .nativeResource = texture_.Get()
    };

    return frame;
}
#endif

} // namespace mist::capture
