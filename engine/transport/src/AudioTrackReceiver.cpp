#include "mist/transport/AudioTrackReceiver.h"
#include "mist/common/Logger.h"
#include "rtc/rtc.hpp"
#include <chrono>
#include <cstring>

namespace mist::transport {

namespace {

uint64_t getNowNs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

} // namespace

AudioTrackReceiver::AudioTrackReceiver() noexcept = default;

AudioTrackReceiver::~AudioTrackReceiver() {
    close();
}

common::Result<void> AudioTrackReceiver::initialize(const WebRtcTransportConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    return common::Result<void>::success();
}

void AudioTrackReceiver::attachTrack(std::shared_ptr<rtc::Track> track) {
    std::lock_guard<std::mutex> lock(mutex_);
    track_ = track;
    if (!track_) return;

    MIST_LOG_INFO(common::SubsystemId::Transport, "Incoming remote AudioTrack attached (mid: " + track->mid() + ")");

    auto depacketizer = std::make_shared<rtc::OpusRtpDepacketizer>();
    track_->setMediaHandler(depacketizer);

    track_->onOpen([this]() {
        isOpen_.store(true);
        MIST_LOG_INFO(common::SubsystemId::Transport, "AudioTrackReceiver opened and ready.");
    });

    track_->onClosed([this]() {
        isOpen_.store(false);
        MIST_LOG_INFO(common::SubsystemId::Transport, "AudioTrackReceiver closed.");
    });

    track_->onError([](const std::string& error) {
        MIST_LOG_ERROR(common::SubsystemId::Transport, "AudioTrackReceiver error: " + error);
    });

    track_->onFrame([this](rtc::binary data, rtc::FrameInfo info) {
        if (data.empty()) return;

        packetsReceived_.fetch_add(1, std::memory_order_relaxed);
        bytesReceived_.fetch_add(data.size(), std::memory_order_relaxed);
        lastRtpTimestamp_.store(info.timestamp, std::memory_order_relaxed);

        AudioFrameInfo frameInfo;
        frameInfo.rtpTimestamp = info.timestamp;
        frameInfo.arrivalTimeNs = getNowNs();

        AudioFrameCallback cb;
        {
            std::lock_guard<std::mutex> innerLock(mutex_);
            cb = frameCb_;
        }
        if (cb) {
            cb(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data.data()), data.size()), frameInfo);
        }
    });
}

void AudioTrackReceiver::setFrameCallback(AudioFrameCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    frameCb_ = std::move(cb);
}

bool AudioTrackReceiver::isTrackOpen() const noexcept {
    return isOpen_.load(std::memory_order_relaxed);
}

void AudioTrackReceiver::close() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    isOpen_.store(false);
    if (track_) {
        try {
            track_->close();
        } catch (...) {
            // Suppress exception on shutdown
        }
        track_.reset();
    }
}

} // namespace mist::transport
