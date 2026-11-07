#include <array>
#include <filesystem>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetProduct.h>
#include <gtest/gtest.h>
#include <particle/assets/ParticleEffectAsset.h>
#include <particle/builder/ParticleEffectBuilder.h>

using namespace sky;
using namespace sky::particle;

TEST(ParticleEffectBuilder, PublishesProductAndRejectsInvalidSources)
{
    RegisterParticleBuilder();
    std::error_code error;
    const auto      root = std::filesystem::temp_directory_path(error) / ("sky-particle-builder-" + Uuid::Create().ToString());
    ASSERT_FALSE(error);
    {
        NativeFileSystemPtr sourceFs = new NativeFileSystem(FilePath(root / "assets"));
        NativeFileSystemPtr products = new NativeFileSystem(FilePath(root / "products"));
        auto               *catalog  = AssetDataBase::Get();
        catalog->SetWorkSpaceFs(sourceFs);
        auto *manager = AssetManager::Get();
        manager->SetSourceCatalog(catalog);
        manager->AddAssetProductBundle(new HashedAssetBundle(products, "particle-tests"));
        const auto file  = sourceFs->CreateOrOpenFile("spark.particle");
        const auto write = [&](std::string_view text) {
            const auto stream = file->WriteAsArchive();
            ASSERT_TRUE(stream->SaveRaw(text.data(), text.size()));
            stream->Flush();
        };
        write(
            R"({"version":1,"emitters":[{"name":"sparks","simTarget":"CPU","capacity":16,"spawn":{"rate":4},"modules":[{"type":"INITIALIZE","params":{"lifetime":1}}],"renderers":[{"type":"SPRITE","blend":"ALPHA"}]}]})");
        const auto info = catalog->RegisterAsset("spark.particle", false);
        ASSERT_NE(info, nullptr);
        AssetBuildRequest request;
        request.file      = file;
        request.assetInfo = info;
        request.bundle    = "particle-tests";
        request.target    = request.bundle;
        AssetBuildResult result;
        AssetBuilderManager::Get()->BuildRequestSync(info->uuid, "particle-tests");
        const auto productFile = products->OpenFile(ProductFilePath(info->uuid));
        ASSERT_NE(productFile, nullptr);
        std::vector<uint8_t> first;
        ASSERT_TRUE(productFile->ReadBin(first));
        EXPECT_FALSE(first.empty());

        write(
            R"({"version":1,"emitters":[{"name":"sparks","simTarget":"CPU","capacity":32,"spawn":{"rate":8},"modules":[{"type":"INITIALIZE","params":{"lifetime":2}}],"renderers":[{"type":"SPRITE","blend":"ALPHA"}]}]})");
        AssetBuilderManager::Get()->BuildRequestSync(info->uuid, "particle-tests");
        const auto changed = products->OpenFile(ProductFilePath(info->uuid));
        ASSERT_NE(changed, nullptr);
        std::vector<uint8_t> recooked;
        ASSERT_TRUE(changed->ReadBin(recooked));
        EXPECT_NE(recooked, first);

        write(R"({"version":1,"emitters":[]})");
        AssetBuilderManager::Get()->BuildRequestSync(info->uuid, "particle-tests");
        const auto failed = products->OpenFile(ProductFilePath(info->uuid));
        ASSERT_NE(failed, nullptr);
        std::vector<uint8_t> second;
        ASSERT_TRUE(failed->ReadBin(second));
        EXPECT_EQ(second, recooked);

        catalog->Reset();
        manager->SetSourceCatalog(nullptr);
        AssetExecutor::Get()->WaitForAll();
    }
    std::filesystem::remove_all(root, error);
}
