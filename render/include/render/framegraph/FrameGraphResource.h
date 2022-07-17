//
// Created by Zach Lee on 2022/6/2.
//

#pragma once

#include <vulkan/Image.h>
#include <vulkan/Buffer.h>

namespace sky {
    class FrameGraphPass;

    class FrameGraphResource {
    public:
        FrameGraphResource() = default;
        virtual ~FrameGraphResource() = default;

        void Reference(FrameGraphPass& pass);

    private:
        FrameGraphPass* first = nullptr;
        FrameGraphPass* last = nullptr;
    };

    class FrameGraphImage : public FrameGraphResource {
    public:
        FrameGraphImage() = default;
        ~FrameGraphImage() = default;

        const VkImageCreateInfo& GetImageInfo() const;

        void Compile();

        drv::ImagePtr GetImage() const;

    private:
        friend class FrameGraphBuilder;
        drv::ImagePtr image;
    };

    class FrameGraphBuffer : public FrameGraphResource {
    public:
        FrameGraphBuffer() = default;
        ~FrameGraphBuffer() = default;

        void Compile();

        drv::BufferPtr GetBuffer() const;

    private:
        friend class FrameGraphBuilder;
        drv::BufferPtr buffer;
    };

}