#include <gtest/gtest.h>
#include "mist/common/Logger.h"
#include <vector>
#include <thread>
#include <string>

using namespace mist::common;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        Logger::instance().resetSinks();
        Logger::instance().setConsoleEnabled(false);
    }

    void TearDown() override {
        Logger::instance().resetSinks();
    }
};

TEST_F(LoggerTest, LevelFiltering) {
    std::vector<LogRecord> received;
    Logger::instance().setCustomSink([&received](const LogRecord& r, std::string_view) {
        received.push_back(r);
    });

    Logger::instance().setLevel(LogLevel::Warning);

    Logger::instance().debug(SubsystemId::Common, "Debug message");
    Logger::instance().info(SubsystemId::Common, "Info message");
    EXPECT_TRUE(received.empty());

    Logger::instance().warn(SubsystemId::Common, "Warning message");
    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].level, LogLevel::Warning);
    EXPECT_EQ(received[0].message, "Warning message");

    Logger::instance().error(SubsystemId::Common, "Error message");
    ASSERT_EQ(received.size(), 2u);
    EXPECT_EQ(received[1].level, LogLevel::Error);

    Logger::instance().critical(SubsystemId::Common, "Critical message");
    ASSERT_EQ(received.size(), 3u);
    EXPECT_EQ(received[2].level, LogLevel::Critical);
}

TEST_F(LoggerTest, SensitiveDataRedaction) {
    std::vector<LogRecord> received;
    Logger::instance().setCustomSink([&received](const LogRecord& r, std::string_view) {
        received.push_back(r);
    });

    Logger::instance().setLevel(LogLevel::Debug);

    Logger::instance().info(SubsystemId::Security, "User authenticated with Bearer secret_token_123");
    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].message, "[REDACTED_SENSITIVE_DATA]");
    EXPECT_EQ(received[0].message.find("secret_token_123"), std::string::npos);

    Logger::instance().info(SubsystemId::Common, "Normal diagnostic message without tokens");
    ASSERT_EQ(received.size(), 2u);
    EXPECT_EQ(received[1].message, "Normal diagnostic message without tokens");
}

TEST_F(LoggerTest, ThreadSafety) {
    std::atomic<uint32_t> messageCount{0};
    Logger::instance().setCustomSink([&messageCount](const LogRecord&, std::string_view) {
        messageCount.fetch_add(1, std::memory_order_relaxed);
    });

    Logger::instance().setLevel(LogLevel::Info);

    constexpr int numThreads = 8;
    constexpr int messagesPerThread = 50;
    std::vector<std::thread> threads;
    threads.reserve(numThreads);

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([]() {
            for (int j = 0; j < messagesPerThread; ++j) {
                Logger::instance().info(SubsystemId::Common, "Concurrent log test");
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(messageCount.load(), static_cast<uint32_t>(numThreads * messagesPerThread));
}
