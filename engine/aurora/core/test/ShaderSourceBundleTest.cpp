#include "ShaderArtifactTest.h"

#include <algorithm>
#include <aurora/shader/ShaderCacheStore.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderResolver.h>
#include <aurora/shader/ShaderTechnique.h>
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
        PackageSources() = default;
        PackageSources(std::map<std::string, std::string> files, std::string prefix) : files(std::move(files)), prefix(std::move(prefix))
        {
        }
        bool FileExist(const sky::FilePath &path) const override
        {
            return files.contains(prefix + Logical(path));
        }
        sky::FilePtr OpenFile(const sky::FilePath &path) override
        {
            const auto found = files.find(prefix + Logical(path));
            return found == files.end() ? sky::FilePtr{} : sky::FilePtr(new PackageText(found->second));
        }
        sky::FilePtr CreateOrOpenFile(const sky::FilePath &) override
        {
            return {};
        }
        sky::FileSystemPtr CreateSubSystem(const std::string &path, bool) override
        {
            return new PackageSources(files, prefix + path + '/');
        }
        std::map<std::string, std::string> files;

    private:
        std::string        prefix;
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
        std::string            readContent;
        ShaderProducerIdentity GetProducerIdentity() const override
        {
            return test::TestProducer();
        }
        bool Compile(const ShaderCompileDesc &desc, ShaderCompileResult &out) override
        {
            std::string included;
            const bool  valid = desc.fileSystem && desc.fileSystem->ReadFile("plugins/a/Pass.h", included);
            readContent       = included;
            return valid && test::TestCompile(desc, GetProducerIdentity(), out);
        }
    };
    class NativeBindingCompiler final : public IShaderCompiler {
    public:
        bool                   conflict = false;
        ShaderProducerIdentity GetProducerIdentity() const override
        {
            return test::TestProducer();
        }
        bool Compile(const ShaderCompileDesc &desc, ShaderCompileResult &out) override
        {
            if (!test::TestCompile(desc, GetProducerIdentity(), out))
                return false;
            ShaderResource resource{"params", ShaderResourceType::UNIFORM_BUFFER, 1, 0, 1};
            resource.stages           = desc.stage;
            resource.declarationOwner = "custom/Main.slang";
            out.reflection.resources.push_back(resource);
            out.reflection.declarations.push_back(resource);
            ShaderBlockMember member{
                .name = "color", .size = 16, .scalarType = ShaderScalarType::FLOAT, .kind = ShaderTypeKind::VECTOR, .cols = 4, .alignment = 16};
            ShaderBlockLayout block{.name = "params", .structName = "Params", .set = 1, .size = 16, .members = {member}, .alignment = 16};
            out.reflection.blocks.push_back(block);
            out.reflection.declaredBlocks.push_back(block);
            out.reflection.nativeBindings.push_back(
                {1, 0, desc.stage, desc.stage == ShaderStageFlagBit::VS ? 2u : 7u, ShaderNativeCategory::METAL_BUFFER});
            if (conflict)
                out.reflection.nativeBindings.push_back({1, 0, desc.stage, 3, ShaderNativeCategory::METAL_BUFFER});
            return SealShaderArtifact(out, out.errorInfo);
        }
    };

} // namespace

TEST(ShaderTechniqueBindings, NativeMappingsMergePerStageAndConflictingDuplicatesFail)
{
    ShaderFileSystem files;
    ASSERT_TRUE(files.AddVirtualFile("custom/Main.slang", "source"));
    ASSERT_TRUE(files.AddVirtualFile("custom/Main.slang.json", R"({"entries":["Vertex","Fragment"],"sources":[],"variants":[]})"));
    NativeBindingCompiler compiler;
    CompilerRegistration  registration(compiler);
    const ShaderTechnique technique{{"custom/Main.slang", "Vertex"}, {"custom/Main.slang", "Fragment"}};
    auto                  compiled = CompileShaderTechnique(files, API::METAL, technique);
    ASSERT_TRUE(std::holds_alternative<RenderTechniqueData>(compiled));
    const std::vector<ShaderNativeBinding> expected{{1, 0, ShaderStageFlagBit::VS, 2, ShaderNativeCategory::METAL_BUFFER},
                                                    {1, 0, ShaderStageFlagBit::FS, 7, ShaderNativeCategory::METAL_BUFFER}};
    EXPECT_EQ(std::get<RenderTechniqueData>(compiled).reflection.nativeBindings, expected);
    compiler.conflict = true;
    compiled          = CompileShaderTechnique(files, API::METAL, technique);
    ASSERT_TRUE(std::holds_alternative<RendererError>(compiled));
    EXPECT_FALSE(std::get<RendererError>(compiled).message.empty());
}

