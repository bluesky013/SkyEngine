#include <algorithm>
#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderResolver.h>
#include <gtest/gtest.h>
#include <map>

using namespace sky::aurora;

namespace {
    class PackageText final : public sky::IFile {
    public:
        explicit PackageText(std::string text) : text(std::move(text))
        {
        }
        bool ReadString(std::string &out) override
        {
            out = text;
            return true;
        }
        bool ReadBin(std::vector<uint8_t> &out) override
        {
            out.assign(text.begin(), text.end());
            return true;
        }
        void ReadData(uint64_t, uint64_t, uint8_t *) override
        {
            ADD_FAILURE() << "Unexpected raw read";
        }
        sky::IStreamArchivePtr ReadAsArchive() override
        {
            ADD_FAILURE() << "Unexpected archive read";
            return {};
        }
        sky::OStreamArchivePtr WriteAsArchive() override
        {
            ADD_FAILURE() << "Read-only package";
            return {};
        }
        sky::BinaryDataPtr ReadBin() override
        {
            ADD_FAILURE() << "Unexpected binary read";
            return {};
        }
        uint64_t AppendData(const char *, uint64_t) override
        {
            ADD_FAILURE() << "Read-only package";
            return 0;
        }
        std::string GetPath() const override
        {
            return {};
        }

    private:
        std::string text;
    };
    class PackageSources final : public sky::IFileSystem {
    public:
        bool FileExist(const sky::FilePath &path) const override
        {
            return files.contains(Logical(path));
        }
        sky::FilePtr OpenFile(const sky::FilePath &path) override
        {
            const auto found = files.find(Logical(path));
            return found == files.end() ? sky::FilePtr{} : sky::FilePtr(new PackageText(found->second));
        }
        sky::FilePtr CreateOrOpenFile(const sky::FilePath &) override
        {
            return {};
        }
        std::map<std::string, std::string> files;

    private:
        static std::string Logical(const sky::FilePath &path)
        {
            auto result = path.GetStr();
            std::ranges::replace(result, '\\', '/');
            return result;
        }
    };
    struct CompilerRegistration {
        explicit CompilerRegistration(IShaderCompiler &compiler) : previous(ShaderCompilerFactory::Get().GetCompiler())
        {
            ShaderCompilerFactory::Get().Register(&compiler);
        }
        ~CompilerRegistration()
        {
            ShaderCompilerFactory::Get().Register(previous);
        }
        IShaderCompiler *previous;
    };
    ShaderSourceBundle Bundle(const std::string &owner, std::string text = "float value;")
    {
        return {owner, {}, {{owner + "/Pass.h", std::move(text)}}};
    }
    class ReadingCompiler final : public IShaderCompiler {
    public:
        bool Compile(const ShaderCompileDesc &desc, ShaderCompileResult &out) override
        {
            std::string included;
            const bool  valid = desc.fileSystem && desc.fileSystem->ReadFile("plugins/a/Pass.h", included);
            out.errorInfo     = valid ? included : "missing";
            return valid;
        }
    };
} // namespace

TEST(ShaderSourceBundle, RegistrationRollsBackEveryOwnerOnCollisionOrMissingDependency)
{
    ShaderFileSystem         files;
    const ShaderSourceBundle initial[]{Bundle("plugins/a", "old")};
    ASSERT_TRUE(files.AddBundles(initial));
    const ShaderSourceBundle collision[]{Bundle("plugins/b"), Bundle("plugins/a", "new")};
    std::string              error;
    EXPECT_FALSE(files.AddBundles(collision, &error));
    EXPECT_NE(error.find("plugins/a"), std::string::npos);
    std::string text;
    EXPECT_FALSE(files.ReadFile("plugins/b/Pass.h", text));
    ASSERT_TRUE(files.ReadFile("plugins/a/Pass.h", text));
    EXPECT_EQ(text, "old");
    auto missing         = Bundle("plugins/b");
    missing.dependencies = {"plugins/absent"};
    EXPECT_FALSE(files.AddBundles(std::span{&missing, 1}, &error));
    EXPECT_FALSE(files.ReadFile("plugins/b/Pass.h", text));
    EXPECT_FALSE(files.AddVirtualFile("plugins/a/Pass.h", "replacement"));
    ASSERT_TRUE(files.AddVirtualFile("plugins/c/Other.h", "loose"));
    const ShaderSourceBundle overlaps[]{Bundle("plugins/c")};
    EXPECT_FALSE(files.AddBundles(overlaps, &error));
    ASSERT_TRUE(files.ReadFile("plugins/c/Other.h", text));
    EXPECT_EQ(text, "loose");
}

