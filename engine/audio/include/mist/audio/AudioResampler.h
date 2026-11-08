#pragma once

#include "mist/audio/IAudioEngine.h"
#include <vector>
#include <span>
#include <cstdint>
#include <cstddef>

namespace mist::audio {

struct AudioFormat {
    uint32_t sampleRate{48000};
    uint16_t channels{2};
    AudioSampleFormat sampleFormat{AudioSampleFormat::Float32};
    uint16_t bitsPerSample{32};
    uint32_t channelMask{0};
};

class AudioResampler {
public:
    AudioResampler();
    ~AudioResampler() = default;

    // Configure input and output formats. Output is always 48kHz stereo Float32.
    common::Result<void> configure(const AudioFormat& inputFormat, uint32_t targetSampleRate = 48000);

    // Push raw audio buffer in input format
    common::Result<void> pushInput(const void* data, size_t inputFrames);

    // Check if at least `frameSize` stereo frames are available
    [[nodiscard]] bool hasFrame(size_t frameSize = 480) const noexcept;

    // Pop `frameSize` interleaved stereo Float32 samples (total frameSize * 2 floats)
    [[nodiscard]] bool popFrame(std::span<float> outStereoPcm, size_t frameSize = 480);

    // Pop `frameSize` interleaved stereo Int16 samples (total frameSize * 2 int16_t)
    [[nodiscard]] bool popFrame(std::span<int16_t> outStereoPcm, size_t frameSize = 480);

    // Reset internal buffers and history
    void reset() noexcept;

    [[nodiscard]] size_t availableFrames() const noexcept;
    [[nodiscard]] const AudioFormat& getInputFormat() const noexcept { return inFormat_; }
    [[nodiscard]] uint32_t getTargetSampleRate() const noexcept { return targetSampleRate_; }

private:
    void convertAndDownmixToStereo(const void* data, size_t inputFrames, std::vector<float>& outStereo);
    void resampleStereo(const std::vector<float>& inStereo, size_t inFrames, std::vector<float>& outStereo);

    AudioFormat inFormat_{};
    uint32_t targetSampleRate_{48000};
    double resampleRatio_{1.0}; // inSampleRate / targetSampleRate

    // Boundary interpolation carry-over
    float lastInLeft_{0.0f};
    float lastInRight_{0.0f};
    double subSamplePos_{0.0};
    bool hasHistory_{false};

    // Intermediate scratch buffers
    std::vector<float> downmixedBuffer_;
    std::vector<float> resampledBuffer_;

    // Playout / output accumulation ring buffer (interleaved stereo floats)
    std::vector<float> fifoBuffer_;
    size_t fifoReadPos_{0};
    size_t fifoWritePos_{0};
    size_t fifoCount_{0};
};

} // namespace mist::audio
