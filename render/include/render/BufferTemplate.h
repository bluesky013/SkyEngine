//
// Created by Zach Lee on 2022/3/4.
//

#pragma once

#include <render/RenderBufferPool.h>
#include <render/DriverManager.h>
#include <cstdint>
#include <list>
#include <memory>
#include <algorithm>

namespace sky {

    template <typename T, typename H>
    class BufferTemplate {
    public:
        using DataType = T;
        using Handle = H;

        Handle Acquire()
        {
            if (!freeList.empty()) {
                auto res = freeList.back();
                freeList.pop_back();
                return Handle(res);
            }
            auto tmp = index;
            ++index;

            if (!pool) {
                InitPool();
            }
            pool->Reserve(index);
            bufferInfo.resize(index);
            return Handle(tmp);
        }

        void Free(Handle& handle)
        {
            freeList.emplace_back(handle.GetIndex());
            handle.Reset();
        }

        void Update(const Handle& handle, const DataType& data)
        {
            auto index = handle.GetIndex();
            auto& info = bufferInfo[index];
            info = data;
            needUpdate = true;
        }

        template <typename Data>
        void Update(const Handle& handle, const Data& data, uint32_t offset)
        {
            auto index = handle.GetIndex();
            uint8_t* ptr = reinterpret_cast<uint8_t*>(&bufferInfo[index]) + offset;
            new (ptr) Data(data);
            needUpdate = true;
        }

        void Flush()
        {
            if (needUpdate) {
                pool->SwapBuffer();
            }
        }

    private:
        void InitPool()
        {
            auto& prop = DriverManager::Get()->GetDevice()->GetProperties();

            RenderBufferPool::Descriptor desc = {};
            desc.blockSize = prop.limits.maxUniformBufferRange;
            desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            desc.memory = VMA_MEMORY_USAGE_CPU_TO_GPU;
            desc.stride = std::max(static_cast<uint32_t>(prop.limits.minUniformBufferOffsetAlignment),
                                   static_cast<uint32_t>(sizeof(T)));

            pool = std::make_unique<RenderBufferPool>(desc);
        }

        uint32_t index = 0;
        bool needUpdate = false;
        std::list<uint32_t> freeList;
        std::vector<T> bufferInfo;
        std::unique_ptr<RenderBufferPool> pool;
    };

}

