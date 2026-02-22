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
        if (buffer) {
            buffers.emplace_back(buffer);
        }
    }

    void RenderResourceGC::CollectImage(const rhi::ImagePtr &image)
    {
        if (image) {
            images.emplace_back(image);
        }
    }

    void RenderResourceGC::CollectImageViews(const rhi::ImageViewPtr &view)
    {
        imagesViews.emplace_back(view);
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
        imagesViews.clear();
    }
} // namespace sky
