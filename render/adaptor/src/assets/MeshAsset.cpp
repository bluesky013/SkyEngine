//
// Created by Zach Lee on 2023/2/26.
//

#include <framework/asset/AssetManager.h>
#include <render/adaptor/assets/MaterialAsset.h>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/RHI.h>

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
            subMesh.material = am->LoadAsset<MaterialInstance>(Uuid::CreateFromString(idStr));

            archive.LoadValue(subMesh.aabb.min.x);
            archive.LoadValue(subMesh.aabb.min.y);
            archive.LoadValue(subMesh.aabb.min.z);
            archive.LoadValue(subMesh.aabb.max.x);
            archive.LoadValue(subMesh.aabb.max.y);
            archive.LoadValue(subMesh.aabb.max.z);
        }
        archive.LoadValue(size);
        vertexBuffers.resize(size);
        for (uint32_t i = 0; i < size; ++i){
            std::string idStr;
            archive.LoadValue(idStr);
            vertexBuffers[i] = am->LoadAsset<Buffer>(Uuid::CreateFromString(idStr));
        }
        {
            std::string idStr;
            archive.LoadValue(idStr);
            indexBuffer = am->LoadAsset<Buffer>(Uuid::CreateFromString(idStr));
        }
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
            archive.SaveValue(subMesh.firstVertex);
            archive.SaveValue(subMesh.vertexCount);
            archive.SaveValue(subMesh.firstIndex);
            archive.SaveValue(subMesh.indexCount);
            archive.SaveValue(subMesh.material ? subMesh.material->GetUuid().ToString() : Uuid().ToString());
            archive.SaveValue(subMesh.aabb.min.x);
            archive.SaveValue(subMesh.aabb.min.y);
            archive.SaveValue(subMesh.aabb.min.z);
            archive.SaveValue(subMesh.aabb.max.x);
            archive.SaveValue(subMesh.aabb.max.y);
            archive.SaveValue(subMesh.aabb.max.z);
        }

        archive.SaveValue(static_cast<uint32_t>(vertexBuffers.size()));
        for (const auto &vtx : vertexBuffers) {
            archive.SaveValue(vtx->GetUuid().ToString());
        }
        archive.SaveValue(indexBuffer ? indexBuffer->GetUuid().ToString() : Uuid().ToString());
        archive.SaveValue(indexType);
        archive.SaveValue(static_cast<uint32_t>(vertexDescriptions.size()));
        for (const auto &vtx : vertexDescriptions) {
            archive.SaveValue(vtx);
        }
    }

    std::shared_ptr<Mesh> CreateMesh(const MeshAssetData &data)
    {
        auto mesh = std::make_shared<Mesh>();

        auto *queue = RHI::Get()->GetDevice()->GetQueue(rhi::QueueType::TRANSFER);
        rhi::TransferTaskHandle handle = 0;
        for (const auto &vb : data.vertexBuffers) {
            auto buffer = std::make_shared<Buffer>();
            buffer->Init(vb->Data().size, rhi::BufferUsageFlagBit::VERTEX | rhi::BufferUsageFlagBit::TRANSFER_DST, rhi::MemoryType::GPU_ONLY);
            handle = buffer->Upload(vb->GetPath(), *queue, data.indexBuffer->Data().GetDataOffset());
            mesh->AddVertexBuffer(buffer);
        }

        if (data.indexBuffer) {
            auto buffer = std::make_shared<Buffer>();
            buffer->Init(data.indexBuffer->Data().size, rhi::BufferUsageFlagBit::INDEX | rhi::BufferUsageFlagBit::TRANSFER_DST, rhi::MemoryType::GPU_ONLY);
            handle = buffer->Upload(data.indexBuffer->GetPath(), *queue, data.indexBuffer->Data().GetDataOffset());
            mesh->SetIndexBuffer(buffer);
        }
        mesh->SetIndexType(data.indexType);

        for (const auto &sub : data.subMeshes) {
            mesh->AddSubMesh(SubMesh {
                sub.firstVertex,
                sub.vertexCount,
                sub.firstIndex,
                sub.indexCount,
                sub.material->CreateInstance(),
                sub.aabb
            });
        }
        queue->Wait(handle);

        return mesh;
    }

}