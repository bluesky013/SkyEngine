//
// Created by Zach Lee on 2022/2/1.
//

#include <render/RenderBufferPool.h>
#include <render/DriverManager.h>

namespace sky {

    RenderBufferPool::RenderBufferPool(const Descriptor& desc)
        : descriptor(desc)
    {
        blockStride = descriptor.count * descriptor.stride;
    }

    RenderBufferPool::~RenderBufferPool()
    {
        freeList.clear();
        blocks.clear();
    }

    RDDynBufferViewPtr RenderBufferPool::Allocate()
    {
        uint32_t index = 0;
        if (!freeList.empty()) {
            index = freeList.back();
            freeList.pop_back();
        } else {
            index = static_cast<uint32_t>(active.size());
        }

        if (index == blocks.size() * descriptor.count) {
            AllocateBlock();
        }
        size_t blockIndex = index / descriptor.count;
        size_t offset = (index % descriptor.count) * descriptor.stride;
        auto res = std::make_shared<DynamicBufferView>(blocks[blockIndex], descriptor.stride, offset, descriptor.frame, blockStride);
        res->SetID(index);
        active.emplace_back(index);

        return res;
    }

    void RenderBufferPool::Free(uint32_t index)
    {
        active.erase(std::find_if(active.begin(), active.end(), [index](uint32_t rhs) {
            return index == rhs;
        }));
        freeList.emplace_back(index);
    }

    void RenderBufferPool::Update()
    {
        for (auto& block : blocks) {
            block->Update();
        }
    }

    void RenderBufferPool::AllocateBlock()
    {
        Buffer::Descriptor bufferDesc = {};
        bufferDesc.size = blockStride * descriptor.frame;
        bufferDesc.memory = descriptor.memory;
        bufferDesc.usage = descriptor.usage;
        bufferDesc.allocCPU = true;
        auto buffer = std::make_shared<Buffer>(bufferDesc);
        buffer->InitRHI();
        blocks.emplace_back(buffer);
    }
}