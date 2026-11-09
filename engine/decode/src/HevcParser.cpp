#include "mist/decode/HevcParser.h"
#include <algorithm>

namespace mist::decode {

namespace {

class BitReader {
public:
    BitReader(const uint8_t* data, size_t size) noexcept
        : data_(data), size_(size) {}

    bool readBit(uint32_t& outBit) noexcept {
        if (bytePos_ >= size_) return false;
        outBit = (data_[bytePos_] >> (7 - bitPos_)) & 1;
        ++bitPos_;
        if (bitPos_ == 8) {
            bitPos_ = 0;
            ++bytePos_;
        }
        return true;
    }

    bool readBits(uint32_t numBits, uint32_t& outVal) noexcept {
        outVal = 0;
        for (uint32_t i = 0; i < numBits; ++i) {
            uint32_t bit = 0;
            if (!readBit(bit)) return false;
            outVal = (outVal << 1) | bit;
        }
        return true;
    }

    bool readUe(uint32_t& outVal) noexcept {
        uint32_t leadingZeros = 0;
        while (true) {
            uint32_t bit = 0;
            if (!readBit(bit)) return false;
            if (bit == 1) break;
            ++leadingZeros;
            if (leadingZeros > 31) return false;
        }
        if (leadingZeros == 0) {
            outVal = 0;
            return true;
        }
        uint32_t suffix = 0;
        if (!readBits(leadingZeros, suffix)) return false;
        outVal = (1u << leadingZeros) - 1u + suffix;
        return true;
    }

