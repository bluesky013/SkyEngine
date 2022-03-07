//
// Created by Zach Lee on 2021/12/5.
//

#include <engine/asset/MeshAsset.h>
#include <framework/asset/ResourceManager.h>
#include <engine/render/DriverManager.h>
#include <vulkan/CommandBuffer.h>

namespace sky {

    CounterPtr<Mesh> Mesh::CreateFromAsset(AssetPtr asset)
    {
        if (!asset) {
            return {};
        }
        auto instance = ResourceManager::Get()->GetOrCreate<Mesh>(asset->GetId());
        auto device = DriverManager::Get()->GetDevice();

        auto shaderAsset = static_cast<MeshAsset*>(asset.Get());
        auto& sourceData = shaderAsset->data;

        uint32_t vtxSize = 0;
        uint32_t idxSize = 0;
        for (auto& buffer : sourceData.meshes) {
            vtxSize += static_cast<uint32_t>(buffer.vertices.size()) * sizeof(Vertex);
            idxSize += static_cast<uint32_t>(buffer.indices.size()) * sizeof(uint32_t);
        }

        drv::Buffer::Descriptor bufferDesc = {};
        bufferDesc.memory = VMA_MEMORY_USAGE_CPU_ONLY;
        bufferDesc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferDesc.size = vtxSize;
        auto vtxStaging = device->CreateDeviceObject<drv::Buffer>(bufferDesc);
        uint8_t* vPtr = vtxStaging->Map();

        bufferDesc.size = idxSize;
        auto idxStaging = device->CreateDeviceObject<drv::Buffer>(bufferDesc);
        uint8_t* iPtr = idxStaging->Map();

        bufferDesc.memory = VMA_MEMORY_USAGE_GPU_ONLY;
        bufferDesc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferDesc.size = vtxSize;
        instance->vertexBuffer = device->CreateDeviceObject<drv::Buffer>(bufferDesc);

        bufferDesc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        bufferDesc.size = idxSize;
        instance->indexBuffer = device->CreateDeviceObject<drv::Buffer>(bufferDesc);

        instance->primitives.reserve(sourceData.meshes.size());
        uint32_t vtxOffset = 0;
        uint32_t idxOffset = 0;
        for (auto& subData : sourceData.meshes) {
            instance->primitives.emplace_back(Primitive{});
            auto& primitive = instance->primitives.back();

            primitive.aabb = subData.aabb;
            primitive.vertexBuffers.emplace_back(BufferView{instance->vertexBuffer, vtxOffset, sizeof(Vertex)});
            primitive.indices = BufferView{instance->indexBuffer, idxOffset, sizeof(uint32_t)};

            {
                uint8_t* vtxPtr = vPtr + vtxOffset;
                uint32_t copySize = static_cast<uint32_t>(subData.vertices.size()) * sizeof(Vertex);
                memcpy(vtxPtr, subData.vertices.data(), copySize);
                vtxOffset += copySize;
            }

            {
                uint8_t* idxPtr = iPtr + idxOffset;
                uint32_t copySize = static_cast<uint32_t>(subData.indices.size()) * sizeof(uint32_t);
                memcpy(idxPtr, subData.indices.data(), copySize);
                idxOffset += copySize;
            }
        }

        vtxStaging->UnMap();
        idxStaging->UnMap();

        auto queue = device->GetQueue({VK_QUEUE_GRAPHICS_BIT});
        drv::CommandBuffer::Descriptor cmdDesc = {};
        auto cmdBuffer = queue->AllocateCommandBuffer(cmdDesc);
        cmdBuffer->Begin();
        cmdBuffer->Copy(vtxStaging->GetNativeHandle(), instance->vertexBuffer->GetNativeHandle(),
                        {0, 0, vtxSize});

        cmdBuffer->Copy(idxStaging->GetNativeHandle(), instance->indexBuffer->GetNativeHandle(),
                        {0, 0, idxSize});

        cmdBuffer->End();
        cmdBuffer->Submit(*queue, {});

        return instance;
    }

}