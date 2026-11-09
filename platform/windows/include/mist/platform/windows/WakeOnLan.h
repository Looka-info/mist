#pragma once

#include "mist/common/Result.h"
#include <array>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace mist::platform::windows {

using MacAddress = std::array<uint8_t, 6>;

class WakeOnLan {
public:
    // Parse MAC address string (e.g. "00:1A:2B:3C:4D:5E" or "00-1A-2B-3C-4D-5E" or "001A2B3C4D5E")
    [[nodiscard]] static common::Result<MacAddress> parseMacAddress(std::string_view macStr) noexcept;

    // Format MAC address to standard string "XX:XX:XX:XX:XX:XX"
    [[nodiscard]] static std::string formatMacAddress(const MacAddress& mac) noexcept;

    // Create a 102-byte standard AMD Magic Packet
    // 6 bytes of 0xFF followed by 16 repetitions of the 6-byte target MAC
    [[nodiscard]] static std::vector<uint8_t> createMagicPacket(const MacAddress& mac) noexcept;

    // Send UDP broadcast packet to wake the remote host
    // simulationOnly skips raw socket calls for deterministic unit tests
    [[nodiscard]] static common::Result<void> sendMagicPacket(
        const MacAddress& mac,
        uint16_t port = 9,
        std::string_view broadcastIp = "255.255.255.255",
        bool simulationOnly = false
    );
};

} // namespace mist::platform::windows
