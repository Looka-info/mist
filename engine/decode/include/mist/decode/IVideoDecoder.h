#pragma once

#include "mist/common/Result.h"
#include "mist/encode/IVideoEncoder.h"
#include <cstdint>
#include <vector>
#include <span>

namespace mist::decode {

struct DecodeConfig {
    encode::VideoCodec codec{encode::VideoCodec::H264};
    bool preferHardwareDecoding{true};
    bool lowLatencyTuning{true};
};

struct DecodedFrame {
    uint32_t width{0};
    uint32_t height{0};
    uint64_t timestampNs{0};
    uint64_t frameNumber{0};
    void* nativeResource{nullptr}; // e.g. ID3D11Texture2D* in Milestone 2
    uint64_t resourceId{0};
    void (*resourceDeleter)(void* res){nullptr};
};

class IVideoDecoder {
public:
    virtual ~IVideoDecoder() = default;

    [[nodiscard]] virtual common::Result<void> initialize(const DecodeConfig& config) = 0;
    [[nodiscard]] virtual common::Result<void> submitPacket(const encode::EncodedPacket& packet) = 0;
    [[nodiscard]] virtual common::Result<DecodedFrame> getNextFrame() = 0;
    [[nodiscard]] virtual common::Result<void> releaseFrame(const DecodedFrame& frame) = 0;
    [[nodiscard]] virtual common::Result<void> flush() = 0;
    [[nodiscard]] virtual common::Result<void> shutdown() = 0;
};

} // namespace mist::decode
