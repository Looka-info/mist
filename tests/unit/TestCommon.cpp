#include <gtest/gtest.h>
#include "mist/common/Error.h"
#include "mist/common/Result.h"
#include "mist/common/Subsystem.h"

using namespace mist::common;

TEST(CommonTest, SubsystemToString) {
    EXPECT_EQ(subsystemToString(SubsystemId::Common), "Common");
    EXPECT_EQ(subsystemToString(SubsystemId::Platform), "Platform");
    EXPECT_EQ(subsystemToString(SubsystemId::Session), "Session");
    EXPECT_EQ(subsystemToString(SubsystemId::Capture), "Capture");
    EXPECT_EQ(subsystemToString(SubsystemId::Encode), "Encode");
    EXPECT_EQ(subsystemToString(SubsystemId::Decode), "Decode");
    EXPECT_EQ(subsystemToString(SubsystemId::Render), "Render");
    EXPECT_EQ(subsystemToString(SubsystemId::Transport), "Transport");
    EXPECT_EQ(subsystemToString(SubsystemId::Audio), "Audio");
    EXPECT_EQ(subsystemToString(SubsystemId::Input), "Input");
    EXPECT_EQ(subsystemToString(SubsystemId::Telemetry), "Telemetry");
    EXPECT_EQ(subsystemToString(SubsystemId::Security), "Security");
}

TEST(CommonTest, ErrorFormatting) {
    Error defaultErr;
    EXPECT_TRUE(defaultErr.isSuccess());
    EXPECT_FALSE(static_cast<bool>(defaultErr));
    EXPECT_EQ(defaultErr.format(), "Success");

    Error err(SubsystemId::Transport, ErrorCode::Timeout, "Connection attempt timed out", 10060);
    EXPECT_FALSE(err.isSuccess());
    EXPECT_TRUE(static_cast<bool>(err));
    EXPECT_EQ(err.subsystem(), SubsystemId::Transport);
    EXPECT_EQ(err.code(), ErrorCode::Timeout);
    EXPECT_EQ(err.platformCode(), 10060);
    EXPECT_NE(err.format().find("Connection attempt timed out"), std::string::npos);
    EXPECT_NE(err.format().find("PlatformCode=10060"), std::string::npos);
}

TEST(CommonTest, ResultWithValue) {
    Result<int> res(42);
    EXPECT_TRUE(res.hasValue());
    EXPECT_FALSE(res.hasError());
    EXPECT_TRUE(static_cast<bool>(res));
    EXPECT_EQ(res.value(), 42);
    EXPECT_EQ(res.valueOr(100), 42);
}

TEST(CommonTest, ResultWithError) {
    Error err(SubsystemId::Common, ErrorCode::InvalidArgument, "Bad input");
    Result<int> res(err);
    EXPECT_FALSE(res.hasValue());
    EXPECT_TRUE(res.hasError());
    EXPECT_FALSE(static_cast<bool>(res));
    EXPECT_EQ(res.error().code(), ErrorCode::InvalidArgument);
    EXPECT_EQ(res.valueOr(100), 100);
    EXPECT_THROW((void)res.value(), std::runtime_error);
}

TEST(CommonTest, ResultVoidSuccess) {
    auto res = Result<void>::success();
    EXPECT_TRUE(res.hasValue());
    EXPECT_FALSE(res.hasError());
    EXPECT_TRUE(static_cast<bool>(res));
}

TEST(CommonTest, ResultVoidError) {
    Error err(SubsystemId::Common, ErrorCode::PermissionDenied, "Access denied");
    Result<void> res(err);
    EXPECT_FALSE(res.hasValue());
    EXPECT_TRUE(res.hasError());
    EXPECT_FALSE(static_cast<bool>(res));
    EXPECT_EQ(res.error().code(), ErrorCode::PermissionDenied);
}
