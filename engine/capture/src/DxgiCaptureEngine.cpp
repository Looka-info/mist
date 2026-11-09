#include "mist/capture/DxgiCaptureEngine.h"
#include "mist/common/Logger.h"

namespace mist::capture {

DxgiCaptureEngine::DxgiCaptureEngine() noexcept = default;

DxgiCaptureEngine::~DxgiCaptureEngine() {
    (void)stop();
}

common::Result<void> DxgiCaptureEngine::initialize(const CaptureConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;

#if defined(_WIN32)
    auto displays = platform::windows::D3D11DeviceManager::enumerateDisplays();
    if (config_.displayIndex >= displays.size()) {
        return common::Error(common::SubsystemId::Capture, common::ErrorCode::NotFound,
                             "Display index " + std::to_string(config_.displayIndex) + " not found. Total displays: " + std::to_string(displays.size()));
    }
    displayInfo_ = displays[config_.displayIndex];

    MIST_LOG_INFO(common::SubsystemId::Capture, "Initializing DXGI Desktop Duplication on display " +
                  std::to_string(displayInfo_.index) + " (" + displayInfo_.deviceName + ") via " + displayInfo_.adapterName);

    // Create D3D11 Device on the target adapter
    auto devRes = platform::windows::D3D11DeviceManager::createDevice(displayInfo_.adapterIndex);
    if (!devRes) {
        return devRes.error();
    }
    device_ = devRes.value().first;
    context_ = devRes.value().second;

    // Get output for this display
    auto outRes = platform::windows::D3D11DeviceManager::getOutputForDisplay(displayInfo_.index);
    if (!outRes) {
        return outRes.error();
    }
    output_ = outRes.value();

    // Initialize duplication interface
    auto dupRes = initDuplication();
    if (!dupRes) {
        return dupRes;
    }

    // Allocate GPU-resident staging/shared texture
    auto texRes = recreateSharedTexture(displayInfo_.width, displayInfo_.height);
    if (!texRes) {
        return texRes;
    }

    timer_.reset();
    return common::Result<void>::success();
#else
    return common::Error(common::SubsystemId::Capture, common::ErrorCode::NotImplemented, "DXGI capture only supported on Windows");
#endif
}

#if defined(_WIN32)
common::Result<void> DxgiCaptureEngine::initDuplication() {
    duplication_.Reset();

    if (!output_ || !device_) {
        return common::Error(common::SubsystemId::Capture, common::ErrorCode::NotInitialized, "Output or Device is null");
    }

    HRESULT hr = output_->DuplicateOutput(device_.Get(), duplication_.GetAddressOf());
    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE) {
            return common::Error(common::SubsystemId::Capture, common::ErrorCode::ResourceExhausted,
                                 "Desktop duplication already active or running in non-interactive session");
        }
        if (hr == DXGI_ERROR_UNSUPPORTED) {
            return common::Error(common::SubsystemId::Capture, common::ErrorCode::FormatNotSupported,
                                 "Desktop duplication unsupported on this adapter/driver");
        }
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Capture, "DuplicateOutput failed");
    }

    MIST_LOG_INFO(common::SubsystemId::Capture, "DXGI Output Duplication successfully acquired.");
    return common::Result<void>::success();
}

common::Result<void> DxgiCaptureEngine::recreateSharedTexture(uint32_t width, uint32_t height, DXGI_FORMAT format) {
    if (!device_) {
        return common::Error(common::SubsystemId::Capture, common::ErrorCode::NotInitialized, "Device is null");
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0; // Pure GPU memory, no CPU readback!
    desc.MiscFlags = 0;

    sharedTexture_.Reset();
    HRESULT hr = device_->CreateTexture2D(&desc, nullptr, sharedTexture_.GetAddressOf());
    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Capture, "CreateTexture2D for sharedTexture failed");
    }

    displayInfo_.width = width;
    displayInfo_.height = height;
    currentFormat_ = static_cast<uint32_t>(format);
    return common::Result<void>::success();
}
#endif

common::Result<void> DxgiCaptureEngine::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    isCapturing_ = true;
    frameCounter_ = 0;
    timer_.reset();
    MIST_LOG_INFO(common::SubsystemId::Capture, "DXGI Capture Engine started.");
    return common::Result<void>::success();
}

common::Result<void> DxgiCaptureEngine::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    isCapturing_ = false;

#if defined(_WIN32)
    if (frameAcquired_ && duplication_) {
        (void)duplication_->ReleaseFrame();
        frameAcquired_ = false;
    }
    duplication_.Reset();
#endif

    MIST_LOG_INFO(common::SubsystemId::Capture, "DXGI Capture Engine stopped.");
    return common::Result<void>::success();
}

common::Result<CapturedFrame> DxgiCaptureEngine::acquireNextFrame(uint32_t timeoutMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isCapturing_) {
        return common::Error(common::SubsystemId::Capture, common::ErrorCode::InvalidState, "Capture engine is not running");
    }

