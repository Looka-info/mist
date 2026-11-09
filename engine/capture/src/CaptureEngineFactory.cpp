#include "mist/capture/CaptureEngineFactory.h"
#include "mist/capture/DxgiCaptureEngine.h"
#include "mist/capture/WgcCaptureEngine.h"
#include "mist/common/Logger.h"

namespace mist::capture {

std::vector<CaptureBackend> CaptureEngineFactory::queryAvailableBackends() {
    std::vector<CaptureBackend> backends;

#if defined(_WIN32)
    backends.push_back(CaptureBackend::DxgiDuplication);
    if (WgcCaptureEngine::isSupported()) {
        backends.push_back(CaptureBackend::WindowsGraphicsCapture);
    }
#endif

    backends.push_back(CaptureBackend::Synthetic);
    return backends;
}

bool CaptureEngineFactory::isBackendSupported(CaptureBackend backend) {
    switch (backend) {
        case CaptureBackend::DxgiDuplication:
#if defined(_WIN32)
            return true;
#else
            return false;
#endif
        case CaptureBackend::WindowsGraphicsCapture:
            return WgcCaptureEngine::isSupported();
        case CaptureBackend::Synthetic:
            return true;
    }
    return false;
}

std::unique_ptr<ICaptureEngine> CaptureEngineFactory::create(CaptureBackend preferredBackend) {
    switch (preferredBackend) {
        case CaptureBackend::WindowsGraphicsCapture:
            return std::make_unique<WgcCaptureEngine>();
        case CaptureBackend::DxgiDuplication:
        default:
            return std::make_unique<DxgiCaptureEngine>();
    }
}

std::pair<std::unique_ptr<ICaptureEngine>, common::Result<void>> CaptureEngineFactory::createAndInitialize(
    const CaptureConfig& config,
    CaptureBackend preferredBackend,
    bool allowFallback
) {
    if (preferredBackend == CaptureBackend::WindowsGraphicsCapture) {
        auto engine = std::make_unique<WgcCaptureEngine>();
        auto res = engine->initialize(config);
        if (res.hasValue()) {
            return {std::move(engine), common::Result<void>::success()};
        }
        if (!allowFallback) {
            return {nullptr, res};
        }
        MIST_LOG_WARN(common::SubsystemId::Capture,
                      "WGC capture initialize failed, falling back to DXGI Duplication: " + res.error().message());
        auto dxgiEngine = std::make_unique<DxgiCaptureEngine>();
        auto dxgiRes = dxgiEngine->initialize(config);
        if (dxgiRes.hasValue()) {
            return {std::move(dxgiEngine), common::Result<void>::success()};
        }
        return {nullptr, dxgiRes};
    }

    // Default: try DXGI first
    auto dxgiEngine = std::make_unique<DxgiCaptureEngine>();
    auto dxgiRes = dxgiEngine->initialize(config);
    if (dxgiRes.hasValue()) {
        return {std::move(dxgiEngine), common::Result<void>::success()};
    }

    if (!allowFallback) {
        return {nullptr, dxgiRes};
    }

    // Fallback to WGC (essential for hybrid laptops, multi-GPU and secondary displays)
    MIST_LOG_WARN(common::SubsystemId::Capture,
                  "DXGI Desktop Duplication initialize failed (" + dxgiRes.error().message() +
                  "), falling back to Windows.Graphics.Capture...");
    auto wgcEngine = std::make_unique<WgcCaptureEngine>();
    auto wgcRes = wgcEngine->initialize(config);
    if (wgcRes.hasValue()) {
        MIST_LOG_INFO(common::SubsystemId::Capture,
                      "Windows.Graphics.Capture fallback initialized successfully.");
        return {std::move(wgcEngine), common::Result<void>::success()};
    }

    return {nullptr, wgcRes};
}

} // namespace mist::capture
