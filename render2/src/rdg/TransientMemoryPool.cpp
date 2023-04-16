//
// Created by Zach Lee on 2023/4/16.
//

#include <render/rdg/TransientMemoryPool.h>

namespace sky::rdg {

    rhi::ImageViewPtr TransientMemoryPool::requestImage(const rdg::GraphImage &desc)
    {
        return {};
    }

    rhi::BufferViewPtr TransientMemoryPool::requestBuffer(const rdg::GraphBuffer &desc)
    {
        return {};
    }
} // namespace sky::rdg