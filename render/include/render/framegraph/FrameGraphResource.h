//
// Created by Zach Lee on 2022/6/2.
//

#pragma once

#include <vulkan/Image.h>
#include <vulkan/Buffer.h>

namespace sky {

    class FrameGraphResource {
    public:
        FrameGraphResource() = default;
        virtual ~FrameGraphResource() = default;
    };

    class FrameGraphImage : public FrameGraphResource {
    public:
        FrameGraphImage() = default;
        ~FrameGraphImage() = default;

    private:
        friend class FrameGraphBuilder;
        drv::ImagePtr image;
    };

    class FrameGraphBuffer : public FrameGraphResource {
    public:
        FrameGraphBuffer() = default;
        ~FrameGraphBuffer() = default;

    private:
        friend class FrameGraphBuilder;
        drv::BufferPtr buffer;
    };

}