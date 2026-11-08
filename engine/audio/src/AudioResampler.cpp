#include "mist/audio/AudioResampler.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace mist::audio {

AudioResampler::AudioResampler() {
    fifoBuffer_.resize(48000 * 2); // 1 second buffer default capacity
}

common::Result<void> AudioResampler::configure(const AudioFormat& inputFormat, uint32_t targetSampleRate) {
    if (inputFormat.sampleRate == 0 || inputFormat.channels == 0) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InvalidArgument, "Invalid sample rate or channel count");
    }
    if (targetSampleRate == 0) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InvalidArgument, "Invalid target sample rate");
    }

    inFormat_ = inputFormat;
    targetSampleRate_ = targetSampleRate;
    resampleRatio_ = static_cast<double>(inFormat_.sampleRate) / static_cast<double>(targetSampleRate_);

    reset();
    return common::Result<void>::success();
}

void AudioResampler::reset() noexcept {
    lastInLeft_ = 0.0f;
    lastInRight_ = 0.0f;
    subSamplePos_ = 0.0;
    hasHistory_ = false;

    downmixedBuffer_.clear();
    resampledBuffer_.clear();

    fifoReadPos_ = 0;
    fifoWritePos_ = 0;
    fifoCount_ = 0;
}

size_t AudioResampler::availableFrames() const noexcept {
    return fifoCount_ / 2;
}

bool AudioResampler::hasFrame(size_t frameSize) const noexcept {
    return availableFrames() >= frameSize;
}

void AudioResampler::convertAndDownmixToStereo(const void* data, size_t inputFrames, std::vector<float>& outStereo) {
    outStereo.resize(inputFrames * 2);

    const uint16_t ch = inFormat_.channels;
    const auto format = inFormat_.sampleFormat;
    const auto* fData = reinterpret_cast<const float*>(data);
    const auto* i16Data = reinterpret_cast<const int16_t*>(data);

    auto getSampleFloat = [&](size_t frameIdx, uint16_t channelIdx) -> float {
        size_t idx = frameIdx * ch + channelIdx;
        if (format == AudioSampleFormat::Float32) {
            return fData[idx];
        } else {
            return static_cast<float>(i16Data[idx]) / 32768.0f;
        }
    };

    if (ch == 1) {
        for (size_t i = 0; i < inputFrames; ++i) {
            float s = getSampleFloat(i, 0);
            outStereo[i * 2 + 0] = s;
            outStereo[i * 2 + 1] = s;
        }
    } else if (ch == 2) {
        for (size_t i = 0; i < inputFrames; ++i) {
            outStereo[i * 2 + 0] = getSampleFloat(i, 0);
            outStereo[i * 2 + 1] = getSampleFloat(i, 1);
        }
    } else if (ch == 6) {
        // 5.1 Surround: FL(0), FR(1), FC(2), LFE(3), BL(4), BR(5)
        // ITU-R BS.775 downmix: L = (FL + 0.7071*FC + 0.7071*BL) / 2.4142
        constexpr float invNorm = 1.0f / 2.41421356f;
        constexpr float sqrt2Inv = 0.70710678f;
        for (size_t i = 0; i < inputFrames; ++i) {
            float fl = getSampleFloat(i, 0);
            float fr = getSampleFloat(i, 1);
            float fc = getSampleFloat(i, 2);
            float bl = getSampleFloat(i, 4);
            float br = getSampleFloat(i, 5);

            outStereo[i * 2 + 0] = (fl + sqrt2Inv * fc + sqrt2Inv * bl) * invNorm;
            outStereo[i * 2 + 1] = (fr + sqrt2Inv * fc + sqrt2Inv * br) * invNorm;
        }
    } else if (ch >= 8) {
        // 7.1 Surround: FL(0), FR(1), FC(2), LFE(3), BL(4), BR(5), SL(6), SR(7)
        constexpr float invNorm = 1.0f / 2.91421356f;
        constexpr float sqrt2Inv = 0.70710678f;
        for (size_t i = 0; i < inputFrames; ++i) {
            float fl = getSampleFloat(i, 0);
            float fr = getSampleFloat(i, 1);
            float fc = getSampleFloat(i, 2);
            float bl = getSampleFloat(i, 4);
            float br = getSampleFloat(i, 5);
            float sl = getSampleFloat(i, 6);
            float sr = getSampleFloat(i, 7);

            outStereo[i * 2 + 0] = (fl + sqrt2Inv * fc + 0.5f * bl + sqrt2Inv * sl) * invNorm;
            outStereo[i * 2 + 1] = (fr + sqrt2Inv * fc + 0.5f * br + sqrt2Inv * sr) * invNorm;
        }
    } else {
        // Fallback for uncommon channel counts (e.g. 3, 4, 5)
        for (size_t i = 0; i < inputFrames; ++i) {
            outStereo[i * 2 + 0] = getSampleFloat(i, 0);
            outStereo[i * 2 + 1] = getSampleFloat(i, (ch > 1) ? 1 : 0);
        }
    }
}

