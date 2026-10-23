#include <aurora/shader/ShaderResolver.h>

#include <aurora/shader/ShaderFileSystem.h>

#include "ShaderArtifactTest.h"

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

namespace {
    ShaderResolver::Request TestRequest(const ShaderVariantSchema &schema)
    {
        ShaderResolver::Request request;
        request.relativePath = "s.slang";
        request.entry        = "mainFS";
        request.schema       = &schema;
        request.schemaFp     = HashSchema(schema);
        request.environment  = DefaultShaderEnvironment(ShaderTarget::SPIRV, ShaderPlatform::MACOS);
        return request;
    }
} // namespace

TEST(ShaderResolverTest, DevelopmentUsesExactInputsBeforeCompiling)
{
    ShaderCacheDirectory directory("resolver");
    ShaderFileSystem     sources;
    sources.AddVirtualFile("s.slang", "// initial source");
    const ShaderVariantSchema  schema;
    const auto                 request = TestRequest(schema);
    TestShaderCompiler         compiler;
    const CompilerRegistration registration(&compiler);
    ShaderResolver             resolver(nullptr, directory.fs.Get(), sources, 1, 999);
    ShaderCompileResult        first, same, changed;
    std::string                error;
    ASSERT_TRUE(resolver.ResolveShader(request, first, &error)) << error;
    ASSERT_TRUE(resolver.ResolveShader(request, same, &error)) << error;
    EXPECT_EQ(compiler.count, 1u);
    EXPECT_EQ(first.artifactId, same.artifactId);
    sources.AddVirtualFile("s.slang", "// changed source");
    ASSERT_TRUE(resolver.ResolveShader(request, changed, &error)) << error;
    EXPECT_EQ(compiler.count, 2u);
    EXPECT_NE(first.artifactId, changed.artifactId);
    compiler.producer.version = "new-version";
    ASSERT_TRUE(resolver.ResolveShader(request, changed, &error)) << error;
    EXPECT_EQ(compiler.count, 3u);
    EXPECT_EQ(changed.identity.producer.version, "new-version");
}

TEST(ShaderResolverTest, ProductsDoNotNeedCurrentCompilerOrSources)
{
    ShaderCacheDirectory directory("products");
    ShaderFileSystem     sources;
    sources.AddVirtualFile("s.slang", "// source");
    const ShaderVariantSchema  schema;
    auto                       request = TestRequest(schema);
    TestShaderCompiler         compiler;
    const CompilerRegistration registration(&compiler);
    ShaderCompileResult        cooked;
    ShaderResolver             development(nullptr, directory.fs.Get(), sources, 7, 11);
    std::string                error;
    ASSERT_TRUE(development.ResolveShader(request, cooked, &error)) << error;
    ShaderCompilerFactory::Get().Register(nullptr);
    ShaderFileSystem empty;
    request.schema   = nullptr;
    request.schemaFp = 0;
    ShaderResolver      products(directory.fs.Get(), nullptr, empty, 7, UINT64_MAX, ShaderResolveMode::PRODUCTS);
    ShaderCompileResult shipped;
    ASSERT_TRUE(products.ResolveShader(request, shipped, &error)) << error;
    EXPECT_EQ(shipped.artifactId, cooked.artifactId);
    EXPECT_EQ(shipped.identity.producer, compiler.producer);
    // A stray source companion cannot redirect PRODUCTS resolution.
    empty.AddVirtualFile("s.slang.json", "invalid companion");
    empty.AddVirtualFile("s.slang", "changed source");
    ASSERT_TRUE(products.ResolveShader(request, shipped, &error)) << error;
    EXPECT_EQ(shipped.artifactId, cooked.artifactId);
}

