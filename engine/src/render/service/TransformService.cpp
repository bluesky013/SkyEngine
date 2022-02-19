//
// Created by Zach Lee on 2022/2/1.
//

#include <engine/render/service/TransformService.h>
#include <engine/render/DriverManager.h>
#include <glm/gtc/matrix_transform.hpp>

namespace sky {

    TransformService::TransformService()
    {
    }

    TransformService::~TransformService()
    {
    }

    TransformService::Handle TransformService::Acquire()
    {
//        if (!freeList.empty()) {
//            auto res = freeList.back();
//            freeList.pop_back();
//            return res;
//        }
//        auto tmp = index;
//        ++index;
//
//        if (!pool) {
//            InitPool();
//        }
//        pool->Reserve(index);
//        return tmp;
        return {};
    }

    void TransformService::UpdateTransform(Handle handle, const Matrix4& trans)
    {
        if (pool) {
//            auto ptr = pool->GetAddress(handle);
//            new (ptr) ObjectInfo{trans, glm::transpose(glm::inverse(glm::mat3(trans)))};
        }
    }

    void TransformService::Free(Handle handle)
    {
//        freeList.emplace_back(handle);
    }

    void TransformService::InitPool()
    {
        auto& prop = DriverManager::Get()->GetDevice()->GetProperties();

        RenderBufferPool::Descriptor desc = {};
        desc.blockSize = prop.limits.maxUniformBufferRange;
        desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        desc.memory = VMA_MEMORY_USAGE_CPU_TO_GPU;
        desc.stride = std::max(static_cast<uint32_t>(prop.limits.minUniformBufferOffsetAlignment),
                               static_cast<uint32_t>(sizeof(ObjectInfo)));

        pool = std::make_unique<RenderBufferPool>(desc);
    }

    void TransformService::OnTick(float time)
    {
        if (pool) {
            pool->SwapBuffer();
        }
    }

}