#if defined(_WIN32)
    if (!duplication_) {
        auto recRes = initDuplication();
        if (!recRes) {
            return recRes.error();
        }
    }

    if (frameAcquired_) {
        (void)duplication_->ReleaseFrame();
        frameAcquired_ = false;
    }

    DXGI_OUTDUPL_FRAME_INFO frameInfo{};
    Microsoft::WRL::ComPtr<IDXGIResource> desktopResource;

    HRESULT hr = duplication_->AcquireNextFrame(timeoutMs, &frameInfo, desktopResource.GetAddressOf());
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        return common::Error(common::SubsystemId::Capture, common::ErrorCode::Timeout, "DXGI AcquireNextFrame timeout");
    }

    if (hr == DXGI_ERROR_ACCESS_LOST) {
        MIST_LOG_WARN(common::SubsystemId::Capture, "DXGI access lost, attempting recovery...");
        auto recRes = initDuplication();
        if (!recRes) {
            return recRes.error();
        }
        return common::Error(common::SubsystemId::Capture, common::ErrorCode::InvalidState, "DXGI access lost, recovered");
    }

    if (FAILED(hr)) {
        if (device_) {
            HRESULT removeReason = device_->GetDeviceRemovedReason();
            if (FAILED(removeReason)) {
                MIST_LOG_ERROR(common::SubsystemId::Capture, "D3D11 Device removed: " + std::to_string(removeReason));
                return platform::windows::hresultToMistError(removeReason, common::SubsystemId::Capture, "D3D11 Device removed");
            }
        }
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Capture, "AcquireNextFrame failed");
    }

    frameAcquired_ = true;

    // Handle cursor pointer updates if available
    if (frameInfo.PointerPosition.Visible) {
        pointerShape_.isVisible = true;
        pointerShape_.xPosition = frameInfo.PointerPosition.Position.x;
        pointerShape_.yPosition = frameInfo.PointerPosition.Position.y;
    }

    if (frameInfo.PointerShapeBufferSize > 0) {
        pointerShape_.shapeBuffer.resize(frameInfo.PointerShapeBufferSize);
        UINT bufferSizeRequired = 0;
        DXGI_OUTDUPL_POINTER_SHAPE_INFO shapeInfo{};
        HRESULT shapeHr = duplication_->GetFramePointerShape(
            frameInfo.PointerShapeBufferSize,
            pointerShape_.shapeBuffer.data(),
            &bufferSizeRequired,
            &shapeInfo
        );
        if (SUCCEEDED(shapeHr)) {
            pointerShape_.type = shapeInfo.Type;
            pointerShape_.width = shapeInfo.Width;
            pointerShape_.height = shapeInfo.Height;
            pointerShape_.pitch = shapeInfo.Pitch;
            pointerShape_.xHotSpot = shapeInfo.HotSpot.x;
            pointerShape_.yHotSpot = shapeInfo.HotSpot.y;
        }
    }

    // Retrieve desktop D3D11 texture
    Microsoft::WRL::ComPtr<ID3D11Texture2D> desktopTexture;
    hr = desktopResource.As(&desktopTexture);
    if (FAILED(hr)) {
        (void)duplication_->ReleaseFrame();
        frameAcquired_ = false;
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Capture, "Failed to cast desktop resource to ID3D11Texture2D");
    }

    D3D11_TEXTURE2D_DESC desc{};
    desktopTexture->GetDesc(&desc);

    // If resolution or format changed, recreate shared GPU texture
    if (desc.Width != displayInfo_.width || desc.Height != displayInfo_.height || currentFormat_ != static_cast<uint32_t>(desc.Format)) {
        MIST_LOG_INFO(common::SubsystemId::Capture, "Display resolution/format changed to " +
                      std::to_string(desc.Width) + "x" + std::to_string(desc.Height) +
                      " (format " + std::to_string(static_cast<uint32_t>(desc.Format)) + ")");
        auto recRes = recreateSharedTexture(desc.Width, desc.Height, desc.Format);
        if (!recRes) {
            (void)duplication_->ReleaseFrame();
            frameAcquired_ = false;
            return recRes.error();
        }
    }

    // Zero-CPU-readback: Perform pure GPU CopyResource into sharedTexture_
    context_->CopyResource(sharedTexture_.Get(), desktopTexture.Get());

    // Release DXGI frame immediately so OS is free to render next frame
    (void)duplication_->ReleaseFrame();
    frameAcquired_ = false;

    ++frameCounter_;
    uint64_t timestampNs = timer_.elapsedNanoseconds();

    PixelFormat outFormat = PixelFormat::B8G8R8A8_UNORM;
    ColorSpace outColorSpace = ColorSpace::BT709_SDR;
    uint32_t bitDepth = 8;

    if (desc.Format == DXGI_FORMAT_R10G10B10A2_UNORM) {
        outFormat = PixelFormat::R10G10B10A2_UNORM;
        outColorSpace = ColorSpace::BT2020_PQ;
        bitDepth = 10;
    } else if (desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT) {
        outFormat = PixelFormat::R10G10B10A2_UNORM;
        outColorSpace = ColorSpace::Linear_ScRGB;
        bitDepth = 16;
    }

    CapturedFrame frame{
        .width = displayInfo_.width,
        .height = displayInfo_.height,
        .format = outFormat,
        .colorSpace = outColorSpace,
        .bitDepth = bitDepth,
        .timestampNs = timestampNs,
        .frameNumber = frameCounter_,
        .nativeResource = sharedTexture_.Get()
    };

    return frame;
#else
    return common::Error(common::SubsystemId::Capture, common::ErrorCode::NotImplemented, "DXGI capture only supported on Windows");
#endif
}

common::Result<void> DxgiCaptureEngine::releaseFrame(const CapturedFrame&) {
    // Shared texture is managed in the bounded internal pool; no-op release
    return common::Result<void>::success();
}

bool DxgiCaptureEngine::isCapturing() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return isCapturing_;
}

} // namespace mist::capture
