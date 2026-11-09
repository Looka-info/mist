#pragma once

#include "mist/common/Result.h"
#include <cstdint>
#include <memory>
#include <span>

namespace mist::capture {

enum class PixelFormat : uint8_t {
    B8G8R8A8_UNORM = 0,
    R8G8B8A8_UNORM,
    NV12,
    P010,                // 10-bit YUV 4:2:0 semi-planar
    R10G10B10A2_UNORM    // 10-bit RGB (DXGI_FORMAT_R10G10B10A2_UNORM) for HDR10
};

enum class ColorSpace : uint8_t {
    BT709_SDR = 0,       // Standard SDR Rec.709 sRGB gamma
    BT2020_PQ,          // HDR10: Rec.2020 wide color gamut + SMPTE ST 2084 Perceptual Quantizer
    BT2020_HLG,         // Hybrid Log-Gamma
    Linear_ScRGB        // FP16 linear scRGB
};

enum class DynamicRangeMode : uint8_t {
    SDR = 0,
    HDR10,
    Auto
};

[[nodiscard]] constexpr std::string_view pixelFormatToString(PixelFormat fmt) noexcept {
    switch (fmt) {
        case PixelFormat::B8G8R8A8_UNORM: return "BGRA8";
        case PixelFormat::R8G8B8A8_UNORM: return "RGBA8";
        case PixelFormat::NV12: return "NV12 (8-bit YUV 4:2:0)";
        case PixelFormat::P010: return "P010 (10-bit YUV 4:2:0)";
        case PixelFormat::R10G10B10A2_UNORM: return "R10G10B10A2 (10-bit HDR)";
    }
    return "Unknown";
}

[[nodiscard]] constexpr std::string_view colorSpaceToString(ColorSpace cs) noexcept {
    switch (cs) {
        case ColorSpace::BT709_SDR: return "BT.709 (SDR)";
        case ColorSpace::BT2020_PQ: return "BT.2020 PQ (HDR10)";
        case ColorSpace::BT2020_HLG: return "BT.2020 HLG";
        case ColorSpace::Linear_ScRGB: return "Linear scRGB";
    }
    return "Unknown";
}

enum class CaptureBackend : uint8_t {
    DxgiDuplication = 0,
    WindowsGraphicsCapture = 1,
    Synthetic = 2
};

[[nodiscard]] constexpr std::string_view captureBackendToString(CaptureBackend b) noexcept {
    switch (b) {
        case CaptureBackend::DxgiDuplication: return "DXGI Desktop Duplication";
        case CaptureBackend::WindowsGraphicsCapture: return "Windows.Graphics.Capture";
        case CaptureBackend::Synthetic: return "Synthetic Generator";
    }
    return "Unknown";
}

struct CaptureConfig {
    uint32_t displayIndex{0};
    uint32_t targetFps{60};
    PixelFormat format{PixelFormat::NV12};
    ColorSpace colorSpace{ColorSpace::BT709_SDR};
    DynamicRangeMode dynamicRange{DynamicRangeMode::Auto};
    uint32_t bitDepth{8};
    bool captureCursor{true};
    bool disableBorder{true};
    bool simulationOnly{false};
};

struct CapturedFrame {
    uint32_t width{0};
    uint32_t height{0};
    PixelFormat format{PixelFormat::NV12};
    ColorSpace colorSpace{ColorSpace::BT709_SDR};
    uint32_t bitDepth{8};
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
    [[nodiscard]] virtual CaptureBackend getBackend() const noexcept = 0;
};

} // namespace mist::capture
