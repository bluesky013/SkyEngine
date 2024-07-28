//
// Created by Zach Lee on 2023/8/31.
//

#include <render/resource/Mesh.h>
#include <render/RHI.h>

namespace sky {

    void Mesh::AddSubMesh(const SubMesh &sub)
    {
        subMeshes.emplace_back(sub);
    }

    void Mesh::AddVertexBuffer(const RDBufferPtr &vb)
    {
        vertexBuffers.emplace_back(vb);
    }

    void Mesh::SetIndexBuffer(const RDBufferPtr &ib)
    {
        indexBuffer = ib;
    }

    void Mesh::SetIndexType(rhi::IndexType type)
    {
        indexType = type;
    }

    void Mesh::SetMaterial(const RDMaterialInstancePtr &mat, uint32_t subMesh)
    {
        subMeshes[subMesh].material = mat;
    }

    void Mesh::AddVertexDescriptions(const std::string &key)
    {
        vertexDescriptions.emplace_back(key);
    }

    void Mesh::SetUploadStream(MeshData&& stream_)
    {
        streamData = std::move(stream_);
        UploadMeshData();
    }

    void Mesh::UploadMeshData()
    {
        auto *queue = RHI::Get()->GetDevice()->GetQueue(rhi::QueueType::TRANSFER);

        vertexBuffers.resize(streamData.vertexStreams.size());
        for (uint32_t i = 0; i < streamData.vertexStreams.size(); ++i) {
            auto &vStream =streamData.vertexStreams[i];

            vertexBuffers[i] = new Buffer();
            vertexBuffers[i]->Init(vStream.size,
                                   rhi::BufferUsageFlagBit::VERTEX | rhi::BufferUsageFlagBit::TRANSFER_DST,
                                   rhi::MemoryType::GPU_ONLY);
            uploadHandle = queue->UploadBuffer(vertexBuffers[i]->GetRHIBuffer(), vStream);
        }

        if (streamData.indexStream.source) {
            auto &iStream = streamData.indexStream;

            indexBuffer = new Buffer();
            indexBuffer->Init(iStream.size, rhi::BufferUsageFlagBit::INDEX | rhi::BufferUsageFlagBit::TRANSFER_DST,
                              rhi::MemoryType::GPU_ONLY);
            uploadHandle = queue->UploadBuffer(indexBuffer->GetRHIBuffer(), iStream);
        }
        queue->Wait(uploadHandle);

//        rhi::TransferTaskHandle handle = 0;
//        for (const auto &vb : data.vertexBuffers) {
//            auto bufferPath = AssetManager::Get()->OpenAsset(vb.buffer);
//            auto buffer = std::make_shared<Buffer>();
//            buffer->Init(vb.size, rhi::BufferUsageFlagBit::VERTEX | rhi::BufferUsageFlagBit::TRANSFER_DST, rhi::MemoryType::GPU_ONLY);
//            handle = buffer->Upload(bufferPath, *queue, sizeof(BufferAssetHeader) + vb.offset);
//            mesh->AddVertexBuffer(buffer);
//        }
//
//        if (data.indexBuffer.buffer) {
//            auto bufferPath = AssetManager::Get()->OpenAsset(data.indexBuffer.buffer);
//            auto buffer = std::make_shared<Buffer>();
//            buffer->Init(data.indexBuffer.size, rhi::BufferUsageFlagBit::INDEX | rhi::BufferUsageFlagBit::TRANSFER_DST, rhi::MemoryType::GPU_ONLY);
//            handle = buffer->Upload(bufferPath, *queue, sizeof(BufferAssetHeader) + data.indexBuffer.offset);
//            mesh->SetIndexBuffer(buffer);
//        }
//        mesh->SetIndexType(data.indexType);
    }

} // namespace sky