TEST(ShaderSourceBundle, RejectsInvalidPathsDuplicateFilesAndCycles)
{
    ShaderFileSystem files;
    for (const char *path : {"../escape", "/absolute", "C:/absolute", "a//b", "a/./b", "a/../b", "a\\b"}) {
        auto invalid               = Bundle("plugins/a");
        invalid.files.front().path = path;
        EXPECT_FALSE(files.AddBundles(std::span{&invalid, 1})) << path;
    }
    auto duplicate = Bundle("plugins/a");
    duplicate.files.push_back(duplicate.files.front());
    EXPECT_FALSE(files.AddBundles(std::span{&duplicate, 1}));
    ShaderSourceBundle cycle[]{Bundle("plugins/a"), Bundle("plugins/b")};
    cycle[0].dependencies = {"plugins/b"};
    cycle[1].dependencies = {"plugins/a"};
    EXPECT_FALSE(files.AddBundles(cycle));
    EXPECT_FALSE(files.IsBundlePath("plugins/a/Pass.h"));
}

TEST(ShaderSourceBundle, SnapshotOnlyExposesDeclaredDependencyClosure)
{
    ShaderFileSystem   files;
    ShaderSourceBundle bundles[]{Bundle("plugins/a"), Bundle("plugins/b"), Bundle("aurora/shader")};
    bundles[0].dependencies = {"aurora/shader"};
    ASSERT_TRUE(files.AddBundles(bundles));
    const auto  snapshot = files.Snapshot("plugins/a/Pass.h");
    std::string text;
    EXPECT_TRUE(snapshot->ReadFile("plugins/a/Pass.h", text));
    EXPECT_TRUE(snapshot->ReadFile("aurora/shader/Pass.h", text));
    EXPECT_FALSE(snapshot->ReadFile("plugins/b/Pass.h", text));
    EXPECT_FALSE(files.RemoveBundle("aurora/shader"));
    ASSERT_TRUE(files.RemoveBundle("plugins/a"));
    ASSERT_TRUE(files.RemoveBundle("aurora/shader"));
    EXPECT_TRUE(snapshot->ReadFile("aurora/shader/Pass.h", text));
}

TEST(ShaderSourceBundle, PreparedTaskOwnsSourceAndVariantAfterProviderRetirement)
{
    ReadingCompiler      compiler;
    CompilerRegistration registration(compiler);
    ShaderCompileTask    task;
    ShaderCompileResult  result;
    {
        ShaderFileSystem         files;
        const ShaderSourceBundle bundle[]{Bundle("plugins/a", "original")};
        EXPECT_TRUE(files.AddBundles(bundle));
        ShaderResolver          resolver(nullptr, nullptr, files, 0, 0);
        ShaderVariantSchema     schema;
        ShaderVariant           variant{{{sky::Name("FLAG"), 7}}};
        ShaderResolver::Request request;
        request.relativePath = "plugins/a/Pass.h";
        request.entry        = "FSMain";
        request.schema       = &schema;
        request.variant      = &variant;
        EXPECT_EQ(resolver.PrepareShader(request, result, task), ShaderResolveStatus::Compile);
        EXPECT_TRUE(files.RemoveBundle("plugins/a"));
        const ShaderSourceBundle replacement[]{Bundle("plugins/a", "replacement")};
        EXPECT_TRUE(files.AddBundles(replacement));
    }
    EXPECT_TRUE(task.Run(compiler, result));
    EXPECT_EQ(result.errorInfo, "original");
    ASSERT_TRUE(task.MakeDesc().variant);
    EXPECT_EQ(task.MakeDesc().variant->entries.front().value, 7u);
}

TEST(ShaderSourceBundle, PackageInterfaceMatchesEmbeddedContentAndFailedLoadsAreAtomic)
{
    PackageSources package;
    package.files["shader-sources.json"] =
        R"({"version":1,"modules":[{"owner":"plugins/a","dependencies":[],"includeRoots":[],"files":["plugins/a/Pass.h"]}]})";
    package.files["plugins/a/Pass.h"] = "float value;\r\n";
    ShaderFileSystem packaged, embedded;
    std::string      error, text;
    ASSERT_TRUE(packaged.LoadBundles(package, "plugins/a", &error)) << error;
    const ShaderSourceBundle bundle[]{Bundle("plugins/a", "float value;\n")};
    ASSERT_TRUE(embedded.AddBundles(bundle));
    uint64_t packageHash = 0, embeddedHash = 0;
    ASSERT_TRUE(ShaderResolver::HashSourceClosure(packaged, "plugins/a/Pass.h", {}, packageHash, text));
    ASSERT_TRUE(ShaderResolver::HashSourceClosure(embedded, "plugins/a/Pass.h", {}, embeddedHash, text));
    EXPECT_EQ(packageHash, embeddedHash);
    package.files["shader-sources.json"] =
        R"({"version":1,"modules":[{"owner":"plugins/b","dependencies":[],"includeRoots":[],"files":["plugins/b/Missing.h"]}]})";
    EXPECT_FALSE(packaged.LoadBundles(package, "plugins/b", &error));
    EXPECT_FALSE(packaged.IsBundlePath("plugins/b/Missing.h"));
    package.files["shader-sources.json"] = R"({"version":1,"modules":[{"owner":false}]})";
    EXPECT_FALSE(packaged.LoadBundles(package, "plugins/b", &error));
    ASSERT_TRUE(packaged.ReadFile("plugins/a/Pass.h", text));
    EXPECT_EQ(text, "float value;\n");
}

