#include <gtest/gtest.h>
#include "mist/decode/AnnexBReader.h"
#include <fstream>

using namespace mist::decode;

static std::string findTestH264() {
    for (const auto& p : { "artifacts/test.h264", "../../artifacts/test.h264", "../../../artifacts/test.h264" }) {
        std::ifstream f(p, std::ios::binary);
        if (f.is_open()) return p;
    }
    return "";
}

TEST(AnnexBReaderTest, OpenInvalidFile) {
    AnnexBReader reader;
    auto res = reader.openFile("non_existent_file.h264");
    EXPECT_FALSE(res.hasValue());
    EXPECT_EQ(res.error().code(), mist::common::ErrorCode::NotFound);
}

TEST(AnnexBReaderTest, OpenMilestone1Artifact) {
    std::string path = findTestH264();
    if (path.empty()) {
        GTEST_SKIP() << "artifacts/test.h264 not present on disk, skipping artifact test.";
    }

    AnnexBReader reader;
    auto res = reader.openFile(path, 60);
    ASSERT_TRUE(res.hasValue()) << res.error().format();

    EXPECT_GT(reader.size(), 0u);
    const auto& info = reader.getStreamInfo();
    EXPECT_GT(info.totalAccessUnits, 0u);
    EXPECT_GE(info.keyframeAccessUnits, 1u);
    EXPECT_EQ(info.targetFps, 60u);
    EXPECT_GT(info.totalBytes, 0u);
    EXPECT_GT(info.estimatedDurationSeconds, 0.0);

    const auto* firstAu = reader.getAccessUnit(0);
    ASSERT_NE(firstAu, nullptr);
    EXPECT_FALSE(firstAu->data.empty());
    EXPECT_TRUE(firstAu->isKeyframe);
}
