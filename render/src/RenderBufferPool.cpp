//
// Created by Zach Lee on 2022/2/1.
//

#include <render/RenderBufferPool.h>
#include <render/DriverManager.h>

namespace sky {

    RenderBufferPool::RenderBufferPool(Descriptor desc)
        : descriptor(std::move(desc))
    {
    }

    RenderBufferPool::~RenderBufferPool()
    {
        freeList.clear();
        blocks.clear();
    }

    RDDynBufferViewPtr RenderBufferPool::Allocate()
    {
        RDDynBufferViewPtr res;
        if (!freeList.empty()) {
            res = freeList.back();
            freeList.pop_back();
        } else {
            size_t currentNum = active.size();
            if (currentNum == blocks.size() * descriptor.count) {
                AllocateBlock(1);
            }
            size_t blockIndex = currentNum / descriptor.count;
            size_t offset = (currentNum % descriptor.count) * descriptor.stride;
            res = std::make_shared<DynamicBufferView>(blocks[blockIndex], descriptor.stride, offset);
        }
        active.emplace_back(res);
        return res;
    }

    void RenderBufferPool::Free(RDDynBufferViewPtr view)
    {
        active.erase(std::find_if(active.begin(), active.end(), [&view](RDDynBufferViewPtr& rhs) {
            return view.get() == rhs.get();
        }));
        freeList.emplace_back(view);
    }

    void RenderBufferPool::Update()
    {
        for (auto& block : blocks) {
            block->Update();
        }
    }

    void RenderBufferPool::AllocateBlock(uint32_t num)
    {
        auto dev = DriverManager::Get()->GetDevice();
        for (uint32_t i = 0; i < num; ++i) {
            Buffer::Descriptor bufferDesc = {};
            bufferDesc.size = descriptor.count * descriptor.stride * descriptor.frame;
            bufferDesc.memory = descriptor.memory;
            bufferDesc.usage = descriptor.usage;
            bufferDesc.allocCPU = true;
            auto buffer = std::make_shared<Buffer>();
            buffer->InitRHI();
            blocks.emplace_back(buffer);
        }
    }
}