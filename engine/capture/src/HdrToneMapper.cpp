#include "mist/capture/HdrToneMapper.h"
#include <cmath>
#include <algorithm>

namespace mist::capture {

namespace {

// SMPTE ST 2084 constants
constexpr float M1 = 2610.0f / 16384.0f;
constexpr float M2 = 2523.0f / 32.0f; // (2523 / 4096) * 128
constexpr float C1 = 3424.0f / 4096.0f;
constexpr float C2 = 2413.0f / 128.0f; // (2413 / 4096) * 32
constexpr float C3 = 2392.0f / 128.0f; // (2392 / 4096) * 32

constexpr float ACES_A = 2.51f;
constexpr float ACES_B = 0.03f;
constexpr float ACES_C = 2.43f;
constexpr float ACES_D = 0.59f;
constexpr float ACES_E = 0.14f;

float clamp01(float v) noexcept {
    return std::clamp(v, 0.0f, 1.0f);
}

} // namespace

float HdrToneMapper::pqToLinearNits(float pq) noexcept {
    if (pq <= 0.0f) {
        return 0.0f;
    }
    float v = clamp01(pq);
    float vPow = std::pow(v, 1.0f / M2);
    float num = std::max(vPow - C1, 0.0f);
    float den = std::max(C2 - C3 * vPow, 1e-6f);
    return 10000.0f * std::pow(num / den, 1.0f / M1);
}

float HdrToneMapper::linearNitsToPq(float nits) noexcept {
    if (nits <= 0.0f) {
        return 0.0f;
    }
    float y = clamp01(nits / 10000.0f);
    float yPow = std::pow(y, M1);
    float num = C1 + C2 * yPow;
    float den = 1.0f + C3 * yPow;
    return clamp01(std::pow(num / den, M2));
}

float HdrToneMapper::linearToBt709(float linear) noexcept {
    float l = clamp01(linear);
    if (l < 0.018f) {
        return 4.5f * l;
    }
    return 1.099f * std::pow(l, 0.45f) - 0.099f;
}

float HdrToneMapper::bt709ToLinear(float nonLinear) noexcept {
    float v = clamp01(nonLinear);
    if (v < 0.081f) {
        return v / 4.5f;
    }
    return std::pow((v + 0.099f) / 1.099f, 1.0f / 0.45f);
}

void HdrToneMapper::transformBt2020ToBt709(float r2020, float g2020, float b2020,
                                          float& outR, float& outG, float& outB) noexcept {
    outR = clamp01(1.6605f * r2020 - 0.5876f * g2020 - 0.0728f * b2020);
    outG = clamp01(-0.1246f * r2020 + 1.1329f * g2020 - 0.0083f * b2020);
    outB = clamp01(-0.0182f * r2020 - 0.1006f * g2020 + 1.1187f * b2020);
}

void HdrToneMapper::transformBt709ToBt2020(float r709, float g709, float b709,
                                          float& outR, float& outG, float& outB) noexcept {
    outR = clamp01(0.6274f * r709 + 0.3293f * g709 + 0.0433f * b709);
    outG = clamp01(0.0691f * r709 + 0.9195f * g709 + 0.0114f * b709);
    outB = clamp01(0.0164f * r709 + 0.0880f * g709 + 0.8956f * b709);
}

float HdrToneMapper::toneMapScalar(float linearLuminance, const ToneMappingConfig& config) noexcept {
    float x = std::max(0.0f, linearLuminance * config.exposure);
    switch (config.op) {
        case ToneMappingOperator::Reinhard:
            return x / (1.0f + x);
        case ToneMappingOperator::AcesFilmic:
            return clamp01((x * (ACES_A * x + ACES_B)) / (x * (ACES_C * x + ACES_D) + ACES_E));
        case ToneMappingOperator::Hable: {
            constexpr float A = 0.15f;
            constexpr float B = 0.50f;
            constexpr float C = 0.10f;
            constexpr float D = 0.20f;
            constexpr float E = 0.02f;
            constexpr float F = 0.30f;
            auto hableCurve = [](float v) {
                return ((v * (A * v + C * B) + D * E) / (v * (A * v + B) + D * F)) - (E / F);
            };
            float whitePoint = hableCurve(config.maxMasteringLuminanceNits / config.targetSdrWhiteLuminanceNits);
            if (whitePoint <= 0.0f) whitePoint = 1.0f;
            return clamp01(hableCurve(x) / whitePoint);
        }
    }
    return x / (1.0f + x);
}

void HdrToneMapper::toneMapHdrRgbToSdrRgb(float rHdr, float gHdr, float bHdr,
                                          const ToneMappingConfig& config,
                                          float& outR, float& outG, float& outB) noexcept {
    // 1. Convert normalized PQ to absolute luminance in nits
    float rNits = pqToLinearNits(rHdr);
    float gNits = pqToLinearNits(gHdr);
    float bNits = pqToLinearNits(bHdr);

    // 2. Rec.2020 relative luminance
    float lumNits = 0.2627f * rNits + 0.6780f * gNits + 0.0593f * bNits;
    float normalizedLum = lumNits / std::max(1.0f, config.targetSdrWhiteLuminanceNits);

    // 3. Tone map luminance
    float mappedLum = toneMapScalar(normalizedLum, config);
    float scale = (normalizedLum > 1e-5f) ? (mappedLum / normalizedLum) : 1.0f;

    // Apply scaling to linear RGB in Rec.2020
    float rLinear2020 = clamp01((rNits / config.targetSdrWhiteLuminanceNits) * scale);
    float gLinear2020 = clamp01((gNits / config.targetSdrWhiteLuminanceNits) * scale);
    float bLinear2020 = clamp01((bNits / config.targetSdrWhiteLuminanceNits) * scale);

    // 4. Transform Rec.2020 to Rec.709 color gamut
    float rLinear709, gLinear709, bLinear709;
    transformBt2020ToBt709(rLinear2020, gLinear2020, bLinear2020, rLinear709, gLinear709, bLinear709);

    // 5. Apply BT.709 OETF to produce display-ready SDR gamma
    outR = linearToBt709(rLinear709);
    outG = linearToBt709(gLinear709);
    outB = linearToBt709(bLinear709);
}

common::Result<void> HdrToneMapper::convertR10G10B10A2ToP010(
    const uint32_t* srcR10,
    uint16_t* dstY,
    uint16_t* dstUv,
    uint32_t width,
    uint32_t height
) noexcept {
    if (!srcR10 || !dstY || !dstUv) {
        return common::Error(common::SubsystemId::Capture, common::ErrorCode::InvalidArgument, "Null buffers in convertR10G10B10A2ToP010");
    }

    if (width % 2 != 0 || height % 2 != 0) {
        return common::Error(common::SubsystemId::Capture, common::ErrorCode::InvalidArgument, "P010 requires even width and height");
    }

    // Process Y plane (full resolution)
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t px = srcR10[y * width + x];
            // Extract 10-bit R, G, B
            float r = static_cast<float>(px & 0x3FF) / 1023.0f;
            float g = static_cast<float>((px >> 10) & 0x3FF) / 1023.0f;
            float b = static_cast<float>((px >> 20) & 0x3FF) / 1023.0f;

            // BT.2020 Luma
            float luma = 0.2627f * r + 0.6780f * g + 0.0593f * b;
            auto luma10 = static_cast<uint16_t>(clamp01(luma) * 1023.0f + 0.5f);
            dstY[y * width + x] = packP010Sample(luma10);
        }
    }

    // Process UV plane (2x2 subsampled, interleaved U0 V0 U1 V1)
    for (uint32_t y = 0; y < height; y += 2) {
        for (uint32_t x = 0; x < width; x += 2) {
            // Average 2x2 block
            float avgR = 0.0f, avgG = 0.0f, avgB = 0.0f;
            for (uint32_t dy = 0; dy < 2; ++dy) {
                for (uint32_t dx = 0; dx < 2; ++dx) {
                    uint32_t px = srcR10[(y + dy) * width + (x + dx)];
                    avgR += static_cast<float>(px & 0x3FF) / 1023.0f;
                    avgG += static_cast<float>((px >> 10) & 0x3FF) / 1023.0f;
                    avgB += static_cast<float>((px >> 20) & 0x3FF) / 1023.0f;
                }
            }
            avgR *= 0.25f;
            avgG *= 0.25f;
            avgB *= 0.25f;

            // BT.2020 Chroma
            float u = -0.1396f * avgR - 0.3604f * avgG + 0.5000f * avgB + 0.5f;
            float v = 0.5000f * avgR - 0.4598f * avgG - 0.0402f * avgB + 0.5f;

            auto u10 = static_cast<uint16_t>(clamp01(u) * 1023.0f + 0.5f);
            auto v10 = static_cast<uint16_t>(clamp01(v) * 1023.0f + 0.5f);

            uint32_t uvIndex = (y / 2) * width + x;
            dstUv[uvIndex] = packP010Sample(u10);
            dstUv[uvIndex + 1] = packP010Sample(v10);
        }
    }

    return common::Result<void>::success();
}

