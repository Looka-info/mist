#include "mist/decode/AnnexBReader.h"
#include "mist/common/Logger.h"
#include <fstream>

namespace mist::decode {

common::Result<void> AnnexBReader::openFile(const std::string& filePath, uint32_t targetFps) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::NotFound,
                             "Failed to open H.264 file: " + filePath);
    }

    auto fileSize = file.tellg();
    if (fileSize <= 0) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::InvalidArgument,
                             "H.264 file is empty: " + filePath);
    }

    // Safety limit: 500 MB max for in-memory offline test bitstreams
    constexpr size_t maxAllowedSize = 500 * 1024 * 1024;
    if (static_cast<size_t>(fileSize) > maxAllowedSize) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::ResourceExhausted,
                             "H.264 file exceeds maximum supported test size (500 MB)");
    }

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

    return openMemory(buffer.data(), buffer.size(), targetFps);
}

common::Result<void> AnnexBReader::openMemory(const uint8_t* data, size_t size, uint32_t targetFps) {
    if (!data || size == 0) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::InvalidArgument,
                             "Buffer is empty or null");
    }

    accessUnits_ = H264Parser::extractAccessUnits(data, size, targetFps);
    if (accessUnits_.empty()) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::InternalError,
                             "No valid H.264 access units found in bitstream");
    }

    info_.totalBytes = size;
    info_.targetFps = targetFps;
    info_.totalAccessUnits = accessUnits_.size();
    info_.keyframeAccessUnits = 0;
    info_.width = 1920;
    info_.height = 1080;

    for (const auto& au : accessUnits_) {
        if (au.isKeyframe) {
            ++info_.keyframeAccessUnits;
        }
        if (au.width > 0 && au.height > 0) {
            info_.width = au.width;
            info_.height = au.height;
        }
    }

    info_.estimatedDurationSeconds = (targetFps > 0) ? (static_cast<double>(info_.totalAccessUnits) / targetFps) : 0.0;

    MIST_LOG_INFO(common::SubsystemId::Decode, "Parsed Annex-B H.264 stream: " +
                  std::to_string(info_.totalAccessUnits) + " AUs (" +
                  std::to_string(info_.keyframeAccessUnits) + " keyframes, " +
                  std::to_string(info_.width) + "x" + std::to_string(info_.height) + " @ " +
                  std::to_string(info_.targetFps) + " FPS, " +
                  std::to_string(info_.totalBytes) + " bytes)");

    return common::Result<void>::success();
}

} // namespace mist::decode
