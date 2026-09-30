//
// ShaderResolver tests: offline hit, source-change recompile, source-absent
// local cache, and no-compiler miss.
//

#include <aurora/shader/ShaderResolver.h>

#include <aurora/shader/ShaderFileSystem.h>

#include <core/file/FileSystem.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

using namespace sky;
using namespace sky::aurora;

namespace {

    struct TempDir {
        std::string path;
        explicit TempDir(const std::string &name)
        {
            path = (std::filesystem::temp_directory_path() / ("sky_shaderresolver_" + name + "_" + std::to_string(::rand()))).string();
            std::filesystem::create_directories(path);
            blobsPath = (std::filesystem::path(path) / "blobs").string();
            std::filesystem::create_directories(blobsPath);
        }
        ~TempDir()
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
        std::string blobsPath;
    };

    NativeFileSystem *MakeFs(const std::string &dir)
    {
        std::filesystem::create_directories(dir);
        return new NativeFileSystem(FilePath(dir));
    }

    class FakeCompiler : public IShaderCompiler {
    public:
        int  count = 0;
        bool Compile(const ShaderCompileDesc &desc, ShaderCompileResult &out) override
        {
            ++count;
            out.data       = {static_cast<uint32_t>(desc.source.size())};
            out.reflection = ShaderReflection{};
            return true;
        }
    };

    ShaderVariantSchema MakeSchema()
    {
        ShaderVariantSchema schema;
        schema.sources.push_back({Name("material"), 0, 8});
        schema.entries.push_back({Name("USE_SHADOWS"), Name("material"), 0, 1, 0});
        schema.totalBits = 8;
        return schema;
    }

} // namespace

TEST(ShaderResolverTest, OfflineHitAfterCompile)
{
    TempDir           offlineDir("offline");
    TempDir           localDir("local");
    TempDir           otherDir("other");
    NativeFileSystem *offline = MakeFs(offlineDir.path);
    NativeFileSystem *local   = MakeFs(localDir.path);
    NativeFileSystem *other   = MakeFs(otherDir.path);

    ShaderFileSystem source;
    source.AddVirtualFile("s.slang", "// v1");

    ShaderVariantSchema schema   = MakeSchema();
    const uint64_t      schemaFp = HashSchema(schema);

    FakeCompiler compiler;
    ShaderCompilerFactory::Get().Register(&compiler);

    ShaderResolver::Request req;
    req.relativePath = "s.slang";
    req.entry        = "FSMain";
    req.stage        = ShaderStageFlagBit::FS;
    req.target       = 0;
    req.variantHash  = 42;
    req.schema       = &schema;
    req.schemaFp     = schemaFp;

    // first resolve: no cache -> compiles, writes local
    {
        ShaderResolver      resolver(offline, local, source, 1, 2);
        ShaderCompileResult out;
        std::string         error;
        ASSERT_TRUE(resolver.ResolveShader(req, out, &error)) << error;
    }
    EXPECT_EQ(compiler.count, 1);

    // second resolve with the produced cache as the (read-only) offline root ->
    // offline hit, no compile.
    {
        ShaderResolver      resolver(local, other, source, 1, 2);
        ShaderCompileResult out;
        std::string         error;
        ASSERT_TRUE(resolver.ResolveShader(req, out, &error)) << error;
    }
    EXPECT_EQ(compiler.count, 1);

    ShaderCompilerFactory::Get().Register(nullptr);
}

TEST(ShaderResolverTest, SourceChangeRecompiles)
{
    TempDir           offlineDir("offline");
    TempDir           localDir("local");
    NativeFileSystem *offline = MakeFs(offlineDir.path);
    NativeFileSystem *local   = MakeFs(localDir.path);

    ShaderFileSystem source;
    source.AddVirtualFile("s.slang", "// v1");

    ShaderVariantSchema schema   = MakeSchema();
    const uint64_t      schemaFp = HashSchema(schema);

    FakeCompiler compiler;
    ShaderCompilerFactory::Get().Register(&compiler);

    ShaderResolver::Request req;
    req.relativePath = "s.slang";
    req.entry        = "FSMain";
    req.stage        = ShaderStageFlagBit::FS;
    req.variantHash  = 1;
    req.schema       = &schema;
    req.schemaFp     = schemaFp;

    ShaderResolver      resolver(offline, local, source, 1, 2);
    ShaderCompileResult out;
    ASSERT_TRUE(resolver.ResolveShader(req, out, nullptr));
    EXPECT_EQ(compiler.count, 1);

    // change source -> sourceHash changes -> offline miss + local stale -> recompile
    source.AddVirtualFile("s.slang", "// v2 changed");
    ASSERT_TRUE(resolver.ResolveShader(req, out, nullptr));
    EXPECT_EQ(compiler.count, 2);

    ShaderCompilerFactory::Get().Register(nullptr);
}

