//
// Created on 2026/10/08.
//

#include <editor/core/asset/AssetMutationService.h>
#include <editor/core/asset/AssetPreviewProvider.h>
#include <editor/core/asset/AssetThumbnailProvider.h>
#include <editor/core/asset/EditorAssetCatalog.h>
#include <editor/core/asset/EditorAssetEditor.h>

#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetCommon.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/CookConfig.h>
#include <framework/serialization/SerializationContext.h>

#include <core/file/FileSystem.h>
#include <core/type/TypeInfoObj.h>

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

    // Reflected test settings (a single int member) mirroring a builder's cook settings.
    struct TestCookSettings {
        uint32_t maxSize = 0;
    };

    class TextBuilder : public sky::AssetBuilder {
    public:
        TextBuilder()
        {
            static bool registered = false;
            if (!registered) {
                registered = true;
                sky::SerializationContext::Get()->Register<TestCookSettings>("EditorTestCookSettings").Member<&TestCookSettings::maxSize>("maxSize");
            }
        }

        const std::vector<std::string> &GetExtensions() const override
        {
            static const std::vector<std::string> exts{".txt"};
            return exts;
        }

        std::string_view QueryType(const std::string &ext) const override
        {
            return ext == ".txt" ? std::string_view("Text") : std::string_view{};
        }

        std::vector<std::pair<std::string, std::string>> DescribeSettings(const sky::ProductBundleKey &,
                                                                          const sky::BuildSettingsOverride &override) const override
        {
            const auto it = override.find("maxSize");
            return {{"maxSize", it != override.end() ? it->second : std::string("0")}};
        }

        const sky::TypeInfoRT *GetSettingsType() const override
        {
            return sky::TypeInfoObj<TestCookSettings>::Get()->RtInfo();
        }

        sky::Any MakeSettings(const sky::ProductBundleKey &, const sky::BuildSettingsOverride &override) const override
        {
            TestCookSettings settings;
            const auto       it = override.find("maxSize");
            if (it != override.end()) {
                settings.maxSize = static_cast<uint32_t>(std::strtoul(it->second.c_str(), nullptr, 10));
            }
            return sky::Any(settings);
        }

        sky::BuildSettingsOverride DiffSettings(const sky::ProductBundleKey &, const sky::Any &edited) const override
        {
            sky::BuildSettingsOverride out;
            const auto                *values = edited.GetAsConst<TestCookSettings>();
            if (values != nullptr && values->maxSize != 0) {
                out["maxSize"] = std::to_string(values->maxSize);
            }
            return out;
        }
    };

} // namespace

using namespace sky::editor;

TEST(EditorAssetCatalog, VpathTreeFromRegisteredSources)
{
    namespace fs = std::filesystem;

    const fs::path root = fs::path("catalog_test_tmp");
    fs::remove_all(root);
    fs::create_directories(root / "textures");
    fs::create_directories(root / "empty");
    {
        std::ofstream out(root / "a.txt");
        out << "hello";
    }
    {
        std::ofstream out(root / "textures" / "b.txt");
        out << "world";
    }
    {
        std::ofstream out(root / "note.unknown"); // unknown extension must not appear
        out << "x";
    }

    auto *manager = sky::AssetBuilderManager::Get();
    auto *builder = new TextBuilder();
    manager->RegisterBuilder(builder);

    sky::FileSystemPtr workSpace = new sky::NativeFileSystem(sky::FilePath(root.string()));
    auto              *db        = sky::AssetDataBase::Get();
    db->Reset();
    db->SetWorkSpaceFs(workSpace);
    db->RebuildCacheFromScan();

    auto *catalog = EditorAssetCatalog::Get();
    auto  roots   = catalog->GetRoots();
    ASSERT_EQ(roots.size(), 1u);
    EXPECT_EQ(roots[0].path, "Project");
    EXPECT_TRUE(roots[0].writable);

    auto folder = catalog->ListFolder("Project");
    ASSERT_EQ(folder.folders.size(), 1u); // only "textures" (empty dirs not represented)
    EXPECT_EQ(folder.folders[0].name, "textures");
    ASSERT_EQ(folder.items.size(), 1u); // a.txt (assets.jsonl excluded)
    EXPECT_EQ(folder.items[0].name, "a.txt");
    EXPECT_EQ(folder.items[0].type, "Text");
    EXPECT_TRUE(folder.items[0].writable);

    auto sub = catalog->ListFolder("Project/textures");
    ASSERT_EQ(sub.items.size(), 1u);
    EXPECT_EQ(sub.items[0].name, "b.txt");

    // Lookup by UUID and type filter.
    const sky::Uuid id = folder.items[0].uuid;
    EditorAssetItem byId;
    EXPECT_TRUE(catalog->Find(id, byId));
    EXPECT_EQ(byId.path, "Project/a.txt");
    EXPECT_EQ(catalog->Gather("Text").size(), 2u);

    manager->UnRegisterBuilder(builder);
    db->Reset();
    fs::remove_all(root);
}

