#include "mist/decode/H264Parser.h"
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
            if (leadingZeros > 31) return false; // Prevent overflow
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

private:
    const uint8_t* data_;
    size_t size_;
    size_t bytePos_{0};
    uint32_t bitPos_{0};
};

// Strips emulation prevention bytes (0x00 0x00 0x03 -> 0x00 0x00)
std::vector<uint8_t> removeEmulationPrevention(const uint8_t* src, size_t size) {
    std::vector<uint8_t> rbsp;
    rbsp.reserve(size);
    size_t i = 0;
    while (i < size) {
        if (i + 2 < size && src[i] == 0 && src[i + 1] == 0 && src[i + 2] == 3) {
            rbsp.push_back(0);
            rbsp.push_back(0);
            i += 3; // Skip emulation prevention byte (0x03)
        } else {
            rbsp.push_back(src[i]);
            ++i;
        }
    }
    return rbsp;
}

} // namespace

std::vector<NalUnit> H264Parser::parseNalUnits(const uint8_t* data, size_t size) noexcept {
    std::vector<NalUnit> units;
    if (!data || size < 4) return units;

    size_t i = 0;
    int lastStartPos = -1;
    size_t lastStartCodeSize = 0;

    auto isStartCode = [&](size_t idx, size_t& scSize) -> bool {
        if (idx + 3 <= size && data[idx] == 0 && data[idx + 1] == 0 && data[idx + 2] == 1) {
            scSize = 3;
            return true;
        }
        if (idx + 4 <= size && data[idx] == 0 && data[idx + 1] == 0 && data[idx + 2] == 0 && data[idx + 3] == 1) {
            scSize = 4;
            return true;
        }
        return false;
    };

    while (i + 3 <= size) {
        size_t scSize = 0;
        if (isStartCode(i, scSize)) {
            if (lastStartPos >= 0) {
                NalUnit unit{};
                unit.offset = static_cast<size_t>(lastStartPos);
                unit.startCodeSize = lastStartCodeSize;
                unit.headerOffset = unit.offset + unit.startCodeSize;
                unit.totalSize = i - unit.offset;
                unit.payloadSize = unit.totalSize - unit.startCodeSize;
                if (unit.headerOffset < size) {
                    unit.type = static_cast<NalType>(data[unit.headerOffset] & 0x1F);
                }
                units.push_back(unit);
            }
            lastStartPos = static_cast<int>(i);
            lastStartCodeSize = scSize;
            i += scSize;
        } else {
            ++i;
        }
    }

    if (lastStartPos >= 0 && static_cast<size_t>(lastStartPos) < size) {
        NalUnit unit{};
        unit.offset = static_cast<size_t>(lastStartPos);
        unit.startCodeSize = lastStartCodeSize;
        unit.headerOffset = unit.offset + unit.startCodeSize;
        unit.totalSize = size - unit.offset;
        unit.payloadSize = unit.totalSize - unit.startCodeSize;
        if (unit.headerOffset < size) {
            unit.type = static_cast<NalType>(data[unit.headerOffset] & 0x1F);
        }
        units.push_back(unit);
    }

    return units;
}

bool H264Parser::isKeyframeAccessUnit(const uint8_t* data, size_t size) noexcept {
    auto nals = parseNalUnits(data, size);
    for (const auto& nal : nals) {
        if (nal.type == NalType::IdrSlice) {
            return true;
        }
    }
    return false;
}

