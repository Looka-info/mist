#pragma once

#include "mist/common/Result.h"
#include "mist/capture/ICaptureEngine.h"
#include <cstdint>
#include <vector>
#include <span>

#if defined(_WIN32)
#include <d3d11.h>
#include <wrl/client.h>
#endif

namespace mist::encode {

enum class VideoCodec : uint8_t {
    H264 = 0,
    HEVC,
    AV1
};

struct EncodeConfig {
    VideoCodec codec{VideoCodec::H264};
    uint32_t width{1920};
    uint32_t height{1080};
    uint32_t targetFps{60};
    uint32_t bitrateKbps{15000};
    uint32_t gopLength{60}; // Keyframe interval
    bool lowLatencyTuning{true};
};

struct EncodedPacket {
    std::vector<uint8_t> data;
    bool isKeyframe{false};
    uint64_t timestampNs{0};
    uint64_t frameNumber{0};
};

class IVideoEncoder {
public:
    virtual ~IVideoEncoder() = default;

    [[nodiscard]] virtual common::Result<void> initialize(const EncodeConfig& config) = 0;
#if defined(_WIN32)
    [[nodiscard]] virtual common::Result<void> initializeWithDevice(
        const EncodeConfig& config,
        Microsoft::WRL::ComPtr<ID3D11Device> device,
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context = nullptr
    ) {
        (void)device;
        (void)context;
        return initialize(config);
    }
#endif
    [[nodiscard]] virtual common::Result<void> encode(const capture::CapturedFrame& frame) = 0;
    [[nodiscard]] virtual common::Result<EncodedPacket> getNextPacket() = 0;
    [[nodiscard]] virtual common::Result<void> requestKeyframe() = 0;
    [[nodiscard]] virtual common::Result<void> setBitrate(uint32_t bitrateKbps) = 0;
    [[nodiscard]] virtual common::Result<void> flush() = 0;
};

} // namespace mist::encode