TEST(ShaderResourceSelection, DevelopmentIgnoresUnrelatedProductsAndLoadsTheSourceInventory)
{
    PackageSources root;
    root.files["assets/shaders/shader-sources.json"] =
        R"({"version":1,"modules":[{"owner":"custom","dependencies":[],"includeRoots":[],"files":["custom/Main.slang"]}]})";
    root.files["assets/shaders/custom/Main.slang"]   = "float sourceValue;";
    root.files["products/shaders/distribution.json"] = R"({"targets":["metal"]})";
    auto selected                                    = LoadShaderResources(root, API::METAL, ShaderResourceMode::DEVELOPMENT);
    ASSERT_TRUE(std::holds_alternative<ShaderResources>(selected));
    const auto &resources = std::get<ShaderResources>(selected);
    EXPECT_FALSE(resources.products);
    std::string text;
    EXPECT_TRUE(resources.sources->ReadFile("custom/Main.slang", text));
    EXPECT_EQ(text, "float sourceValue;");
}

TEST(ShaderResourceSelection, ProductsRequireADeclaredBackendAndDecodableCurrentIndex)
{
    PackageSources root;
    EXPECT_TRUE(std::holds_alternative<std::string>(LoadShaderResources(root, API::METAL, ShaderResourceMode::PRODUCTS)));
    root.files["products/shaders/distribution.json"] = test::TestProductManifest(DefaultShaderEnvironment(ShaderTarget::MSL));
    root.files["products/shaders/metal/index.bin"]   = "invalid index";
    EXPECT_TRUE(std::holds_alternative<std::string>(LoadShaderResources(root, API::METAL, ShaderResourceMode::PRODUCTS)));
    ShaderCacheIndex index;
    const auto       key = test::TestCacheKey({}, "custom/Main.slang", ShaderTarget::MSL, CurrentShaderPlatform());
    ASSERT_TRUE(UpsertArtifact(index, key.sourcePath, {}, key.schemaFp, key, test::TestArtifact(key), {}));
    const auto bytes = EncodeIndex(index);
    root.files["products/shaders/metal/index.bin"].assign(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    EXPECT_TRUE(std::holds_alternative<std::string>(LoadShaderResources(root, API::VULKAN, ShaderResourceMode::PRODUCTS)));
    auto selected = LoadShaderResources(root, API::METAL, ShaderResourceMode::PRODUCTS);
    ASSERT_TRUE(std::holds_alternative<ShaderResources>(selected));
    EXPECT_TRUE(std::get<ShaderResources>(selected).products);
    std::string text;
    EXPECT_FALSE(std::get<ShaderResources>(selected).sources->ReadFile("custom/Main.slang", text));
    auto environment                                 = key.environment;
    environment.options                              = {"optimization=0"};
    root.files["products/shaders/distribution.json"] = test::TestProductManifest(environment);
    EXPECT_TRUE(std::holds_alternative<std::string>(LoadShaderResources(root, API::METAL, ShaderResourceMode::PRODUCTS)));
    environment.platform                             = CurrentShaderPlatform() == ShaderPlatform::IOS ? ShaderPlatform::MACOS : ShaderPlatform::IOS;
    root.files["products/shaders/distribution.json"] = test::TestProductManifest(environment);
    EXPECT_TRUE(std::holds_alternative<std::string>(LoadShaderResources(root, API::METAL, ShaderResourceMode::PRODUCTS)));
}

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
        ShaderResolver      resolver(nullptr, nullptr, files, 0, 0);
        ShaderVariantSchema schema;
        schema.sources.push_back({sky::Name("material"), 0, 8});
        schema.entries.push_back({sky::Name("FLAG"), sky::Name("material"), 0, 8});
        schema.totalBits = 8;
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
    EXPECT_EQ(compiler.readContent, "original");
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
