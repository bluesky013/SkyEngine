//
// Created by Zach Lee on 2021/12/13.
//

#pragma once
#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"
#include "vulkan/Basic.h"
#include <vector>

namespace sky::drv {

    class Device;
    class RenderPass;

    class FrameBuffer : public DevObject {
    public:
        ~FrameBuffer();

        struct Descriptor {
            uint32_t width;
            uint32_t height;
            RenderPass* pass = nullptr;
            std::vector<VkImageView> views;
        };

        bool Init(const Descriptor&);

        VkFramebuffer GetNativeHandle() const;

    private:
        friend class Device;
        FrameBuffer(Device&);

        VkFramebuffer frameBuffer;
    };

}