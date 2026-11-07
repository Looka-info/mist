#include "mist/capture/ColorConverter.h"
#include "mist/common/Logger.h"

namespace mist::capture {

ColorConverter::ColorConverter() noexcept = default;

#if defined(_WIN32)
common::Result<void> ColorConverter::initialize(ID3D11Device* device, ID3D11DeviceContext* context, uint32_t width, uint32_t height) {
    if (!device || !context) {
        return common::Error(common::SubsystemId::Capture, common::ErrorCode::InvalidArgument, "Device or Context is null");
    }

    device_ = device;
    context_ = context;
    width_ = width;
    height_ = height;

    HRESULT hr = device_->QueryInterface(__uuidof(ID3D11VideoDevice), reinterpret_cast<void**>(videoDevice_.GetAddressOf()));
    if (FAILED(hr)) {
        MIST_LOG_WARN(common::SubsystemId::Capture, "ID3D11VideoDevice not supported by D3D11 device");
        useVideoProcessor_ = false;
        return common::Result<void>::success();
    }

    hr = context_->QueryInterface(__uuidof(ID3D11VideoContext), reinterpret_cast<void**>(videoContext_.GetAddressOf()));
    if (FAILED(hr)) {
        MIST_LOG_WARN(common::SubsystemId::Capture, "ID3D11VideoContext not supported");
        useVideoProcessor_ = false;
        return common::Result<void>::success();
    }

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC contentDesc{};
    contentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    contentDesc.InputFrameRate.Numerator = 60;
    contentDesc.InputFrameRate.Denominator = 1;
    contentDesc.InputWidth = width_;
    contentDesc.InputHeight = height_;
    contentDesc.OutputWidth = width_;
    contentDesc.OutputHeight = height_;
    contentDesc.OutputFrameRate.Numerator = 60;
    contentDesc.OutputFrameRate.Denominator = 1;
    contentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    hr = videoDevice_->CreateVideoProcessorEnumerator(&contentDesc, vpEnum_.GetAddressOf());
    if (FAILED(hr)) {
        MIST_LOG_WARN(common::SubsystemId::Capture, "CreateVideoProcessorEnumerator failed");
        useVideoProcessor_ = false;
        return common::Result<void>::success();
    }

    UINT flags = 0;
    hr = vpEnum_->CheckVideoProcessorFormat(DXGI_FORMAT_B8G8R8A8_UNORM, &flags);
    if (FAILED(hr) || !(flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT)) {
        MIST_LOG_WARN(common::SubsystemId::Capture, "BGRA8 input format not supported by Video Processor");
        useVideoProcessor_ = false;
        return common::Result<void>::success();
    }

    flags = 0;
    hr = vpEnum_->CheckVideoProcessorFormat(DXGI_FORMAT_NV12, &flags);
    if (FAILED(hr) || !(flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT)) {
        MIST_LOG_WARN(common::SubsystemId::Capture, "NV12 output format not supported by Video Processor");
        useVideoProcessor_ = false;
        return common::Result<void>::success();
    }

    hr = videoDevice_->CreateVideoProcessor(vpEnum_.Get(), 0, videoProcessor_.GetAddressOf());
    if (FAILED(hr)) {
        MIST_LOG_WARN(common::SubsystemId::Capture, "CreateVideoProcessor failed");
        useVideoProcessor_ = false;
        return common::Result<void>::success();
    }

    // Allocate bounded NV12 texture pool (2 textures)
    D3D11_TEXTURE2D_DESC nv12Desc{};
    nv12Desc.Width = width_;
    nv12Desc.Height = height_;
    nv12Desc.MipLevels = 1;
    nv12Desc.ArraySize = 1;
    nv12Desc.Format = DXGI_FORMAT_NV12;
    nv12Desc.SampleDesc.Count = 1;
    nv12Desc.SampleDesc.Quality = 0;
    nv12Desc.Usage = D3D11_USAGE_DEFAULT;
    nv12Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    nv12Desc.CPUAccessFlags = 0;
    nv12Desc.MiscFlags = 0;

    for (int i = 0; i < 2; ++i) {
        hr = device_->CreateTexture2D(&nv12Desc, nullptr, nv12Textures_[i].GetAddressOf());
        if (FAILED(hr)) {
            return platform::windows::hresultToMistError(hr, common::SubsystemId::Capture, "CreateTexture2D for NV12 failed");
        }

        D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outViewDesc{};
        outViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
        outViewDesc.Texture2D.MipSlice = 0;
        hr = videoDevice_->CreateVideoProcessorOutputView(
            reinterpret_cast<ID3D11Resource*>(nv12Textures_[i].Get()),
            vpEnum_.Get(),
            &outViewDesc,
            outputViews_[i].GetAddressOf()
        );
        if (FAILED(hr)) {
            return platform::windows::hresultToMistError(hr, common::SubsystemId::Capture, "CreateVideoProcessorOutputView failed");
        }
    }

    useVideoProcessor_ = true;
    MIST_LOG_INFO(common::SubsystemId::Capture, "Hardware D3D11 VideoProcessor initialized for BGRA-to-NV12 conversion.");
    return common::Result<void>::success();
}

common::Result<ID3D11Texture2D*> ColorConverter::convertBgraToNv12(ID3D11Texture2D* bgraTexture) {
    if (!useVideoProcessor_ || !bgraTexture) {
        // Direct pass-through if VideoProcessor not active
        return bgraTexture;
    }

    timer_.reset();

    currentSlot_ = (currentSlot_ + 1) % 2;
    auto targetNv12 = nv12Textures_[currentSlot_].Get();
    auto outView = outputViews_[currentSlot_].Get();

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inViewDesc{};
    inViewDesc.FourCC = 0;
    inViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    inViewDesc.Texture2D.MipSlice = 0;
    inViewDesc.Texture2D.ArraySlice = 0;

    Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> inputView;
    HRESULT hr = videoDevice_->CreateVideoProcessorInputView(
        reinterpret_cast<ID3D11Resource*>(bgraTexture),
        vpEnum_.Get(),
        &inViewDesc,
        inputView.GetAddressOf()
    );
    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Capture, "CreateVideoProcessorInputView failed");
    }

    D3D11_VIDEO_PROCESSOR_STREAM stream{};
    stream.Enable = TRUE;
    stream.pInputSurface = inputView.Get();

    hr = videoContext_->VideoProcessorBlt(videoProcessor_.Get(), outView, 0, 1, &stream);
    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Capture, "VideoProcessorBlt failed");
    }

    lastConversionTimeMs_ = timer_.elapsedMilliseconds();
    return targetNv12;
}
#endif

} // namespace mist::capture
