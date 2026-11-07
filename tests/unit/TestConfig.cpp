#include <gtest/gtest.h>
#include "mist/common/Config.h"

using namespace mist::common;

TEST(ConfigTest, DefaultConfigIsValid) {
    AppConfig config;
    auto res = config.validate();
    EXPECT_TRUE(res.hasValue()) << res.error().format();
    EXPECT_EQ(config.host.targetFps, 60u);
    EXPECT_EQ(config.host.targetBitrateKbps, 15000u);
    EXPECT_EQ(config.host.port, 47998u);
    EXPECT_EQ(config.client.serverPort, 47998u);
}

TEST(ConfigTest, InvalidHostConfigRejected) {
    AppConfig config;

    // Invalid port 0
    config.host.port = 0;
    EXPECT_FALSE(config.validate());

    // Invalid FPS
    config.host.port = 47998;
    config.host.targetFps = 5; // below 15
    EXPECT_FALSE(config.validate());
    config.host.targetFps = 300; // above 240
    EXPECT_FALSE(config.validate());

    // Invalid Bitrate
    config.host.targetFps = 60;
    config.host.targetBitrateKbps = 100; // below 500
    EXPECT_FALSE(config.validate());
    config.host.targetBitrateKbps = 200000; // above 100000
    EXPECT_FALSE(config.validate());

    // Empty bind address
    config.host.targetBitrateKbps = 15000;
    config.host.bindAddress = "";
    EXPECT_FALSE(config.validate());
}

TEST(ConfigTest, InvalidClientConfigRejected) {
    AppConfig config;
    config.client.serverAddress = "";
    EXPECT_FALSE(config.validate());

    config.client.serverAddress = "127.0.0.1";
    config.client.serverPort = 0;
    EXPECT_FALSE(config.validate());
}

TEST(ConfigTest, SerializationAndDeserialization) {
    AppConfig original;
    original.host.port = 48000;
    original.host.targetFps = 120;
    original.host.targetBitrateKbps = 25000;
    original.client.serverAddress = "192.168.1.100";
    original.client.lowLatencyMode = true;
    original.logging.level = LogLevel::Debug;

    std::string serialized = original.serializeToString();
    EXPECT_FALSE(serialized.empty());

    auto loadedRes = AppConfig::loadFromString(serialized);
    ASSERT_TRUE(loadedRes.hasValue()) << loadedRes.error().format();

    const auto& loaded = loadedRes.value();
    EXPECT_EQ(loaded.host.port, 48000u);
    EXPECT_EQ(loaded.host.targetFps, 120u);
    EXPECT_EQ(loaded.host.targetBitrateKbps, 25000u);
    EXPECT_EQ(loaded.client.serverAddress, "192.168.1.100");
    EXPECT_TRUE(loaded.client.lowLatencyMode);
    EXPECT_EQ(loaded.logging.level, LogLevel::Debug);
}

TEST(ConfigTest, RejectInvalidSchemaVersion) {
    std::string badVersionConfig =
        "schema_version=99\n"
        "app_name=Mist\n"
        "[host]\n"
        "bind_address=0.0.0.0\n"
        "port=47998\n"
        "target_fps=60\n"
        "target_bitrate_kbps=15000\n"
        "enable_hw_encode=true\n"
        "max_sessions=1\n"
        "allow_remote_input=true\n"
        "[client]\n"
        "server_address=127.0.0.1\n"
        "server_port=47998\n"
        "low_latency_mode=true\n"
        "fullscreen=false\n"
        "stats_overlay=false\n";

    auto res = AppConfig::loadFromString(badVersionConfig);
    EXPECT_FALSE(res.hasValue());
    EXPECT_EQ(res.error().code(), ErrorCode::FormatNotSupported);
}
