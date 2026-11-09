#pragma once

#include "IVideoEncoder.h"
#include "NvencVideoEncoder.h"
#include "AmfVideoEncoder.h"
#include "QsvVideoEncoder.h"
#include "SoftwareVideoEncoder.h"
#include <string>
#include <vector>
#include <memory>

namespace mist::encode {

enum class EncoderBackend : uint8_t {
    Auto = 0,
    Nvenc,
    Amf,
    Qsv,
    Software
};

struct SystemEncoderCapabilities {
    bool hasNvenc{false};
    bool hasAmf{false};
    bool hasQsv{false};
    bool hasSoftware{true};
    EncoderBackend preferredBackend{EncoderBackend::Software};
    std::string gpuVendor;
    std::string gpuModel;
    std::vector<std::string> availableBackends;
};

class VideoEncoderFactory {
public:
    [[nodiscard]] static SystemEncoderCapabilities querySystemCapabilities(ID3D11Device* device = nullptr) noexcept;

    [[nodiscard]] static common::Result<std::unique_ptr<IVideoEncoder>> createEncoder(
        EncoderBackend backend = EncoderBackend::Auto,
        ID3D11Device* device = nullptr
    );

    [[nodiscard]] static std::string backendToString(EncoderBackend backend) noexcept;
    [[nodiscard]] static EncoderBackend stringToBackend(const std::string& name) noexcept;
};

} // namespace mist::encode
