#include "mist/audio/OpusAudioEncoder.h"
#include <opus.h>
#include <string>

namespace mist::audio {

OpusAudioEncoder::OpusAudioEncoder() {
    tempPacketBuffer_.resize(4000); // 4000 bytes max payload per Opus packet
}

OpusAudioEncoder::~OpusAudioEncoder() {
    if (encoder_) {
        opus_encoder_destroy(encoder_);
        encoder_ = nullptr;
    }
}

OpusAudioEncoder::OpusAudioEncoder(OpusAudioEncoder&& other) noexcept
    : encoder_(other.encoder_), config_(other.config_), tempPacketBuffer_(std::move(other.tempPacketBuffer_)) {
    other.encoder_ = nullptr;
}

OpusAudioEncoder& OpusAudioEncoder::operator=(OpusAudioEncoder&& other) noexcept {
    if (this != &other) {
        if (encoder_) {
            opus_encoder_destroy(encoder_);
        }
        encoder_ = other.encoder_;
        config_ = other.config_;
        tempPacketBuffer_ = std::move(other.tempPacketBuffer_);
        other.encoder_ = nullptr;
    }
    return *this;
}

common::Result<void> OpusAudioEncoder::initialize(const OpusEncoderConfig& config) {
    if (encoder_) {
        opus_encoder_destroy(encoder_);
        encoder_ = nullptr;
    }

    if (config.sampleRate != 48000 && config.sampleRate != 24000 &&
        config.sampleRate != 16000 && config.sampleRate != 12000 && config.sampleRate != 8000) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InvalidArgument, "Unsupported Opus sample rate");
    }
    if (config.channels != 1 && config.channels != 2) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InvalidArgument, "Unsupported channel count (must be 1 or 2)");
    }

    int app = OPUS_APPLICATION_RESTRICTED_LOWDELAY;
    if (config.application == OpusApplication::Audio) {
        app = OPUS_APPLICATION_AUDIO;
    } else if (config.application == OpusApplication::Voip) {
        app = OPUS_APPLICATION_VOIP;
    }

    int err = OPUS_OK;
    encoder_ = opus_encoder_create(static_cast<opus_int32>(config.sampleRate), config.channels, app, &err);
    if (err != OPUS_OK || !encoder_) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Failed to create Opus encoder: " + std::to_string(err));
    }

    config_ = config;

    // Configure bitrate
    opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(static_cast<opus_int32>(config.bitrateBps)));
    // Configure VBR
    opus_encoder_ctl(encoder_, OPUS_SET_VBR(1));
    // In-band FEC
    opus_encoder_ctl(encoder_, OPUS_SET_INBAND_FEC(config.enableFec ? 1 : 0));
    // Expected packet loss
    opus_encoder_ctl(encoder_, OPUS_SET_PACKET_LOSS_PERC(static_cast<opus_int32>(config.expectedLossPercent)));
    // Complexity
    opus_encoder_ctl(encoder_, OPUS_SET_COMPLEXITY(config.complexity));
    // Signal hint: music / game audio
    opus_encoder_ctl(encoder_, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));

    return common::Result<void>::success();
}

common::Result<size_t> OpusAudioEncoder::encode(std::span<const float> pcmStereo, std::span<uint8_t> outBitstream) {
    if (!encoder_) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::NotInitialized, "Opus encoder not initialized");
    }

    size_t expectedSamples = config_.frameSizeSamples * config_.channels;
    if (pcmStereo.size() < expectedSamples) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InvalidArgument, "Insufficient PCM samples for configured frame size");
    }

    opus_int32 bytesEncoded = opus_encode_float(
        encoder_,
        pcmStereo.data(),
        static_cast<int>(config_.frameSizeSamples),
        outBitstream.data(),
        static_cast<opus_int32>(outBitstream.size())
    );

    if (bytesEncoded < 0) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Opus encode failed with error: " + std::to_string(bytesEncoded));
    }

    return static_cast<size_t>(bytesEncoded);
}

common::Result<std::vector<uint8_t>> OpusAudioEncoder::encode(std::span<const float> pcmStereo) {
    auto res = encode(pcmStereo, std::span<uint8_t>(tempPacketBuffer_.data(), tempPacketBuffer_.size()));
    if (res.hasError()) {
        return res.error();
    }

    size_t bytes = res.value();
    std::vector<uint8_t> packet(tempPacketBuffer_.begin(), tempPacketBuffer_.begin() + static_cast<std::ptrdiff_t>(bytes));
    return packet;
}

common::Result<void> OpusAudioEncoder::setBitrate(uint32_t bitrateBps) {
    if (!encoder_) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::NotInitialized, "Opus encoder not initialized");
    }
    int err = opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(static_cast<opus_int32>(bitrateBps)));
    if (err != OPUS_OK) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Failed to set Opus bitrate");
    }
    config_.bitrateBps = bitrateBps;
    return common::Result<void>::success();
}

common::Result<void> OpusAudioEncoder::setExpectedLossPercent(uint32_t lossPercent) {
    if (!encoder_) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::NotInitialized, "Opus encoder not initialized");
    }
    int err = opus_encoder_ctl(encoder_, OPUS_SET_PACKET_LOSS_PERC(static_cast<opus_int32>(lossPercent)));
    if (err != OPUS_OK) {
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Failed to set Opus packet loss percentage");
    }
    config_.expectedLossPercent = lossPercent;
    return common::Result<void>::success();
}

void OpusAudioEncoder::reset() noexcept {
    if (encoder_) {
        opus_encoder_ctl(encoder_, OPUS_RESET_STATE);
    }
}

} // namespace mist::audio
