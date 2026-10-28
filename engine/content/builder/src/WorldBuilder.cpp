//
// Created on 2026/10/27.
//

#include <content/builder/WorldBuilder.h>

#include <framework/asset/AssetManager.h>
#include <framework/world/WorldAsset.h>
#include <unordered_set>
#include <utility>

namespace sky::builder {

    std::string_view WorldBuilder::QueryType(const std::string &ext) const
    {
        return ext == ".world" ? AssetTraits<World>::ASSET_TYPE : std::string_view{};
    }

    void WorldBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        result.retCode = AssetBuildRetCode::FAILED;
        result.error.clear();
        if (!request.assetInfo || !request.assetInfo->uuid || !request.file) {
            result.error = "World cook requires an identified source file";
            return;
        }
        const auto input = request.file->ReadAsArchive();
        if (!input || !input->IsOpen()) {
            result.error = "World source cannot be opened";
            return;
        }
        WorldAssetData cooked;
        if (!CookWorldJson(*input, cooked, result.error))
            return;

        auto      *manager = AssetManager::Get();
        const auto product = manager->CreateBuildAsset(request.assetInfo->uuid, Name(AssetTraits<World>::ASSET_TYPE.data()));
        if (!product || product->GetType() != Name(AssetTraits<World>::ASSET_TYPE.data())) {
            result.error = "World asset handler is unavailable";
            return;
        }
        auto asset    = std::static_pointer_cast<Asset<World>>(product);
        asset->Data() = std::move(cooked);
        std::unordered_set<Uuid> dependencies;
        for (const auto &root : asset->Data().roots) {
            if (root.uuid == request.assetInfo->uuid) {
                result.error = "World cannot depend on its own product";
                return;
            }
            if (dependencies.insert(root.uuid).second)
                AddAssetDependency(asset, request, root.uuid);
        }
        const auto saved = manager->SaveAsset(asset, request.bundle);
        if (!saved) {
            result.error = saved.message;
            return;
        }
        result.retCode = AssetBuildRetCode::SUCCESS;
    }

} // namespace sky::builder
