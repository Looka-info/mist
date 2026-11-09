#pragma once

#include "mist/encode/IVideoEncoder.h"
#include <string>

namespace mist::transport {

struct CodecCapabilities {
    bool supportsH264{true};
    bool supportsHevc{false};
    bool supportsAv1{false};
    bool supports10Bit{false};
    bool supportsHdr{false};
    std::string preferredCodec; // e.g. "hevc", "h264", "av1"
};

struct NegotiatedCodecResult {
    encode::VideoCodec selectedCodec{encode::VideoCodec::H264};
    std::string codecName{"H264"};
    bool isHardwareAccelerated{true};
    bool is10Bit{false};
    bool isHdr{false};
    int rtpPayloadType{96};
    std::string sdpFormatString;
    std::string rationale;
};

class CodecNegotiator {
public:
    [[nodiscard]] static NegotiatedCodecResult negotiate(
        const CodecCapabilities& hostCaps,
        const CodecCapabilities& clientCaps,
        const std::string& hostPreference = ""
    ) noexcept;

    [[nodiscard]] static std::string codecToString(encode::VideoCodec codec) noexcept;
    [[nodiscard]] static encode::VideoCodec stringToCodec(const std::string& name) noexcept;
};

} // namespace mist::transport
