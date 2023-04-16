//
// Created by Zach Lee on 2023/4/16.
//

#pragma once

#include <unordered_map>
#include <render/rdg/TransientPool.h>

namespace sky::rdg {

    class TransientMemoryPool : public TransientPool {
    public:
        TransientMemoryPool() = default;
        ~TransientMemoryPool() override = default;

    private:
        rhi::ImageViewPtr requestImage(const rdg::GraphImage &desc) override;
        rhi::BufferViewPtr requestBuffer(const rdg::GraphBuffer &desc) override;
    };

} // namespace sky::rdg