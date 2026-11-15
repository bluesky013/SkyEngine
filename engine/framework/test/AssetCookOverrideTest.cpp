//
// Per-asset cook override -> build request plumbing (see per-asset-cook-settings).
//

#include <core/file/FileSystem.h>
#include <framework/asset/AssetBuilder.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetCommon.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/world/WorldAsset.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace sky;

namespace {

    class CapturingBuilder : public AssetBuilder {
    public:
        const std::vector<std::string> &GetExtensions() const override
        {
            static const std::vector<std::string> exts{".txt"};
            return exts;
        }
        std::string_view QueryType(const std::string &ext) const override
        {
            return ext == ".txt" ? std::string_view("Text") : std::string_view{};
        }
        void Request(const AssetBuildRequest &request, AssetBuildResult &result) override
        {
            lastSettings   = request.settings;
            lastTarget     = request.target;
            result.retCode = AssetBuildRetCode::SUCCESS;
        }

        BuildSettingsOverride lastSettings;
        std::string           lastTarget;
    };

} // namespace

TEST(AssetCookOverrideTest, BuildRequestCarriesManifestOverride)
{
    namespace fs = std::filesystem;

    const fs::path root = fs::path("asset_cook_override_tmp");
    fs::remove_all(root);
    fs::create_directories(root);
    {
        std::ofstream out(root / "a.txt");
        out << "x";
    }

    auto *manager = AssetBuilderManager::Get();
    auto *builder = new CapturingBuilder();
    manager->RegisterBuilder(builder);

    FileSystemPtr workSpace = new NativeFileSystem(FilePath(root.string()));
    auto         *db        = AssetDataBase::Get();
    db->Reset();
    db->SetWorkSpaceFs(workSpace);
    EXPECT_FALSE(db->RebuildCacheFromScan().has_value());

    auto src = db->FindAsset(FilePath("a.txt"));
    ASSERT_TRUE(src != nullptr);

    ASSERT_TRUE(db->SetCookJson(src->uuid, R"({"settings":{"common":{"maxSize":"321","generateMip":"false"}}})"));

    manager->BuildRequestSync(src->uuid, "common");
    EXPECT_EQ(builder->lastTarget, "common");
    ASSERT_EQ(builder->lastSettings.count("maxSize"), 1u);
    EXPECT_EQ(builder->lastSettings.at("maxSize"), "321");
    EXPECT_EQ(builder->lastSettings.at("generateMip"), "false");

    manager->UnRegisterBuilder(builder);
    db->Reset();
    fs::remove_all(root);
}

TEST(AssetCookOverrideTest, EnginePresetsFillMissingProjectConfigsAndProjectFilesTakePriority)
{
    class ConfigProbe final : public AssetBuilder {
    public:
        const std::vector<std::string> &GetExtensions() const override
        {
            return extensions;
        }
        std::string_view QueryType(const std::string &) const override
        {
            return {};
        }
        void Request(const AssetBuildRequest &, AssetBuildResult &) override
        {
        }
        void LoadConfig(const FileSystemPtr &fs) override
        {
            content.clear();
            if (const auto file = fs->OpenFile("mesh_build_presets.json"))
                EXPECT_TRUE(file->ReadString(content));
        }
        std::string              content;
        std::vector<std::string> extensions;
    };
    struct Cleanup {
        std::filesystem::path root;
        ~Cleanup()
        {
            EXPECT_TRUE(AssetManager::Get()->ResetRuntimeState());
            std::error_code error;
            std::filesystem::remove_all(root, error);
            EXPECT_FALSE(error);
        }
    } cleanup{std::filesystem::temp_directory_path() / ("sky-engine-cook-defaults-" + Uuid::Create().ToString())};
    const auto engine  = cleanup.root / "engine";
    const auto project = cleanup.root / "project";
    std::filesystem::create_directories(engine / "configs");
    std::filesystem::create_directories(project);
    std::ofstream(engine / "configs/mesh_build_presets.json") << "engine mesh presets";
    std::ofstream(engine / "configs/asset_cook.jsonc") << R"({"bundles":["engine-common"]})";
    AssetBuilderManager manager;
    auto               *probe = new ConfigProbe();
    manager.RegisterBuilder(probe);
    manager.SetEngineFs(new NativeFileSystem(FilePath(engine)));
    ASSERT_TRUE(manager.SetWorkSpaceFs(new NativeFileSystem(FilePath(project))));
    EXPECT_EQ(probe->content, "engine mesh presets");
    EXPECT_EQ(manager.GetCookConfig().GetBundles(), std::vector<std::string>{"engine-common"});
    const auto world = AssetManager::Get()->CreateBuildAsset(Uuid::Create(), Name("World"));
    ASSERT_TRUE(world);
    EXPECT_TRUE(AssetManager::Get()->SaveAsset(world, "engine-common"));
    std::filesystem::create_directories(project / "configs");
    std::ofstream(project / "configs/mesh_build_presets.json") << "project mesh presets";
    std::ofstream(project / "configs/asset_build_presets.json") << R"({"bundles":["project-common"]})";
    ASSERT_TRUE(manager.SetWorkSpaceFs(new NativeFileSystem(FilePath(project))));
    EXPECT_EQ(probe->content, "project mesh presets");
    EXPECT_EQ(manager.GetCookConfig().GetBundles(), std::vector<std::string>{"project-common"});
    EXPECT_TRUE(AssetManager::Get()->SaveAsset(world, "project-common"));
    std::ofstream(project / "configs/asset_build_presets.json") << "invalid json";
    EXPECT_FALSE(manager.SetWorkSpaceFs(new NativeFileSystem(FilePath(project))));
    EXPECT_EQ(manager.GetCookConfig().GetBundles(), std::vector<std::string>{"project-common"});
    manager.UnRegisterBuilder(probe);
}