common::Result<void> HdrToneMapper::toneMapP010ToNv12(
    const uint16_t* srcY,
    const uint16_t* srcUv,
    uint8_t* dstY,
    uint8_t* dstUv,
    uint32_t width,
    uint32_t height,
    const ToneMappingConfig& config
) noexcept {
    if (!srcY || !srcUv || !dstY || !dstUv) {
        return common::Error(common::SubsystemId::Capture, common::ErrorCode::InvalidArgument, "Null buffers in toneMapP010ToNv12");
    }

    if (width % 2 != 0 || height % 2 != 0) {
        return common::Error(common::SubsystemId::Capture, common::ErrorCode::InvalidArgument, "Requires even width and height");
    }

    // Tone map Y plane (10-bit PQ to 8-bit SDR)
    for (uint32_t i = 0; i < width * height; ++i) {
        uint16_t y10 = unpackP010Sample(srcY[i]);
        float normY = static_cast<float>(y10) / 1023.0f;
        float nits = pqToLinearNits(normY);
        float normLum = nits / std::max(1.0f, config.targetSdrWhiteLuminanceNits);
        float mappedLum = toneMapScalar(normLum, config);
        float sdrY = linearToBt709(mappedLum);
        dstY[i] = static_cast<uint8_t>(clamp01(sdrY) * 255.0f + 0.5f);
    }

    // Map UV plane (downscale 10-bit chroma to 8-bit)
    uint32_t numChromaPairs = (width * height) / 2;
    for (uint32_t i = 0; i < numChromaPairs; ++i) {
        uint16_t c10 = unpackP010Sample(srcUv[i]);
        dstUv[i] = static_cast<uint8_t>((c10 >> 2) & 0xFF);
    }

    return common::Result<void>::success();
}

} // namespace mist::capture
