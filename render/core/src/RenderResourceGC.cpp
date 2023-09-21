//
// Created by Zach Lee on 2023/9/5.
//

#include <render/RenderResourceGC.h>

namespace sky {

    RenderResourceGC::~RenderResourceGC()
    {
        Clear();
    }

    void RenderResourceGC::CollectBuffer(const rhi::BufferPtr &buffer)
    {
        buffers.emplace_back(buffer);
    }

    void RenderResourceGC::CollectImage(const rhi::ImagePtr &image)
    {
        images.emplace_back(image);
    }

    void RenderResourceGC::CollectDescriptorSet(const rhi::DescriptorSetPtr &set)
    {
        sets.emplace_back(set);
    }

    void RenderResourceGC::Clear()
    {
        buffers.clear();
        images.clear();
        sets.clear();
    }
} // namespace sky
