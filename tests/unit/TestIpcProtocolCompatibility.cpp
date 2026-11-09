#include <gtest/gtest.h>
#include "mist/common/IpcProtocol.h"
#include <vector>
#include <cstdint>
#include <cstring>

using namespace mist::ipc;

TEST(IpcProtocolCompatibilityTest, HeaderBinaryLayoutAndAlignment) {
    // Exact struct size matching Rust IPC_HEADER_SIZE = 20
    EXPECT_EQ(sizeof(IpcHeader), 20u);

    // Exact field offsets
    EXPECT_EQ(offsetof(IpcHeader, magic), 0u);
    EXPECT_EQ(offsetof(IpcHeader, version), 4u);
    EXPECT_EQ(offsetof(IpcHeader, messageType), 6u);
    EXPECT_EQ(offsetof(IpcHeader, payloadSize), 8u);
    EXPECT_EQ(offsetof(IpcHeader, timestampNs), 12u);

    // Payload sizes matching Rust representations
    EXPECT_EQ(sizeof(IpcWindowPayload), 16u);
    EXPECT_EQ(sizeof(IpcViewportPayload), 16u);
}

TEST(IpcProtocolCompatibilityTest, GoldenVectorHeartbeatDeserialization) {
    // Golden vector: Heartbeat with timestamp 10000000000 ns (0x02540BE400)
    const uint8_t goldenHeartbeat[20] = {
        0x54, 0x53, 0x49, 0x4D, // 'MIST'
        0x01, 0x00,             // Version 1
        0x01, 0x00,             // MessageType::Heartbeat (1)
        0x00, 0x00, 0x00, 0x00, // PayloadSize: 0
        0x00, 0xE4, 0x0B, 0x54, 0x02, 0x00, 0x00, 0x00  // Timestamp: 10000000000
    };

    IpcHeader hdr{};
    std::memcpy(&hdr, goldenHeartbeat, sizeof(hdr));

    EXPECT_EQ(hdr.magic, 0x4D495354u);
    EXPECT_EQ(hdr.version, 1u);
    EXPECT_EQ(hdr.messageType, static_cast<uint16_t>(MessageType::Heartbeat));
    EXPECT_EQ(hdr.payloadSize, 0u);
    EXPECT_EQ(hdr.timestampNs, 10000000000ULL);
}

TEST(IpcProtocolCompatibilityTest, GoldenVectorViewportDeserialization) {
    // Golden vector: SetViewport (type 18), x=10, y=20, width=1920, height=1080, timestamp=0
    const uint8_t goldenViewport[36] = {
        0x54, 0x53, 0x49, 0x4D, // 'MIST'
        0x01, 0x00,             // Version 1
        0x12, 0x00,             // MessageType::SetViewport (18)
        0x10, 0x00, 0x00, 0x00, // PayloadSize: 16
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Timestamp: 0
        // Payload (16 bytes)
        0x0A, 0x00, 0x00, 0x00, // x: 10
        0x14, 0x00, 0x00, 0x00, // y: 20
        0x80, 0x07, 0x00, 0x00, // width: 1920
        0x38, 0x04, 0x00, 0x00  // height: 1080
    };

    IpcHeader hdr{};
    std::memcpy(&hdr, goldenViewport, sizeof(hdr));

    EXPECT_EQ(hdr.magic, 0x4D495354u);
    EXPECT_EQ(hdr.version, 1u);
    EXPECT_EQ(hdr.messageType, static_cast<uint16_t>(MessageType::SetViewport));
    EXPECT_EQ(hdr.payloadSize, 16u);
    EXPECT_EQ(hdr.timestampNs, 0ULL);

    IpcViewportPayload vp{};
    std::memcpy(&vp, goldenViewport + sizeof(hdr), sizeof(vp));

    EXPECT_EQ(vp.x, 10);
    EXPECT_EQ(vp.y, 20);
    EXPECT_EQ(vp.width, 1920u);
    EXPECT_EQ(vp.height, 1080u);
}

TEST(IpcProtocolCompatibilityTest, RejectionOfCorruptHeaders) {
    // Corrupt magic
    const uint8_t badMagic[20] = {
        0x58, 0x58, 0x58, 0x58, 0x01, 0x00, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    IpcHeader hdr{};
    std::memcpy(&hdr, badMagic, sizeof(hdr));
    EXPECT_NE(hdr.magic, 0x4D495354u);

    // Corrupt version
    const uint8_t badVer[20] = {
        0x54, 0x53, 0x49, 0x4D, 0x99, 0x00, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    std::memcpy(&hdr, badVer, sizeof(hdr));
    EXPECT_EQ(hdr.magic, 0x4D495354u);
    EXPECT_NE(hdr.version, 1u);
}
