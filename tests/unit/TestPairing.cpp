#include <gtest/gtest.h>
#include "mist/security/PairingManager.h"
#include <cstdio>

using namespace mist::security;

TEST(PairingTest, CodeGenerationAndFormat) {
    PairingManager pm;
    auto res = pm.generatePairingCode(300);
    ASSERT_TRUE(res.hasValue());
    std::string code = res.value();

    EXPECT_EQ(code.length(), 7); // XXX-XXX
    EXPECT_EQ(code[3], '-');
    EXPECT_EQ(pm.getActivePairingCode(), code);
}

TEST(PairingTest, ExpirationAndLockout) {
    PairingManager pm;
    uint64_t t = 1000;
    auto res = pm.generatePairingCode(10, t); // 10s lifetime starting at t
    ASSERT_TRUE(res.hasValue());
    std::string code = res.value();

    // 1. Invalid code attempt
    PairingRequest req{};
    req.deviceId = "client-dev-01";
    req.deviceName = "Living Room TV";
    req.pairingCode = "BAD-COD";
    auto failRes = pm.submitPairingRequest(req, t);
    EXPECT_FALSE(failRes.hasValue());

    // 2. 4 more failed attempts -> trigger lockout
    for (int i = 0; i < 4; ++i) {
        (void)pm.submitPairingRequest(req, t);
    }

    // Now even correct code should be rejected during lockout
    req.pairingCode = code;
    auto lockedRes = pm.submitPairingRequest(req, t);
    EXPECT_FALSE(lockedRes.hasValue());
    EXPECT_EQ(lockedRes.error().code(), mist::common::ErrorCode::PermissionDenied);

    // 3. Expiration test after lockout expires
    t += 70; // past 60s lockout, also past 10s code expiration
    auto expRes = pm.submitPairingRequest(req, t);
    EXPECT_FALSE(expRes.hasValue());
    EXPECT_EQ(expRes.error().code(), mist::common::ErrorCode::ResourceExhausted);
}

TEST(PairingTest, ApprovalAndPermissions) {
    PairingManager pm;
    auto res = pm.generatePairingCode(300);
    ASSERT_TRUE(res.hasValue());
    std::string code = res.value();

    PairingRequest req{};
    req.deviceId = "client-laptop-99";
    req.deviceName = "Gaming Laptop";
    req.publicKeyFingerprint = "AA:BB:CC:DD:EE:FF";
    req.pairingCode = code;

    auto subRes = pm.submitPairingRequest(req);
    ASSERT_TRUE(subRes.hasValue());

    EXPECT_FALSE(pm.isDeviceTrusted(req.deviceId));

    DevicePermissions perms{};
    perms.allowVideo = true;
    perms.allowAudio = true;
    perms.allowKeyboard = true;
    perms.allowMouse = true;
    perms.allowGamepad = false;

    auto appRes = pm.approvePairingRequest(req.deviceId, perms);
    ASSERT_TRUE(appRes.hasValue());

    EXPECT_TRUE(pm.isDeviceTrusted(req.deviceId, req.publicKeyFingerprint));
    auto pRes = pm.getDevicePermissions(req.deviceId);
    ASSERT_TRUE(pRes.hasValue());
    EXPECT_TRUE(pRes.value().allowKeyboard);
    EXPECT_FALSE(pRes.value().allowGamepad);

    // Revocation
    auto revRes = pm.revokeDevice(req.deviceId);
    EXPECT_TRUE(revRes.hasValue());
    EXPECT_FALSE(pm.isDeviceTrusted(req.deviceId));
}

