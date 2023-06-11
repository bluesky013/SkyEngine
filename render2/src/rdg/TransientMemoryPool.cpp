//
// Created by Zach Lee on 2023/4/16.
//

#include <render/rdg/TransientMemoryPool.h>

namespace sky::rdg {

    rhi::ImageViewPtr TransientMemoryPool::RequestImage(const rdg::GraphImage &desc)
    {
        return {};
    }

    rhi::BufferViewPtr TransientMemoryPool::RequestBuffer(const rdg::GraphBuffer &desc)
    {
        return {};
    }

    void TransientMemoryPool::RecycleImage(rhi::ImageViewPtr &image, const rdg::GraphImage &desc)
    {

    }

    void TransientMemoryPool::RecycleBuffer(rhi::BufferViewPtr &buffer, const rdg::GraphBuffer &desc)
    {

    }

} // namespace sky::rdg