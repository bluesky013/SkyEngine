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
        void ResetPool() override;

        ImageObject *RequestImage(const rdg::GraphImage &desc) override;
        BufferObject *RequestBuffer(const rdg::GraphBuffer &desc) override;

        void RecycleImage(rhi::ImagePtr &image, const rdg::GraphImage &desc) override;
        void RecycleBuffer(rhi::BufferPtr &buffer, const rdg::GraphBuffer &desc) override;

        std::unordered_map<rdg::GraphImage, std::list<CacheItem<std::unique_ptr<ImageObject>>>> images;
        std::unordered_map<rdg::GraphBuffer, std::list<CacheItem<std::unique_ptr<BufferObject>>>> buffers;
    };

} // namespace sky::rdg