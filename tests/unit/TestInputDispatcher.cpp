#include <gtest/gtest.h>
#include "mist/input/InputDispatcher.h"
#include "mist/input/InputReceiver.h"
#include <thread>
#include <chrono>

using namespace mist::input;

class InputDispatcherTest : public ::testing::Test {};

TEST_F(InputDispatcherTest, QueueBoundingAndDrop) {
    auto keyboard = std::make_shared<WindowsKeyboardInjector>(/*simulationOnly=*/true);
    auto mouse = std::make_shared<WindowsMouseInjector>(/*simulationOnly=*/true);
    auto gamepad = std::make_shared<HostVirtualGamepad>(/*simulationOnly=*/true);

    InputDispatcher dispatcher(keyboard, mouse, gamepad);
    // Note: Do not call initialize() so worker thread doesn't pop items immediately

    // Enqueue 100 discrete key events
    for (uint32_t i = 1; i <= 100; ++i) {
        InputPacketHeader hdr{};
        hdr.sequenceNumber = i;
        KeyEvent ev{0x41, i, KeyAction::Press, false};
        (void)dispatcher.enqueuePacket(hdr, ev);
    }

    // Queue depth must be bounded at 64
    EXPECT_LE(dispatcher.getQueueDepth(), 64u);
    EXPECT_GT(dispatcher.getEventsDropped(), 0u);
}

TEST_F(InputDispatcherTest, ContinuousMovementCoalescing) {
    auto keyboard = std::make_shared<WindowsKeyboardInjector>(/*simulationOnly=*/true);
    auto mouse = std::make_shared<WindowsMouseInjector>(/*simulationOnly=*/true);
    auto gamepad = std::make_shared<HostVirtualGamepad>(/*simulationOnly=*/true);

    InputDispatcher dispatcher(keyboard, mouse, gamepad);

    // Enqueue 50 rapid relative mouse moves without worker running
    for (int i = 0; i < 50; ++i) {
        InputPacketHeader hdr{};
        hdr.sequenceNumber = static_cast<uint32_t>(i + 1);
        MouseMoveEvent ev{2, 3, false, 0.0f, 0.0f};
        (void)dispatcher.enqueuePacket(hdr, ev);
    }

    // Since they are relative mouse moves, they coalesce into 1 pending slot!
    EXPECT_EQ(dispatcher.getQueueDepth(), 1u);
    EXPECT_EQ(dispatcher.getEventsDropped(), 0u);
}

TEST_F(InputDispatcherTest, ReceiverAndDispatcherIntegration) {
    auto permManager = std::make_shared<InputPermissionManager>("test-session");
    permManager->grantPermission(0x07);

    auto keyboard = std::make_shared<WindowsKeyboardInjector>(/*simulationOnly=*/true);
    auto mouse = std::make_shared<WindowsMouseInjector>(/*simulationOnly=*/true);
    auto gamepad = std::make_shared<HostVirtualGamepad>(/*simulationOnly=*/true);
    auto dispatcher = std::make_shared<InputDispatcher>(keyboard, mouse, gamepad);
    ASSERT_TRUE(dispatcher->initialize().hasValue());

    auto receiver = std::make_shared<InputReceiver>(permManager, dispatcher);

    // Send valid sequential packets
    for (uint32_t seq = 1; seq <= 5; ++seq) {
        InputPacketHeader hdr{};
        hdr.version = INPUT_PROTOCOL_VERSION;
        hdr.sequenceNumber = seq;
        std::strcpy(hdr.sessionId, "test-session");

        KeyEvent ev{0x41, seq, KeyAction::Press, false};
        auto bytes = InputProtocol::serializeKeyEvent(hdr, ev);
        auto res = receiver->processPacket("input-events", bytes);
        EXPECT_TRUE(res.hasValue());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_EQ(dispatcher->getEventsProcessed(), 5u);

    // Send stale/duplicate sequence number (seq = 3)
    InputPacketHeader staleHdr{};
    staleHdr.version = INPUT_PROTOCOL_VERSION;
    staleHdr.sequenceNumber = 3;
    std::strcpy(staleHdr.sessionId, "test-session");
    KeyEvent staleEv{0x41, 3, KeyAction::Release, false};
    auto staleBytes = InputProtocol::serializeKeyEvent(staleHdr, staleEv);
    auto staleRes = receiver->processPacket("input-events", staleBytes);
    EXPECT_FALSE(staleRes.hasValue());
    EXPECT_GT(receiver->getStaleDropped(), 0u);

    (void)dispatcher->shutdown();
}
