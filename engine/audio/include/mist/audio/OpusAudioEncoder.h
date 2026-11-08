#pragma once

#include "mist/common/Result.h"
#include <cstdint>
#include <vector>
#include <span>

struct OpusEncoder;

namespace mist::audio {

enum class OpusApplication : uint8_t {
    RestrictedLowDelay = 0,
    Audio,
    Voip
};

struct OpusEncoderConfig {
    uint32_t sampleRate{48000};
    uint16_t channels{2};
    uint32_t bitrateBps{128000}; // 128 kbps standard
    uint32_t frameSizeSamples{480}; // 10ms at 48kHz (480 samples, or 960 for 20ms)
    OpusApplication application{OpusApplication::RestrictedLowDelay};
    bool enableFec{true};
    uint32_t expectedLossPercent{5};
    int complexity{8}; // 0-10
};

class OpusAudioEncoder {
public:
    OpusAudioEncoder();
    ~OpusAudioEncoder();

    OpusAudioEncoder(const OpusAudioEncoder&) = delete;
    OpusAudioEncoder& operator=(const OpusAudioEncoder&) = delete;
    OpusAudioEncoder(OpusAudioEncoder&&) noexcept;
    OpusAudioEncoder& operator=(OpusAudioEncoder&&) noexcept;

    [[nodiscard]] common::Result<void> initialize(const OpusEncoderConfig& config);
    [[nodiscard]] common::Result<size_t> encode(std::span<const float> pcmStereo, std::span<uint8_t> outBitstream);
    [[nodiscard]] common::Result<std::vector<uint8_t>> encode(std::span<const float> pcmStereo);

    [[nodiscard]] common::Result<void> setBitrate(uint32_t bitrateBps);
    [[nodiscard]] common::Result<void> setExpectedLossPercent(uint32_t lossPercent);
    void reset() noexcept;

    [[nodiscard]] const OpusEncoderConfig& getConfig() const noexcept { return config_; }
    [[nodiscard]] bool isInitialized() const noexcept { return encoder_ != nullptr; }

private:
    OpusEncoder* encoder_{nullptr};
    OpusEncoderConfig config_{};
    std::vector<uint8_t> tempPacketBuffer_;
};

} // namespace mist::audio
