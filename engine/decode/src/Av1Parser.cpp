#include "mist/decode/Av1Parser.h"
#include <algorithm>

namespace mist::decode {

bool Av1Parser::readLeb128(const uint8_t* data, size_t size, size_t& offset, uint64_t& value, size_t& bytesRead) {
    value = 0;
    bytesRead = 0;
    for (size_t i = 0; i < 8; ++i) {
        if (offset >= size) {
            return false;
        }
        uint8_t byte = data[offset++];
        value |= static_cast<uint64_t>(byte & 0x7F) << (i * 7);
        ++bytesRead;
        if (!(byte & 0x80)) {
            return true;
        }
    }
    return false;
}

size_t Av1Parser::writeLeb128(uint64_t value, uint8_t* out) {
    size_t len = 0;
    do {
        uint8_t byte = static_cast<uint8_t>(value & 0x7F);
        value >>= 7;
        if (value > 0) {
            byte |= 0x80;
        }
        if (out) {
            out[len] = byte;
        }
        ++len;
    } while (value > 0);
    return len;
}

std::vector<Av1ObuUnit> Av1Parser::parseObus(const uint8_t* data, size_t size) {
    std::vector<Av1ObuUnit> obus;
    if (!data || size == 0) {
        return obus;
    }

    size_t offset = 0;
    while (offset < size) {
        size_t obuStart = offset;
        uint8_t headerByte = data[offset++];

        // bit 7 must be 0 (obu_forbidden_bit)
        if (headerByte & 0x80) {
            break;
        }

        uint8_t obuTypeInt = (headerByte >> 3) & 0x0F;
        bool hasExtension = (headerByte & 0x04) != 0;
        bool hasSizeField = (headerByte & 0x02) != 0;

        uint8_t temporalId = 0;
        uint8_t spatialId = 0;

        if (hasExtension) {
            if (offset >= size) break;
            uint8_t extByte = data[offset++];
            temporalId = (extByte >> 5) & 0x07;
            spatialId = (extByte >> 3) & 0x03;
        }

        size_t payloadSize = 0;
        if (hasSizeField) {
            uint64_t val = 0;
            size_t lebBytes = 0;
            if (!readLeb128(data, size, offset, val, lebBytes)) {
                break;
            }
            payloadSize = static_cast<size_t>(val);
        } else {
            payloadSize = size - offset;
        }

        size_t headerSize = offset - obuStart;
        size_t totalSize = headerSize + payloadSize;

        Av1ObuUnit unit{
            .type = static_cast<Av1ObuType>(obuTypeInt),
            .offset = obuStart,
            .headerSize = headerSize,
            .payloadSize = payloadSize,
            .totalSize = totalSize,
            .hasExtension = hasExtension,
            .temporalId = temporalId,
            .spatialId = spatialId
        };
        obus.push_back(unit);

        offset += payloadSize;
    }

    return obus;
}

bool Av1Parser::isKeyframeAccessUnit(const uint8_t* data, size_t size) {
    auto obus = parseObus(data, size);
    for (const auto& obu : obus) {
        if (obu.type == Av1ObuType::SequenceHeader) {
            return true;
        }
        if (obu.type == Av1ObuType::Frame || obu.type == Av1ObuType::FrameHeader) {
            size_t payloadOffset = obu.offset + obu.headerSize;
            if (payloadOffset < size) {
                // In uncompressed frame header:
                // If show_existing_frame bit is 0, frame_type is the next 2 bits.
                // frame_type 0 = KEY_FRAME
                uint8_t firstPayloadByte = data[payloadOffset];
                bool showExistingFrame = (firstPayloadByte & 0x80) != 0;
                if (!showExistingFrame) {
                    uint8_t frameType = (firstPayloadByte >> 5) & 0x03;
                    if (frameType == 0) { // KEY_FRAME
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

Av1SequenceMetadata Av1Parser::parseSequenceHeader(const uint8_t* data, size_t size) {
    Av1SequenceMetadata meta{};
    auto obus = parseObus(data, size);
    for (const auto& obu : obus) {
        if (obu.type == Av1ObuType::SequenceHeader) {
            size_t payloadOffset = obu.offset + obu.headerSize;
            if (payloadOffset < size && obu.payloadSize >= 4) {
                uint8_t b0 = data[payloadOffset];
                meta.profile = (b0 >> 5) & 0x07;
                // Check 10-bit / high bitdepth flag
                meta.is10Bit = (b0 & 0x01) != 0 || meta.profile == 2;
                meta.bitDepth = meta.is10Bit ? 10 : 8;
                meta.isHdr = meta.is10Bit;
                meta.level = (obu.payloadSize > 1) ? (data[payloadOffset + 1] >> 3) & 0x1F : 0;
                meta.isValid = true;
                return meta;
            }
        }
    }
    return meta;
}

} // namespace mist::decode