TEST(ShaderResolverTest, SourceAbsentUsesLocalCache)
{
    TempDir           offlineDir("offline");
    TempDir           localDir("local");
    NativeFileSystem *offline = MakeFs(offlineDir.path);
    NativeFileSystem *local   = MakeFs(localDir.path);

    ShaderFileSystem source;
    source.AddVirtualFile("s.slang", "// v1");

    ShaderVariantSchema schema   = MakeSchema();
    const uint64_t      schemaFp = HashSchema(schema);

    FakeCompiler compiler;
    ShaderCompilerFactory::Get().Register(&compiler);

    ShaderResolver::Request req;
    req.relativePath = "s.slang";
    req.entry        = "FSMain";
    req.stage        = ShaderStageFlagBit::FS;
    req.variantHash  = 5;
    req.schema       = &schema;
    req.schemaFp     = schemaFp;

    {
        ShaderResolver      resolver(offline, local, source, 1, 2);
        ShaderCompileResult out;
        ASSERT_TRUE(resolver.ResolveShader(req, out, nullptr));
    }
    EXPECT_EQ(compiler.count, 1);

    // no source available -> local cache is used
    ShaderFileSystem    emptySource;
    ShaderResolver      resolver(offline, local, emptySource, 1, 2);
    ShaderCompileResult out;
    std::string         error;
    ASSERT_TRUE(resolver.ResolveShader(req, out, &error)) << error;
    EXPECT_EQ(compiler.count, 1);

    ShaderCompilerFactory::Get().Register(nullptr);
}

TEST(ShaderResolverTest, EntryLevelKeysDoNotCollide)
{
    TempDir           offlineDir("offline");
    TempDir           localDir("local");
    NativeFileSystem *offline = MakeFs(offlineDir.path);
    NativeFileSystem *local   = MakeFs(localDir.path);

    ShaderFileSystem source;
    source.AddVirtualFile("lit.slang", "// shared vs/fs");

    ShaderVariantSchema schema   = MakeSchema();
    const uint64_t      schemaFp = HashSchema(schema);

    FakeCompiler compiler;
    ShaderCompilerFactory::Get().Register(&compiler);

    ShaderResolver resolver(offline, local, source, 1, 2);

    ShaderResolver::Request vs;
    vs.relativePath = "lit.slang";
    vs.entry        = "mainVS";
    vs.stage        = ShaderStageFlagBit::VS;
    vs.variantHash  = 100;
    vs.schema       = &schema;
    vs.schemaFp     = schemaFp;

    ShaderResolver::Request fs = vs;
    fs.entry                   = "mainFS";
    fs.stage                   = ShaderStageFlagBit::FS;

    ShaderCompileResult vsOut;
    ShaderCompileResult fsOut;
    ASSERT_TRUE(resolver.ResolveShader(vs, vsOut, nullptr));
    ASSERT_TRUE(resolver.ResolveShader(fs, fsOut, nullptr));
    EXPECT_EQ(compiler.count, 2); // distinct keys -> both compiled, neither overwrote

    // both are cached and loadable with the cache as the read-only root
    ShaderResolver      cached(local, local, source, 1, 2);
    ShaderCompileResult vsCached;
    ShaderCompileResult fsCached;
    ASSERT_TRUE(cached.ResolveShader(vs, vsCached, nullptr));
    ASSERT_TRUE(cached.ResolveShader(fs, fsCached, nullptr));
    EXPECT_EQ(compiler.count, 2);

    ShaderCompilerFactory::Get().Register(nullptr);
}

TEST(ShaderResolverTest, NoCompilerMissFails)
{
    TempDir           offlineDir("offline");
    TempDir           localDir("local");
    NativeFileSystem *offline = MakeFs(offlineDir.path);
    NativeFileSystem *local   = MakeFs(localDir.path);

    ShaderFileSystem source;
    source.AddVirtualFile("s.slang", "// v1");

    ShaderVariantSchema schema = MakeSchema();

    ShaderResolver::Request req;
    req.relativePath = "s.slang";
    req.entry        = "FSMain";
    req.stage        = ShaderStageFlagBit::FS;
    req.variantHash  = 9;
    req.schema       = &schema;
    req.schemaFp     = HashSchema(schema);

    ShaderCompilerFactory::Get().Register(nullptr);

    ShaderResolver      resolver(offline, local, source, 1, 2);
    ShaderCompileResult out;
    std::string         error;
    EXPECT_FALSE(resolver.ResolveShader(req, out, &error));
    EXPECT_FALSE(error.empty());
}
