//
// Created by Zach Lee on 2022/5/27.
//


#include <engine/render/resources/Buffer.h>
#include <engine/render/DriverManager.h>
#include <vulkan/CommandBuffer.h>

namespace sky {

    Buffer::Buffer(const Descriptor& desc) : descriptor(desc)
    {
        if (desc.useHost) {
            rawData.resize(desc.size);
        }
    }

    void Buffer::InitRHI()
    {
        if (rhiBuffer) {
            return;
        }

        drv::Buffer::Descriptor desc = {};
        desc.size = descriptor.size;
        desc.memory = descriptor.memory;
        desc.usage = descriptor.usage;
        rhiBuffer = DriverManager::Get()->GetDevice()->CreateDeviceObject<drv::Buffer>(desc);
    }

    bool Buffer::IsValid() const
    {
        return !!rhiBuffer;
    }

    void Buffer::Update(const uint8_t* data, uint64_t size)
    {
        if (descriptor.memory == VMA_MEMORY_USAGE_GPU_ONLY) {
            auto device = DriverManager::Get()->GetDevice();
            auto queue = device->GetQueue({VK_QUEUE_GRAPHICS_BIT});

            drv::Buffer::Descriptor stagingDes = {};
            stagingDes.memory = VMA_MEMORY_USAGE_CPU_TO_GPU;
            stagingDes.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            stagingDes.size = descriptor.size;
            auto stagingBuffer = device->CreateDeviceObject<drv::Buffer>(stagingDes);
            uint8_t* dst = stagingBuffer->Map();
            memcpy(dst, data, size);
            stagingBuffer->UnMap();

            auto cmd = queue->AllocateCommandBuffer({});
            cmd->Begin();

            cmd->Copy(stagingBuffer, rhiBuffer, VkBufferCopy{0, 0, descriptor.size});

            cmd->End();
            cmd->Submit(*queue, {});
            cmd->Wait();
        } else {
            uint8_t* dst = rhiBuffer->Map();
            memcpy(dst, data, descriptor.size);
            rhiBuffer->UnMap();
        }
    }

    void Buffer::Update()
    {
        Update(rawData.data(), rawData.size());
    }


}