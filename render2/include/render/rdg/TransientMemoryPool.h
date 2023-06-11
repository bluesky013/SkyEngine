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
        rhi::ImageViewPtr RequestImage(const rdg::GraphImage &desc) override;
        rhi::BufferViewPtr RequestBuffer(const rdg::GraphBuffer &desc) override;

        void RecycleImage(rhi::ImageViewPtr &image, const rdg::GraphImage &desc) override;
        void RecycleBuffer(rhi::BufferViewPtr &buffer, const rdg::GraphBuffer &desc) override;
    };

} // namespace sky::rdg