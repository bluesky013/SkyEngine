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

        rhi::ImageViewPtr RequestImage(const rdg::GraphImage &desc) override;
        rhi::BufferViewPtr RequestBuffer(const rdg::GraphBuffer &desc) override;

        void RecycleImage(rhi::ImageViewPtr &image, const rdg::GraphImage &desc) override;
        void RecycleBuffer(rhi::BufferViewPtr &buffer, const rdg::GraphBuffer &desc) override;

        template <typename T>
        struct CacheItem {
            T item;
            uint32_t count = 0;
            bool allocated = false;
        };

        std::unordered_map<rdg::GraphImage, std::list<CacheItem<rhi::ImageViewPtr>>> images;
        std::unordered_map<rdg::GraphBuffer, std::list<CacheItem<rhi::BufferViewPtr>>> buffers;
    };

} // namespace sky::rdg