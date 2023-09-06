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
        void ResetPool() override;

        ImageObject *RequestImage(const rdg::GraphImage &desc) override;
        BufferObject *RequestBuffer(const rdg::GraphBuffer &desc) override;

        void RecycleImage(rhi::ImagePtr &image, const rdg::GraphImage &desc) override;
        void RecycleBuffer(rhi::BufferPtr &buffer, const rdg::GraphBuffer &desc) override;
    };

} // namespace sky::rdg