TEST(PairingTest, JsonPersistence) {
    const std::string testFile = "test_trusted_devices.json";
    (void)std::remove(testFile.c_str());

    {
        PairingManager pm;
        auto res = pm.generatePairingCode(300);
        ASSERT_TRUE(res.hasValue());

        PairingRequest req{};
        req.deviceId = "persist-dev-1";
        req.deviceName = "Persistence Test Client";
        req.publicKeyFingerprint = "11:22:33:44:55:66";
        req.pairingCode = res.value();

        ASSERT_TRUE(pm.submitPairingRequest(req).hasValue());
        ASSERT_TRUE(pm.approvePairingRequest(req.deviceId).hasValue());

        auto saveRes = pm.saveTrustedDevices(testFile);
        EXPECT_TRUE(saveRes.hasValue());
    }

    {
        PairingManager pmLoaded;
        auto loadRes = pmLoaded.loadTrustedDevices(testFile);
        EXPECT_TRUE(loadRes.hasValue());
        EXPECT_TRUE(pmLoaded.isDeviceTrusted("persist-dev-1", "11:22:33:44:55:66"));
    }

    (void)std::remove(testFile.c_str());
}

TEST(PairingTest, SecurityAbusePrevention) {
    PairingManager pm;
    auto codeRes = pm.generatePairingCode(300);
    ASSERT_TRUE(codeRes.hasValue());
    std::string code = codeRes.value();

    // 1. Invalid empty device ID rejection
    PairingRequest emptyIdReq{};
    emptyIdReq.deviceId = "";
    emptyIdReq.deviceName = "Bad Dev";
    emptyIdReq.pairingCode = code;
    auto emptyRes = pm.submitPairingRequest(emptyIdReq);
    EXPECT_FALSE(emptyRes.hasValue());
    EXPECT_EQ(emptyRes.error().code(), mist::common::ErrorCode::InvalidArgument);

    // 2. Legitimate submission & approval
    PairingRequest legitimateReq{};
    legitimateReq.deviceId = "client-legit-01";
    legitimateReq.deviceName = "Legitimate Peer";
    legitimateReq.publicKeyFingerprint = "11:22:33:44:55:66";
    legitimateReq.pairingCode = code;
    ASSERT_TRUE(pm.submitPairingRequest(legitimateReq).hasValue());
    ASSERT_TRUE(pm.approvePairingRequest(legitimateReq.deviceId).hasValue());

    // 3. Single-use enforcement: Attempting to reuse the consumed code must fail
    PairingRequest reuseReq{};
    reuseReq.deviceId = "client-attacker-02";
    reuseReq.deviceName = "Attacker Peer";
    reuseReq.publicKeyFingerprint = "99:88:77:66:55:44";
    reuseReq.pairingCode = code;
    auto reuseRes = pm.submitPairingRequest(reuseReq);
    EXPECT_FALSE(reuseRes.hasValue());
    EXPECT_EQ(reuseRes.error().code(), mist::common::ErrorCode::ResourceExhausted);

    // 4. Default permissions guarantee: input must be FALSE by default
    auto defaultPerms = pm.getDevicePermissions(legitimateReq.deviceId);
    ASSERT_TRUE(defaultPerms.hasValue());
    EXPECT_TRUE(defaultPerms.value().allowVideo);
    EXPECT_TRUE(defaultPerms.value().allowAudio);
    EXPECT_FALSE(defaultPerms.value().allowKeyboard);
    EXPECT_FALSE(defaultPerms.value().allowMouse);
    EXPECT_FALSE(defaultPerms.value().allowGamepad);

    // 5. Fingerprint verification and mismatch rejection
    EXPECT_TRUE(pm.isDeviceTrusted(legitimateReq.deviceId, "11:22:33:44:55:66"));
    EXPECT_FALSE(pm.isDeviceTrusted(legitimateReq.deviceId, "WRONG:FINGERPRINT"));

    // 6. Device revocation blocks future session connection
    ASSERT_TRUE(pm.revokeDevice(legitimateReq.deviceId).hasValue());
    EXPECT_FALSE(pm.isDeviceTrusted(legitimateReq.deviceId));
    EXPECT_FALSE(pm.getDevicePermissions(legitimateReq.deviceId).hasValue());
}
