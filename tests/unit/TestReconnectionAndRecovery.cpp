#include <gtest/gtest.h>
#include "mist/session/ReconnectionManager.h"
#include "mist/session/SessionResumeManager.h"
#include "mist/session/PowerStateManager.h"

using namespace mist;
using namespace mist::session;

TEST(ReconnectionManagerTest, ExponentialBackoffProgressionAndCapping) {
    ReconnectionConfig config{};
    config.initialIntervalMs = 500;
    config.maxIntervalMs = 4000;
    config.backoffMultiplier = 2.0f;
    config.maxAttempts = 5;
    config.jitterFactor = 0.0f; // Disable jitter for predictable math check

    ReconnectionManager manager(config);
    EXPECT_EQ(manager.getStatus(), ReconnectionStatus::Idle);
    EXPECT_FALSE(manager.isReconnecting());

    // Delay formula check
    EXPECT_EQ(manager.computeBackoffDelay(0), 500u);
    EXPECT_EQ(manager.computeBackoffDelay(1), 1000u);
    EXPECT_EQ(manager.computeBackoffDelay(2), 2000u);
    EXPECT_EQ(manager.computeBackoffDelay(3), 4000u);
    EXPECT_EQ(manager.computeBackoffDelay(4), 4000u); // Capped at maxIntervalMs
}

TEST(ReconnectionManagerTest, FastReconnectTransientWindow) {
    ReconnectionConfig config{};
    config.initialIntervalMs = 500;
    config.fastReconnectWindowMs = 3000;

    ReconnectionManager manager(config);

    // 1. Transient drop (0.5s ago) -> 0ms immediate retry
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    uint64_t nowNs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
    uint64_t recentDropNs = nowNs - 500000000ULL; // 0.5s ago

    uint32_t delay = manager.startReconnecting(recentDropNs);
    EXPECT_EQ(delay, 0u);
    EXPECT_EQ(manager.getStatus(), ReconnectionStatus::Reconnecting);

    // 2. Old drop (10s ago) -> normal initial delay (> 0ms)
    uint64_t oldDropNs = nowNs - 10000000000ULL;
    manager.reset();
    uint32_t oldDelay = manager.startReconnecting(oldDropNs);
    EXPECT_GT(oldDelay, 0u);
}

TEST(ReconnectionManagerTest, ExhaustionAfterMaxAttempts) {
    ReconnectionConfig config{};
    config.initialIntervalMs = 100;
    config.maxAttempts = 3;

    ReconnectionManager manager(config);
    manager.startReconnecting(0);

    // Attempt 1 fail
    auto res1 = manager.recordAttemptFailure();
    ASSERT_TRUE(res1.hasValue());
    EXPECT_EQ(manager.getAttemptCount(), 1u);
    EXPECT_EQ(manager.getStatus(), ReconnectionStatus::WaitingRetry);

    // Attempt 2 fail
    auto res2 = manager.recordAttemptFailure();
    ASSERT_TRUE(res2.hasValue());
    EXPECT_EQ(manager.getAttemptCount(), 2u);

    // Attempt 3 fail -> Exhausted
    auto res3 = manager.recordAttemptFailure();
    EXPECT_FALSE(res3.hasValue());
    EXPECT_EQ(res3.error().code(), common::ErrorCode::ResourceExhausted);
    EXPECT_EQ(manager.getStatus(), ReconnectionStatus::Exhausted);
}

