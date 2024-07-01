//
// Created by Zach Lee on 2023/2/26.
//

#include <framework/asset/AssetManager.h>
#include <render/RHI.h>
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
            archive.LoadValue(reinterpret_cast<char *>(&subMeshes[i]), sizeof(SubMeshAssetData));
        }
        archive.LoadValue(size);
        vertexBuffers.resize(size);
        for (uint32_t i = 0; i < size; ++i){
            archive.LoadValue(reinterpret_cast<char *>(&vertexBuffers[i]), sizeof(BufferViewData));
        }
        archive.LoadValue(reinterpret_cast<char *>(&indexBuffer), sizeof(BufferViewData));
        archive.LoadValue(indexType);
        archive.LoadValue(size);
        vertexDescriptions.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            archive.LoadValue(vertexDescriptions[i]);
        }
    }

    void MeshAssetData::Save(BinaryOutputArchive &archive) const
    {
        archive.SaveValue(static_cast<uint32_t>(subMeshes.size()));
        for (const auto &subMesh : subMeshes) {
            archive.SaveValue(reinterpret_cast<const char*>(&subMesh), sizeof(SubMeshAssetData));
        }

        archive.SaveValue(static_cast<uint32_t>(vertexBuffers.size()));
        for (const auto &vtx : vertexBuffers) {
            archive.SaveValue(reinterpret_cast<const char*>(&vtx), sizeof(BufferViewData));
        }
        archive.SaveValue(reinterpret_cast<const char*>(&indexBuffer), sizeof(BufferViewData));
        archive.SaveValue(indexType);
        archive.SaveValue(static_cast<uint32_t>(vertexDescriptions.size()));
        for (const auto &vtx : vertexDescriptions) {
            archive.SaveValue(vtx);
        }
    }

    std::shared_ptr<Mesh> CreateMesh(const MeshAssetData &data)
    {
        auto mesh = std::make_shared<Mesh>();

        auto *am = AssetManager::Get();
        auto *queue = RHI::Get()->GetDevice()->GetQueue(rhi::QueueType::TRANSFER);
        rhi::TransferTaskHandle handle = 0;
        for (const auto &vb : data.vertexBuffers) {
//            auto bufferPath = AssetManager::Get()->OpenAsset(vb.buffer);
//            auto buffer = std::make_shared<Buffer>();
//            buffer->Init(vb.size, rhi::BufferUsageFlagBit::VERTEX | rhi::BufferUsageFlagBit::TRANSFER_DST, rhi::MemoryType::GPU_ONLY);
//            handle = buffer->Upload(bufferPath, *queue, sizeof(BufferAssetHeader) + vb.offset);
//            mesh->AddVertexBuffer(buffer);
        }

        if (data.indexBuffer.buffer) {
//            auto bufferPath = AssetManager::Get()->OpenAsset(data.indexBuffer.buffer);
//            auto buffer = std::make_shared<Buffer>();
//            buffer->Init(data.indexBuffer.size, rhi::BufferUsageFlagBit::INDEX | rhi::BufferUsageFlagBit::TRANSFER_DST, rhi::MemoryType::GPU_ONLY);
//            handle = buffer->Upload(bufferPath, *queue, sizeof(BufferAssetHeader) + data.indexBuffer.offset);
//            mesh->SetIndexBuffer(buffer);
        }
        mesh->SetIndexType(data.indexType);

        for (const auto &sub : data.subMeshes) {
            auto mat = am->LoadAsset<MaterialInstance>(sub.material)->CreateInstance();

            mesh->AddSubMesh(SubMesh {
                sub.firstVertex,
                sub.vertexCount,
                sub.firstIndex,
                sub.indexCount,
                mat,
                sub.aabb
            });
        }
        queue->Wait(handle);

        return mesh;
    }

}