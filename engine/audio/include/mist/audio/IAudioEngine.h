#pragma once

#include "mist/common/Result.h"
#include <cstdint>
#include <vector>
#include <span>

namespace mist::audio {

enum class AudioSampleFormat : uint8_t {
    Int16 = 0,
    Float32
};

struct AudioConfig {
    uint32_t sampleRate{48000};
    uint16_t channels{2};
    AudioSampleFormat format{AudioSampleFormat::Float32};
    uint32_t bufferFrameSize{480}; // 10ms at 48kHz
};

struct AudioPacket {
    std::vector<uint8_t> data;
    uint64_t timestampNs{0};
    uint32_t frames{0};
};

class IAudioCapture {
public:
    virtual ~IAudioCapture() = default;

    [[nodiscard]] virtual common::Result<void> initialize(const AudioConfig& config) = 0;
    [[nodiscard]] virtual common::Result<void> start() = 0;
    [[nodiscard]] virtual common::Result<void> stop() = 0;
    [[nodiscard]] virtual common::Result<AudioPacket> readPacket(uint32_t timeoutMs) = 0;
};

class IAudioPlayback {
public:
    virtual ~IAudioPlayback() = default;

    [[nodiscard]] virtual common::Result<void> initialize(const AudioConfig& config) = 0;
    [[nodiscard]] virtual common::Result<void> start() = 0;
    [[nodiscard]] virtual common::Result<void> stop() = 0;
    [[nodiscard]] virtual common::Result<void> writePacket(const AudioPacket& packet) = 0;
};

} // namespace mist::audio
