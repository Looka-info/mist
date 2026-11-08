#pragma once

#include "mist/common/Result.h"
#include <cstdint>
#include <vector>
#include <span>

struct OpusDecoder;

namespace mist::audio {

struct OpusDecoderConfig {
    uint32_t sampleRate{48000};
    uint16_t channels{2};
};

class OpusAudioDecoder {
public:
    OpusAudioDecoder();
    ~OpusAudioDecoder();

    OpusAudioDecoder(const OpusAudioDecoder&) = delete;
    OpusAudioDecoder& operator=(const OpusAudioDecoder&) = delete;
    OpusAudioDecoder(OpusAudioDecoder&&) noexcept;
    OpusAudioDecoder& operator=(OpusAudioDecoder&&) noexcept;

    [[nodiscard]] common::Result<void> initialize(const OpusDecoderConfig& config = {});
    
    // Decode incoming Opus payload to interleaved stereo float samples.
    // Returns the number of frames decoded (e.g. 480 or 960).
    [[nodiscard]] common::Result<size_t> decode(
        std::span<const uint8_t> opusPayload,
        std::span<float> outPcmStereo,
        bool decodeFec = false
    );

    // Conceal a lost packet (PLC)
    [[nodiscard]] common::Result<size_t> decodeLostFrame(
        std::span<float> outPcmStereo,
        size_t expectedFrames = 480
    );

    void reset() noexcept;

    [[nodiscard]] const OpusDecoderConfig& getConfig() const noexcept { return config_; }
    [[nodiscard]] bool isInitialized() const noexcept { return decoder_ != nullptr; }

private:
    OpusDecoder* decoder_{nullptr};
    OpusDecoderConfig config_{};
};

} // namespace mist::audio
