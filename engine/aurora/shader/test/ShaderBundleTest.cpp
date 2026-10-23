#include "plugins/first/shaders/ToonPass.h"
#include "plugins/second/shaders/ToonPass.h"
#include <algorithm>
#include <aurora/shader/ShaderCompilerSlang.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderResolver.h>
#include <core/file/FileSystem.h>
#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;

namespace {
    class ShaderBundles : public testing::TestWithParam<ShaderTarget> {
    protected:
        void SetUp() override
        {
            previous = ShaderCompilerFactory::Get().GetCompiler();
            ShaderCompilerFactory::Get().Register(&compiler);
        }
        void TearDown() override
        {
            ShaderCompilerFactory::Get().Register(previous);
        }
        ShaderResolver::Request Request(const std::string &owner) const
        {
            ShaderResolver::Request request;
            request.relativePath = owner + "/pipeline/ToonPass.slang";
            request.entry        = "FSMain";
            request.stage        = ShaderStageFlagBit::FS;
            request.target       = static_cast<uint32_t>(GetParam());
            return request;
        }
        ShaderCompilerSlang compiler;
        IShaderCompiler    *previous = nullptr;
    };
} // namespace

TEST_P(ShaderBundles, IndependentNamesMatchLogicalInterfacesInRuntimeAndCookSources)
{
    ShaderFileSystem                embedded;
    NativeFileSystem                root{FilePath(SHADER_TEST_BUNDLE_ROOT)};
    std::vector<ShaderSourceBundle> bundles;
    ASSERT_TRUE(LoadShaderSourceBundles(root, "", bundles));
    std::ranges::reverse(bundles);
    ASSERT_TRUE(embedded.AddBundles(bundles));
    for (const auto &owner : {std::string("first"), std::string("second")}) {
        SCOPED_TRACE(owner);
        ShaderFileSystem packaged;
        std::string      error;
        ASSERT_TRUE(packaged.LoadBundles(root, owner, &error)) << error;
        ShaderResolver      runtime(nullptr, nullptr, embedded, 0, 0), cook(nullptr, nullptr, packaged, 0, 0);
        ShaderCompileTask   runtimeTask, cookTask;
        ShaderCompileResult runtimeResult, cookResult;
        const auto          request = Request(owner);
        ASSERT_EQ(runtime.PrepareShader(request, runtimeResult, runtimeTask, &error), ShaderResolveStatus::Compile) << error;
        ASSERT_EQ(cook.PrepareShader(request, cookResult, cookTask, &error), ShaderResolveStatus::Compile) << error;
        EXPECT_EQ(runtimeTask.sourceHash, cookTask.sourceHash);
        EXPECT_EQ(runtimeTask.schemaFp, cookTask.schemaFp);
        ASSERT_TRUE(runtimeTask.Run(compiler, runtimeResult, &error)) << error;
        ASSERT_TRUE(cookTask.Run(compiler, cookResult, &error)) << error;
        EXPECT_EQ(runtimeResult.data, cookResult.data);
        const auto &schema = owner == "first" ? test_pipeline::first::GetToonPassReflection() : test_pipeline::second::GetToonPassReflection();
        ASSERT_EQ(runtimeResult.reflection.blocks.size(), schema.blocks.size());
        for (const auto &block : schema.blocks) {
            const auto found = std::find_if(runtimeResult.reflection.blocks.begin(), runtimeResult.reflection.blocks.end(),
                                            [&](const auto &actual) { return actual.set == block.set && actual.binding == block.binding; });
            ASSERT_NE(found, runtimeResult.reflection.blocks.end());
            ASSERT_EQ(found->members.size(), block.members.size());
            for (size_t i = 0; i < block.members.size(); ++i) {
                EXPECT_EQ(found->members[i].name, block.members[i].name);
                EXPECT_EQ(found->members[i].scalarType, block.members[i].scalarType);
                EXPECT_EQ(found->members[i].kind, block.members[i].kind);
                EXPECT_EQ(found->members[i].arrayCount, block.members[i].arrayCount);
            }
        }
    }
}