TEST(SessionResumeTest, CryptographicTokenIssueAndConsumption) {
    std::string secret = "test_host_master_secret_key_12345";
    SessionResumeConfig config{};
    config.tokenValiditySec = 180;

    SessionResumeManager manager(secret, config);

    std::string sessionId = "sess-alpha-99";
    std::string guestId = "guest-xyz";
    uint64_t currentSec = 1700000000ULL;

    auto issueRes = manager.issueToken(sessionId, guestId, currentSec);
    ASSERT_TRUE(issueRes.hasValue());
    const auto& token = issueRes.value();
    EXPECT_FALSE(token.rawToken.empty());
    EXPECT_EQ(token.sessionId, sessionId);
    EXPECT_EQ(token.guestId, guestId);
    EXPECT_EQ(token.expiresAtSec, currentSec + 180);

    // 1. Valid resume
    auto valRes = manager.validateAndConsumeToken(token.rawToken, guestId, currentSec + 10);
    ASSERT_TRUE(valRes.hasValue());
    EXPECT_EQ(valRes.value(), sessionId);

    // 2. Replay attack: consuming the same token again must fail
    auto replayRes = manager.validateAndConsumeToken(token.rawToken, guestId, currentSec + 12);
    EXPECT_FALSE(replayRes.hasValue());
    EXPECT_EQ(replayRes.error().code(), common::ErrorCode::AuthenticationFailed);
}

TEST(SessionResumeTest, ExpiredTokenRejected) {
    std::string secret = "test_host_master_secret_key_12345";
    SessionResumeConfig config{};
    config.tokenValiditySec = 60;

    SessionResumeManager manager(secret, config);
    uint64_t currentSec = 1700000000ULL;

    auto issueRes = manager.issueToken("sess-1", "guest-1", currentSec);
    ASSERT_TRUE(issueRes.hasValue());

    // Validate 100 seconds later (past 60s validity)
    auto res = manager.validateAndConsumeToken(issueRes.value().rawToken, "guest-1", currentSec + 100);
    EXPECT_FALSE(res.hasValue());
    EXPECT_EQ(res.error().code(), common::ErrorCode::Timeout);
}

TEST(SessionResumeTest, TamperedTokenRejected) {
    std::string secret = "test_host_master_secret_key_12345";
    SessionResumeManager manager(secret);
    uint64_t currentSec = 1700000000ULL;

    auto issueRes = manager.issueToken("sess-1", "guest-1", currentSec);
    ASSERT_TRUE(issueRes.hasValue());

    std::string tampered = issueRes.value().rawToken;
    // Alter signature hex at the end
    tampered.back() = (tampered.back() == 'a') ? 'b' : 'a';

    auto res = manager.validateAndConsumeToken(tampered, "guest-1", currentSec + 5);
    EXPECT_FALSE(res.hasValue());
    EXPECT_EQ(res.error().code(), common::ErrorCode::AuthenticationFailed);
}

TEST(PowerStateTest, SuspendResumeLifecycleAndHooks) {
    PowerStateManager manager;
    EXPECT_EQ(manager.getPowerState(), PowerState::Active);
    EXPECT_FALSE(manager.isSuspended());

    int suspendHookRan = 0;
    int resumeHookRan = 0;

    manager.registerSuspendHook("FreezePipeline", [&]() {
        suspendHookRan++;
    });

    manager.registerResumeHook("ResetDevice", [&]() {
        resumeHookRan++;
    });

    // 1. Suspend transition
    manager.notifySuspend();
    EXPECT_EQ(manager.getPowerState(), PowerState::Suspended);
    EXPECT_TRUE(manager.isSuspended());
    EXPECT_EQ(manager.getSuspendCount(), 1u);
    EXPECT_EQ(suspendHookRan, 1);

    // 2. Resume transition
    manager.notifyResume();
    EXPECT_EQ(manager.getPowerState(), PowerState::Active);
    EXPECT_FALSE(manager.isSuspended());
    EXPECT_EQ(manager.getResumeCount(), 1u);
    EXPECT_EQ(resumeHookRan, 1);

    // 3. Win32 power broadcast handler
    EXPECT_TRUE(manager.handlePowerBroadcast(0x0004 /* PBT_APMSUSPEND */));
    EXPECT_TRUE(manager.isSuspended());
    EXPECT_EQ(manager.getSuspendCount(), 2u);

    EXPECT_TRUE(manager.handlePowerBroadcast(0x0012 /* PBT_APMRESUMEAUTOMATIC */));
    EXPECT_FALSE(manager.isSuspended());
    EXPECT_EQ(manager.getResumeCount(), 2u);
}
