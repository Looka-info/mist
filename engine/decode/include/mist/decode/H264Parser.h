#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <span>

namespace mist::decode {

enum class NalType : uint8_t {
    Unspecified = 0,
    NonIdrSlice = 1,
    SlicePartitionA = 2,
    SlicePartitionB = 3,
    SlicePartitionC = 4,
    IdrSlice = 5,
    Sei = 6,
    Sps = 7,
    Pps = 8,
    Aud = 9,
    EndOfSequence = 10,
    EndOfStream = 11,
    FillerData = 12,
    Unknown = 31
};

struct NalUnit {
    NalType type{NalType::Unspecified};
    size_t startCodeSize{0}; // 3 or 4
    size_t offset{0};        // Byte offset from buffer start
    size_t headerOffset{0};  // Offset of NAL header byte
    size_t payloadSize{0};   // Size excluding start code
    size_t totalSize{0};     // Size including start code
};

struct SpsMetadata {
    uint32_t profileIdc{0};
    uint32_t levelIdc{0};
    uint32_t width{0};
    uint32_t height{0};
    bool isValid{false};
};

struct AccessUnit {
    std::vector<uint8_t> data;
    bool isKeyframe{false};
    uint32_t width{0};
    uint32_t height{0};
    uint64_t sequenceNumber{0};
    uint64_t timestampNs{0};
};

class H264Parser {
public:
    [[nodiscard]] static std::vector<NalUnit> parseNalUnits(const uint8_t* data, size_t size) noexcept;
    [[nodiscard]] static bool isKeyframeAccessUnit(const uint8_t* data, size_t size) noexcept;
    [[nodiscard]] static SpsMetadata parseSps(const uint8_t* spsPayload, size_t size) noexcept;
    [[nodiscard]] static std::vector<AccessUnit> extractAccessUnits(const uint8_t* data, size_t size, uint32_t targetFps = 60);
};

} // namespace mist::decode
