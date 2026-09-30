//
// ShaderUsage tests: collection dedup, JSON union flush, grouping, offline build.
//

#include <aurora/shader/ShaderUsage.h>

#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderResolver.h>

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
            path = (std::filesystem::temp_directory_path() / ("sky_shaderusage_" + name + "_" + std::to_string(::rand()))).string();
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
        int  count = 0;
        bool Compile(const ShaderCompileDesc &desc, ShaderCompileResult &out) override
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

    ShaderUsageEntry MakeEntry(const char *entry, uint32_t stage)
    {
        ShaderUsageEntry e;
        e.relativePath = "s.slang";
        e.entry        = entry;
        e.stage        = stage;
        e.target       = 0;
        e.variantHash  = 7;
        return e;
    }

} // namespace

TEST(ShaderUsageTest, DedupAndToggle)
{
    ShaderUsageCollector collector;
    collector.Record(MakeEntry("mainFS", 0x02));
    EXPECT_EQ(collector.Size(), 0u); // disabled

    collector.SetEnabled(true);
    collector.Record(MakeEntry("mainFS", 0x02));
    collector.Record(MakeEntry("mainFS", 0x02)); // duplicate
    collector.Record(MakeEntry("mainVS", 0x01));
    EXPECT_EQ(collector.Size(), 2u);
}

TEST(ShaderUsageTest, Grouping)
{
    std::vector<ShaderUsageEntry> usage  = {MakeEntry("mainVS", 0x01), MakeEntry("mainFS", 0x02)};
    auto                          groups = GroupShaderUsage(usage);
    ASSERT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0].entries.size(), 2u);
}

TEST(ShaderUsageTest, FlushUnionMerge)
{
    TempDir           dir("flush");
    NativeFileSystem *fs = MakeFs(dir.path);

    ShaderUsageCollector a;
    a.SetEnabled(true);
    a.Record(MakeEntry("mainVS", 0x01));
    ASSERT_TRUE(a.Flush(*fs, "shader_usage.json"));

    ShaderUsageCollector b;
    b.SetEnabled(true);
    b.Record(MakeEntry("mainFS", 0x02));
    ASSERT_TRUE(b.Flush(*fs, "shader_usage.json"));

    std::vector<ShaderUsageEntry> merged;
    ASSERT_TRUE(ShaderUsageCollector::Load(*fs, "shader_usage.json", merged));
    EXPECT_EQ(merged.size(), 2u);
}

TEST(ShaderUsageTest, OfflineBuild)
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

    std::vector<ShaderUsageEntry> usage = {MakeEntry("mainVS", 0x01), MakeEntry("mainFS", 0x02)};

    {
        ShaderResolver resolver(offline, local, source, 1, 2);
        std::string    error;
        ASSERT_TRUE(BuildUsageCache(resolver, usage, &error)) << error;
    }
    EXPECT_EQ(compiler.count, 2);

    // cached: resolve with a read-only local root and no source, no recompile
    ShaderFileSystem emptySource;
    ShaderResolver   cached(local, local, emptySource, 1, 2);
    for (const auto &e : usage) {
        ShaderResolver::Request req;
        req.relativePath = e.relativePath;
        req.entry        = e.entry;
        req.stage        = static_cast<ShaderStageFlagBit>(e.stage);
        req.target       = e.target;
        req.variantHash  = e.variantHash;
        ShaderCompileResult out;
        std::string         error;
        ASSERT_TRUE(cached.ResolveShader(req, out, &error)) << error;
    }
    EXPECT_EQ(compiler.count, 2);

    ShaderCompilerFactory::Get().Register(nullptr);
}
