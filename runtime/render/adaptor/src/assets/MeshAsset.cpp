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
        archive.LoadValue(skeleton);

        uint32_t size = 0;
        archive.LoadValue(size);

        // subMesh
        subMeshes.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            archive.LoadValue(reinterpret_cast<char *>(&subMeshes[i]), sizeof(SubMeshAssetData));
        }

        // buffers
        archive.LoadValue(size);
        buffers.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            archive.LoadValue(buffers[i].type);
            archive.LoadValue(buffers[i].offset);
            archive.LoadValue(buffers[i].size);
            archive.LoadValue(buffers[i].stride);
        }

        // vertex streams
        archive.LoadValue(size);
        attributes.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            archive.LoadValue(attributes[i].sematic);
            archive.LoadValue(attributes[i].binding);
            archive.LoadValue(attributes[i].offset);
            archive.LoadValue(attributes[i].format);
            archive.LoadValue(attributes[i].rate);
        }

        // data size
        archive.LoadValue(indexBuffer);
        archive.LoadValue(indexType);
        archive.LoadValue(dataSize);
        dataOffset = static_cast<uint32_t>(archive.GetStream().Tell());
    }

    void MeshAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(version);
        archive.SaveValue(skeleton);

        // subMesh
        archive.SaveValue(static_cast<uint32_t>(subMeshes.size()));
        for (const auto &subMesh : subMeshes) {
            archive.SaveValue(reinterpret_cast<const char*>(&subMesh), sizeof(SubMeshAssetData));
        }

        // primitives
        archive.SaveValue(static_cast<uint32_t>(buffers.size()));
        for (const auto &primitive : buffers) {
            archive.SaveValue(primitive.type);
            archive.SaveValue(primitive.offset);
            archive.SaveValue(primitive.size);
            archive.SaveValue(primitive.stride);
        }

        // vertex streams
        archive.SaveValue(static_cast<uint32_t>(attributes.size()));
        for (const auto &stream : attributes) {
            archive.SaveValue(stream.sematic);
            archive.SaveValue(stream.binding);
            archive.SaveValue(stream.offset);
            archive.SaveValue(stream.format);
            archive.SaveValue(stream.rate);
        }

        // index
        archive.SaveValue(indexBuffer);
        archive.SaveValue(indexType);

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

        mesh->SetVertexAttributes(data.attributes);
        mesh->SetIndexType(data.indexType);

        MeshData meshData = {};
        auto *fileStream = new rhi::FileStream(file, data.dataOffset);

        for (const auto &buffer : data.buffers) {
            rhi::BufferUploadRequest request = {};

            request.source = fileStream;
            request.offset = buffer.offset;
            request.size   = buffer.size;
            meshData.vertexStreams.emplace_back(request, buffer.stride);
        }

        if (data.indexType != rhi::IndexType::NONE) {
            SKY_ASSERT(data.indexBuffer < data.buffers.size());
            rhi::BufferUploadRequest &request = meshData.indexStream;
            const auto &buffer = data.buffers[data.indexBuffer];
            request.source = fileStream;
            request.offset = buffer.offset;
            request.size   = buffer.size;
        }
        mesh->SetUploadStream(std::move(meshData));
        return mesh;
    }

}