#include <gtest/gtest.h>
#include "mist/services/ISignalingClient.h"
#include "mist/services/WebSocketSignalingClient.h"
#include "mist/security/Security.h"

using namespace mist;
using namespace mist::services;

TEST(TestSignaling, MessageTypesAndStructure) {
    SignalingMessage offerMsg{};
    offerMsg.type = SignalingMessageType::Offer;
    offerMsg.payloadJson = "v=0\r\no=- 0 0 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\n";
    offerMsg.senderId = "host-1";
    offerMsg.targetId = "client-1";

    EXPECT_EQ(offerMsg.type, SignalingMessageType::Offer);
    EXPECT_FALSE(offerMsg.payloadJson.empty());

    SignalingMessage candidateMsg{};
    candidateMsg.type = SignalingMessageType::IceCandidate;
    candidateMsg.payloadJson = "candidate:1 1 UDP 2130706431 127.0.0.1 50000 typ host|video";
    EXPECT_EQ(candidateMsg.type, SignalingMessageType::IceCandidate);
}

TEST(TestSignaling, WebSocketClientLifecycle) {
    WebSocketSignalingClient client;
    EXPECT_FALSE(client.isConnected());
    EXPECT_EQ(client.getRole(), "host");

    client.setSessionInfo("session-test-42", "client");
    EXPECT_EQ(client.getSessionId(), "session-test-42");
    EXPECT_EQ(client.getRole(), "client");

    // Invalid connection URL handling
    security::AuthToken token{"test-token", 3600};
    // Should fail cleanly or attempt connection without crashing
    auto res = client.connect("ws://127.0.0.1:59999", token);
    // Even if open fails or connects in background, disconnect is safe
    (void)client.disconnect();
    EXPECT_FALSE(client.isConnected());
}

TEST(TestSignaling, MessageCallbackRegistration) {
    WebSocketSignalingClient client;
    bool callbackInvoked = false;
    client.setMessageCallback([&callbackInvoked](const SignalingMessage& /*msg*/) {
        callbackInvoked = true;
    });

    // Verify disconnect is clean
    auto discRes = client.disconnect();
    EXPECT_TRUE(discRes.hasValue());
    EXPECT_FALSE(callbackInvoked);
}
