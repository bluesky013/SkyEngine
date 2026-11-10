#include <curve/assets/CurveSource.h>
#include <curve/builder/CurveBuilder.h>
#include <filesystem>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetProduct.h>
#include <gtest/gtest.h>

using namespace sky;
using namespace sky::curve;

TEST(CurveBuilder, CooksValidSourcesAndKeepsPreviousProductOnFailure)
{
    RegisterCurveBuilder();
    std::error_code error;
    const auto      root = std::filesystem::temp_directory_path(error) / ("sky-curve-builder-" + Uuid::Create().ToString());
    ASSERT_FALSE(error);
    {
        NativeFileSystemPtr sourceFs = new NativeFileSystem(FilePath(root / "assets"));
        NativeFileSystemPtr products = new NativeFileSystem(FilePath(root / "products"));
        auto               *catalog  = AssetDataBase::Get();
        catalog->SetWorkSpaceFs(sourceFs);
        auto *manager = AssetManager::Get();
        manager->SetSourceCatalog(catalog);
        manager->AddAssetProductBundle(new HashedAssetBundle(products, "curve-tests"));

        const auto file  = sourceFs->CreateOrOpenFile("fade.curve");
        const auto write = [&](std::string_view text) {
            const auto stream = file->WriteAsArchive();
            ASSERT_TRUE(stream->SaveRaw(text.data(), text.size()));
            stream->Flush();
        };
        write(
            R"({"version":1,"channels":1,"interpolation":"linear","preWrap":"clamp","postWrap":"clamp","keys":[{"time":0,"value":[0]},{"time":1,"value":[1]}]})");
        const auto info = catalog->RegisterAsset("fade.curve", false);
        ASSERT_NE(info, nullptr);
        AssetBuilderManager::Get()->BuildRequestSync(info->uuid, "curve-tests");
        const auto productFile = products->OpenFile(ProductFilePath(info->uuid));
        ASSERT_NE(productFile, nullptr);
        std::vector<uint8_t> first;
        ASSERT_TRUE(productFile->ReadBin(first));
        EXPECT_FALSE(first.empty());

        write(
            R"({"version":1,"channels":1,"interpolation":"step","preWrap":"mirror","postWrap":"mirror","keys":[{"time":0,"value":[0]},{"time":1,"value":[2]}]})");
        AssetBuilderManager::Get()->BuildRequestSync(info->uuid, "curve-tests");
        const auto changed = products->OpenFile(ProductFilePath(info->uuid));
        ASSERT_NE(changed, nullptr);
        std::vector<uint8_t> recooked;
        ASSERT_TRUE(changed->ReadBin(recooked));
        EXPECT_NE(recooked, first);

        write(R"({"version":2,"channels":1,"interpolation":"linear","preWrap":"clamp","postWrap":"clamp","keys":[{"time":0,"value":[0]}]})");
        AssetBuilderManager::Get()->BuildRequestSync(info->uuid, "curve-tests");
        const auto failed = products->OpenFile(ProductFilePath(info->uuid));
        ASSERT_NE(failed, nullptr);
        std::vector<uint8_t> second;
        ASSERT_TRUE(failed->ReadBin(second));
        EXPECT_EQ(second, recooked);

        std::string oversized =
            R"({"version":1,"channels":1,"interpolation":"linear","preWrap":"clamp","postWrap":"clamp","keys":[{"time":0,"value":[9]}]})";
        oversized.resize(MAX_CURVE_SOURCE_BYTES + 1, ' ');
        write(oversized);
        AssetBuildRequest request;
        request.file      = file;
        request.assetInfo = info;
        request.target = request.bundle = "curve-tests";
        AssetBuildResult result{};
        const auto      *builderManager = AssetBuilderManager::Get();
        auto            *builder        = builderManager->QueryBuilder(".curve");
        ASSERT_NE(builder, nullptr);
        builder->Request(request, result);
        EXPECT_EQ(result.retCode, AssetBuildRetCode::FAILED);
        EXPECT_EQ(result.error, "Curve source size is invalid");
        std::vector<uint8_t> afterOversized;
        ASSERT_TRUE(failed->ReadBin(afterOversized));
        EXPECT_EQ(afterOversized, recooked);

        catalog->Reset();
        manager->SetSourceCatalog(nullptr);
        AssetExecutor::Get()->WaitForAll();
    }
    std::filesystem::remove_all(root, error);
}
