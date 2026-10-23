#include <aurora/shader/ShaderUsage.h>

#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderResolver.h>

#include "ShaderArtifactTest.h"

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

namespace {
    ShaderUsageEntry UsageEntry(const char *entry = "mainFS", ShaderStageFlagBit stage = ShaderStageFlagBit::FS)
    {
        ShaderUsageEntry value;
        value.relativePath = "s.slang";
        value.entry        = entry;
        value.stage        = static_cast<uint32_t>(stage);
        value.environment  = DefaultShaderEnvironment(ShaderTarget::SPIRV, ShaderPlatform::MACOS);
        return value;
    }
} // namespace
TEST(ShaderUsageTest, DedupIncludesFullTargetEnvironment)
{
    ShaderUsageCollector collector;
    ASSERT_TRUE(collector.Record(UsageEntry()));
    EXPECT_EQ(collector.Size(), 0u);
    collector.SetEnabled(true);
    ASSERT_TRUE(collector.Record(UsageEntry()));
    ASSERT_TRUE(collector.Record(UsageEntry()));
    auto other                 = UsageEntry();
    other.environment.platform = ShaderPlatform::IOS;
    ASSERT_TRUE(collector.Record(other));
    other                          = UsageEntry();
    other.environment.matrixLayout = ShaderMatrixLayout::ROW_MAJOR;
    ASSERT_TRUE(collector.Record(other));
    EXPECT_EQ(collector.Size(), 3u);
    other                      = UsageEntry();
    other.environment.platform = ShaderPlatform::UNKNOWN;
    EXPECT_FALSE(collector.Record(other));
}
TEST(ShaderUsageTest, GroupingKeepsPlatformAndOptionsSeparate)
{
    std::vector<ShaderUsageEntry> usage{UsageEntry("mainVS", ShaderStageFlagBit::VS), UsageEntry()};
    auto                          other = UsageEntry();
    other.environment.platform          = ShaderPlatform::IOS;
    usage.push_back(other);
    other                     = UsageEntry();
    other.environment.options = {"different-option"};
    usage.push_back(other);
    const auto groups = GroupShaderUsage(usage);
    ASSERT_EQ(groups.size(), 3u);
    EXPECT_EQ(groups[0].entries.size(), 2u);
}
TEST(ShaderUsageTest, FlushMergesAndPreservesCompleteEnvironment)
{
    ShaderCacheDirectory directory("usage");
    ShaderUsageCollector first, second;
    first.SetEnabled(true);
    second.SetEnabled(true);
    auto vertex                             = UsageEntry("mainVS", ShaderStageFlagBit::VS);
    vertex.environment.minimumVersion       = 20300;
    vertex.environment.requiredCapabilities = 9;
    vertex.environment.options              = {"b", "a"};
    ASSERT_TRUE(first.Record(vertex));
    std::string error;
    ASSERT_TRUE(first.Flush(*directory.fs, "usage.json", &error)) << error;
    ASSERT_TRUE(second.Record(UsageEntry()));
    ASSERT_TRUE(second.Flush(*directory.fs, "usage.json", &error)) << error;
    std::vector<ShaderUsageEntry> loaded;
    ASSERT_TRUE(ShaderUsageCollector::Load(*directory.fs, "usage.json", loaded, &error)) << error;
    ASSERT_EQ(loaded.size(), 2u);
    EXPECT_EQ(loaded[0].environment.minimumVersion, 20300u);
    EXPECT_EQ(loaded[0].environment.requiredCapabilities, 9u);
    EXPECT_EQ(loaded[0].environment.options, (std::vector<std::string>{"a", "b"}));
}
TEST(ShaderUsageTest, PreviousUsageNeedsExplicitRecollection)
{
    ShaderCacheDirectory directory("oldusage");
    const std::string    old = R"({"entries":[{"path":"s.slang","entry":"mainFS","stage":2,"target":0}]})";
    ASSERT_TRUE(WriteFileAtomic(*directory.fs, "usage.json", {old.begin(), old.end()}));
    std::vector<ShaderUsageEntry> output{UsageEntry()};
    std::string                   error;
    EXPECT_FALSE(ShaderUsageCollector::Load(*directory.fs, "usage.json", output, &error));
    EXPECT_NE(error.find("recollect"), std::string::npos);
    EXPECT_EQ(output.size(), 1u);
    ShaderUsageCollector collector;
    collector.SetEnabled(true);
    ASSERT_TRUE(collector.Record(UsageEntry()));
    EXPECT_FALSE(collector.Flush(*directory.fs, "usage.json", &error));
    std::string preserved;
    ASSERT_TRUE(directory.fs->OpenFile(FilePath("usage.json"))->ReadString(preserved));
    EXPECT_EQ(preserved, old);
}
TEST(ShaderUsageTest, CookAndCompilerFreePublishedVerification)
{
    ShaderCacheDirectory directory("usagecook");
    ShaderFileSystem     sources;
    sources.AddVirtualFile("s.slang", "// source");
    sources.AddVirtualFile("s.slang.json", R"({"sources":[],"variants":[]})");
    TestShaderCompiler                  compiler;
    const CompilerRegistration          registration(&compiler);
    const std::vector<ShaderUsageEntry> usage{UsageEntry("mainVS", ShaderStageFlagBit::VS), UsageEntry()};
    ShaderResolver                      development(nullptr, directory.fs.Get(), sources, 0, 0);
    std::string                         error;
    ASSERT_TRUE(BuildUsageCache(development, usage, &error)) << error;
    EXPECT_EQ(compiler.count, 2u);
    ShaderCompilerFactory::Get().Register(nullptr);
    ShaderFileSystem empty;
    ShaderResolver   products(directory.fs.Get(), nullptr, empty, 0, UINT64_MAX, ShaderResolveMode::PRODUCTS);
    ASSERT_TRUE(BuildUsageCache(products, usage, &error)) << error;
    EXPECT_EQ(compiler.count, 2u);
}
