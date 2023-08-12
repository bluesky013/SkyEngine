//
// Created by Zach Lee on 2023/4/16.
//

#include <render/rdg/TransientMemoryPool.h>

namespace sky::rdg {

    void TransientMemoryPool::ResetPool()
    {
    }

    rhi::ImagePtr TransientMemoryPool::RequestImage(const rdg::GraphImage &desc)
    {
        return {};
    }

    rhi::BufferPtr TransientMemoryPool::RequestBuffer(const rdg::GraphBuffer &desc)
    {
        return {};
    }

    void TransientMemoryPool::RecycleImage(rhi::ImagePtr &image, const rdg::GraphImage &desc)
    {

    }

    void TransientMemoryPool::RecycleBuffer(rhi::BufferPtr &buffer, const rdg::GraphBuffer &desc)
    {

    }

} // namespace sky::rdg