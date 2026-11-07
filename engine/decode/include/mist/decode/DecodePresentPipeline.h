#pragma once

#include "D3D11VideoDecoder.h"
#include "AnnexBReader.h"
#include "mist/render/D3D11Renderer.h"
#include "mist/telemetry/Metrics.h"
#include <string>
#include <thread>
#include <atomic>
#include <queue>
#include <condition_variable>

namespace mist::decode {

struct DecodeBenchmarkReport {
    std::string hardwareGpu;
    std::string decoderBackend;
    uint32_t width{1920};
    uint32_t height{1080};
    uint32_t targetFps{60};
    double actualFps{0.0};
    double averageBitrateKbps{0.0};

    uint64_t accessUnitsIngested{0};
    uint64_t framesDecoded{0};
    uint64_t framesPresented{0};
    uint64_t framesDropped{0};
    uint64_t decodeErrors{0};

    telemetry::HistogramSnapshot decodeLatencyUs;
    telemetry::HistogramSnapshot presentLatencyUs;
    telemetry::HistogramSnapshot decodeToPresentLatencyUs;

    [[nodiscard]] std::string toJson() const;
};

class DecodePresentPipeline {
public:
    DecodePresentPipeline() noexcept;
    ~DecodePresentPipeline();

    [[nodiscard]] common::Result<void> initialize(
        const DecodeConfig& decConfig,
        const render::RenderConfig& rndConfig
    );

    [[nodiscard]] common::Result<DecodeBenchmarkReport> runBitstream(
        const std::string& h264FilePath,
        uint32_t targetFps = 60,
        uint32_t maxDurationSeconds = 0
    );

    [[nodiscard]] common::Result<void> stop();
    [[nodiscard]] DecodeBenchmarkReport generateReport() const;
    [[nodiscard]] bool isRunning() const noexcept { return isRunning_.load(); }

private:
    DecodeConfig decConfig_;
    render::RenderConfig rndConfig_;

    std::unique_ptr<D3D11VideoDecoder> decoder_;
    std::unique_ptr<render::D3D11Renderer> renderer_;

    std::atomic<bool> isRunning_{false};
    std::atomic<bool> stopRequested_{false};

    platform::windows::HighResolutionTimer pipelineTimer_;

    // Telemetry histograms
    std::shared_ptr<telemetry::LatencyHistogram> histDecodeLatency_;
    std::shared_ptr<telemetry::LatencyHistogram> histPresentLatency_;
    std::shared_ptr<telemetry::LatencyHistogram> histDecodeToPresent_;

    // Metrics counters
    std::atomic<uint64_t> accessUnitsIngested_{0};
    std::atomic<uint64_t> framesDecoded_{0};
    std::atomic<uint64_t> framesPresented_{0};
    std::atomic<uint64_t> framesDropped_{0};
    std::atomic<uint64_t> decodeErrors_{0};
    std::atomic<uint64_t> totalBytesIngested_{0};
};

} // namespace mist::decode
