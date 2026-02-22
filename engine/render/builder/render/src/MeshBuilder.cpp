//
// Created by blues on 2024/7/14.
//

#include <builder/render/MeshBuilder.h>
#include <builder/render/mesh/MeshletBuilder.h>
#include <framework/asset/AssetBuilderManager.h>

namespace sky::builder {

    void ClusterMeshDeleter::operator()(MeshletBuilder * _Ptr) const noexcept
    {
        delete _Ptr;
    }

    MeshBuilder::MeshBuilder()
    {
        clusterBuilder.reset(new MeshletBuilder());
    }

    void MeshBuilder::Request(const AssetBuildRequest &request, AssetBuildResult &result)
    {
        auto asset = AssetManager::Get()->FindOrCreateAsset<Mesh>(request.assetInfo->uuid);

        auto archive = request.file->ReadAsArchive();
        BinaryInputArchive bin(*archive);

        auto &data = asset->Data();
        data.Load(bin);

        asset->ResetDependencies();

        const auto& matData = asset->Data();
        for (auto& mat : matData.materials) {
            AssetBuilderManager::Get()->BuildRequest(mat, request.target);
            asset->AddDependencies(mat);
        }

        if (data.skeleton) {
            asset->AddDependencies(data.skeleton);
        }

        data.rawData.storage.resize(data.dataSize);
        bin.LoadValue(reinterpret_cast<char*>(data.rawData.storage.data()), data.dataSize);
        // MeshletBuilder::BuildFromMeshData(data);

        AssetManager::Get()->SaveAsset(asset, request.target);
        result.retCode = AssetBuildRetCode::SUCCESS;
    }

} // namespace sky::builder