TEST(ShaderResolverTest, DownstreamProducerChangesOnlyInvalidateTheirTarget)
{
    class TargetCompiler : public IShaderCompiler {
    public:
        uint64_t               downstream = 1;
        uint32_t               count      = 0;
        bool                   available  = true;
        ShaderProducerIdentity GetProducerIdentity() const override
        {
            return TestProducer();
        }
        ShaderProducerIdentity GetProducerIdentity(ShaderTarget target) const override
        {
            if (target == ShaderTarget::DXIL && !available)
                return {};
            auto identity = GetProducerIdentity();
            if (target == ShaderTarget::DXIL)
                identity.configuration = downstream;
            return identity;
        }
        bool Compile(const ShaderCompileDesc &desc, ShaderCompileResult &result) override
        {
            ++count;
            return TestCompile(desc, GetProducerIdentity(desc.target), result);
        }
    };
    ShaderCacheDirectory directory("targetproducer");
    ShaderFileSystem     sources;
    sources.AddVirtualFile("s.slang", "// unchanged source");
    const ShaderVariantSchema schema;
    auto                      spirv = TestRequest(schema);
    auto                      dxil  = spirv;
    dxil.target                     = static_cast<uint32_t>(ShaderTarget::DXIL);
    dxil.environment                = DefaultShaderEnvironment(ShaderTarget::DXIL, ShaderPlatform::WINDOWS);
    TargetCompiler             compiler;
    const CompilerRegistration registration(&compiler);
    ShaderResolver             resolver(nullptr, directory.fs.Get(), sources, 0, 0);
    ShaderCompileResult        spirvFirst, dxilFirst, current;
    std::string                error;
    ASSERT_TRUE(resolver.ResolveShader(spirv, spirvFirst, &error)) << error;
    ASSERT_TRUE(resolver.ResolveShader(dxil, dxilFirst, &error)) << error;
    ASSERT_EQ(compiler.count, 2u);
    compiler.available = false;
    EXPECT_FALSE(resolver.ResolveShader(dxil, current, &error));
    EXPECT_NE(error.find("producer identity"), std::string::npos);
    EXPECT_EQ(compiler.count, 2u);
    compiler.available = true;
    ++compiler.downstream;
    ASSERT_TRUE(resolver.ResolveShader(spirv, current, &error)) << error;
    EXPECT_EQ(current.artifactId, spirvFirst.artifactId);
    EXPECT_EQ(compiler.count, 2u);
    ASSERT_TRUE(resolver.ResolveShader(dxil, current, &error)) << error;
    EXPECT_EQ(compiler.count, 3u);
    EXPECT_NE(current.artifactId, dxilFirst.artifactId);
    EXPECT_EQ(current.identity.producer, compiler.GetProducerIdentity(ShaderTarget::DXIL));
    const auto publishedId = current.artifactId;
    ++compiler.downstream;
    ShaderFileSystem empty;
    ShaderResolver   products(directory.fs.Get(), nullptr, empty, 0, 0, ShaderResolveMode::PRODUCTS);
    ASSERT_TRUE(products.ResolveShader(dxil, current, &error)) << error;
    EXPECT_EQ(current.artifactId, publishedId);
    EXPECT_EQ(compiler.count, 3u);
}

TEST(ShaderResolverTest, DevelopmentDoesNotBecomeImplicitProductsMode)
{
    const ShaderVariantSchema schema;
    auto                      request = TestRequest(schema);
    ShaderCacheDirectory      directory("explicitmode");
    ShaderFileSystem          sources;
    sources.AddVirtualFile("s.slang", "// source");
    TestShaderCompiler         compiler;
    const CompilerRegistration registration(&compiler);
    ShaderResolver             resolver(nullptr, directory.fs.Get(), sources, 0, 0);
    ShaderCompileResult        result;
    ASSERT_TRUE(resolver.ResolveShader(request, result));
    ShaderFileSystem empty;
    ShaderResolver   noSource(directory.fs.Get(), nullptr, empty, 0, 0);
    std::string      error;
    EXPECT_FALSE(noSource.ResolveShader(request, result, &error));
    EXPECT_NE(error.find("DEVELOPMENT"), std::string::npos);
    ShaderCompilerFactory::Get().Register(nullptr);
    EXPECT_FALSE(resolver.ResolveShader(request, result, &error));
    EXPECT_NE(error.find("producer identity"), std::string::npos);
}

