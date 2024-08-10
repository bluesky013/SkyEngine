//
// Created by blues on 2024/7/14.
//

#include <builder/render/MeshBuilder.h>
#include <core/archive/MemoryArchive.h>
#include <framework/asset/AssetBuilderManager.h>

namespace sky::builder {

    void MeshBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        auto asset = AssetManager::Get()->FindOrCreateAsset<Mesh>(request.assetInfo->uuid);

        auto archive = request.file->ReadAsArchive();
        BinaryInputArchive bin(*archive);

        auto &data = asset->Data();
        data.Load(bin);

        asset->ResetDependencies();
        for (auto &sub : asset->Data().subMeshes) {
            AssetBuilderManager::Get()->BuildRequest(sub.material, request.target);
            asset->AddDependencies(sub.material);
        }

        if (data.skeleton) {
            asset->AddDependencies(data.skeleton);
        }

        data.rawData.storage.resize(data.dataSize);
        bin.LoadValue(reinterpret_cast<char*>(data.rawData.storage.data()), data.dataSize);

        AssetManager::Get()->SaveAsset(asset, request.target);
        result.retCode = AssetBuildRetCode::SUCCESS;
    }

} // namespace sky::builder