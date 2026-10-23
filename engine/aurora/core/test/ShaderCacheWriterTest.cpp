#include <aurora/shader/ShaderCacheWriter.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderResolver.h>

#include "ShaderArtifactTest.h"

#include <gtest/gtest.h>

#include <thread>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

namespace {
    ShaderCacheWriter::Pending TestPending(uint32_t entry)
    {
        ShaderCacheWriter::Pending pending;
        pending.key          = TestCacheKey({}, "s" + std::to_string(entry) + ".slang");
        pending.result       = TestArtifact(pending.key);
        pending.relativePath = pending.key.sourcePath;
        pending.schemaFp     = pending.key.schemaFp;
        pending.sourceHash   = pending.key.sourceHash;
        return pending;
    }
    class RejectIndexFileSystem : public NativeFileSystem {
    public:
        explicit RejectIndexFileSystem(const FilePath &root) : NativeFileSystem(root)
        {
        }
        bool Rename(const FilePath &from, const FilePath &to) const override
        {
            return to.FileName() != "index.bin" && NativeFileSystem::Rename(from, to);
        }
    };
} // namespace

TEST(ShaderCacheWriterTest, ConcurrentSubmitAndStopDoNotLoseAcceptedWrites)
{
    ShaderCacheDirectory directory("writerconcurrent");
    ShaderCacheWriter    writer(directory.fs.Get());
    ASSERT_TRUE(writer.Start());
    constexpr uint32_t       count = 24;
    std::vector<std::thread> workers;
    for (uint32_t worker = 0; worker < 4; ++worker) {
        workers.emplace_back([&writer, worker] {
            for (uint32_t item = 0; item < 6; ++item) {
                EXPECT_TRUE(writer.Submit(TestPending(worker * 6 + item)));
            }
        });
    }
    for (auto &worker : workers) {
        worker.join();
    }
    std::string error;
    ASSERT_TRUE(writer.Flush(&error)) << error;
    ASSERT_TRUE(writer.Stop(&error)) << error;
    for (uint32_t i = 0; i < count; ++i) {
        const auto          pending = TestPending(i);
        ShaderCacheArtifact record;
        ASSERT_TRUE(writer.LookupArtifact(pending.key, record)) << i;
        ShaderCompileResult result;
        EXPECT_TRUE(ShaderBlobStore(directory.fs.Get(), nullptr).LoadArtifact(record, result));
    }
    EXPECT_FALSE(writer.Submit(TestPending(count), &error));
    EXPECT_FALSE(error.empty());
}

TEST(ShaderCacheWriterTest, FailedIndexIsReportedAndNeverPublishedToSnapshot)
{
    ShaderCacheDirectory directory("writerfailure");
    FileSystemPtr        failing = new RejectIndexFileSystem(FilePath(directory.path));
    ShaderCacheWriter    writer(failing.Get());
    ASSERT_TRUE(writer.Start());
    const auto pending = TestPending(0);
    ASSERT_TRUE(writer.Submit(pending));
    std::string error;
    EXPECT_FALSE(writer.Flush(&error));
    EXPECT_NE(error.find("index publication failed"), std::string::npos);
    ShaderCacheArtifact record;
    EXPECT_FALSE(writer.LookupArtifact(pending.key, record));
    EXPECT_FALSE(failing->FileExist(FilePath("vulkan/index.bin")));
    EXPECT_FALSE(writer.Submit(TestPending(1), &error));
    EXPECT_FALSE(writer.Stop(&error));
}

TEST(ShaderCacheWriterTest, ResolverReadsOnlyDurableSnapshots)
{
    ShaderCacheDirectory directory("writerresolver");
    ShaderCacheWriter    writer(directory.fs.Get());
    ASSERT_TRUE(writer.Start());
    ShaderFileSystem sources;
    sources.AddVirtualFile("s.slang", "// source");
    const ShaderVariantSchema schema;
    ShaderResolver::Request   request;
    request.relativePath = "s.slang";
    request.entry        = "mainFS";
    request.schema       = &schema;
    request.environment  = DefaultShaderEnvironment(ShaderTarget::SPIRV, ShaderPlatform::MACOS);
    TestShaderCompiler         compiler;
    const CompilerRegistration registration(&compiler);
    ShaderResolver             resolver(nullptr, directory.fs.Get(), sources, 0, 0);
    resolver.SetCacheWriter(&writer);
    ShaderCompileResult first, second;
    std::string         error;
    ASSERT_TRUE(resolver.ResolveShader(request, first, &error)) << error;
    ASSERT_TRUE(writer.Flush(&error)) << error;
    ASSERT_TRUE(resolver.ResolveShader(request, second, &error)) << error;
    EXPECT_EQ(compiler.count, 1u);
    EXPECT_EQ(first.artifactId, second.artifactId);
    ASSERT_TRUE(writer.Stop());
}

TEST(ShaderCacheWriterTest, InvalidArtifactCannotPublishIndex)
{
    ShaderCacheDirectory directory("writerinvalid");
    ShaderCacheWriter    writer(directory.fs.Get());
    ASSERT_TRUE(writer.Start());
    auto pending = TestPending(0);
    pending.result.data.back() ^= 1;
    ASSERT_TRUE(writer.Submit(pending));
    std::string error;
    EXPECT_FALSE(writer.Flush(&error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(directory.fs->FileExist(FilePath("vulkan/index.bin")));
    ShaderCacheArtifact record;
    EXPECT_FALSE(writer.LookupArtifact(pending.key, record));
}
