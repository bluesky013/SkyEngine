//
// Created on 2026/10/22.
//

#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderResolver.h>
#include <aurora/shader/ShaderUsage.h>
#include <gtest/gtest.h>
#include <set>

using namespace sky;
using namespace sky::aurora;

TEST(ShaderDistribution, RelocatedProductsResolveWithoutSourcesOrCompiler)
{
    NativeFileSystem products{FilePath(SHADER_TEST_PACKAGE_ROOT)};
    if (!products.FileExist("requirements.json")) {
        GTEST_SKIP() << "Run python/tests/run_shader_package_tests.py to prepare the real project cook fixture";
    }
    ASSERT_EQ(ShaderCompilerFactory::Get().GetCompiler(), nullptr);
    std::vector<ShaderUsageEntry> requirements;
    ASSERT_TRUE(ShaderUsageCollector::Load(products, "requirements.json", requirements));
    ASSERT_GE(requirements.size(), 30u);
    ShaderFileSystem      emptySources;
    ShaderResolver        resolver(&products, nullptr, emptySources, 0, 0);
    std::set<std::string> owners;
    std::set<uint32_t>    targets;
    for (const auto &entry : requirements) {
        SCOPED_TRACE(entry.relativePath + ":" + entry.entry);
        ShaderResolver::Request request;
        request.relativePath = entry.relativePath;
        request.entry        = entry.entry;
        request.stage        = static_cast<ShaderStageFlagBit>(entry.stage);
        request.target       = entry.target;
        request.variantHash  = entry.variantHash;
        ShaderCompileResult result;
        ShaderCompileTask   task;
        std::string         error, source;
        EXPECT_FALSE(emptySources.ReadFile(entry.relativePath, source));
        ASSERT_EQ(resolver.PrepareShader(request, result, task, &error), ShaderResolveStatus::Ready) << error;
        EXPECT_FALSE(result.data.empty());
        owners.insert(entry.relativePath.substr(0, entry.relativePath.find('/')));
        targets.insert(entry.target);
    }
    EXPECT_TRUE(owners.contains("aurora"));
    EXPECT_TRUE(owners.contains("first"));
    EXPECT_TRUE(owners.contains("second"));
    EXPECT_EQ(targets.size(), 3u);
}