SpsMetadata H264Parser::parseSps(const uint8_t* spsPayload, size_t size) noexcept {
    SpsMetadata meta{};
    if (!spsPayload || size < 4) return meta;

    // Remove emulation prevention bytes from payload
    auto rbsp = removeEmulationPrevention(spsPayload, size);
    if (rbsp.size() < 4) return meta;

    BitReader reader(rbsp.data(), rbsp.size());

    // NAL header (1 byte): forbidden_zero_bit (1), nal_ref_idc (2), nal_unit_type (5)
    uint32_t header = 0;
    if (!reader.readBits(8, header)) return meta;

    uint32_t profileIdc = 0;
    if (!reader.readBits(8, profileIdc)) return meta;
    meta.profileIdc = profileIdc;

    uint32_t constraintFlags = 0;
    if (!reader.readBits(8, constraintFlags)) return meta;

    uint32_t levelIdc = 0;
    if (!reader.readBits(8, levelIdc)) return meta;
    meta.levelIdc = levelIdc;

    uint32_t spsId = 0;
    if (!reader.readUe(spsId)) return meta;

    if (profileIdc == 100 || profileIdc == 110 || profileIdc == 122 || profileIdc == 244 ||
        profileIdc == 44  || profileIdc == 83  || profileIdc == 86  || profileIdc == 118 ||
        profileIdc == 128) {
        uint32_t chromaFormatIdc = 0;
        if (!reader.readUe(chromaFormatIdc)) return meta;
        if (chromaFormatIdc == 3) {
            uint32_t separateColourPlane = 0;
            if (!reader.readBit(separateColourPlane)) return meta;
        }
        uint32_t bitDepthLuma = 0;
        if (!reader.readUe(bitDepthLuma)) return meta;
        uint32_t bitDepthChroma = 0;
        if (!reader.readUe(bitDepthChroma)) return meta;

        uint32_t qpprimeYZeroTransformBypass = 0;
        if (!reader.readBit(qpprimeYZeroTransformBypass)) return meta;

        uint32_t seqScalingMatrixPresent = 0;
        if (!reader.readBit(seqScalingMatrixPresent)) return meta;
        if (seqScalingMatrixPresent) {
            uint32_t count = (chromaFormatIdc != 3) ? 8 : 12;
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t seqScalingListPresent = 0;
                if (!reader.readBit(seqScalingListPresent)) return meta;
                if (seqScalingListPresent) {
                    uint32_t sizeOfScalingList = (i < 6) ? 16 : 64;
                    uint32_t lastScale = 8;
                    uint32_t nextScale = 8;
                    for (uint32_t j = 0; j < sizeOfScalingList; ++j) {
                        if (nextScale != 0) {
                            uint32_t deltaScale = 0;
                            if (!reader.readUe(deltaScale)) return meta;
                            nextScale = (lastScale + deltaScale) & 0xFF;
                        }
                        lastScale = (nextScale == 0) ? lastScale : nextScale;
                    }
                }
            }
        }
    }

    uint32_t log2MaxFrameNumMinus4 = 0;
    if (!reader.readUe(log2MaxFrameNumMinus4)) return meta;

    uint32_t picOrderCntType = 0;
    if (!reader.readUe(picOrderCntType)) return meta;
    if (picOrderCntType == 0) {
        uint32_t log2MaxPicOrderCntLsbMinus4 = 0;
        if (!reader.readUe(log2MaxPicOrderCntLsbMinus4)) return meta;
    } else if (picOrderCntType == 1) {
        uint32_t deltaPicOrderAlwaysZero = 0;
        if (!reader.readBit(deltaPicOrderAlwaysZero)) return meta;
        uint32_t offsetForNonRefPic = 0;
        if (!reader.readUe(offsetForNonRefPic)) return meta;
        uint32_t offsetForTopToBottomField = 0;
        if (!reader.readUe(offsetForTopToBottomField)) return meta;
        uint32_t numRefFramesInPicOrderCntCycle = 0;
        if (!reader.readUe(numRefFramesInPicOrderCntCycle)) return meta;
        for (uint32_t i = 0; i < numRefFramesInPicOrderCntCycle; ++i) {
            uint32_t offset = 0;
            if (!reader.readUe(offset)) return meta;
        }
    }

    uint32_t maxNumRefFrames = 0;
    if (!reader.readUe(maxNumRefFrames)) return meta;

    uint32_t gapsInFrameNumAllowed = 0;
    if (!reader.readBit(gapsInFrameNumAllowed)) return meta;

    uint32_t picWidthInMbsMinus1 = 0;
    if (!reader.readUe(picWidthInMbsMinus1)) return meta;

    uint32_t picHeightInMapUnitsMinus1 = 0;
    if (!reader.readUe(picHeightInMapUnitsMinus1)) return meta;

    uint32_t frameMbsOnlyFlag = 0;
    if (!reader.readBit(frameMbsOnlyFlag)) return meta;

    if (!frameMbsOnlyFlag) {
        uint32_t mbAdaptiveFrameFieldFlag = 0;
        if (!reader.readBit(mbAdaptiveFrameFieldFlag)) return meta;
    }

    uint32_t direct8x8InferenceFlag = 0;
    if (!reader.readBit(direct8x8InferenceFlag)) return meta;

    uint32_t frameCroppingFlag = 0;
    if (!reader.readBit(frameCroppingFlag)) return meta;

    uint32_t cropLeft = 0, cropRight = 0, cropTop = 0, cropBottom = 0;
    if (frameCroppingFlag) {
        if (!reader.readUe(cropLeft)) return meta;
        if (!reader.readUe(cropRight)) return meta;
        if (!reader.readUe(cropTop)) return meta;
        if (!reader.readUe(cropBottom)) return meta;
    }

    meta.width = (picWidthInMbsMinus1 + 1) * 16 - (cropLeft + cropRight) * 2;
    meta.height = ((2 - frameMbsOnlyFlag) * (picHeightInMapUnitsMinus1 + 1) * 16) - (cropTop + cropBottom) * 2;
    meta.isValid = (meta.width > 0 && meta.height > 0);
    return meta;
}

