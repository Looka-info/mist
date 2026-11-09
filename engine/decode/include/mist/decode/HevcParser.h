#pragma once

#include "mist/decode/H264Parser.h" // for AccessUnit
#include <cstdint>
#include <cstddef>
#include <vector>
#include <span>

namespace mist::decode {

enum class HevcNalType : uint8_t {
    TrailN = 0,
    TrailR = 1,
    TsaN = 2,
    TsaR = 3,
    StsaN = 4,
    StsaR = 5,
    RadlN = 6,
    RadlR = 7,
    RaslN = 8,
    RaslR = 9,
    BlaWLp = 16,
    BlaWRadl = 17,
    BlaNLp = 18,
    IdrWRadl = 19,
    IdrNLp = 20,
    CraNut = 21,
    Vps = 32,
    Sps = 33,
    Pps = 34,
    Aud = 35,
    Eos = 36,
    Eob = 37,
    Fd = 38,
    PrefixSei = 39,
    SuffixSei = 40,
    Unknown = 63
};

struct HevcNalUnit {
    HevcNalType type{HevcNalType::Unknown};
    uint8_t nuhLayerId{0};
    uint8_t nuhTemporalIdPlus1{0};
    size_t startCodeSize{0}; // 3 or 4
    size_t offset{0};        // Byte offset from buffer start
    size_t headerOffset{0};  // Offset of NAL header byte
    size_t payloadSize{0};   // Size excluding start code
    size_t totalSize{0};     // Size including start code
};

struct HevcSpsMetadata {
    uint32_t profileIdc{0};
    uint32_t levelIdc{0};
    uint32_t width{0};
    uint32_t height{0};
    bool isValid{false};
};

class HevcParser {
public:
    [[nodiscard]] static std::vector<HevcNalUnit> parseNalUnits(const uint8_t* data, size_t size) noexcept;
    [[nodiscard]] static bool isKeyframeAccessUnit(const uint8_t* data, size_t size) noexcept;
    [[nodiscard]] static HevcSpsMetadata parseSps(const uint8_t* spsPayload, size_t size) noexcept;
    [[nodiscard]] static std::vector<AccessUnit> extractAccessUnits(const uint8_t* data, size_t size, uint32_t targetFps = 60);
};

} // namespace mist::decode
