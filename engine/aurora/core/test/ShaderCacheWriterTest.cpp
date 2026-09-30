//
// ShaderCacheWriter tests: async serialized writes, no lost updates under
// concurrent Submit, and resolver integration.
//

#include <aurora/shader/ShaderCacheWriter.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderResolver.h>

#include <core/file/FileSystem.h>

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

using namespace sky;
using namespace sky::aurora;

namespace {

    struct TempDir {
        std::string path;
        explicit TempDir(const std::string &name)
        {
            path = (std::filesystem::temp_directory_path() / ("sky_shaderwriter_" + name + "_" + std::to_string(::rand()))).string();
            std::filesystem::create_directories(path);
        }
        ~TempDir()
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    };

    NativeFileSystem *MakeFs(const std::string &dir)
    {
        std::filesystem::create_directories(dir);
        return new NativeFileSystem(FilePath(dir));
    }

    class FakeCompiler : public IShaderCompiler {
    public:
        std::atomic<int> count{0};
        bool             Compile(const ShaderCompileDesc &desc, ShaderCompileResult &out) override
        {
            ++count;
            out.data       = {static_cast<uint32_t>(desc.source.size())};
            out.reflection = ShaderReflection{};
            return true;
        }
    };

    const char *kCompanion = R"({
        "sources": [{"name": "material", "width": 8}],
        "variants": [{"key": "USE_SHADOWS", "source": "material", "width": 1, "default": 0}]
    })";

} // namespace

TEST(ShaderCacheWriterTest, ConcurrentSubmitNoLoss)
{
    TempDir           dir("writer");
    NativeFileSystem *local = MakeFs(dir.path);

    ShaderCacheWriter writer(local);
    writer.Start();

    constexpr int kThreads   = 4;
    constexpr int kPerThread = 32;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t] {
            for (int i = 0; i < kPerThread; ++i) {
                ShaderCacheWriter::Pending p;
                p.key.target       = 0;
                p.key.variantHash  = static_cast<uint64_t>(t * kPerThread + i);
                p.key.stage        = 1;
                p.key.entryHash    = static_cast<uint64_t>(i);
                p.key.layoutFp     = 1;
                p.key.toolchainFp  = 2;
                p.key.schemaFp     = 3;
                p.key.sourceHash   = 4;
                p.result.data      = {static_cast<uint32_t>(i)};
                p.schema.totalBits = 8;
                p.schemaFp         = 3;
                p.sourceHash       = 4;
                p.relativePath     = "s" + std::to_string(t) + ".slang";
                writer.Submit(std::move(p));
            }
        });
    }
    for (auto &th : threads) {
        th.join();
    }
    writer.Flush();

    for (int t = 0; t < kThreads; ++t) {
        for (int i = 0; i < kPerThread; ++i) {
            ShaderCacheArtifact art;
            ASSERT_TRUE(writer.LookupArtifact(0, "s" + std::to_string(t) + ".slang", 3, static_cast<uint64_t>(t * kPerThread + i), 1,
                                              static_cast<uint64_t>(i), art))
                << "lost artifact t=" << t << " i=" << i;
        }
    }

    writer.Stop();
}

TEST(ShaderCacheWriterTest, ResolverAsyncStoreAndHit)
{
    TempDir           offlineDir("offline");
    TempDir           localDir("local");
    NativeFileSystem *offline = MakeFs(offlineDir.path);
    NativeFileSystem *local   = MakeFs(localDir.path);

    ShaderFileSystem source;
    source.AddVirtualFile("s.slang", "// v1");
    source.AddVirtualFile("s.slang.json", kCompanion);

    FakeCompiler compiler;
    ShaderCompilerFactory::Get().Register(&compiler);

    ShaderCacheWriter writer(local);
    writer.Start();

    ShaderResolver::Request vs;
    vs.relativePath = "s.slang";
    vs.entry        = "mainVS";
    vs.stage        = ShaderStageFlagBit::VS;
    vs.target       = 0;
    vs.variantHash  = 5;

    ShaderResolver::Request fs = vs;
    fs.entry                   = "mainFS";
    fs.stage                   = ShaderStageFlagBit::FS;

    {
        ShaderResolver resolver(offline, local, source, 1, 2);
        resolver.SetCacheWriter(&writer);
        ShaderCompileResult a;
        ShaderCompileResult b;
        ASSERT_TRUE(resolver.ResolveShader(vs, a, nullptr));
        ASSERT_TRUE(resolver.ResolveShader(fs, b, nullptr));
    }
    writer.Flush();
    EXPECT_EQ(compiler.count.load(), 2);

    // second pass with no source: reads the writer snapshot -> no recompile
    ShaderFileSystem emptySource;
    {
        ShaderResolver resolver(offline, local, emptySource, 1, 2);
        resolver.SetCacheWriter(&writer);
        ShaderCompileResult a;
        ShaderCompileResult b;
        ASSERT_TRUE(resolver.ResolveShader(vs, a, nullptr));
        ASSERT_TRUE(resolver.ResolveShader(fs, b, nullptr));
    }
    EXPECT_EQ(compiler.count.load(), 2);

    writer.Stop();
    ShaderCompilerFactory::Get().Register(nullptr);
}
