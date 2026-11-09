#pragma once

#include "mist/capture/ICaptureEngine.h"
#include "mist/common/Result.h"
#include <cstdint>
#include <cstddef>
#include <vector>

namespace mist::capture {

enum class ToneMappingOperator : uint8_t {
    Reinhard = 0,
    AcesFilmic,
    Hable
};

struct ToneMappingConfig {
    ToneMappingOperator op{ToneMappingOperator::AcesFilmic};
    float maxMasteringLuminanceNits{1000.0f};
    float targetSdrWhiteLuminanceNits{100.0f};
    float exposure{1.0f};
};

class HdrToneMapper {
public:
    // SMPTE ST 2084 Perceptual Quantizer (PQ) EOTF: Normalized PQ [0..1] -> Absolute Luminance [0..10,000 cd/m^2]
    [[nodiscard]] static float pqToLinearNits(float pq) noexcept;

    // Inverse SMPTE ST 2084 EOTF: Absolute Luminance [0..10,000 cd/m^2] -> Normalized PQ [0..1]
    [[nodiscard]] static float linearNitsToPq(float nits) noexcept;

    // ITU-R BT.709 OETF (Linear [0..1] -> Non-linear gamma encoded [0..1])
    [[nodiscard]] static float linearToBt709(float linear) noexcept;

    // Inverse ITU-R BT.709 (Non-linear [0..1] -> Linear [0..1])
    [[nodiscard]] static float bt709ToLinear(float nonLinear) noexcept;

    // Rec.2020 -> Rec.709 Color Gamut Matrix Conversion
    static void transformBt2020ToBt709(float r2020, float g2020, float b2020,
                                       float& outR, float& outG, float& outB) noexcept;

    // Rec.709 -> Rec.2020 Color Gamut Matrix Conversion
    static void transformBt709ToBt2020(float r709, float g709, float b709,
                                       float& outR, float& outG, float& outB) noexcept;

    // Tone mapping scalar operator
    [[nodiscard]] static float toneMapScalar(float linearLuminance, const ToneMappingConfig& config) noexcept;

    // End-to-end HDR (BT.2020 PQ normalized [0..1]) to SDR (BT.709 gamma [0..1]) tone mapping
    static void toneMapHdrRgbToSdrRgb(float rHdr, float gHdr, float bHdr,
                                      const ToneMappingConfig& config,
                                      float& outR, float& outG, float& outB) noexcept;

    // 10-bit P010 format MSB-alignment helpers (upper 10 bits carry data, lower 6 bits zero)
    [[nodiscard]] static constexpr uint16_t packP010Sample(uint16_t val10Bit) noexcept {
        return static_cast<uint16_t>((val10Bit & 0x03FF) << 6);
    }

    [[nodiscard]] static constexpr uint16_t unpackP010Sample(uint16_t packed16Bit) noexcept {
        return static_cast<uint16_t>((packed16Bit >> 6) & 0x03FF);
    }

    // Convert DXGI R10G10B10A2 packed buffer to P010 semi-planar buffer (Y and UV planes)
    [[nodiscard]] static common::Result<void> convertR10G10B10A2ToP010(
        const uint32_t* srcR10,
        uint16_t* dstY,
        uint16_t* dstUv,
        uint32_t width,
        uint32_t height
    ) noexcept;

    // Tone-map 10-bit HDR P010 semi-planar buffer to 8-bit SDR NV12 buffer
    [[nodiscard]] static common::Result<void> toneMapP010ToNv12(
        const uint16_t* srcY,
        const uint16_t* srcUv,
        uint8_t* dstY,
        uint8_t* dstUv,
        uint32_t width,
        uint32_t height,
        const ToneMappingConfig& config
    ) noexcept;
};

} // namespace mist::capture
