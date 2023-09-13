//
// Created by Zach Lee on 2023/2/26.
//

#include <framework/asset/AssetManager.h>
#include <render/adaptor/assets/MaterialAsset.h>
#include <render/adaptor/assets/MeshAsset.h>

namespace sky {
    void MeshAssetData::Load(BinaryInputArchive &archive)
    {
        auto *am = AssetManager::Get();
        uint32_t size = 0;
        archive.LoadValue(size);
        subMeshes.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            SubMeshAssetData &subMesh = subMeshes[i];
            archive.LoadValue(subMesh.firstVertex);
            archive.LoadValue(subMesh.vertexCount);
            archive.LoadValue(subMesh.firstIndex);
            archive.LoadValue(subMesh.indexCount);
            std::string idStr;
            archive.LoadValue(idStr);
            subMesh.material = am->LoadAsset<Material>(Uuid::CreateFromString(idStr));
        }
        {
            std::string idStr;
            archive.LoadValue(idStr);
            vertexBuffer = am->LoadAsset<Buffer>(Uuid::CreateFromString(idStr));
        }
        {
            std::string idStr;
            archive.LoadValue(idStr);
            indexBuffer = am->LoadAsset<Buffer>(Uuid::CreateFromString(idStr));
        }
        archive.LoadValue(vertexDescription);
    }

    void MeshAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(static_cast<uint32_t>(subMeshes.size()));
        for (const auto &subMesh : subMeshes) {
            archive.SaveValue(subMesh.firstVertex);
            archive.SaveValue(subMesh.vertexCount);
            archive.SaveValue(subMesh.firstIndex);
            archive.SaveValue(subMesh.indexCount);
            archive.SaveValue(subMesh.material ? subMesh.material->GetUuid().ToString() : Uuid().ToString());
        }

        archive.SaveValue(vertexBuffer ? vertexBuffer->GetUuid().ToString() : Uuid().ToString());
        archive.SaveValue(indexBuffer ? indexBuffer->GetUuid().ToString() : Uuid().ToString());
        archive.SaveValue(vertexDescription);
    }

}