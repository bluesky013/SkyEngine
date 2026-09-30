//
// ShaderCacheStore tests: index/blob codec round-trip, content-addressed blob
// store, multi-root resolution, and key invalidation.
//

#include <aurora/shader/ShaderCacheStore.h>

#include <core/file/FileSystem.h>
#include <core/file/MultiFileSystem.h>

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
            path = (std::filesystem::temp_directory_path() / ("sky_shadercache_" + name + "_" + std::to_string(::rand()))).string();
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

} // namespace

TEST(ShaderCacheStoreTest, IndexRoundTrip)
{
    TempDir       dir("index");
    FileSystemPtr local = MakeFs(dir.path);

    ShaderVariantSchema schema;
    schema.sources.push_back({Name("material"), 0, 8});
    schema.entries.push_back({Name("USE_SHADOWS"), Name("material"), 0, 1, 0});
    schema.totalBits = 8;

    ShaderCacheIndex index;
    index.layoutFp    = 7;
    index.toolchainFp = 9;

    ShaderCachePathEntry entry;
    entry.schemaFp = HashSchema(schema);
    entry.schema   = schema;
    entry.artifacts.emplace_back(ArtifactKey(42, 1, 2, HashEntryName("FSMain")), ShaderCacheArtifact{1234, 99, {"common.slang"}});
    index.paths.emplace_back("material/lit.slang", entry);

    ASSERT_TRUE(SaveIndex(*local, "", index));

    ShaderCacheIndex loaded;
    ASSERT_TRUE(LoadIndex(*local, "", loaded));
    EXPECT_EQ(loaded.layoutFp, 7u);
    EXPECT_EQ(loaded.toolchainFp, 9u);

    const ShaderCachePathEntry *pe = FindPathEntry(loaded, "material/lit.slang");
    ASSERT_NE(pe, nullptr);
    EXPECT_EQ(pe->schemaFp, entry.schemaFp);
    const ShaderCacheArtifact *art = FindArtifact(*pe, 42, 1, 2, HashEntryName("FSMain"));
    ASSERT_NE(art, nullptr);
    EXPECT_EQ(art->compileHash, 1234u);
    EXPECT_EQ(art->sourceDeps.size(), 1u);
}

TEST(ShaderCacheStoreTest, BlobRoundTrip)
{
    TempDir           dir("blob");
    NativeFileSystem *localRaw = MakeFs(dir.path);

    ShaderCacheKey key{};
    key.sourceHash  = 1;
    key.variantHash = 2;
    key.layoutFp    = 3;
    key.schemaFp    = 4;
    key.toolchainFp = 5;
    key.target      = 1;

    ShaderCompileResult result;
    result.data = {0xdeadbeefu, 0x12345678u};
    ShaderResource res;
    res.name    = "gGlobal";
    res.type    = ShaderResourceType::UNIFORM_BUFFER;
    res.set     = 0;
    res.binding = 1;
    result.reflection.resources.push_back(res);
    result.reflection.threadGroupSize[0] = 8;

    ShaderBlobStore store(localRaw, localRaw, 3, 5);
    store.Store(key, result);

    ShaderCompileResult loaded;
    ASSERT_TRUE(store.Load(key, loaded));
    ASSERT_EQ(loaded.data.size(), 2u);
    EXPECT_EQ(loaded.data[0], 0xdeadbeefu);
    ASSERT_EQ(loaded.reflection.resources.size(), 1u);
    EXPECT_EQ(loaded.reflection.resources[0].name, "gGlobal");
    EXPECT_EQ(loaded.reflection.threadGroupSize[0], 8u);
}

TEST(ShaderCacheStoreTest, MultiRootOfflinePriority)
{
    TempDir           offlineDir("offline");
    TempDir           localDir("local");
    NativeFileSystem *offline = MakeFs(offlineDir.path);
    NativeFileSystem *local   = MakeFs(localDir.path);

    ShaderCacheKey key{};
    key.sourceHash  = 11;
    key.variantHash = 22;
    key.layoutFp    = 1;
    key.schemaFp    = 2;
    key.toolchainFp = 3;
    key.target      = 0;

    ShaderCompileResult result;
    result.data = {7u, 8u};
    ShaderBlobStore(offline, offline, 1, 3).Store(key, result);

    MultiFileSystem mfs;
    mfs.AddFileSystem(offline);
    mfs.AddFileSystem(local);

    ShaderBlobStore     store(&mfs, local, 1, 3);
    ShaderCompileResult loaded;
    ASSERT_TRUE(store.Load(key, loaded));
    ASSERT_EQ(loaded.data.size(), 2u);
    EXPECT_EQ(loaded.data[0], 7u);
}

TEST(ShaderCacheStoreTest, PerTargetDirectories)
{
    TempDir           dir("targets");
    NativeFileSystem *local = MakeFs(dir.path);

    ShaderCompileResult result;
    result.data = {1u};

    ShaderCacheKey vulkan{};
    vulkan.target = static_cast<uint32_t>(ShaderTarget::SPIRV);
    ShaderBlobStore(local, local, 0, 0).Store(vulkan, result);

    ShaderCacheKey metal{};
    metal.target = static_cast<uint32_t>(ShaderTarget::MSL);
    ShaderBlobStore(local, local, 0, 0).Store(metal, result);

    ShaderCacheKey d3d12{};
    d3d12.target = static_cast<uint32_t>(ShaderTarget::DXIL);
    ShaderBlobStore(local, local, 0, 0).Store(d3d12, result);

    EXPECT_TRUE(std::filesystem::exists(std::filesystem::path(dir.path) / "vulkan" / "blobs"));
    EXPECT_TRUE(std::filesystem::exists(std::filesystem::path(dir.path) / "metal" / "blobs"));
    EXPECT_TRUE(std::filesystem::exists(std::filesystem::path(dir.path) / "d3d12" / "blobs"));
}

TEST(ShaderCacheStoreTest, KeyInvalidation)
{
    ShaderCacheKey a{};
    a.layoutFp       = 1;
    ShaderCacheKey b = a;
    b.layoutFp       = 2;
    EXPECT_NE(CompileHash(a), CompileHash(b));

    ShaderCacheKey c = a;
    c.schemaFp       = 9;
    EXPECT_NE(CompileHash(a), CompileHash(c));

    ShaderVariantSchema s1;
    s1.sources.push_back({Name("material"), 0, 8});
    s1.totalBits           = 8;
    ShaderVariantSchema s2 = s1;
    s2.totalBits           = 16;
    EXPECT_NE(HashSchema(s1), HashSchema(s2));
}