TEST_P(ShaderBundles, UndeclaredOrMissingIncludesFailWithoutMountOrderFallback)
{
    ShaderFileSystem   files;
    ShaderSourceBundle bundles[]{{"plugins/a",
                                  {},
                                  {{"plugins/a/pipeline/ToonPass.slang", "#include <plugins/b/Value.h>\nfloat4 FSMain():SV_Target{return value;}"},
                                   {"plugins/a/pipeline/ToonPass.slang.json", "{}"}}},
                                 {"plugins/b", {}, {{"plugins/b/Value.h", "static const float4 value = float4(1,0,0,1);"}}}};
    ASSERT_TRUE(files.AddBundles(bundles));
    ShaderResolver      resolver(nullptr, nullptr, files, 0, 0);
    ShaderCompileResult result;
    std::string         error;
    EXPECT_FALSE(resolver.ResolveShader(Request("plugins/a"), result, &error));
    EXPECT_NE(error.find("plugins/b/Value.h"), std::string::npos);
    ASSERT_TRUE(files.RemoveBundle("plugins/a"));
    bundles[0].dependencies = {"plugins/b"};
    ASSERT_TRUE(files.AddBundles(std::span{bundles, 1}));
    ASSERT_TRUE(resolver.ResolveShader(Request("plugins/a"), result, &error)) << error;
}

TEST_P(ShaderBundles, CompileFinishesAfterSourcesAndResolverAreDestroyed)
{
    ShaderCompileFuture future;
    ShaderCompileResult immediate;
    {
        ShaderFileSystem files;
        NativeFileSystem root{FilePath(SHADER_TEST_BUNDLE_ROOT)};
        ASSERT_TRUE(files.LoadBundles(root, "first"));
        ShaderResolver resolver(nullptr, nullptr, files, 0, 0);
        std::string    error;
        ASSERT_EQ(resolver.ResolveShaderAsync(Request("first"), immediate, future, &error), ShaderResolveStatus::Compile) << error;
        ASSERT_TRUE(files.RemoveBundle("first"));
    }
    future.RunInline();
    ASSERT_TRUE(future.Ready());
    EXPECT_TRUE(future.Succeeded()) << future.Result().errorInfo;
}

TEST_P(ShaderBundles, LocalMacroIncludesAndCustomRootsUseTheSameCanonicalDependencyScope)
{
    const ShaderSourceBundle shared{
        "common", {}, {{"common/include/Value.slangh", "static const float4 sharedColor = float4(1,0,0,1);"}}, {"common/include"}};
    const ShaderSourceBundle material{"first",
                                      {"common"},
                                      {{"first/pipeline/ToonPass.slang", "#define LOCAL \"Local.slangh\"\n#include LOCAL\n#include <Value.slangh>\n"
                                                                         "float4 FSMain():SV_Target{return localColor * sharedColor;}"},
                                       {"first/pipeline/Local.slangh", "static const float4 localColor = float4(1,1,1,1);"},
                                       {"first/pipeline/ToonPass.slang.json", "{}"}}};
    ShaderCompileResult      expected;
    uint64_t                 expectedHash = 0;
    for (const bool reversed : {false, true}) {
        ShaderFileSystem         files;
        const ShaderSourceBundle bundles[]{reversed ? material : shared, reversed ? shared : material};
        std::string              error;
        ASSERT_TRUE(files.AddBundles(bundles, &error)) << error;
        ShaderResolver      resolver(nullptr, nullptr, files, 0, 0);
        ShaderCompileTask   task;
        ShaderCompileResult result;
        ASSERT_EQ(resolver.PrepareShader(Request("first"), result, task, &error), ShaderResolveStatus::Compile) << error;
        ASSERT_TRUE(task.Run(compiler, result, &error)) << error;
        if (reversed) {
            EXPECT_EQ(result.data, expected.data);
            EXPECT_EQ(task.sourceHash, expectedHash);
        } else {
            expected     = result;
            expectedHash = task.sourceHash;
        }
    }
}

TEST(ShaderBundleLoading, IndependentModuleInventoriesLoadWithoutPathRemapping)
{
    NativeFileSystem root{FilePath(SHADER_TEST_BUNDLE_ROOT)};
    ShaderFileSystem files;
    std::string      error, text;
    ASSERT_TRUE(files.LoadBundles(root, "first", &error)) << error;
    ASSERT_TRUE(files.LoadBundles(root, "second", &error)) << error;
    EXPECT_FALSE(files.LoadBundles(root, "second", &error));
    EXPECT_TRUE(files.ReadFile("first/pipeline/ToonPass.slangh", text));
    EXPECT_TRUE(files.ReadFile("second/pipeline/ToonPass.slangh", text));
    EXPECT_FALSE(files.LoadBundles(root, "plugins/missing", &error));
    EXPECT_TRUE(files.ReadFile("first/pipeline/ToonPass.slangh", text));
}

#if defined(_WIN32)
INSTANTIATE_TEST_SUITE_P(Targets, ShaderBundles, testing::Values(ShaderTarget::SPIRV, ShaderTarget::DXIL, ShaderTarget::MSL));
#else
INSTANTIATE_TEST_SUITE_P(Targets, ShaderBundles, testing::Values(ShaderTarget::SPIRV, ShaderTarget::MSL));
#endif
