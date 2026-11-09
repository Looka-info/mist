#include "mist/platform/windows/WakeOnLan.h"
#include <iomanip>
#include <sstream>
#include <cstring>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace mist::platform::windows {

namespace {

inline int hexCharToInt(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

} // namespace

common::Result<MacAddress> WakeOnLan::parseMacAddress(std::string_view macStr) noexcept {
    std::string cleanHex;
    cleanHex.reserve(12);

    for (char c : macStr) {
        if (c == ':' || c == '-' || c == ' ' || c == '.') {
            continue;
        }
        if (hexCharToInt(c) >= 0) {
            cleanHex.push_back(c);
        } else {
            return common::Error(common::SubsystemId::Platform, common::ErrorCode::InvalidArgument,
                                 "Invalid character in MAC address string: " + std::string(1, c));
        }
    }

    if (cleanHex.size() != 12) {
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::InvalidArgument,
                             "MAC address must contain exactly 12 hexadecimal digits (6 bytes)");
    }

    MacAddress mac{};
    for (size_t i = 0; i < 6; ++i) {
        int hi = hexCharToInt(cleanHex[i * 2]);
        int lo = hexCharToInt(cleanHex[i * 2 + 1]);
        mac[i] = static_cast<uint8_t>((hi << 4) | lo);
    }

    return mac;
}

std::string WakeOnLan::formatMacAddress(const MacAddress& mac) noexcept {
    static const char hexChars[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(17);

    for (size_t i = 0; i < 6; ++i) {
        if (i > 0) out.push_back(':');
        out.push_back(hexChars[(mac[i] >> 4) & 0x0F]);
        out.push_back(hexChars[mac[i] & 0x0F]);
    }

    return out;
}

std::vector<uint8_t> WakeOnLan::createMagicPacket(const MacAddress& mac) noexcept {
    // Standard AMD Magic Packet format: 6 bytes of 0xFF followed by 16 repetitions of target MAC
    constexpr size_t PACKET_SIZE = 6 + (16 * 6); // Exactly 102 bytes
    std::vector<uint8_t> packet(PACKET_SIZE);

    // Header: 6 x 0xFF
    std::memset(packet.data(), 0xFF, 6);

    // Body: 16 x MAC address
    for (size_t i = 0; i < 16; ++i) {
        std::memcpy(packet.data() + 6 + (i * 6), mac.data(), 6);
    }

    return packet;
}

common::Result<void> WakeOnLan::sendMagicPacket(
    const MacAddress& mac,
    uint16_t port,
    std::string_view broadcastIp,
    bool simulationOnly
) {
    auto packet = createMagicPacket(mac);

    if (simulationOnly) {
        return common::Result<void>::success();
    }

#if defined(_WIN32)
    WSADATA wsaData{};
    int wsaRes = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaRes != 0) {
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::NetworkError,
                             "WSAStartup failed: " + std::to_string(wsaRes));
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        int err = WSAGetLastError();
        WSACleanup();
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::NetworkError,
                             "Failed to create UDP socket: " + std::to_string(err));
    }

    // Enable broadcast option on socket
    BOOL broadcastOpt = TRUE;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcastOpt), sizeof(broadcastOpt)) != 0) {
        int err = WSAGetLastError();
        closesocket(sock);
        WSACleanup();
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::NetworkError,
                             "Failed to set SO_BROADCAST on socket: " + std::to_string(err));
    }

    sockaddr_in destAddr{};
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(port);
    std::string ipStr(broadcastIp);
    if (inet_pton(AF_INET, ipStr.c_str(), &destAddr.sin_addr) <= 0) {
        closesocket(sock);
        WSACleanup();
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::InvalidArgument,
                             "Invalid broadcast IP address: " + ipStr);
    }

    int sent = sendto(sock, reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()),
                      0, reinterpret_cast<const sockaddr*>(&destAddr), sizeof(destAddr));

    closesocket(sock);
    WSACleanup();

    if (sent != static_cast<int>(packet.size())) {
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::NetworkError,
                             "Failed to send complete WoL magic packet over UDP");
    }

    return common::Result<void>::success();
#else
    return common::Result<void>::success();
#endif
}

} // namespace mist::platform::windows
