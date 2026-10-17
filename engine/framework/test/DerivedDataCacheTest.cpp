//
// Created by blues on 2026/10/5.
//

#include <gtest/gtest.h>

#include <framework/asset/DerivedDataCache.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using namespace sky;

namespace {

    class CountingBuilder : public IDerivedDataBuilder {
    public:
        explicit CountingBuilder(std::string builderId) : id(std::move(builderId)) {}

        std::string GetId() const override { return id; }
        uint32_t GetVersion() const override { return version; }

        bool Build(const std::vector<uint8_t> &source, const std::string &settings,
                   std::vector<uint8_t> &out) const override
        {
            ++calls;
            out.assign(source.begin(), source.end());
            out.push_back(static_cast<uint8_t>(settings.size()));
            return true;
        }

        std::string id;
        uint32_t version = 1;
        mutable int calls = 0;
    };

    std::filesystem::path MakeTempRoot(const std::string &name)
    {
        const auto root = std::filesystem::temp_directory_path() / ("sky-ddc-test-" + name);
        std::error_code error;
        std::filesystem::remove_all(root, error);
        return root;
    }

} // namespace

TEST(DerivedDataCacheTest, MissThenHitThenInvalidate)
{
    auto &cache = DerivedDataCache::Get();
    auto builder = std::make_shared<CountingBuilder>("ddc-test-basic");
    cache.Register(builder);
    cache.SetRoot(MakeTempRoot("basic").string());

    const std::vector<uint8_t> source = {1, 2, 3, 4};
    std::vector<uint8_t> first;
    ASSERT_TRUE(cache.Fetch(source, builder->GetId(), "16x16", "Win32", first));
    EXPECT_EQ(builder->calls, 1);
    EXPECT_FALSE(first.empty());

    // Same inputs -> served from the cache without invoking the builder again.
    std::vector<uint8_t> second;
    ASSERT_TRUE(cache.Fetch(source, builder->GetId(), "16x16", "Win32", second));
    EXPECT_EQ(builder->calls, 1);
    EXPECT_EQ(first, second);

    // Settings and platform are part of the key.
    std::vector<uint8_t> resized;
    ASSERT_TRUE(cache.Fetch(source, builder->GetId(), "32x32", "Win32", resized));
    EXPECT_EQ(builder->calls, 2);

    std::vector<uint8_t> otherPlatform;
    ASSERT_TRUE(cache.Fetch(source, builder->GetId(), "16x16", "Android", otherPlatform));
    EXPECT_EQ(builder->calls, 3);
}

TEST(DerivedDataCacheTest, VersionInvalidates)
{
    auto &cache = DerivedDataCache::Get();
    auto builder = std::make_shared<CountingBuilder>("ddc-test-version");
    cache.Register(builder);
    cache.SetRoot(MakeTempRoot("version").string());

    const std::vector<uint8_t> source = {9, 8, 7};
    std::vector<uint8_t> first;
    ASSERT_TRUE(cache.Fetch(source, builder->GetId(), "", "Win32", first));
    EXPECT_EQ(builder->calls, 1);

    // Bumping the builder version changes the key for identical inputs.
    builder->version = 2;
    std::vector<uint8_t> second;
    ASSERT_TRUE(cache.Fetch(source, builder->GetId(), "", "Win32", second));
    EXPECT_EQ(builder->calls, 2);
}