TEST(ShaderResolverTest, PublishedEnvironmentAndEntryMustMatch)
{
    ShaderCacheDirectory directory("isolation");
    ShaderFileSystem     sources;
    sources.AddVirtualFile("s.slang", "// source");
    const ShaderVariantSchema schema;
    auto                      request = TestRequest(schema);
    request.target                    = static_cast<uint32_t>(ShaderTarget::MSL);
    request.environment               = DefaultShaderEnvironment(ShaderTarget::MSL, ShaderPlatform::MACOS);
    TestShaderCompiler         compiler;
    const CompilerRegistration registration(&compiler);
    ShaderResolver             development(nullptr, directory.fs.Get(), sources, 0, 0);
    ShaderCompileResult        result;
    ASSERT_TRUE(development.ResolveShader(request, result));
    ShaderFileSystem empty;
    ShaderResolver   products(directory.fs.Get(), directory.fs.Get(), empty, 0, 0, ShaderResolveMode::PRODUCTS);
    request.environment.platform = ShaderPlatform::IOS;
    std::string error;
    EXPECT_FALSE(products.ResolveShader(request, result, &error));
    EXPECT_NE(error.find("environment"), std::string::npos);
    request.environment.platform = ShaderPlatform::MACOS;
    request.entry                = "otherFS";
    EXPECT_FALSE(products.ResolveShader(request, result, &error));
    request.entry = "mainFS";
    request.stage = ShaderStageFlagBit::VS;
    EXPECT_FALSE(products.ResolveShader(request, result, &error));
}

TEST(ShaderResolverTest, SpecializationOnlySplitsD3D12Artifacts)
{
    ShaderCacheDirectory directory("specialization");
    ShaderFileSystem     sources;
    sources.AddVirtualFile("s.slang", "// source");
    ShaderVariantSchema schema;
    schema.sources        = {{Name("material"), 0, 8}};
    schema.entries        = {{Name("MODE"), Name("material"), 0, 1, 0, true, 3}};
    schema.totalBits      = 8;
    auto          request = TestRequest(schema);
    ShaderVariant variant;
    variant.entries = {{Name("MODE"), 0}};
    request.variant = &variant;
    TestShaderCompiler         compiler;
    const CompilerRegistration registration(&compiler);
    ShaderResolver             resolver(nullptr, directory.fs.Get(), sources, 0, 0);
    ShaderCompileResult        zero, one;
    ASSERT_TRUE(resolver.ResolveShader(request, zero));
    variant.entries[0].value = 1;
    ASSERT_TRUE(resolver.ResolveShader(request, one));
    EXPECT_EQ(zero.artifactId, one.artifactId);
    EXPECT_EQ(compiler.count, 1u);
    request.target           = static_cast<uint32_t>(ShaderTarget::DXIL);
    request.environment      = DefaultShaderEnvironment(ShaderTarget::DXIL, ShaderPlatform::WINDOWS);
    variant.entries[0].value = 0;
    ASSERT_TRUE(resolver.ResolveShader(request, zero));
    variant.entries[0].value = 1;
    ASSERT_TRUE(resolver.ResolveShader(request, one));
    EXPECT_NE(zero.artifactId, one.artifactId);
    EXPECT_EQ(compiler.count, 3u);
}

TEST(ShaderResolverTest, ProductsSelectManifestOptionsWithoutAnExplicitEnvironment)
{
    ShaderCacheDirectory directory("manifestoptions");
    ShaderFileSystem     sources;
    sources.AddVirtualFile("s.slang", "// source");
    const ShaderVariantSchema schema;
    auto                      request = TestRequest(schema);
    request.environment               = DefaultShaderEnvironment(ShaderTarget::SPIRV);
    request.environment.options       = {"optimization=0"};
    TestShaderCompiler         compiler;
    const CompilerRegistration registration(&compiler);
    ShaderResolver             development(nullptr, directory.fs.Get(), sources, 0, 0);
    ShaderCompileResult        cooked;
    std::string                error;
    ASSERT_TRUE(development.ResolveShader(request, cooked, &error)) << error;
    const auto manifest = TestProductManifest(request.environment);
    ASSERT_TRUE(WriteFileAtomic(*directory.fs, "distribution.json", {manifest.begin(), manifest.end()}));
    ShaderCompilerFactory::Get().Register(nullptr);
    ShaderFileSystem empty;
    ShaderResolver   products(directory.fs.Get(), nullptr, empty, 0, 0, ShaderResolveMode::PRODUCTS);
    request.environment = {};
    ShaderCompileResult shipped;
    ASSERT_TRUE(products.ResolveShader(request, shipped, &error)) << error;
    EXPECT_EQ(shipped.artifactId, cooked.artifactId);
    EXPECT_EQ(shipped.identity.environment.options, (std::vector<std::string>{"optimization=0"}));
    request.environment = DefaultShaderEnvironment(ShaderTarget::SPIRV);
    EXPECT_FALSE(products.ResolveShader(request, shipped, &error));
    EXPECT_NE(error.find("environment"), std::string::npos);
}
