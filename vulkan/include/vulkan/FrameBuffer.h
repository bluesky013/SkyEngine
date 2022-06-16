//
// Created by Zach Lee on 2021/12/13.
//

#pragma once
#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"
#include "vulkan/Basic.h"
#include "vulkan/RenderPass.h"
#include "vulkan/ImageView.h"
#include <vector>

namespace sky::drv {

    class Device;

    class FrameBuffer : public DevObject {
    public:
        ~FrameBuffer();

        struct Descriptor {
            VkExtent2D extent = {1, 1};
            RenderPassPtr pass;
            std::vector<ImageViewPtr> views;
        };

        bool Init(const Descriptor&);

        VkFramebuffer GetNativeHandle() const;

        const VkExtent2D& GetExtent() const;

    private:
        friend class Device;
        FrameBuffer(Device&);

        VkFramebuffer frameBuffer;
        Descriptor descriptor;
    };

    using FrameBufferPtr = std::shared_ptr<FrameBuffer>;

}