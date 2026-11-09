#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <span>

namespace mist::decode {

enum class Av1ObuType : uint8_t {
    Unknown = 0,
    SequenceHeader = 1,
    TemporalDelimiter = 2,
    FrameHeader = 3,
    TileGroup = 4,
    Metadata = 5,
    Frame = 6,
    RedundantFrameHeader = 7,
    TileList = 8,
    Padding = 15
};

struct Av1ObuUnit {
    Av1ObuType type{Av1ObuType::Unknown};
    size_t offset{0};
    size_t headerSize{0};
    size_t payloadSize{0};
    size_t totalSize{0};
    bool hasExtension{false};
    uint8_t temporalId{0};
    uint8_t spatialId{0};
};

struct Av1SequenceMetadata {
    uint8_t profile{0}; // 0 = Main, 1 = High, 2 = Professional
    uint8_t level{0};
    uint32_t width{0};
    uint32_t height{0};
    uint32_t bitDepth{8};
    bool is10Bit{false};
    bool isHdr{false};
    bool isValid{false};
};

class Av1Parser {
public:
    [[nodiscard]] static std::vector<Av1ObuUnit> parseObus(const uint8_t* data, size_t size);
    [[nodiscard]] static bool isKeyframeAccessUnit(const uint8_t* data, size_t size);
    [[nodiscard]] static Av1SequenceMetadata parseSequenceHeader(const uint8_t* data, size_t size);

    // LEB128 variable-length integer parsing
    static bool readLeb128(const uint8_t* data, size_t size, size_t& offset, uint64_t& value, size_t& bytesRead);
    static size_t writeLeb128(uint64_t value, uint8_t* out);
};

} // namespace mist::decode
