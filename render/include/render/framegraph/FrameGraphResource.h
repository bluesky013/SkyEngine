//
// Created by Zach Lee on 2022/6/2.
//

#pragma once

#include <vulkan/Buffer.h>
#include <vulkan/Image.h>

namespace sky {
    class FrameGraphPass;

    class FrameGraphResource {
    public:
        FrameGraphResource()          = default;
        virtual ~FrameGraphResource() = default;

        void Reference(FrameGraphPass &pass);

    private:
        FrameGraphPass *first = nullptr;
        FrameGraphPass *last  = nullptr;
    };

    class FrameGraphImage : public FrameGraphResource {
    public:
        FrameGraphImage()  = default;
        ~FrameGraphImage() = default;

        const VkImageCreateInfo &GetImageInfo() const;

        void Compile();

        vk::ImagePtr GetImage() const;

    private:
        friend class FrameGraphBuilder;
        vk::ImagePtr image;
    };

    class FrameGraphBuffer : public FrameGraphResource {
    public:
        FrameGraphBuffer()  = default;
        ~FrameGraphBuffer() = default;

        void Compile();

        vk::BufferPtr GetBuffer() const;

    private:
        friend class FrameGraphBuilder;
        vk::BufferPtr buffer;
    };

} // namespace sky
