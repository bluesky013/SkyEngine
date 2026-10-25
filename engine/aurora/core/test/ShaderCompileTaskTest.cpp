//
// ShaderCompileTask / async resolver tests: inline render-parallel run and
// async worker run of the extracted compile payload.
//

#include "ShaderArtifactTest.h"

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
        std::atomic<int>       count{0};
        ShaderProducerIdentity GetProducerIdentity() const override
        {
            return test::TestProducer();
        }
        bool Compile(const ShaderCompileDesc &desc, ShaderCompileResult &out) override
        {
            ++count;
            return test::TestCompile(desc, GetProducerIdentity(), out);
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

    auto compiler = std::make_shared<FakeCompiler>();
    ASSERT_TRUE(ShaderCompilerFactory::Get().RegisterOwned(compiler));

    auto writer = std::make_shared<ShaderCacheWriter>(local);
    ASSERT_TRUE(writer->Start());

    ShaderResolver resolver(offline, local, source, 1, 2);
    ASSERT_TRUE(resolver.SetAsyncCacheOwners(FileSystemPtr(local), writer));

    ShaderCompileResult immediate;
    ShaderCompileFuture future;
    ASSERT_EQ(resolver.ResolveShaderAsync(MakeRequest(), immediate, future, nullptr), ShaderResolveStatus::Compile);
    future.RunInline();
    future.RunInline();
    ASSERT_TRUE(future.Ready());
    ASSERT_TRUE(future.Succeeded());
    EXPECT_EQ(compiler->count.load(), 1);

    writer->Flush();
    writer->Stop();
    ShaderCompilerFactory::Get().Register(nullptr);
}

TEST(ShaderCompileTaskTest, FutureRetainsCompilerModuleAfterResolverAndRegistrationAreGone)
{
    test::ShaderCacheDirectory       products("future-owners");
    ShaderCompileFuture              future;
    std::weak_ptr<FakeCompiler>      weakCompiler;
    std::weak_ptr<int>               weakModule;
    std::weak_ptr<ShaderCacheWriter> weakWriter;
    {
        auto compiler = std::make_shared<FakeCompiler>();
        auto module   = std::make_shared<int>(1);
        weakCompiler  = compiler;
        weakModule    = module;
        ASSERT_TRUE(ShaderCompilerFactory::Get().RegisterOwned(compiler, module));
        ShaderFileSystem source;
        source.AddVirtualFile("s.slang", "// captured source");
        source.AddVirtualFile("s.slang.json", kCompanion);
        auto writer = std::make_shared<ShaderCacheWriter>(products.fs.Get());
        weakWriter  = writer;
        ASSERT_TRUE(writer->Start());
        ShaderResolver resolver(nullptr, products.fs.Get(), source, 0, 0);
        ASSERT_TRUE(resolver.SetAsyncCacheOwners(products.fs, writer));
        ShaderCompileResult result;
        ASSERT_EQ(resolver.ResolveShaderAsync(MakeRequest(), result, future), ShaderResolveStatus::Compile);
        ShaderCompilerFactory::Get().Unregister(compiler.get());
    }
    products.fs = nullptr;
    EXPECT_FALSE(weakCompiler.expired());
    EXPECT_FALSE(weakModule.expired());
    EXPECT_FALSE(weakWriter.expired());
    ThreadPool pool(2);
    future.RunAsync(pool);
    future.RunInline();
    future.RunAsync(pool);
    pool.WaitIdle();
    ASSERT_TRUE(future.Succeeded()) << future.Result().errorInfo;
    EXPECT_EQ(weakCompiler.lock()->count.load(), 1);
    ASSERT_TRUE(weakWriter.lock()->Flush());
    future = {};
    EXPECT_TRUE(weakCompiler.expired());
    EXPECT_TRUE(weakModule.expired());
    EXPECT_TRUE(weakWriter.expired());
    NativeFileSystem verify{FilePath(products.path)};
    ShaderCacheIndex index;
    EXPECT_TRUE(LoadIndex(verify, ShaderTargetDirName(0), index));
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

    auto compiler = std::make_shared<FakeCompiler>();
    ASSERT_TRUE(ShaderCompilerFactory::Get().RegisterOwned(compiler));

    auto writer = std::make_shared<ShaderCacheWriter>(local);
    ASSERT_TRUE(writer->Start());

    ShaderResolver resolver(offline, local, source, 1, 2);
    ASSERT_TRUE(resolver.SetAsyncCacheOwners(FileSystemPtr(local), writer));

    ShaderCompileResult immediate;
    ShaderCompileFuture future;
    ASSERT_EQ(resolver.ResolveShaderAsync(MakeRequest(), immediate, future, nullptr), ShaderResolveStatus::Compile);

    ThreadPool pool(2);
    future.RunAsync(pool);
    future.RunAsync(pool);
    future.RunInline();
    pool.WaitIdle();
    ASSERT_TRUE(future.Ready());
    ASSERT_TRUE(future.Succeeded());
    EXPECT_EQ(compiler->count.load(), 1);

    writer->Flush();

    // Development hits require the same source closure and producer identity.
    ShaderResolver cached(offline, local, source, 1, 2);
    cached.SetCacheWriter(writer.get());
    ShaderCompileResult out;
    ASSERT_TRUE(cached.ResolveShader(MakeRequest(), out, nullptr));
    EXPECT_EQ(compiler->count.load(), 1);

    writer->Stop();
    ShaderCompilerFactory::Get().Register(nullptr);
}
