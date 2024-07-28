//
// Created by Zach Lee on 2023/2/26.
//

#include <framework/asset/AssetManager.h>
#include <core/archive/MemoryArchive.h>
#include <render/RHI.h>
#include <render/adaptor/assets/MeshAsset.h>

namespace sky {
    void MeshAssetData::Load(BinaryInputArchive &archive)
    {
        archive.LoadValue(version);

        uint32_t size = 0;
        archive.LoadValue(size);

        // subMesh
        subMeshes.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            archive.LoadValue(reinterpret_cast<char *>(&subMeshes[i]), sizeof(SubMeshAssetData));
        }

        // vertex desc
        archive.LoadValue(size);
        vertexDescriptions.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            archive.LoadValue(vertexDescriptions[i]);
        }

        // primitives
        archive.LoadValue(size);
        primitives.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            archive.LoadValue(primitives[i].offset);
            archive.LoadValue(primitives[i].size);
            archive.LoadValue(primitives[i].stride);
        }

        // indices
        archive.LoadValue(indices.offset);
        archive.LoadValue(indices.size);
        archive.LoadValue(indices.indexType);

        // data size
        archive.LoadValue(dataSize);
        dataOffset = static_cast<uint32_t>(archive.GetStream().Tell());
    }

    void MeshAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(version);

        // subMesh
        archive.SaveValue(static_cast<uint32_t>(subMeshes.size()));
        for (const auto &subMesh : subMeshes) {
            archive.SaveValue(reinterpret_cast<const char*>(&subMesh), sizeof(SubMeshAssetData));
        }

        // vertex desc
        archive.SaveValue(static_cast<uint32_t>(vertexDescriptions.size()));
        for (const auto &vtx : vertexDescriptions) {
            archive.SaveValue(vtx);
        }

        // primitives
        archive.SaveValue(static_cast<uint32_t>(primitives.size()));
        for (const auto &primitive : primitives) {
            archive.SaveValue(primitive.offset);
            archive.SaveValue(primitive.size);
            archive.SaveValue(primitive.stride);
        }

        // index
        archive.SaveValue(indices.offset);
        archive.SaveValue(indices.size);
        archive.SaveValue(indices.indexType);

        // data size
        archive.SaveValue(dataSize);

        SKY_ASSERT(dataSize == static_cast<uint32_t>(rawData.storage.size()));

        // save raw
        archive.SaveValue(reinterpret_cast<const char*>(rawData.storage.data()), dataSize);
    }

    CounterPtr<Mesh> CreateMeshFromAsset(const MeshAssetPtr &asset)
    {
        const auto &data = asset->Data();
        const auto &uuid = asset->GetUuid();

        auto *am = AssetManager::Get();
        auto file = am->OpenFile(uuid);
        SKY_ASSERT(file);

        auto *mesh = new Mesh();
        for (const auto &sub : data.subMeshes) {
            auto matAsset = am->FindAsset<MaterialInstance>(sub.material);
            auto mat = CreateMaterialInstanceFromAsset(matAsset);

            mesh->AddSubMesh(SubMesh {
                sub.firstVertex,
                sub.vertexCount,
                sub.firstIndex,
                sub.indexCount,
                mat,
                sub.aabb
            });
        }

        for (const auto &desc : data.vertexDescriptions) {
            mesh->AddVertexDescriptions(desc);
        }
        mesh->SetIndexType(data.indices.indexType);

        MeshData meshData = {};
        auto *fileStream = new rhi::FileStream(file, data.dataOffset);

        for (const auto &prim : data.primitives) {
            rhi::BufferUploadRequest request = {};
            request.source = fileStream;
            request.offset = prim.offset;
            request.size = prim.size;
            meshData.vertexStreams.emplace_back(request);
        }

        if (data.indices.size != 0) {
            rhi::BufferUploadRequest &request = meshData.indexStream;
            request.source = fileStream;
            request.offset = data.indices.offset;
            request.size = data.indices.size;
        }
        mesh->SetUploadStream(std::move(meshData));
        return mesh;
    }

}