//
// Created by Zach Lee on 2023/2/26.
//

#include <framework/asset/AssetManager.h>
#include <render/adaptor/assets/MaterialAsset.h>
#include <render/adaptor/assets/MeshAsset.h>

namespace sky {
    void MeshAssetData::Load(BinaryInputArchive &archive)
    {
        uint32_t size = 0;
        archive.LoadValue(size);
        vertexDescriptions.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            VertexDesc &vDesc = vertexDescriptions[i];
            archive.LoadValue(vDesc.name);
            archive.LoadValue(vDesc.index);
            archive.LoadValue(vDesc.offset);
            archive.LoadValue(vDesc.format);
        }

        size = 0;
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
            subMesh.material = AssetManager::Get()->LoadAsset<Material>(Uuid::CreateFromString(idStr));
        }
    }

    void MeshAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(static_cast<uint32_t>(vertexDescriptions.size()));
        for (const auto &vDesc : vertexDescriptions) {
            archive.SaveValue(vDesc.name);
            archive.SaveValue(vDesc.index);
            archive.SaveValue(vDesc.offset);
            archive.SaveValue(vDesc.format);
        }

        archive.SaveValue(static_cast<uint32_t>(subMeshes.size()));
        for (const auto &subMesh : subMeshes) {
            archive.SaveValue(subMesh.firstVertex);
            archive.SaveValue(subMesh.vertexCount);
            archive.SaveValue(subMesh.firstIndex);
            archive.SaveValue(subMesh.indexCount);
            archive.SaveValue(subMesh.material ? subMesh.material->GetUuid().ToString() : Uuid().ToString());
        }

        archive.SaveValue(static_cast<uint32_t>(vertexBuffers.size()));
        for (const auto &vb : vertexBuffers) {
            auto size = static_cast<uint32_t>(vb.size());
            archive.SaveValue(size);
            archive.SaveValue(reinterpret_cast<const char*>(vb.data()), size);
        }
        auto size = static_cast<uint32_t>(indexBuffer.size());
        archive.SaveValue(size);
        archive.SaveValue(reinterpret_cast<const char*>(indexBuffer.data()), size);
    }

}