    bool skipBits(uint32_t numBits) noexcept {
        for (uint32_t i = 0; i < numBits; ++i) {
            uint32_t dummy = 0;
            if (!readBit(dummy)) return false;
        }
        return true;
    }

private:
    const uint8_t* data_;
    size_t size_;
    size_t bytePos_{0};
    uint32_t bitPos_{0};
};

std::vector<uint8_t> removeEmulationPrevention(const uint8_t* src, size_t size) {
    std::vector<uint8_t> rbsp;
    rbsp.reserve(size);
    size_t i = 0;
    while (i < size) {
        if (i + 2 < size && src[i] == 0 && src[i + 1] == 0 && src[i + 2] == 3) {
            rbsp.push_back(0);
            rbsp.push_back(0);
            i += 3;
        } else {
            rbsp.push_back(src[i]);
            ++i;
        }
    }
    return rbsp;
}

} // anonymous namespace

std::vector<HevcNalUnit> HevcParser::parseNalUnits(const uint8_t* data, size_t size) noexcept {
    std::vector<HevcNalUnit> units;
    if (!data || size < 5) return units;

    struct StartCodePos {
        size_t offset;
        size_t size;
    };
    std::vector<StartCodePos> startCodes;

    for (size_t i = 0; i + 2 < size; ++i) {
        if (data[i] == 0 && data[i + 1] == 0) {
            if (data[i + 2] == 1) {
                if (i > 0 && data[i - 1] == 0) {
                    startCodes.push_back({i - 1, 4});
                } else {
                    startCodes.push_back({i, 3});
                }
                i += 2;
            }
        }
    }

    if (startCodes.empty()) return units;

    units.reserve(startCodes.size());
    for (size_t i = 0; i < startCodes.size(); ++i) {
        const auto& cur = startCodes[i];
        size_t headerOffset = cur.offset + cur.size;
        if (headerOffset + 2 > size) continue;

        size_t nextOffset = (i + 1 < startCodes.size()) ? startCodes[i + 1].offset : size;
        size_t totalSize = nextOffset - cur.offset;
        size_t payloadSize = nextOffset - headerOffset;

        uint8_t byte0 = data[headerOffset];
        uint8_t byte1 = data[headerOffset + 1];

        uint8_t rawType = static_cast<uint8_t>((byte0 >> 1) & 0x3F);
        uint8_t layerId = static_cast<uint8_t>(((byte0 & 1) << 5) | ((byte1 >> 3) & 0x1F));
        uint8_t temporalId = static_cast<uint8_t>(byte1 & 0x07);

        HevcNalUnit unit{};
        unit.type = static_cast<HevcNalType>(rawType);
        unit.nuhLayerId = layerId;
        unit.nuhTemporalIdPlus1 = temporalId;
        unit.startCodeSize = cur.size;
        unit.offset = cur.offset;
        unit.headerOffset = headerOffset;
        unit.payloadSize = payloadSize;
        unit.totalSize = totalSize;

        units.push_back(unit);
    }

    return units;
}

bool HevcParser::isKeyframeAccessUnit(const uint8_t* data, size_t size) noexcept {
    auto units = parseNalUnits(data, size);
    for (const auto& u : units) {
        uint8_t typeVal = static_cast<uint8_t>(u.type);
        // HEVC IRAP types: 16 (BLA_W_LP) through 21 (CRA_NUT)
        if (typeVal >= 16 && typeVal <= 21) {
            return true;
        }
    }
    return false;
}

HevcSpsMetadata HevcParser::parseSps(const uint8_t* spsPayload, size_t size) noexcept {
    HevcSpsMetadata meta{};
    if (!spsPayload || size < 4) return meta;

    // Remove emulation prevention bytes
    auto rbsp = removeEmulationPrevention(spsPayload, size);
    if (rbsp.size() < 4) return meta;

    // Skip 2-byte NAL unit header
    BitReader reader(rbsp.data() + 2, rbsp.size() - 2);

    uint32_t spsVideoParamSetId = 0;
    if (!reader.readBits(4, spsVideoParamSetId)) return meta;

    uint32_t maxSubLayersMinus1 = 0;
    if (!reader.readBits(3, maxSubLayersMinus1)) return meta;

    uint32_t temporalIdNestingFlag = 0;
    if (!reader.readBit(temporalIdNestingFlag)) return meta;

    // Profile Tier Level
    uint32_t profileSpace = 0;
    if (!reader.readBits(2, profileSpace)) return meta;
    uint32_t tierFlag = 0;
    if (!reader.readBit(tierFlag)) return meta;
    uint32_t profileIdc = 0;
    if (!reader.readBits(5, profileIdc)) return meta;

    meta.profileIdc = profileIdc;

    // 32-bit compatibility flags + 4 constraint flags + 44 reserved = 80 bits
    if (!reader.skipBits(32 + 4 + 44)) return meta;

    uint32_t levelIdc = 0;
    if (!reader.readBits(8, levelIdc)) return meta;
    meta.levelIdc = levelIdc;

    // Sub-layer profile presence flags
    std::vector<uint32_t> subLayerProfilePresent(maxSubLayersMinus1);
    std::vector<uint32_t> subLayerLevelPresent(maxSubLayersMinus1);
    for (uint32_t i = 0; i < maxSubLayersMinus1; ++i) {
        if (!reader.readBit(subLayerProfilePresent[i])) return meta;
        if (!reader.readBit(subLayerLevelPresent[i])) return meta;
    }
    if (maxSubLayersMinus1 > 0) {
        if (!reader.skipBits(2 * (8 - maxSubLayersMinus1))) return meta;
    }

    for (uint32_t i = 0; i < maxSubLayersMinus1; ++i) {
        if (subLayerProfilePresent[i]) {
            if (!reader.skipBits(2 + 1 + 5 + 32 + 4 + 44)) return meta;
        }
        if (subLayerLevelPresent[i]) {
            if (!reader.skipBits(8)) return meta;
        }
    }

    uint32_t spsSeqParameterSetId = 0;
    if (!reader.readUe(spsSeqParameterSetId)) return meta;

    uint32_t chromaFormatIdc = 0;
    if (!reader.readUe(chromaFormatIdc)) return meta;
    if (chromaFormatIdc == 3) {
        uint32_t separateColourPlaneFlag = 0;
        if (!reader.readBit(separateColourPlaneFlag)) return meta;
    }

    uint32_t picWidthInLumaSamples = 0;
    if (!reader.readUe(picWidthInLumaSamples)) return meta;

    uint32_t picHeightInLumaSamples = 0;
    if (!reader.readUe(picHeightInLumaSamples)) return meta;

    uint32_t conformanceWindowFlag = 0;
    if (!reader.readBit(conformanceWindowFlag)) return meta;

    uint32_t cropLeft = 0, cropRight = 0, cropTop = 0, cropBottom = 0;
    if (conformanceWindowFlag) {
        if (!reader.readUe(cropLeft)) return meta;
        if (!reader.readUe(cropRight)) return meta;
        if (!reader.readUe(cropTop)) return meta;
        if (!reader.readUe(cropBottom)) return meta;
    }

    uint32_t subWidthC = (chromaFormatIdc == 1 || chromaFormatIdc == 2) ? 2 : 1;
    uint32_t subHeightC = (chromaFormatIdc == 1) ? 2 : 1;

    meta.width = picWidthInLumaSamples - (cropLeft + cropRight) * subWidthC;
    meta.height = picHeightInLumaSamples - (cropTop + cropBottom) * subHeightC;
    meta.isValid = (meta.width > 0 && meta.height > 0);

    return meta;
}

std::vector<AccessUnit> HevcParser::extractAccessUnits(
    const uint8_t* data,
    size_t size,
    uint32_t targetFps
) {
    std::vector<AccessUnit> aus;
    auto units = parseNalUnits(data, size);
    if (units.empty()) return aus;

    std::vector<size_t> auStartIndices;
    for (size_t i = 0; i < units.size(); ++i) {
        uint8_t t = static_cast<uint8_t>(units[i].type);
        // Start of new Access Unit: AUD (35), VPS (32), or first slice of picture
        if (t == static_cast<uint8_t>(HevcNalType::Aud) ||
            t == static_cast<uint8_t>(HevcNalType::Vps) ||
            (t >= 16 && t <= 21 && auStartIndices.empty())) {
            auStartIndices.push_back(i);
        }
    }

    if (auStartIndices.empty()) {
        auStartIndices.push_back(0);
    }

    uint64_t frameDurationNs = 1000000000ULL / std::max(1u, targetFps);

    for (size_t k = 0; k < auStartIndices.size(); ++k) {
        size_t startUnitIdx = auStartIndices[k];
        size_t endUnitIdx = (k + 1 < auStartIndices.size()) ? auStartIndices[k + 1] : units.size();

        size_t byteStart = units[startUnitIdx].offset;
        size_t byteEnd = (endUnitIdx < units.size()) ? units[endUnitIdx].offset : size;

        AccessUnit au{};
        au.data.assign(data + byteStart, data + byteEnd);
        au.sequenceNumber = k + 1;
        au.timestampNs = k * frameDurationNs;

        for (size_t u = startUnitIdx; u < endUnitIdx; ++u) {
            uint8_t t = static_cast<uint8_t>(units[u].type);
            if (t >= 16 && t <= 21) {
                au.isKeyframe = true;
            }
            if (units[u].type == HevcNalType::Sps) {
                auto spsMeta = parseSps(data + units[u].headerOffset, units[u].payloadSize);
                if (spsMeta.isValid) {
                    au.width = spsMeta.width;
                    au.height = spsMeta.height;
                }
            }
        }

        aus.push_back(std::move(au));
    }

    return aus;
}

} // namespace mist::decode
