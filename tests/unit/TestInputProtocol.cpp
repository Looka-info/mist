#include <gtest/gtest.h>
#include "mist/input/InputProtocol.h"
#include <cmath>

using namespace mist::input;

class InputProtocolTest : public ::testing::Test {};

TEST_F(InputProtocolTest, KeyEventRoundTrip) {
    InputPacketHeader hdr{};
    hdr.version = INPUT_PROTOCOL_VERSION;
    hdr.sequenceNumber = 42;
    hdr.timestampNs = 123456789;
    std::strcpy(hdr.sessionId, "session-test-key");

    KeyEvent ev{};
    ev.virtualKey = 0x41;
    ev.scanCode = 0x1E;
    ev.action = KeyAction::Press;
    ev.isExtended = false;

    auto bytes = InputProtocol::serializeKeyEvent(hdr, ev);
    EXPECT_GT(bytes.size(), sizeof(InputPacketHeader));

    auto res = InputProtocol::deserialize(bytes);
    ASSERT_TRUE(res.hasValue());

    const auto& [outHdr, outEv] = res.value();
    EXPECT_EQ(outHdr.version, INPUT_PROTOCOL_VERSION);
    EXPECT_EQ(outHdr.sequenceNumber, 42u);
    EXPECT_EQ(outHdr.timestampNs, 123456789u);
    EXPECT_STREQ(outHdr.sessionId, "session-test-key");

    ASSERT_TRUE(std::holds_alternative<KeyEvent>(outEv));
    const auto& parsed = std::get<KeyEvent>(outEv);
    EXPECT_EQ(parsed.virtualKey, 0x41u);
    EXPECT_EQ(parsed.scanCode, 0x1Eu);
    EXPECT_EQ(parsed.action, KeyAction::Press);
    EXPECT_FALSE(parsed.isExtended);
}

TEST_F(InputProtocolTest, MouseMoveRoundTrip) {
    InputPacketHeader hdr{};
    hdr.sequenceNumber = 100;
    std::strcpy(hdr.sessionId, "session-test-mouse");

    MouseMoveEvent ev{};
    ev.deltaX = -15;
    ev.deltaY = 32;
    ev.isAbsolute = true;
    ev.normalizedX = 0.45f;
    ev.normalizedY = 0.85f;

    auto bytes = InputProtocol::serializeMouseMoveEvent(hdr, ev);
    auto res = InputProtocol::deserialize(bytes);
    ASSERT_TRUE(res.hasValue());

    const auto& [outHdr, outEv] = res.value();
    ASSERT_TRUE(std::holds_alternative<MouseMoveEvent>(outEv));
    const auto& parsed = std::get<MouseMoveEvent>(outEv);
    EXPECT_EQ(parsed.deltaX, -15);
    EXPECT_EQ(parsed.deltaY, 32);
    EXPECT_TRUE(parsed.isAbsolute);
    EXPECT_NEAR(parsed.normalizedX, 0.45f, 0.001f);
    EXPECT_NEAR(parsed.normalizedY, 0.85f, 0.001f);
}

TEST_F(InputProtocolTest, MouseButtonAndWheelRoundTrip) {
    InputPacketHeader hdr{};
    hdr.sequenceNumber = 101;

    // Button
    MouseButtonEvent btnEv{MouseButton::Right, true};
    auto btnBytes = InputProtocol::serializeMouseButtonEvent(hdr, btnEv);
    auto btnRes = InputProtocol::deserialize(btnBytes);
    ASSERT_TRUE(btnRes.hasValue());
    ASSERT_TRUE(std::holds_alternative<MouseButtonEvent>(btnRes.value().second));
    EXPECT_EQ(std::get<MouseButtonEvent>(btnRes.value().second).button, MouseButton::Right);
    EXPECT_TRUE(std::get<MouseButtonEvent>(btnRes.value().second).isDown);

    // Wheel
    MouseWheelEvent wheelEv{120, -240};
    auto wheelBytes = InputProtocol::serializeMouseWheelEvent(hdr, wheelEv);
    auto wheelRes = InputProtocol::deserialize(wheelBytes);
    ASSERT_TRUE(wheelRes.hasValue());
    ASSERT_TRUE(std::holds_alternative<MouseWheelEvent>(wheelRes.value().second));
    EXPECT_EQ(std::get<MouseWheelEvent>(wheelRes.value().second).wheelDeltaX, 120);
    EXPECT_EQ(std::get<MouseWheelEvent>(wheelRes.value().second).wheelDeltaY, -240);
}

TEST_F(InputProtocolTest, GamepadRoundTrip) {
    InputPacketHeader hdr{};
    hdr.sequenceNumber = 102;

    GamepadState g{};
    g.index = 1;
    g.buttons = 0x0008; // Start
    g.leftTrigger = 200;
    g.rightTrigger = 250;
    g.thumbLX = -12000;
    g.thumbLY = 18000;
    g.thumbRX = 25000;
    g.thumbRY = -30000;
    g.isConnected = true;

    auto bytes = InputProtocol::serializeGamepadState(hdr, g);
    auto res = InputProtocol::deserialize(bytes);
    ASSERT_TRUE(res.hasValue());

    ASSERT_TRUE(std::holds_alternative<GamepadState>(res.value().second));
    const auto& parsed = std::get<GamepadState>(res.value().second);
    EXPECT_EQ(parsed.index, 1u);
    EXPECT_EQ(parsed.buttons, 0x0008u);
    EXPECT_EQ(parsed.leftTrigger, 200u);
    EXPECT_EQ(parsed.rightTrigger, 250u);
    EXPECT_EQ(parsed.thumbLX, -12000);
    EXPECT_EQ(parsed.thumbLY, 18000);
    EXPECT_EQ(parsed.thumbRX, 25000);
    EXPECT_EQ(parsed.thumbRY, -30000);
    EXPECT_TRUE(parsed.isConnected);
}

TEST_F(InputProtocolTest, SizeAndVersionGuards) {
    // 1. Smaller than header
    std::vector<uint8_t> smallBuf(30, 0);
    auto resSmall = InputProtocol::deserialize(smallBuf);
    EXPECT_FALSE(resSmall.hasValue());

    // 2. Larger than MAX_INPUT_PACKET_SIZE
    std::vector<uint8_t> largeBuf(300, 0);
    auto resLarge = InputProtocol::deserialize(largeBuf);
    EXPECT_FALSE(resLarge.hasValue());

    // 3. Invalid version
    InputPacketHeader badHdr{};
    badHdr.version = 999;
    KeyEvent ev{};
    auto badBytes = InputProtocol::serializeKeyEvent(badHdr, ev);
    auto resBadVer = InputProtocol::deserialize(badBytes);
    EXPECT_FALSE(resBadVer.hasValue());
}
