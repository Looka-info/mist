#include <gtest/gtest.h>
#include "mist/decode/D3D11VideoDecoder.h"
#include "mist/decode/AnnexBReader.h"
#include "mist/platform/windows/D3D11Device.h"
#include <fstream>

using namespace mist::decode;
using namespace mist::platform::windows;
using namespace mist::common;

static std::string findTestH264() {
    for (const auto& p : { "artifacts/test.h264", "../../artifacts/test.h264", "../../../artifacts/test.h264" }) {
        std::ifstream f(p, std::ios::binary);
        if (f.is_open()) return p;
    }
    return "";
}

TEST(DecoderTest, QueryCapabilities) {
    auto cap = D3D11VideoDecoder::queryCapabilities();
    // Querying capabilities must never crash
    if (cap.isHardwareAccelerated) {
        EXPECT_TRUE(cap.supportsH264);
        EXPECT_FALSE(cap.deviceName.empty());
    }
}

TEST(DecoderTest, D3D11HardwareDecoding) {
#if defined(_WIN32)
    auto cap = D3D11VideoDecoder::queryCapabilities();
    if (!cap.supportsH264) {
        GTEST_SKIP() << "Direct3D 11 hardware H.264 video decoding unavailable";
    }

    std::string path = findTestH264();
    if (path.empty()) {
        GTEST_SKIP() << "artifacts/test.h264 unavailable for hardware decoding test";
    }

    AnnexBReader reader;
    auto readRes = reader.openFile(path, 60);
    ASSERT_TRUE(readRes.hasValue()) << readRes.error().format();

    auto devRes = D3D11DeviceManager::createDevice(0);
    ASSERT_TRUE(devRes.hasValue());

    auto decRes = D3D11VideoDecoder::create();
    ASSERT_TRUE(decRes.hasValue()) << decRes.error().format();
    auto decoder = std::move(decRes.value());

    DecodeConfig config{
        .codec = mist::encode::VideoCodec::H264,
        .preferHardwareDecoding = true,
        .lowLatencyTuning = true
    };

    auto initRes = decoder->initializeWithDevice(config, devRes.value().first, devRes.value().second);
    ASSERT_TRUE(initRes.hasValue()) << initRes.error().format();
    EXPECT_TRUE(decoder->isInitialized());

    // Submit first 10 access units and verify frame decoding
    size_t framesDecoded = 0;
    const auto& aus = reader.getAccessUnits();
    size_t testCount = std::min(aus.size(), size_t{15});

    for (size_t i = 0; i < testCount; ++i) {
        const auto& au = aus[i];
        mist::encode::EncodedPacket packet{
            .data = au.data,
            .isKeyframe = au.isKeyframe,
            .timestampNs = au.timestampNs,
            .frameNumber = au.sequenceNumber
        };

        auto subRes = decoder->submitPacket(packet);
        EXPECT_TRUE(subRes.hasValue()) << subRes.error().format();

        while (true) {
            auto frameRes = decoder->getNextFrame();
            if (!frameRes) break;

            ++framesDecoded;
            const auto& frame = frameRes.value();
            EXPECT_GT(frame.width, 0u);
            EXPECT_GT(frame.height, 0u);
            EXPECT_NE(frame.nativeResource, nullptr);

            auto relRes = decoder->releaseFrame(frame);
            EXPECT_TRUE(relRes.hasValue());
        }
    }

    EXPECT_GT(framesDecoded, 0u) << "Expected hardware decoder to output at least 1 decoded frame";

    auto flushRes = decoder->flush();
    EXPECT_TRUE(flushRes.hasValue());
#endif
}
