#include <animation/assets/GraphAsset.h>
#include <animation/builder/GraphBuilder.h>
#include <array>
#include <filesystem>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>

using namespace sky;
using namespace sky::animation;

namespace {
    class Products final : public NativeFileSystem {
    public:
        using NativeFileSystem::NativeFileSystem;
        bool                failCommit = false;
        FileOperationStatus ReplaceFileAtomic(const FilePath &from, const FilePath &to) const override
        {
            if (failCommit && to.FileName() == "current.bin") {
                return {FileOperationError::IOError, "injected graph commit failure"};
            }
            return NativeFileSystem::ReplaceFileAtomic(from, to);
        }
    };
} // namespace

TEST(GraphBuilder, GeneratedDependenciesAndFailedRecookPreservePublishedProduct)
{
    AssetBuilderManager::Get()->RegisterBuilder(new GraphBuilder());
    std::error_code error;
    const auto      root = std::filesystem::temp_directory_path(error) / ("sky-graph-builder-" + Uuid::Create().ToString());
    ASSERT_FALSE(error);
    {
        NativeFileSystemPtr  sourceFs = new NativeFileSystem(FilePath(root / "assets"));
        CounterPtr<Products> products = new Products(FilePath(root / "products"));
        auto                *catalog  = AssetDataBase::Get();
        catalog->SetWorkSpaceFs(sourceFs);
        auto *manager = AssetManager::Get();
        manager->SetSourceCatalog(catalog);
        manager->AddAssetProductBundle(new HashedAssetBundle(products, "graph-tests"));
        ASSERT_TRUE(sourceFs->WriteFileExclusive("character.model", std::array<uint8_t, 1>{1}));
        const auto model = catalog->RegisterAsset("character.model", false);
        ASSERT_NE(model, nullptr);
        GeneratedAssetManifest                        manifest;
        const std::array<GeneratedAssetDescriptor, 2> children{{{"rig/main", "AuroraSkeleton"}, {"clip/walk", "AnimationClip"}}};
        ASSERT_TRUE(catalog->UpdateGeneratedProducts(model->uuid, children, {}, manifest));
        GraphSource source;
        auto       &graph = source.definition;
        graph.document    = Uuid::Create();
        for (const auto &child : manifest.records) {
            if (child.type == "AuroraSkeleton") {
                graph.skeleton = child.uuid;
            }
        }
        GraphNode clip;
        clip.id = Uuid::Create();
        for (const auto &child : manifest.records) {
            if (child.type == "AnimationClip") {
                clip.clip = child.uuid;
            }
        }
        GraphNode output;
        output.id              = Uuid::Create();
        output.kind            = GraphNodeKind::Output;
        graph.output           = output.id;
        graph.nodes            = {clip, output};
        graph.links            = {{clip.id, GraphPin::Pose, output.id, GraphPin::InputA}};
        const auto file        = sourceFs->CreateOrOpenFile("walk.graph");
        const auto writeSource = [&] {
            std::string text;
            EXPECT_TRUE(WriteGraphSource(source, text));
            const auto stream = file->WriteAsArchive();
            EXPECT_TRUE(stream->SaveRaw(text.data(), text.size()));
            stream->Flush();
        };
        writeSource();
        const auto info = catalog->RegisterAsset("walk.graph", false);
        ASSERT_NE(info, nullptr);
        AssetBuildRequest request;
        request.file      = file;
        request.assetInfo = info;
        request.bundle    = "graph-tests";
        request.target    = request.bundle;
        GraphBuilder     builder;
        AssetBuildResult result;
        builder.Request(request, result);
        ASSERT_EQ(result.retCode, AssetBuildRetCode::SUCCESS) << result.error;
        AssetProductGroupStore  store(products);
        ProductGroupSnapshotPtr first;
        ASSERT_TRUE(store.Pin(info->uuid, first));
        EXPECT_EQ(info->dependencies.size(), 2u);
        graph.nodes[0].rate = 2.f;
        writeSource();
        builder.Request(request, result);
        ASSERT_EQ(result.retCode, AssetBuildRetCode::SUCCESS) << result.error;
        ProductGroupSnapshotPtr second;
        ASSERT_TRUE(store.Pin(info->uuid, second));
        EXPECT_NE(second->generation, first->generation);
        products->failCommit = true;
        graph.nodes[0].rate  = 3.f;
        writeSource();
        builder.Request(request, result);
        EXPECT_EQ(result.retCode, AssetBuildRetCode::FAILED);
        ProductGroupSnapshotPtr retained;
        ASSERT_TRUE(store.Pin(info->uuid, retained));
        EXPECT_EQ(retained->generation, second->generation);
        products->failCommit = false;
        graph.nodes[0].clip  = graph.skeleton;
        writeSource();
        builder.Request(request, result);
        EXPECT_EQ(result.retCode, AssetBuildRetCode::FAILED);
        EXPECT_NE(result.error.find("wrong type"), std::string::npos);
        graph.nodes[0].clip = Uuid::Create();
        writeSource();
        builder.Request(request, result);
        EXPECT_EQ(result.retCode, AssetBuildRetCode::FAILED);
        FilePtr oldProduct;
        EXPECT_TRUE(store.Open(*first, info->uuid, oldProduct));
        EXPECT_NE(oldProduct, nullptr);
        catalog->Reset();
        manager->SetSourceCatalog(nullptr);
        AssetExecutor::Get()->WaitForAll();
    }
    std::filesystem::remove_all(root, error);
    EXPECT_FALSE(error) << error.message();
}