namespace {

    class FakeAssetEditor : public IEditorAssetEditor {
    public:
        std::string GetAssetType() const override
        {
            return "Text";
        }
        void Open(const sky::Uuid &uuid) override
        {
            lastOpened = uuid;
            opened     = true;
        }

        bool      opened = false;
        sky::Uuid lastOpened;
    };

    class FakeOverride : public IEditorAssetCatalog {
    public:
        std::vector<EditorAssetItem> Gather(const std::string &) const override
        {
            return {};
        }
        bool GetType(const sky::Uuid &, std::string &) const override
        {
            return false;
        }
        bool GetName(const sky::Uuid &, std::string &) const override
        {
            return false;
        }
    };

    class FakeThumbnailProvider : public IAssetThumbnailProvider {
    public:
        bool GetThumbnail(const sky::Uuid &, const std::string &type, std::string &outKey) override
        {
            if (type == "Text") {
                outKey = "thumb:Text";
                return true;
            }
            return false;
        }
    };

    class FakePreviewProvider : public IAssetPreviewProvider {
    public:
        bool GetPreview(const sky::Uuid &uuid, const std::string &type) override
        {
            lastType = type;
            lastUuid = uuid;
            return type == "Text";
        }
        std::string lastType;
        sky::Uuid   lastUuid;
    };

} // namespace

TEST(EditorAssetCatalog, CookStateTransitions)
{
    auto           *catalog = EditorAssetCatalog::Get();
    const sky::Uuid id      = sky::Uuid::Create();
    EXPECT_EQ(catalog->GetCookState(id), AssetCookState::NotCooked);

    catalog->OnBuildFinished(id, "common", true, "");
    EXPECT_EQ(catalog->GetCookState(id), AssetCookState::Ready);

    catalog->OnBuildFinished(id, "common", false, "boom");
    EXPECT_EQ(catalog->GetCookState(id), AssetCookState::Failed);
    std::string error;
    EXPECT_TRUE(catalog->GetCookError(id, error));
    EXPECT_EQ(error, "boom");
}

TEST(EditorAssetCatalog, MutationPreservesIdentity)
{
    namespace fs = std::filesystem;

    const fs::path root = fs::path("catalog_mutation_tmp");
    fs::remove_all(root);
    fs::create_directories(root);
    {
        std::ofstream out(root / "a.txt");
        out << "hello";
    }

    auto *manager = sky::AssetBuilderManager::Get();
    auto *builder = new TextBuilder();
    manager->RegisterBuilder(builder);

    sky::FileSystemPtr workSpace = new sky::NativeFileSystem(sky::FilePath(root.string()));
    auto              *db        = sky::AssetDataBase::Get();
    db->Reset();
    db->SetWorkSpaceFs(workSpace);
    db->RebuildCacheFromScan();

    auto           *catalog = EditorAssetCatalog::Get();
    EditorAssetItem item;
    ASSERT_TRUE(catalog->FindByPath("Project/a.txt", item));
    const sky::Uuid original = item.uuid;

    // Move keeps the UUID.
    ASSERT_TRUE(AssetMutationService::Get()->Move("Project/a.txt", "Project/b.txt"));
    EditorAssetItem moved;
    ASSERT_TRUE(catalog->FindByPath("Project/b.txt", moved));
    EXPECT_EQ(moved.uuid, original);
    EXPECT_FALSE(catalog->FindByPath("Project/a.txt", item));

    // Duplicate assigns a new UUID.
    const sky::Uuid copy = AssetMutationService::Get()->Duplicate("Project/b.txt", "Project/c.txt");
    EXPECT_TRUE(static_cast<bool>(copy));
    EXPECT_NE(copy, original);
    EditorAssetItem duplicated;
    ASSERT_TRUE(catalog->FindByPath("Project/c.txt", duplicated));
    EXPECT_EQ(duplicated.uuid, copy);

    // Delete removes identity.
    EXPECT_TRUE(AssetMutationService::Get()->Delete(original));
    EXPECT_FALSE(catalog->FindByPath("Project/b.txt", moved));

    manager->UnRegisterBuilder(builder);
    db->Reset();
    fs::remove_all(root);
}

