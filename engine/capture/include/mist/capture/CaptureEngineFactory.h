#pragma once

#include "mist/capture/ICaptureEngine.h"
#include <memory>
#include <vector>
#include <utility>

namespace mist::capture {

class CaptureEngineFactory {
public:
    // Query which capture backends are supported on the current host system
    [[nodiscard]] static std::vector<CaptureBackend> queryAvailableBackends();

    // Check if a specific backend is supported
    [[nodiscard]] static bool isBackendSupported(CaptureBackend backend);

    // Create an uninitialized instance of a capture engine
    [[nodiscard]] static std::unique_ptr<ICaptureEngine> create(
        CaptureBackend preferredBackend = CaptureBackend::DxgiDuplication
    );

    // Factory method that creates and attempts initialization
    // If allowFallback is true and initialization fails, automatically cascades:
    // DxgiDuplication -> WindowsGraphicsCapture
    [[nodiscard]] static std::pair<std::unique_ptr<ICaptureEngine>, common::Result<void>> createAndInitialize(
        const CaptureConfig& config,
        CaptureBackend preferredBackend = CaptureBackend::DxgiDuplication,
        bool allowFallback = true
    );
};

} // namespace mist::capture
