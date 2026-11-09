#include "mist/transport/CodecNegotiator.h"
#include <algorithm>
#include <cctype>

namespace mist::transport {

namespace {

std::string toLower(std::string s) noexcept {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

} // namespace

NegotiatedCodecResult CodecNegotiator::negotiate(
    const CodecCapabilities& hostCaps,
    const CodecCapabilities& clientCaps,
    const std::string& hostPreference
) noexcept {
    NegotiatedCodecResult result;

    std::string pref = toLower(hostPreference);
    if (pref.empty()) {
        pref = toLower(hostCaps.preferredCodec);
    }
    if (pref.empty()) {
        pref = toLower(clientCaps.preferredCodec);
    }

    // 1. If explicitly requested HEVC/H265:
    if (pref == "hevc" || pref == "h265") {
        if (hostCaps.supportsHevc && clientCaps.supportsHevc) {
            result.selectedCodec = encode::VideoCodec::HEVC;
            result.codecName = "H265";
            result.rtpPayloadType = 98;
            result.sdpFormatString = "H265/90000";
            result.isHardwareAccelerated = true;
            result.rationale = "HEVC preferred and supported by both host and client";
            return result;
        }
    }

    // 2. If explicitly requested H264:
    if (pref == "h264") {
        if (hostCaps.supportsH264 && clientCaps.supportsH264) {
            result.selectedCodec = encode::VideoCodec::H264;
            result.codecName = "H264";
            result.rtpPayloadType = 96;
            result.sdpFormatString = "H264/90000";
            result.isHardwareAccelerated = true;
            result.rationale = "H.264 explicitly preferred and supported by both peers";
            return result;
        }
    }

    // 3. If explicitly requested AV1:
    if (pref == "av1") {
        if (hostCaps.supportsAv1 && clientCaps.supportsAv1) {
            result.selectedCodec = encode::VideoCodec::AV1;
            result.codecName = "AV1";
            result.rtpPayloadType = 97;
            result.sdpFormatString = "AV1/90000";
            result.isHardwareAccelerated = true;
            result.is10Bit = hostCaps.supports10Bit && clientCaps.supports10Bit;
            result.isHdr = hostCaps.supportsHdr && clientCaps.supportsHdr;
            result.rationale = "AV1 preferred and supported by both peers (10-bit HDR: " +
                               std::string(result.isHdr ? "enabled" : "disabled") + ")";
            return result;
        }
    }

    // 4. Default priority: AV1 (if both support) > HEVC (if both support) > H264 (if both support)
    if (hostCaps.supportsAv1 && clientCaps.supportsAv1) {
        result.selectedCodec = encode::VideoCodec::AV1;
        result.codecName = "AV1";
        result.rtpPayloadType = 97;
        result.sdpFormatString = "AV1/90000";
        result.isHardwareAccelerated = true;
        result.is10Bit = hostCaps.supports10Bit && clientCaps.supports10Bit;
        result.isHdr = hostCaps.supportsHdr && clientCaps.supportsHdr;
        result.rationale = "Both peers support next-gen AV1 codec (maximum compression efficiency)";
        return result;
    }

    if (hostCaps.supportsHevc && clientCaps.supportsHevc) {
        result.selectedCodec = encode::VideoCodec::HEVC;
        result.codecName = "H265";
        result.rtpPayloadType = 98;
        result.sdpFormatString = "H265/90000";
        result.isHardwareAccelerated = true;
        result.is10Bit = hostCaps.supports10Bit && clientCaps.supports10Bit;
        result.isHdr = hostCaps.supportsHdr && clientCaps.supportsHdr;
        result.rationale = "Both peers support hardware-accelerated HEVC/H.265 (higher efficiency)";
        return result;
    }

    if (hostCaps.supportsH264 && clientCaps.supportsH264) {
        result.selectedCodec = encode::VideoCodec::H264;
        result.codecName = "H264";
        result.rtpPayloadType = 96;
        result.sdpFormatString = "H264/90000";
        result.isHardwareAccelerated = true;
        result.rationale = "Universal H.264 negotiated for maximum compatibility";
        return result;
    }

    // Fallback if neither preferred pair matches
    result.selectedCodec = encode::VideoCodec::H264;
    result.codecName = "H264";
    result.rtpPayloadType = 96;
    result.sdpFormatString = "H264/90000";
    result.isHardwareAccelerated = false;
    result.rationale = "Fallback to baseline H.264";
    return result;
}

std::string CodecNegotiator::codecToString(encode::VideoCodec codec) noexcept {
    switch (codec) {
        case encode::VideoCodec::H264:
            return "H264";
        case encode::VideoCodec::HEVC:
            return "H265";
        case encode::VideoCodec::AV1:
            return "AV1";
    }
    return "Unknown";
}

encode::VideoCodec CodecNegotiator::stringToCodec(const std::string& name) noexcept {
    std::string s = toLower(name);
    if (s == "hevc" || s == "h265" || s == "h.265") {
        return encode::VideoCodec::HEVC;
    }
    if (s == "av1") {
        return encode::VideoCodec::AV1;
    }
    return encode::VideoCodec::H264;
}

} // namespace mist::transport
