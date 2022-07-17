//
// Created by Zach Lee on 2022/6/2.
//

#include <render/framegraph/FrameGraphResource.h>
#include <vulkan/Buffer.h>
#include <vulkan/Image.h>

namespace sky {

    void FrameGraphResource::Reference(FrameGraphPass& pass)
    {
        if (first == nullptr) {
            first = &pass;
        } else {
            last = &pass;
        }
    }

    const VkImageCreateInfo& FrameGraphImage::GetImageInfo() const
    {
        return image->GetImageInfo();
    }

    drv::ImagePtr FrameGraphImage::GetImage() const
    {
        return image;
    }

    void FrameGraphImage::Compile()
    {
    }

    drv::BufferPtr FrameGraphBuffer::GetBuffer() const
    {
        return buffer;
    }

    void FrameGraphBuffer::Compile()
    {
    }

}