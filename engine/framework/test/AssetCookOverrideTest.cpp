//
// Per-asset cook override -> build request plumbing (see per-asset-cook-settings).
//

#include <core/file/FileSystem.h>
#include <framework/asset/AssetBuilder.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetCommon.h>
#include <framework/asset/AssetDataBase.h>

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
    db->RebuildCacheFromScan();

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
