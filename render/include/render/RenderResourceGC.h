//
// Created by Zach Lee on 2023/9/5.
//

#pragma once

#include <rhi/Device.h>

namespace sky {

    class RenderResourceGC {
    public:
        RenderResourceGC() = default;
        ~RenderResourceGC() = default;

        void CollectBuffer(const rhi::BufferPtr &buffer);
        void CollectImage(const rhi::ImagePtr &image);
        void CollectDescriptorSet(const rhi::DescriptorSetPtr &set);
        void Clear();

    private:
        std::vector<rhi::BufferPtr> buffers;
        std::vector<rhi::ImagePtr> images;
        std::vector<rhi::DescriptorSetPtr> sets;
    };


} // namespace sky