TEST(EditorAssetCatalog, CookTargetsAndPerTargetState)
{
    namespace fs = std::filesystem;

    const fs::path root = fs::path("catalog_cook_tmp");
    fs::remove_all(root);
    fs::create_directories(root);
    {
        std::ofstream out(root / "a.txt");
        out << "hello";
    }

    auto *manager = sky::AssetBuilderManager::Get();
    auto *builder = new TextBuilder();
    manager->RegisterBuilder(builder);

    sky::CookConfig cookConfig;
    cookConfig.Parse(R"({"bundles":["common","tex_pc"],"presets":{"windows":["common","tex_pc"]}})");
    cookConfig.SetActivePlatform("windows");
    manager->SetCookConfig(std::move(cookConfig));

    sky::FileSystemPtr workSpace = new sky::NativeFileSystem(sky::FilePath(root.string()));
    auto              *db        = sky::AssetDataBase::Get();
    db->Reset();
    db->SetWorkSpaceFs(workSpace);
    db->RebuildCacheFromScan();

    auto *catalog = EditorAssetCatalog::Get();
    catalog->Refresh();
    EditorAssetItem item;
    ASSERT_TRUE(catalog->FindByPath("Project/a.txt", item));

    // Targets fall back to the active platform's preset bundles.
    const auto config = catalog->GetCookConfig(item.uuid);
    EXPECT_EQ(config.activePlatform, "windows");
    ASSERT_EQ(config.targets.size(), 2u);
    EXPECT_EQ(config.targets[0], "common");
    EXPECT_EQ(config.targets[1], "tex_pc");

    // Per-(uuid, bundle) state: common ready, tex_pc failed -> aggregate failed.
    catalog->OnBuildFinished(item.uuid, "common", true, "");
    catalog->OnBuildFinished(item.uuid, "tex_pc", false, "boom");

    EXPECT_EQ(catalog->GetCookState(item.uuid, "common"), AssetCookState::Ready);
    EXPECT_EQ(catalog->GetCookState(item.uuid, "tex_pc"), AssetCookState::Failed);
    EXPECT_EQ(catalog->GetCookState(item.uuid), AssetCookState::Failed);
    std::string error;
    EXPECT_TRUE(catalog->GetCookError(item.uuid, error));
    EXPECT_EQ(error, "boom");

    const auto infos = catalog->GetTargetInfos(item.uuid);
    ASSERT_EQ(infos.size(), 2u);
    EXPECT_EQ(infos[0].target, "common");
    EXPECT_EQ(infos[0].state, AssetCookState::Ready);
    EXPECT_EQ(infos[1].target, "tex_pc");
    EXPECT_EQ(infos[1].state, AssetCookState::Failed);

    manager->SetCookConfig(sky::CookConfig{});
    manager->UnRegisterBuilder(builder);
    db->Reset();
    fs::remove_all(root);
}

TEST(EditorAssetCatalog, CrossDllSingletonIdentity)
{
    // GetEditorAssetCatalog() resolves to the environment-held singleton by default.
    EXPECT_EQ(GetEditorAssetCatalog(), static_cast<IEditorAssetCatalog *>(EditorAssetCatalog::Get()));
    EXPECT_EQ(EditorAssetCatalog::Get(), EditorAssetCatalog::Get());

    // The injectable override is honored and restored.
    FakeOverride overrideCatalog;
    SetEditorAssetCatalog(&overrideCatalog);
    EXPECT_EQ(GetEditorAssetCatalog(), static_cast<IEditorAssetCatalog *>(&overrideCatalog));
    SetEditorAssetCatalog(nullptr);
    EXPECT_EQ(GetEditorAssetCatalog(), static_cast<IEditorAssetCatalog *>(EditorAssetCatalog::Get()));
}

TEST(EditorAssetEditorRegistry, RegisterFindUnregister)
{
    FakeAssetEditor editor;
    auto           *registry = EditorAssetEditorRegistry::Get();
    EXPECT_EQ(registry->Find("Text"), nullptr);

    registry->Register(&editor);
    EXPECT_EQ(registry->Find("Text"), &editor);

    editor.Open(sky::Uuid{});
    EXPECT_TRUE(editor.opened);

    registry->Unregister(&editor);
    EXPECT_EQ(registry->Find("Text"), nullptr);
}

