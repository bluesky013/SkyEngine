//
// Aurora mesh builder (see AuroraMeshBuilder.h).
//

#include <aurora/cook/mesh/AuroraMeshBuilder.h>

#include <aurora/adaptor/assets/MeshAsset.h>
#include <aurora/adaptor/assets/SkeletonAsset.h>
#include <aurora/cook/mesh/MeshAssembler.h>
#include <aurora/cook/mesh/MeshAssetWriter.h>
#include <aurora/cook/mesh/MeshOptimizer.h>
#include <aurora/cook/mesh/MeshSource.h>

#include <framework/asset/AssetManager.h>
#include <framework/serialization/JsonArchive.h>

#include <core/logger/Logger.h>

static const char *TAG = "AuroraMeshBuilder";

namespace sky::aurora {

    std::string_view AuroraMeshBuilder::QueryType(const std::string &) const
    {
        return AssetTraits<Mesh>::ASSET_TYPE;
    }

    namespace {

        // Move the build-scratch vertex/index/skin streams into the product bulk
        // (the header keeps only the section table). Order must match the offsets
        // written by WriteMeshAsset.
        void CommitMeshBulk(Asset<Mesh> &asset)
        {
            auto &data = asset.Data();
            if (!data.vertexData.empty()) {
                asset.AddBulk(data.vertexData.data(), data.vertexData.size());
            }
            if (!data.indexData.empty()) {
                asset.AddBulk(data.indexData.data(), data.indexData.size());
            }
            if (!data.skinData.empty()) {
                asset.AddBulk(data.skinData.data(), data.skinData.size());
            }
            data.vertexData.clear();
            data.indexData.clear();
            data.skinData.clear();
        }

    } // namespace

    void AuroraMeshBuilder::LoadConfig(const FileSystemPtr &cfg)
    {
        if (cfg == nullptr) {
            return;
        }
        auto file = cfg->OpenFile(FilePath("mesh_build_presets.json"));
        if (!file) {
            LOG_W(TAG, "mesh_build_presets.json not found; using fallback config");
            return;
        }

        auto             archive = file->ReadAsArchive();
        JsonInputArchive json(*archive);
        presets.LoadJson(json);
        LOG_I(TAG, "loaded %u mesh build bundles (default '%s')", static_cast<uint32_t>(presets.bundles.size()), presets.defaultBundle.c_str());
    }

    void AuroraMeshBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        result.retCode = AssetBuildRetCode::FAILED;

        std::string                  bundleKey;
        const cook::MeshBuildConfig *config = presets.Resolve(request.target, bundleKey);
        if (config == nullptr) {
            LOG_E(TAG, "no mesh build config for target '%s'", request.target.c_str());
            return;
        }

        std::vector<uint8_t> bytes;
        if (!request.file->ReadBin(bytes) || bytes.empty()) {
            LOG_E(TAG, "failed to read source %s", request.assetInfo->path.GetStr().c_str());
            return;
        }

        cook::CookMeshSource source;
        if (!cook::LoadMeshSource(bytes, request.assetInfo->ext, source)) {
            LOG_E(TAG, "mesh import failed: %s", request.assetInfo->path.GetStr().c_str());
            return;
        }

        cook::CookedMesh             cooked;
        cook::MeshAssembler::Payload assemble;
        assemble.source = &source;
        assemble.config = config;
        assemble.out    = &cooked;
        cook::MeshAssembler(assemble).DoWork();

        if (config->optimize) {
            cook::MeshOptimizer::Payload optimize;
            optimize.mesh = &cooked;
            cook::MeshOptimizer(optimize).DoWork();
        }

        auto *manager = AssetManager::Get();

        // The skeleton rides in its own asset; skin data is absorbed into the mesh.
        Uuid skeletonUuid;
        if (config->skinning && source.Skinned()) {
            skeletonUuid       = Uuid::Create();
            auto skeletonAsset = manager->FindOrCreateAsset<sky::Skeleton>(skeletonUuid);
            if (!skeletonAsset) {
                LOG_E(TAG, "failed to create skeleton asset %s", skeletonUuid.ToString().c_str());
                return;
            }
            cook::WriteSkeletonAsset(source, skeletonAsset->Data());
            manager->SaveAsset(skeletonAsset, bundleKey);
        }

        auto asset = manager->FindOrCreateAsset<Mesh>(request.assetInfo->uuid);
        if (!asset) {
            LOG_E(TAG, "failed to create mesh asset %s", request.assetInfo->uuid.ToString().c_str());
            return;
        }
        asset->ResetDependencies();
        if (static_cast<bool>(skeletonUuid)) {
            asset->AddDependencies(skeletonUuid);
        }

        cook::WriteMeshAsset(cooked, source.skin, skeletonUuid, asset->Data());
        CommitMeshBulk(*asset);
        manager->SaveAsset(asset, bundleKey);
        result.retCode = AssetBuildRetCode::SUCCESS;
    }

} // namespace sky::aurora
