//
// Created by Zach Lee on 2022/11/13.
//

#pragma once

#include <rhi/Core.h>
#include <rhi/RenderPass.h>
#include <rhi/ImageView.h>

namespace sky::rhi {

    class FrameBuffer {
    public:
        FrameBuffer()          = default;
        virtual ~FrameBuffer() = default;

        struct Descriptor {
            Extent2D                  extent = {1, 1};
            RenderPassPtr             pass;
            std::vector<ImageViewPtr> views;
        };

        const Extent2D &GetExtent() const { return extent; }

    protected:
        Extent2D extent;
    };
    using FrameBufferPtr = std::shared_ptr<FrameBuffer>;
};
