//
// Aurora mesh builder (see AuroraMeshBuilder.h).
//

#include <aurora/cook/mesh/AuroraMeshBuilder.h>

#include <aurora/adaptor/assets/MeshAsset.h>
#include <aurora/adaptor/assets/SkinAsset.h>
#include <aurora/cook/mesh/MeshAssetWriter.h>
#include <aurora/cook/mesh/MeshAssembler.h>
#include <aurora/cook/mesh/MeshOptimizer.h>
#include <aurora/cook/mesh/MeshletBuilder.h>
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

        auto archive = file->ReadAsArchive();
        JsonInputArchive json(*archive);
        presets.LoadJson(json);
        LOG_I(TAG, "loaded %u mesh build bundles (default '%s')", static_cast<uint32_t>(presets.bundles.size()),
              presets.defaultBundle.c_str());
    }

    void AuroraMeshBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        result.retCode = AssetBuildRetCode::FAILED;

        std::string                bundleKey;
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

        cook::CookedMesh cooked;
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

        if (config->meshlets) {
            cook::MeshletBuilder::Payload meshlets;
            meshlets.mesh = &cooked;
            cook::MeshletBuilder(meshlets).DoWork();
        }

        auto *manager = AssetManager::Get();

        // skin rides in its own asset; the mesh references it by uuid
        Uuid skinUuid;
        if (config->skinning && source.Skinned()) {
            skinUuid        = Uuid::Create();
            auto skinAsset  = manager->FindOrCreateAsset<Skin>(skinUuid);
            if (!skinAsset) {
                LOG_E(TAG, "failed to create skin asset %s", skinUuid.ToString().c_str());
                return;
            }
            cook::WriteSkinAsset(source.skin, skinAsset->Data());
            manager->SaveAsset(skinAsset, bundleKey);
        }

        auto asset = manager->FindOrCreateAsset<Mesh>(request.assetInfo->uuid);
        if (!asset) {
            LOG_E(TAG, "failed to create mesh asset %s", request.assetInfo->uuid.ToString().c_str());
            return;
        }
        if (static_cast<bool>(skinUuid)) {
            asset->AddDependencies(skinUuid);
        }

        cook::WriteMeshAsset(cooked, skinUuid, asset->Data());
        manager->SaveAsset(asset, bundleKey);
        result.retCode = AssetBuildRetCode::SUCCESS;
    }

} // namespace sky::aurora