void AudioResampler::resampleStereo(const std::vector<float>& inStereo, size_t inFrames, std::vector<float>& outStereo) {
    if (inFormat_.sampleRate == targetSampleRate_) {
        // 1:1 pass-through
        outStereo.assign(inStereo.begin(), inStereo.begin() + static_cast<std::ptrdiff_t>(inFrames * 2));
        if (inFrames > 0) {
            lastInLeft_ = inStereo[(inFrames - 1) * 2 + 0];
            lastInRight_ = inStereo[(inFrames - 1) * 2 + 1];
            hasHistory_ = true;
        }
        return;
    }

    outStereo.clear();
    if (inFrames == 0) return;

    // Estimate output frames
    size_t estimatedOutFrames = static_cast<size_t>(std::ceil(static_cast<double>(inFrames) / resampleRatio_)) + 8;
    outStereo.reserve(estimatedOutFrames * 2);

    double currentPos = subSamplePos_;

    while (currentPos < static_cast<double>(inFrames)) {
        size_t idx0 = static_cast<size_t>(std::floor(currentPos));
        size_t idx1 = idx0 + 1;
        float frac = static_cast<float>(currentPos - static_cast<double>(idx0));

        float left0, right0, left1, right1;

        if (idx0 < inFrames) {
            left0 = inStereo[idx0 * 2 + 0];
            right0 = inStereo[idx0 * 2 + 1];
        } else {
            left0 = lastInLeft_;
            right0 = lastInRight_;
        }

        if (idx1 < inFrames) {
            left1 = inStereo[idx1 * 2 + 0];
            right1 = inStereo[idx1 * 2 + 1];
        } else {
            left1 = left0;
            right1 = right0;
        }

        float outL = (1.0f - frac) * left0 + frac * left1;
        float outR = (1.0f - frac) * right0 + frac * right1;

        // Soft clamp
        outL = std::clamp(outL, -1.0f, 1.0f);
        outR = std::clamp(outR, -1.0f, 1.0f);

        outStereo.push_back(outL);
        outStereo.push_back(outR);

        currentPos += resampleRatio_;
    }

    subSamplePos_ = currentPos - static_cast<double>(inFrames);
    lastInLeft_ = inStereo[(inFrames - 1) * 2 + 0];
    lastInRight_ = inStereo[(inFrames - 1) * 2 + 1];
    hasHistory_ = true;
}

common::Result<void> AudioResampler::pushInput(const void* data, size_t inputFrames) {
    if (!data || inputFrames == 0) {
        return common::Result<void>::success();
    }

    // 1. Convert sample format and downmix to stereo float
    convertAndDownmixToStereo(data, inputFrames, downmixedBuffer_);

    // 2. Resample to 48kHz
    resampleStereo(downmixedBuffer_, inputFrames, resampledBuffer_);

    // 3. Push to FIFO ring buffer
    size_t samplesToAdd = resampledBuffer_.size();
    if (fifoCount_ + samplesToAdd > fifoBuffer_.size()) {
        // Grow FIFO if needed
        std::vector<float> newBuffer(std::max(fifoBuffer_.size() * 2, fifoCount_ + samplesToAdd + 48000));
        for (size_t i = 0; i < fifoCount_; ++i) {
            newBuffer[i] = fifoBuffer_[(fifoReadPos_ + i) % fifoBuffer_.size()];
        }
        fifoReadPos_ = 0;
        fifoWritePos_ = fifoCount_;
        fifoBuffer_ = std::move(newBuffer);
    }

    for (float sample : resampledBuffer_) {
        fifoBuffer_[fifoWritePos_] = sample;
        fifoWritePos_ = (fifoWritePos_ + 1) % fifoBuffer_.size();
    }
    fifoCount_ += samplesToAdd;

    return common::Result<void>::success();
}

bool AudioResampler::popFrame(std::span<float> outStereoPcm, size_t frameSize) {
    size_t requiredSamples = frameSize * 2;
    if (fifoCount_ < requiredSamples || outStereoPcm.size() < requiredSamples) {
        return false;
    }

    for (size_t i = 0; i < requiredSamples; ++i) {
        outStereoPcm[i] = fifoBuffer_[fifoReadPos_];
        fifoReadPos_ = (fifoReadPos_ + 1) % fifoBuffer_.size();
    }
    fifoCount_ -= requiredSamples;
    return true;
}

bool AudioResampler::popFrame(std::span<int16_t> outStereoPcm, size_t frameSize) {
    size_t requiredSamples = frameSize * 2;
    if (fifoCount_ < requiredSamples || outStereoPcm.size() < requiredSamples) {
        return false;
    }

    for (size_t i = 0; i < requiredSamples; ++i) {
        float s = fifoBuffer_[fifoReadPos_];
        fifoReadPos_ = (fifoReadPos_ + 1) % fifoBuffer_.size();
        float clamped = std::clamp(s, -1.0f, 1.0f);
        outStereoPcm[i] = static_cast<int16_t>(clamped * 32767.0f);
    }
    fifoCount_ -= requiredSamples;
    return true;
}

} // namespace mist::audio
