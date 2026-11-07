#pragma once

#include "mist/common/Result.h"
#include "H264Parser.h"
#include <string>
#include <vector>

namespace mist::decode {

struct StreamInfo {
    uint32_t width{0};
    uint32_t height{0};
    uint32_t targetFps{60};
    size_t totalAccessUnits{0};
    size_t keyframeAccessUnits{0};
    size_t totalBytes{0};
    double estimatedDurationSeconds{0.0};
};

class AnnexBReader {
public:
    AnnexBReader() noexcept = default;

    [[nodiscard]] common::Result<void> openFile(const std::string& filePath, uint32_t targetFps = 60);
    [[nodiscard]] common::Result<void> openMemory(const uint8_t* data, size_t size, uint32_t targetFps = 60);

    [[nodiscard]] const StreamInfo& getStreamInfo() const noexcept { return info_; }
    [[nodiscard]] const std::vector<AccessUnit>& getAccessUnits() const noexcept { return accessUnits_; }
    [[nodiscard]] size_t size() const noexcept { return accessUnits_.size(); }
    [[nodiscard]] bool empty() const noexcept { return accessUnits_.empty(); }

    [[nodiscard]] const AccessUnit* getAccessUnit(size_t index) const noexcept {
        if (index < accessUnits_.size()) return &accessUnits_[index];
        return nullptr;
    }

private:
    StreamInfo info_;
    std::vector<AccessUnit> accessUnits_;
};

} // namespace mist::decode
