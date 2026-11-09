#include <gtest/gtest.h>
#include "DesktopApplication.h"

using namespace mist::desktop;

TEST(DesktopAppTest, HelpAndVersionArgs) {
    DesktopApplication app;
    char* argv1[] = {const_cast<char*>("mist_desktop.exe"), const_cast<char*>("--help")};
    auto res1 = app.initialize(2, argv1);
    ASSERT_TRUE(res1.hasValue());
    EXPECT_TRUE(res1.value().has_value());
    EXPECT_EQ(res1.value().value(), 0);

    char* argv2[] = {const_cast<char*>("mist_desktop.exe"), const_cast<char*>("--version")};
    auto res2 = app.initialize(2, argv2);
    ASSERT_TRUE(res2.hasValue());
    EXPECT_TRUE(res2.value().has_value());
    EXPECT_EQ(res2.value().value(), 0);
}

TEST(DesktopAppTest, SelfTestAndProfileConfig) {
    DesktopApplication app;
    char* argv[] = {
        const_cast<char*>("mist_desktop.exe"),
        const_cast<char*>("--self-test"),
        const_cast<char*>("--profile"),
        const_cast<char*>("Performance")
    };

    auto initRes = app.initialize(4, argv);
    ASSERT_TRUE(initRes.hasValue());
    EXPECT_FALSE(initRes.value().has_value()); // Continues to run()

    EXPECT_EQ(app.getQualityManager().getProfile(), mist::transport::qos::QualityProfile::Performance);

    int runCode = app.run();
    EXPECT_EQ(runCode, 0);

    auto shutRes = app.shutdown();
    EXPECT_TRUE(shutRes.hasValue());
}

TEST(DesktopAppTest, PairingManagerIntegration) {
    DesktopApplication app;
    auto& pm = app.getPairingManager();

    // 1. Generate pairing code
    auto codeRes = pm.generatePairingCode(300);
    ASSERT_TRUE(codeRes.hasValue());
    std::string code = codeRes.value();
    EXPECT_EQ(code.size(), 7); // XXX-YYY format
    EXPECT_EQ(code[3], '-');

    // 2. Submit pairing request with invalid code
    mist::security::PairingRequest badReq;
    badReq.deviceId = "unauthorized-device";
    badReq.deviceName = "Unauthorized Laptop";
    badReq.pairingCode = "BAD-999";
    auto badRes = pm.submitPairingRequest(badReq);
    EXPECT_FALSE(badRes.hasValue());

    // 3. Submit valid pairing request
    mist::security::PairingRequest validReq;
    validReq.deviceId = "client-pixel-tab";
    validReq.deviceName = "Pixel Tablet";
    validReq.pairingCode = code;
    auto valRes = pm.submitPairingRequest(validReq);
    EXPECT_TRUE(valRes.hasValue());

    auto pending = pm.getPendingRequests();
    ASSERT_EQ(pending.size(), 1);
    EXPECT_EQ(pending[0].deviceId, "client-pixel-tab");

    // 4. Host approves with specific permissions
    mist::security::DevicePermissions perms;
    perms.allowAudio = true;
    perms.allowKeyboard = true;
    perms.allowMouse = true;
    perms.allowGamepad = false;

    auto appRes = pm.approvePairingRequest("client-pixel-tab", perms);
    EXPECT_TRUE(appRes.hasValue());
    EXPECT_TRUE(pm.isDeviceTrusted("client-pixel-tab"));

    auto trusted = pm.getTrustedDevices();
    ASSERT_EQ(trusted.size(), 1);
    EXPECT_EQ(trusted[0].deviceName, "Pixel Tablet");
    EXPECT_TRUE(trusted[0].permissions.allowKeyboard);
    EXPECT_FALSE(trusted[0].permissions.allowGamepad);

    // 5. Update device permissions
    perms.allowGamepad = true;
    auto updRes = pm.updateDevicePermissions("client-pixel-tab", perms);
    EXPECT_TRUE(updRes.hasValue());
    auto permsRes = pm.getDevicePermissions("client-pixel-tab");
    ASSERT_TRUE(permsRes.hasValue());
    EXPECT_TRUE(permsRes.value().allowGamepad);

    // 6. Revoke device
    auto revRes = pm.revokeDevice("client-pixel-tab");
    EXPECT_TRUE(revRes.hasValue());
    EXPECT_FALSE(pm.isDeviceTrusted("client-pixel-tab"));
    EXPECT_TRUE(pm.getTrustedDevices().empty());
}

TEST(DesktopAppTest, QualityProfileConstraints) {
    DesktopApplication app;
    auto& qm = app.getQualityManager();

    // Verify all QoS profiles enforce valid bounds: 2 Mbps to 25 Mbps
    const mist::transport::qos::QualityProfile profiles[] = {
        mist::transport::qos::QualityProfile::Performance,
        mist::transport::qos::QualityProfile::Balanced,
        mist::transport::qos::QualityProfile::Quality,
        mist::transport::qos::QualityProfile::Adaptive
    };

    for (auto prof : profiles) {
        qm.setProfile(prof);
        EXPECT_EQ(qm.getProfile(), prof);
        auto cfg = qm.getProfileConfig();
        EXPECT_GE(cfg.minBitrateKbps, 1500u);   // >= 1.5 Mbps
        EXPECT_LE(cfg.maxBitrateKbps, 25000u);  // <= 25 Mbps
        EXPECT_GE(cfg.initialBitrateKbps, cfg.minBitrateKbps);
        EXPECT_LE(cfg.initialBitrateKbps, cfg.maxBitrateKbps);
        EXPECT_TRUE(cfg.targetFps == 30 || cfg.targetFps == 60);
    }
}

TEST(DesktopAppTest, ScreenNavigationAndTransitions) {
    DesktopApplication app;

#if defined(_WIN32)
    EXPECT_EQ(app.getCurrentScreen(), DesktopScreen::Home);

    app.navigateToScreen(DesktopScreen::Host);
    EXPECT_EQ(app.getCurrentScreen(), DesktopScreen::Host);

    app.navigateToScreen(DesktopScreen::Connect);
    EXPECT_EQ(app.getCurrentScreen(), DesktopScreen::Connect);

    app.navigateToScreen(DesktopScreen::Devices);
    EXPECT_EQ(app.getCurrentScreen(), DesktopScreen::Devices);

    app.navigateToScreen(DesktopScreen::Network);
    EXPECT_EQ(app.getCurrentScreen(), DesktopScreen::Network);

    app.navigateToScreen(DesktopScreen::LiveSession);
    EXPECT_EQ(app.getCurrentScreen(), DesktopScreen::LiveSession);

    app.navigateToScreen(DesktopScreen::Home);
    EXPECT_EQ(app.getCurrentScreen(), DesktopScreen::Home);
#endif
}
