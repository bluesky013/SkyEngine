//
// ShaderCompileTask / async resolver tests: inline render-parallel run and
// async worker run of the extracted compile payload.
//

#include <aurora/shader/ShaderCacheWriter.h>
#include <aurora/shader/ShaderCompileTask.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderResolver.h>

#include <core/async/ThreadPool.h>
#include <core/file/FileSystem.h>

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <string>

using namespace sky;
using namespace sky::aurora;

namespace {

    struct TempDir {
        std::string path;
        explicit TempDir(const std::string &name)
        {
            path = (std::filesystem::temp_directory_path() / ("sky_shadertask_" + name + "_" + std::to_string(::rand()))).string();
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

    ShaderResolver::Request MakeRequest()
    {
        ShaderResolver::Request req;
        req.relativePath = "s.slang";
        req.entry        = "mainFS";
        req.stage        = ShaderStageFlagBit::FS;
        req.target       = 0;
        req.variantHash  = 3;
        return req;
    }

} // namespace

TEST(ShaderCompileTaskTest, RunInline)
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

    ShaderResolver resolver(offline, local, source, 1, 2);
    resolver.SetCacheWriter(&writer);

    ShaderCompileResult immediate;
    ShaderCompileFuture future;
    ASSERT_EQ(resolver.ResolveShaderAsync(MakeRequest(), immediate, future, nullptr), ShaderResolveStatus::Compile);
    future.RunInline();
    ASSERT_TRUE(future.Ready());
    ASSERT_TRUE(future.Succeeded());
    EXPECT_EQ(compiler.count.load(), 1);

    writer.Flush();
    writer.Stop();
    ShaderCompilerFactory::Get().Register(nullptr);
}

TEST(ShaderCompileTaskTest, RunAsyncAndCacheHit)
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

    ShaderResolver resolver(offline, local, source, 1, 2);
    resolver.SetCacheWriter(&writer);

    ShaderCompileResult immediate;
    ShaderCompileFuture future;
    ASSERT_EQ(resolver.ResolveShaderAsync(MakeRequest(), immediate, future, nullptr), ShaderResolveStatus::Compile);

    ThreadPool pool(2);
    future.RunAsync(pool);
    pool.WaitIdle();
    ASSERT_TRUE(future.Ready());
    ASSERT_TRUE(future.Succeeded());
    EXPECT_EQ(compiler.count.load(), 1);

    writer.Flush();

    // second resolve with no source hits the writer snapshot, no recompile
    ShaderFileSystem emptySource;
    ShaderResolver   cached(offline, local, emptySource, 1, 2);
    cached.SetCacheWriter(&writer);
    ShaderCompileResult out;
    ASSERT_TRUE(cached.ResolveShader(MakeRequest(), out, nullptr));
    EXPECT_EQ(compiler.count.load(), 1);

    writer.Stop();
    ShaderCompilerFactory::Get().Register(nullptr);
}
