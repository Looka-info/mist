#include "mist/encode/VideoEncoderFactory.h"
#include "mist/common/Logger.h"
#include <algorithm>
#include <cctype>

namespace mist::encode {

namespace {

std::string toLower(std::string s) noexcept {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

} // namespace

SystemEncoderCapabilities VideoEncoderFactory::querySystemCapabilities(ID3D11Device* device) noexcept {
    SystemEncoderCapabilities caps{};
    caps.hasSoftware = true;
    caps.availableBackends.push_back("Software");

    // 1. Query NVIDIA NVENC
    auto nvencCap = NvencVideoEncoder::queryCapabilities(device);
    caps.hasNvenc = nvencCap.isSupported;
    if (caps.hasNvenc) {
        caps.availableBackends.push_back("NVENC");
        caps.gpuVendor = "NVIDIA";
    }

    // 2. Query AMD AMF
    auto amfCap = AmfVideoEncoder::queryCapabilities(device);
    caps.hasAmf = amfCap.isSupported;
    if (caps.hasAmf) {
        caps.availableBackends.push_back("AMF");
        if (caps.gpuVendor.empty()) {
            caps.gpuVendor = "AMD";
            caps.gpuModel = amfCap.gpuName;
        }
    }

    // 3. Query Intel QuickSync
    auto qsvCap = QsvVideoEncoder::queryCapabilities(device);
    caps.hasQsv = qsvCap.isSupported;
    if (caps.hasQsv) {
        caps.availableBackends.push_back("QuickSync");
        if (caps.gpuVendor.empty()) {
            caps.gpuVendor = "Intel";
            caps.gpuModel = qsvCap.gpuName;
        }
    }

    // Determine preferred backend (Hardware priority: NVENC > AMF > QuickSync > Software fallback)
    if (caps.hasNvenc) {
        caps.preferredBackend = EncoderBackend::Nvenc;
    } else if (caps.hasAmf) {
        caps.preferredBackend = EncoderBackend::Amf;
    } else if (caps.hasQsv) {
        caps.preferredBackend = EncoderBackend::Qsv;
    } else {
        caps.preferredBackend = EncoderBackend::Software;
        if (caps.gpuVendor.empty()) {
            caps.gpuVendor = "Generic CPU";
            caps.gpuModel = "Software Fallback";
        }
    }

    return caps;
}

common::Result<std::unique_ptr<IVideoEncoder>> VideoEncoderFactory::createEncoder(
    EncoderBackend backend,
    ID3D11Device* device
) {
    if (backend == EncoderBackend::Auto) {
        auto caps = querySystemCapabilities(device);
        backend = caps.preferredBackend;
    }

    auto createSoftware = []() -> common::Result<std::unique_ptr<IVideoEncoder>> {
        auto res = SoftwareVideoEncoder::create();
        if (res) {
            return std::unique_ptr<IVideoEncoder>(std::move(res.value()));
        }
        return res.error();
    };

    switch (backend) {
        case EncoderBackend::Nvenc: {
            auto res = NvencVideoEncoder::create();
            if (res) {
                return std::unique_ptr<IVideoEncoder>(std::move(res.value()));
            }
            MIST_LOG_WARN(common::SubsystemId::Encode, "NVENC creation failed, falling back to Software encoder: " + res.error().format());
            return createSoftware();
        }
        case EncoderBackend::Amf: {
            auto res = AmfVideoEncoder::create();
            if (res) {
                return std::unique_ptr<IVideoEncoder>(std::move(res.value()));
            }
            MIST_LOG_WARN(common::SubsystemId::Encode, "AMF creation failed, falling back to Software encoder: " + res.error().format());
            return createSoftware();
        }
        case EncoderBackend::Qsv: {
            auto res = QsvVideoEncoder::create();
            if (res) {
                return std::unique_ptr<IVideoEncoder>(std::move(res.value()));
            }
            MIST_LOG_WARN(common::SubsystemId::Encode, "QuickSync creation failed, falling back to Software encoder: " + res.error().format());
            return createSoftware();
        }
        case EncoderBackend::Software:
        default:
            return createSoftware();
    }
}

std::string VideoEncoderFactory::backendToString(EncoderBackend backend) noexcept {
    switch (backend) {
        case EncoderBackend::Auto:
            return "Auto";
        case EncoderBackend::Nvenc:
            return "NVENC";
        case EncoderBackend::Amf:
            return "AMF";
        case EncoderBackend::Qsv:
            return "QuickSync";
        case EncoderBackend::Software:
            return "Software";
    }
    return "Unknown";
}

EncoderBackend VideoEncoderFactory::stringToBackend(const std::string& name) noexcept {
    std::string s = toLower(name);
    if (s == "nvenc" || s == "nvidia") {
        return EncoderBackend::Nvenc;
    }
    if (s == "amf" || s == "amd" || s == "vce" || s == "vcn") {
        return EncoderBackend::Amf;
    }
    if (s == "qsv" || s == "intel" || s == "quicksync" || s == "vpl") {
        return EncoderBackend::Qsv;
    }
    if (s == "software" || s == "cpu" || s == "x264") {
        return EncoderBackend::Software;
    }
    return EncoderBackend::Auto;
}

} // namespace mist::encode
