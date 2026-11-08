#include "mist/audio/OpusAudioDecoder.h"
#include <opus.h>
#include <string>

namespace mist::audio {

OpusAudioDecoder::OpusAudioDecoder() = default;

OpusAudioDecoder::~OpusAudioDecoder() {
    if (decoder_) {
        opus_decoder_destroy(decoder_);
        decoder_ = nullptr;
    }
}

OpusAudioDecoder::OpusAudioDecoder(OpusAudioDecoder&& other) noexcept
    : decoder_(other.decoder_), config_(other.config_) {
    other.decoder_ = nullptr;
}

OpusAudioDecoder& OpusAudioDecoder::operator=(OpusAudioDecoder&& other) noexcept {
    if (this != &other) {
        if (decoder_) {
            opus_decoder_destroy(decoder_);
        }
        decoder_ = other.decoder_;
        config_ = other.config_;
        other.decoder_ = nullptr;
    }
    return *this;
}

common::Result<void> OpusAudioDecoder::initialize(const OpusDecoderConfig& config) {
    if (decoder_) {
        opus_decoder_destroy(decoder_);
        decoder_ = nullptr;
    }

    if (config.sampleRate != 48000 && config.sampleRate != 24000 &&
        config.sampleRate != 16000 && config.sampleRate != 12000 && config.sampleRate != 8000) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InvalidArgument, "Unsupported Opus sample rate");
    }
    if (config.channels != 1 && config.channels != 2) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InvalidArgument, "Unsupported channel count (must be 1 or 2)");
    }

    int err = OPUS_OK;
    decoder_ = opus_decoder_create(static_cast<opus_int32>(config.sampleRate), config.channels, &err);
    if (err != OPUS_OK || !decoder_) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Failed to create Opus decoder: " + std::to_string(err));
    }

    config_ = config;
    return common::Result<void>::success();
}

common::Result<size_t> OpusAudioDecoder::decode(
    std::span<const uint8_t> opusPayload,
    std::span<float> outPcmStereo,
    bool decodeFec
) {
    if (!decoder_) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::NotInitialized, "Opus decoder not initialized");
    }

    int maxFrames = static_cast<int>(outPcmStereo.size() / config_.channels);
    if (maxFrames <= 0) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InvalidArgument, "Output buffer too small");
    }

    int framesDecoded = opus_decode_float(
        decoder_,
        opusPayload.empty() ? nullptr : opusPayload.data(),
        static_cast<opus_int32>(opusPayload.size()),
        outPcmStereo.data(),
        maxFrames,
        decodeFec ? 1 : 0
    );

    if (framesDecoded < 0) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Opus decode failed with error: " + std::to_string(framesDecoded));
    }

    return static_cast<size_t>(framesDecoded);
}

common::Result<size_t> OpusAudioDecoder::decodeLostFrame(
    std::span<float> outPcmStereo,
    size_t expectedFrames
) {
    if (!decoder_) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::NotInitialized, "Opus decoder not initialized");
    }

    int maxFrames = static_cast<int>(outPcmStereo.size() / config_.channels);
    if (maxFrames < static_cast<int>(expectedFrames)) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InvalidArgument, "Output buffer too small for PLC frame");
    }

    // Passing NULL payload invokes Packet Loss Concealment (PLC)
    int framesDecoded = opus_decode_float(
        decoder_,
        nullptr,
        0,
        outPcmStereo.data(),
        static_cast<int>(expectedFrames),
        0
    );

    if (framesDecoded < 0) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Opus PLC decode failed with error: " + std::to_string(framesDecoded));
    }

    return static_cast<size_t>(framesDecoded);
}

void OpusAudioDecoder::reset() noexcept {
    if (decoder_) {
        opus_decoder_ctl(decoder_, OPUS_RESET_STATE);
    }
}

} // namespace mist::audio