TEST(AssetThumbnailProviderRegistry, ProviderSeam)
{
    FakeThumbnailProvider provider;
    auto                 *registry = AssetThumbnailProviderRegistry::Get();
    EXPECT_EQ(registry->GetProvider(), nullptr);

    registry->SetProvider(&provider);
    std::string key;
    ASSERT_NE(registry->GetProvider(), nullptr);
    EXPECT_TRUE(registry->GetProvider()->GetThumbnail(sky::Uuid{}, "Text", key));
    EXPECT_EQ(key, "thumb:Text");
    EXPECT_FALSE(registry->GetProvider()->GetThumbnail(sky::Uuid{}, "Mesh", key));

    registry->SetProvider(nullptr);
}

TEST(AssetPreviewProviderRegistry, ProviderSeam)
{
    FakePreviewProvider provider;
    auto               *registry = AssetPreviewProviderRegistry::Get();
    EXPECT_EQ(registry->GetProvider(), nullptr);

    registry->SetProvider(&provider);
    ASSERT_NE(registry->GetProvider(), nullptr);
    EXPECT_TRUE(registry->GetProvider()->GetPreview(sky::Uuid{}, "Text"));
    EXPECT_EQ(provider.lastType, "Text");
    EXPECT_FALSE(registry->GetProvider()->GetPreview(sky::Uuid{}, "Mesh"));

    registry->SetProvider(nullptr);
}

TEST(EditorAssetCatalog, CookOverrideReadWrite)
{
    namespace fs = std::filesystem;

    const fs::path root = fs::path("catalog_override_tmp");
    fs::remove_all(root);
    fs::create_directories(root);
    {
        std::ofstream out(root / "a.txt");
        out << "hello";
    }

    auto *manager = sky::AssetBuilderManager::Get();
    auto *builder = new TextBuilder();
    manager->RegisterBuilder(builder);

    sky::CookConfig cookConfig;
    cookConfig.Parse(R"({"bundles":["common","tex_pc"],"presets":{"windows":["common","tex_pc"]}})");
    cookConfig.SetActivePlatform("windows");
    manager->SetCookConfig(std::move(cookConfig));

    sky::FileSystemPtr workSpace = new sky::NativeFileSystem(sky::FilePath(root.string()));
    auto              *db        = sky::AssetDataBase::Get();
    db->Reset();
    db->SetWorkSpaceFs(workSpace);
    db->RebuildCacheFromScan();

    auto *catalog = EditorAssetCatalog::Get();
    catalog->Refresh();
    EditorAssetItem item;
    ASSERT_TRUE(catalog->FindByPath("Project/a.txt", item));

    // Reflected settings round-trip: effective object + preset baseline.
    auto settings = catalog->GetCookSettings(item.uuid, "tex_pc");
    ASSERT_TRUE(settings.IsValid());
    const auto *values = settings.object.GetAsConst<TestCookSettings>();
    ASSERT_NE(values, nullptr);
    EXPECT_EQ(values->maxSize, 0u); // no override yet -> default

    // Edit the reflected object and persist -> sparse override written.
    settings.object.GetAs<TestCookSettings>()->maxSize = 512;
    ASSERT_TRUE(catalog->ApplyCookSettings(item.uuid, "tex_pc", settings.object));
    EXPECT_NE(db->GetCookJson(item.uuid).find("maxSize"), std::string::npos);

    // Re-read reflects the override.
    settings = catalog->GetCookSettings(item.uuid, "tex_pc");
    ASSERT_TRUE(settings.IsValid());
    ASSERT_NE(settings.object.GetAsConst<TestCookSettings>(), nullptr);
    EXPECT_EQ(settings.object.GetAsConst<TestCookSettings>()->maxSize, 512u);

    // Effective display via GetTargetInfos reflects the override.
    bool foundOverride = false;
    for (const auto &info : catalog->GetTargetInfos(item.uuid)) {
        if (info.target != "tex_pc") {
            continue;
        }
        for (const auto &s : info.settings) {
            if (s.key == "maxSize") {
                foundOverride = (s.value == "512");
            }
        }
    }
    EXPECT_TRUE(foundOverride);

    // Persisted to the on-disk manifest.
    {
        std::ifstream     in(root / "assets.jsonl");
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        EXPECT_NE(text.find("maxSize"), std::string::npos);
    }

    // Reset (edit back to the preset default) removes the override key.
    settings                                           = catalog->GetCookSettings(item.uuid, "tex_pc");
    settings.object.GetAs<TestCookSettings>()->maxSize = 0;
    ASSERT_TRUE(catalog->ApplyCookSettings(item.uuid, "tex_pc", settings.object));
    EXPECT_EQ(db->GetCookJson(item.uuid).find("maxSize"), std::string::npos);

    manager->SetCookConfig(sky::CookConfig{});
    manager->UnRegisterBuilder(builder);
    db->Reset();
    fs::remove_all(root);
}