std::vector<AccessUnit> H264Parser::extractAccessUnits(const uint8_t* data, size_t size, uint32_t targetFps) {
    std::vector<AccessUnit> accessUnits;
    if (!data || size == 0) return accessUnits;

    auto nals = parseNalUnits(data, size);
    if (nals.empty()) return accessUnits;

    const uint64_t frameIntervalNs = (targetFps > 0) ? (1000000000ULL / targetFps) : 16666666ULL;
    uint64_t seq = 0;

    std::vector<size_t> auStartIndices;

    for (size_t idx = 0; idx < nals.size(); ++idx) {
        const auto& nal = nals[idx];
        if (nal.type == NalType::Aud) {
            auStartIndices.push_back(idx);
        } else if (nal.type == NalType::Sps && (auStartIndices.empty() || auStartIndices.back() != idx)) {
            // SPS often accompanies the start of a keyframe AU if AUD is absent
            if (idx == 0 || nals[idx - 1].type == NalType::IdrSlice || nals[idx - 1].type == NalType::NonIdrSlice) {
                auStartIndices.push_back(idx);
            }
        }
    }

    // If no AUDs found, partition by slice boundaries
    if (auStartIndices.empty()) {
        for (size_t idx = 0; idx < nals.size(); ++idx) {
            if (nals[idx].type == NalType::IdrSlice || nals[idx].type == NalType::NonIdrSlice) {
                auStartIndices.push_back(idx);
            }
        }
    }

    if (auStartIndices.empty()) {
        // Fallback: entire buffer is one AU
        AccessUnit au{};
        au.data.assign(data, data + size);
        au.isKeyframe = isKeyframeAccessUnit(data, size);
        au.sequenceNumber = ++seq;
        au.timestampNs = seq * frameIntervalNs;
        accessUnits.push_back(std::move(au));
        return accessUnits;
    }

    for (size_t i = 0; i < auStartIndices.size(); ++i) {
        size_t nalStartIdx = auStartIndices[i];
        size_t nalEndIdx = (i + 1 < auStartIndices.size()) ? auStartIndices[i + 1] : nals.size();

        size_t byteStart = nals[nalStartIdx].offset;
        size_t byteEnd = (nalEndIdx < nals.size()) ? nals[nalEndIdx].offset : size;

        AccessUnit au{};
        au.data.assign(data + byteStart, data + byteEnd);
        au.isKeyframe = isKeyframeAccessUnit(au.data.data(), au.data.size());
        au.sequenceNumber = ++seq;
        au.timestampNs = seq * frameIntervalNs;

        // Check if AU has SPS for resolution
        for (size_t j = nalStartIdx; j < nalEndIdx; ++j) {
            if (nals[j].type == NalType::Sps) {
                auto spsMeta = parseSps(data + nals[j].headerOffset, nals[j].payloadSize);
                if (spsMeta.isValid) {
                    au.width = spsMeta.width;
                    au.height = spsMeta.height;
                }
            }
        }

        accessUnits.push_back(std::move(au));
    }

    return accessUnits;
}

} // namespace mist::decode
