//
// Created on 2026/10/15.
//

#include "AndroidAssetUtils.h"
#include <cstring>
#include <gtest/gtest.h>

using namespace sky;

TEST(AndroidAssetPathTest, LogicalPathsAreCanonicalAcrossHostSeparators)
{
    std::string path;
    EXPECT_TRUE(NormalizeAndroidAssetPath("configs\\modules_game.json", path));
    EXPECT_EQ(path, "configs/modules_game.json");
    EXPECT_TRUE(NormalizeAndroidAssetPath("/products//./common/../desktop//", path));
    EXPECT_EQ(path, "products/desktop");
    EXPECT_TRUE(NormalizeAndroidAssetPath("", path));
    EXPECT_TRUE(path.empty());
    path = "configs\\modules_game.json";
    EXPECT_TRUE(NormalizeAndroidAssetPath(path, path));
    EXPECT_EQ(path, "configs/modules_game.json");
}

TEST(AndroidAssetPathTest, PathsOutsideThePackageOrWithEmbeddedNullFail)
{
    std::string path = "previous";
    EXPECT_FALSE(NormalizeAndroidAssetPath("../configs/x", path));
    EXPECT_TRUE(path.empty());
    EXPECT_FALSE(NormalizeAndroidAssetPath("configs/../../x", path));
    EXPECT_FALSE(NormalizeAndroidAssetPath(std::string_view("a\0b", 3), path));
}

TEST(AndroidAssetReadTest, PartialReadsAreCombinedUntilAllBytesArrive)
{
    const std::string payload = "compressed-asset-content";
    std::string       output(payload.size(), '\0');
    size_t            offset = 0;
    const auto        reader = [&payload, &offset](char *data, size_t requested) {
        const size_t size = std::min({size_t{3}, requested, payload.size() - offset});
        std::memcpy(data, payload.data() + offset, size);
        offset += size;
        return static_cast<int>(size);
    };
    EXPECT_TRUE(ReadAndroidAssetBytes(reader, output.data(), output.size()));
    EXPECT_EQ(output, payload);
}

TEST(AndroidAssetReadTest, EarlyEofAndReadErrorsNeverReportSuccess)
{
    char output[4] = {};
    int  reads     = 0;
    EXPECT_FALSE(ReadAndroidAssetBytes(
        [&reads](char *data, size_t) {
            if (reads++ == 0) {
                data[0] = 'x';
                return 1;
            }
            return 0;
        },
        output, sizeof(output)));
    EXPECT_FALSE(ReadAndroidAssetBytes([](char *, size_t) { return -1; }, output, sizeof(output)));
    EXPECT_FALSE(ReadAndroidAssetBytes([](char *, size_t) { return 0; }, nullptr, 1));
    EXPECT_TRUE(ReadAndroidAssetBytes([](char *, size_t) { return -1; }, nullptr, 0));
}
