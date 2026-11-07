#include <gtest/gtest.h>
#include "mist/security/Security.h"

using namespace mist::security;
using namespace mist::common;

TEST(SecurityTest, DeviceIdentityValidation) {
    DeviceIdentity validDevice{
        .deviceId = "host-device-012345",
        .deviceName = "RyzenHost",
        .publicKeyFingerprint = "SHA256:abc123def456"
    };
    EXPECT_TRUE(validDevice.validate().hasValue());

    DeviceIdentity tooShortId{
        .deviceId = "short",
        .deviceName = "RyzenHost",
        .publicKeyFingerprint = ""
    };
    EXPECT_FALSE(tooShortId.validate().hasValue());

    DeviceIdentity emptyName{
        .deviceId = "host-device-012345",
        .deviceName = "",
        .publicKeyFingerprint = ""
    };
    EXPECT_FALSE(emptyName.validate().hasValue());
}

TEST(SecurityTest, AuthTokenValidationAndExpiration) {
    AuthToken validToken{
        .rawToken = "secure-random-token-987654321",
        .expiresAtEpochSec = 2000000000
    };
    EXPECT_TRUE(validToken.validate().hasValue());
    EXPECT_FALSE(validToken.isExpired(1000000000));
    EXPECT_TRUE(validToken.isExpired(2000000001));

    AuthToken shortToken{
        .rawToken = "short",
        .expiresAtEpochSec = 0
    };
    EXPECT_FALSE(shortToken.validate().hasValue());
}

TEST(SecurityTest, ConstantTimeComparison) {
    EXPECT_TRUE(constantTimeEquals("", ""));
    EXPECT_TRUE(constantTimeEquals("secret123456", "secret123456"));
    EXPECT_FALSE(constantTimeEquals("secret123456", "secret123457"));
    EXPECT_FALSE(constantTimeEquals("secret123456", "secret123"));
    EXPECT_FALSE(constantTimeEquals("secret123", "secret123456"));
}
