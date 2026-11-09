#pragma once

#include "DxgiCaptureEngine.h"
#include "SyntheticFrameGenerator.h"
#include "ColorConverter.h"
#include "mist/encode/VideoEncoderFactory.h"
#include "mist/telemetry/Metrics.h"
#include <functional>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>

namespace mist::pipeline {

struct PipelineReport {
    std::string hardwareGpu;
    std::string driverVersion;
    uint32_t width{1920};
    uint32_t height{1080};
    uint32_t targetFps{60};
    double actualFps{0.0};
    double averageBitrateKbps{0.0};

    // Frame counts
    uint64_t framesAcquired{0};
    uint64_t framesEncoded{0};
    uint64_t framesDropped{0};
    uint64_t captureTimeouts{0};
    uint64_t captureErrors{0};
    uint64_t encodeErrors{0};

    // Telemetry snapshots
    telemetry::HistogramSnapshot captureLatencyUs;
    telemetry::HistogramSnapshot conversionLatencyUs;
    telemetry::HistogramSnapshot encodeLatencyUs;
    telemetry::HistogramSnapshot captureToBitstreamLatencyUs;

    [[nodiscard]] std::string toJson() const;
};

class CaptureEncodePipeline {
public:
    CaptureEncodePipeline() noexcept;
    ~CaptureEncodePipeline();

    [[nodiscard]] common::Result<void> initialize(
        const capture::CaptureConfig& capConfig,
        const encode::EncodeConfig& encConfig,
        bool useSyntheticSource = false
    );

    [[nodiscard]] common::Result<void> start();
    [[nodiscard]] common::Result<void> stop();
    [[nodiscard]] common::Result<PipelineReport> runForDuration(
        uint32_t durationSeconds,
        const std::string& outputH264Path = ""
    );

    [[nodiscard]] PipelineReport generateReport() const;
    [[nodiscard]] bool isRunning() const noexcept { return isRunning_.load(); }

    using EncodedPacketCallback = std::function<void(const encode::EncodedPacket&)>;
    void setEncodedPacketCallback(EncodedPacketCallback cb) { packetCb_ = std::move(cb); }
    void requestKeyframe() noexcept;
    [[nodiscard]] common::Result<void> setBitrate(uint32_t bitrateKbps) noexcept;

private:
    void captureWorker();
    void encodeWorker(const std::string& outputH264Path);

    EncodedPacketCallback packetCb_;

    capture::CaptureConfig capConfig_;
    encode::EncodeConfig encConfig_;
    bool useSynthetic_{false};

    std::unique_ptr<capture::DxgiCaptureEngine> captureEngine_;
    std::unique_ptr<capture::SyntheticFrameGenerator> syntheticGen_;
    std::unique_ptr<capture::ColorConverter> colorConverter_;
    std::unique_ptr<encode::IVideoEncoder> encoder_;

    std::atomic<bool> isRunning_{false};
    std::atomic<bool> stopRequested_{false};

    std::thread captureThread_;
    std::thread encodeThread_;

    mutable std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::queue<capture::CapturedFrame> pendingFrames_;

    // Telemetry histograms
    std::shared_ptr<telemetry::LatencyHistogram> histCaptureLatency_;
    std::shared_ptr<telemetry::LatencyHistogram> histConversionLatency_;
    std::shared_ptr<telemetry::LatencyHistogram> histEncodeLatency_;
    std::shared_ptr<telemetry::LatencyHistogram> histCaptureToBitstream_;

    // Atomic metrics
    std::atomic<uint64_t> framesAcquired_{0};
    std::atomic<uint64_t> framesEncoded_{0};
    std::atomic<uint64_t> framesDropped_{0};
    std::atomic<uint64_t> captureTimeouts_{0};
    std::atomic<uint64_t> captureErrors_{0};
    std::atomic<uint64_t> encodeErrors_{0};
    std::atomic<uint64_t> totalEncodedBytes_{0};

    platform::windows::HighResolutionTimer pipelineTimer_;
};

} // namespace mist::pipeline
