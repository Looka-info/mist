#pragma once

#include "mist/common/Result.h"
#include <cstdint>
#include <memory>
#include <span>

namespace mist::capture {

enum class PixelFormat : uint8_t {
    B8G8R8A8_UNORM = 0,
    R8G8B8A8_UNORM,
    NV12
};

struct CaptureConfig {
    uint32_t displayIndex{0};
    uint32_t targetFps{60};
    PixelFormat format{PixelFormat::NV12};
    bool captureCursor{true};
};

struct CapturedFrame {
    uint32_t width{0};
    uint32_t height{0};
    PixelFormat format{PixelFormat::NV12};
    uint64_t timestampNs{0};
    uint64_t frameNumber{0};
    void* nativeResource{nullptr}; // e.g. ID3D11Texture2D* in Milestone 1
};

class ICaptureEngine {
public:
    virtual ~ICaptureEngine() = default;

    [[nodiscard]] virtual common::Result<void> initialize(const CaptureConfig& config) = 0;
    [[nodiscard]] virtual common::Result<void> start() = 0;
    [[nodiscard]] virtual common::Result<void> stop() = 0;
    [[nodiscard]] virtual common::Result<CapturedFrame> acquireNextFrame(uint32_t timeoutMs) = 0;
    [[nodiscard]] virtual common::Result<void> releaseFrame(const CapturedFrame& frame) = 0;
    [[nodiscard]] virtual bool isCapturing() const noexcept = 0;
};

} // namespace mist::capture
