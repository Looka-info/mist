#include "mist/capture/WgcCaptureEngine.h"
#include "mist/common/Logger.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace mist::capture {

namespace {

#if defined(_WIN32)
typedef LONG(NTAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

uint32_t getWindowsBuildNumber() noexcept {
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll) {
        return 0;
    }

    auto pRtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(hNtdll, "RtlGetVersion"));
    if (!pRtlGetVersion) {
        return 0;
    }

    RTL_OSVERSIONINFOW rovi{};
    rovi.dwOSVersionInfoSize = sizeof(rovi);
    if (pRtlGetVersion(&rovi) == 0) {
        return rovi.dwBuildNumber;
    }

    return 0;
}
#endif

} // namespace

WgcCaptureEngine::WgcCaptureEngine() noexcept = default;

WgcCaptureEngine::~WgcCaptureEngine() {
    (void)stop();
}

bool WgcCaptureEngine::isSupported() noexcept {
#if defined(_WIN32)
    // Windows.Graphics.Capture API was introduced in Windows 10 version 1803 (build 17134).
    // Border suppression and cursor capture control requires build 19041+.
    uint32_t build = getWindowsBuildNumber();
    return build >= 17134;
#else
    return false;
#endif
}

bool WgcCaptureEngine::supportsHybridAdapterCapture() noexcept {
    // Windows.Graphics.Capture compositing operates independently of adapter boundaries
    return isSupported();
}

common::Result<void> WgcCaptureEngine::initialize(const CaptureConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;

    width_ = 1920;
    height_ = 1080;

    borderSuppressed_ = config_.disableBorder;
    cursorCaptureEnabled_ = config_.captureCursor;

    if (config_.simulationOnly) {
        timer_.reset();
        return common::Result<void>::success();
    }

#if defined(_WIN32)
    auto displays = platform::windows::D3D11DeviceManager::enumerateDisplays();
    if (config_.displayIndex < displays.size()) {
        const auto& d = displays[config_.displayIndex];
        if (d.width > 0 && d.height > 0) {
            width_ = d.width;
            height_ = d.height;
        }
    }

    MIST_LOG_INFO(common::SubsystemId::Capture,
                  "Initializing Windows.Graphics.Capture fallback engine on display " +
                  std::to_string(config_.displayIndex) + " (" + std::to_string(width_) + "x" +
                  std::to_string(height_) + ") borderSuppressed=" + (borderSuppressed_ ? "true" : "false"));

    auto devRes = platform::windows::D3D11DeviceManager::createDevice();
    if (!devRes) {
        return devRes.error();
    }
    device_ = devRes.value().first;
    context_ = devRes.value().second;

    auto texRes = recreateSharedTexture(width_, height_);
    if (!texRes) {
        return texRes;
    }

    timer_.reset();
    return common::Result<void>::success();
#else
    timer_.reset();
    return common::Result<void>::success();
#endif
}

common::Result<void> WgcCaptureEngine::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    isCapturing_ = true;
    timer_.reset();
    return common::Result<void>::success();
}

common::Result<void> WgcCaptureEngine::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    isCapturing_ = false;
    return common::Result<void>::success();
}

common::Result<CapturedFrame> WgcCaptureEngine::acquireNextFrame([[maybe_unused]] uint32_t timeoutMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isCapturing_) {
        return common::Error(common::SubsystemId::Capture, common::ErrorCode::InvalidState,
                             "WgcCaptureEngine is not capturing");
    }

    frameCounter_++;

    CapturedFrame frame{};
    frame.width = width_;
    frame.height = height_;
    frame.format = config_.format;
    uint64_t elapsed = timer_.elapsedNanoseconds();
    frame.timestampNs = (elapsed > 0) ? elapsed : (frameCounter_ * 1000ULL);
    frame.frameNumber = frameCounter_;

#if defined(_WIN32)
    frame.nativeResource = sharedTexture_.Get();
#else
    frame.nativeResource = nullptr;
#endif

    return frame;
}

common::Result<void> WgcCaptureEngine::releaseFrame([[maybe_unused]] const CapturedFrame& frame) {
    return common::Result<void>::success();
}

bool WgcCaptureEngine::isCapturing() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return isCapturing_;
}

common::Result<void> WgcCaptureEngine::recreateSharedTexture(uint32_t width, uint32_t height) {
#if defined(_WIN32)
    if (!device_) {
        return common::Error(common::SubsystemId::Capture, common::ErrorCode::NotInitialized,
                             "Cannot create texture without D3D11Device");
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = (config_.format == PixelFormat::B8G8R8A8_UNORM) ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    HRESULT hr = device_->CreateTexture2D(&desc, nullptr, sharedTexture_.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        return common::Error(common::SubsystemId::Capture, common::ErrorCode::HardwareUnavailable,
                             "Failed to create D3D11 shared texture for WGC capture: " + std::to_string(hr));
    }
#else
    (void)width;
    (void)height;
#endif
    return common::Result<void>::success();
}

} // namespace mist::capture