TEST(ShaderSourceBundle, IdenticalBasenamesAndChangedDependenciesHaveDistinctHashes)
{
    ShaderFileSystem   files;
    ShaderSourceBundle bundles[]{Bundle("plugins/a", "#include <shared/data/Pass.h>\n"), Bundle("plugins/b", "#include <shared/data/Pass.h>\n"),
                                 Bundle("shared/data", "float value;\r\n")};
    bundles[0].dependencies = {"shared/data"};
    bundles[1].dependencies = {"shared/data"};
    ASSERT_TRUE(files.AddBundles(bundles));
    uint64_t    first = 0, second = 0, changed = 0;
    std::string text;
    ASSERT_TRUE(ShaderResolver::HashSourceClosure(files, "plugins/a/Pass.h", {}, first, text));
    ASSERT_TRUE(ShaderResolver::HashSourceClosure(files, "plugins/b/Pass.h", {}, second, text));
    EXPECT_NE(first, second);
    ASSERT_TRUE(files.RemoveBundle("plugins/a"));
    ASSERT_TRUE(files.RemoveBundle("plugins/b"));
    ASSERT_TRUE(files.RemoveBundle("shared/data"));
    bundles[2].files.front().content = "float changed;\n";
    ASSERT_TRUE(files.AddBundles(bundles));
    ASSERT_TRUE(ShaderResolver::HashSourceClosure(files, "plugins/a/Pass.h", {}, changed, text));
    EXPECT_NE(first, changed);
}

TEST(ShaderSourceBundle, BundleHashCoversMacroIncludesAndIgnoresManifestOrder)
{
    ShaderFileSystem files;
    auto             bundle = Bundle("plugins/a", "#define HEADER \"Indirect.h\"\n#include HEADER\n");
    bundle.files.push_back({"plugins/a/Indirect.h", "float original;"});
    ASSERT_TRUE(files.AddBundles(std::span{&bundle, 1}));
    uint64_t    before = 0, reordered = 0, changed = 0;
    std::string source;
    ASSERT_TRUE(ShaderResolver::HashSourceClosure(files, "plugins/a/Pass.h", {}, before, source));
    ASSERT_TRUE(files.RemoveBundle("plugins/a"));
    std::ranges::reverse(bundle.files);
    ASSERT_TRUE(files.AddBundles(std::span{&bundle, 1}));
    ASSERT_TRUE(ShaderResolver::HashSourceClosure(files, "plugins/a/Pass.h", {}, reordered, source));
    EXPECT_EQ(before, reordered);
    ASSERT_TRUE(files.RemoveBundle("plugins/a"));
    bundle.files.front().content = "float changed;";
    ASSERT_TRUE(files.AddBundles(std::span{&bundle, 1}));
    ASSERT_TRUE(ShaderResolver::HashSourceClosure(files, "plugins/a/Pass.h", {}, changed, source));
    EXPECT_NE(before, changed);
}

TEST(ShaderSourceBundle, CaseCollisionsAndAmbiguousIncludeRootsFailBeforePublication)
{
    ShaderFileSystem files;
    auto             first = Bundle("first");
    first.files            = {{"first/include/Value.slangh", "first"}};
    first.includeRoots     = {"first/include"};
    ASSERT_TRUE(files.AddBundles(std::span{&first, 1}));
    const auto before    = files.BundleFingerprint();
    auto       duplicate = first;
    duplicate.files.push_back({"first/include/value.slangh", "second"});
    ShaderFileSystem empty;
    EXPECT_FALSE(empty.AddBundles(std::span{&duplicate, 1}));
    auto second = Bundle("First");
    EXPECT_FALSE(files.AddBundles(std::span{&second, 1}));
    EXPECT_FALSE(files.AddVirtualFile("FIRST/injected.slang", "injected"));
    second              = Bundle("second");
    second.files        = {{"second/include/Value.slangh", "second"}};
    second.includeRoots = {"second/include"};
    second.dependencies = {"first"};
    EXPECT_FALSE(files.AddBundles(std::span{&second, 1}));
    EXPECT_EQ(files.BundleFingerprint(), before);
    std::string text;
    EXPECT_FALSE(files.ReadFile("First/include/Value.slangh", text));
    EXPECT_FALSE(files.ReadFile("second/include/Value.slangh", text));
    EXPECT_TRUE(files.ReadFile("first/include/Value.slangh", text));
    EXPECT_EQ(text, "first");
}
