//
// Created by Zach Lee on 2023/4/16.
//

#pragma once

#include <unordered_map>
#include <render/rdg/TransientPool.h>

namespace sky::rdg {

    class TransientObjectPool : public TransientPool {
    public:
        TransientObjectPool() = default;
        ~TransientObjectPool() override = default;

    private:
        rhi::ImageViewPtr requestImage(const rdg::GraphImage &desc) override;
        rhi::BufferViewPtr requestBuffer(const rdg::GraphBuffer &desc) override;

        std::unordered_map<rdg::GraphImage, rhi::ImageViewPtr> images;
        std::unordered_map<rdg::GraphBuffer, rhi::BufferViewPtr> buffers;
    };

} // namespace sky::rdg