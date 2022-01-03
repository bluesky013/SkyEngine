//
// Created by Zach Lee on 2022/1/2.
//

#pragma once
#include <vulkan/FrameBuffer.h>
#include <vulkan/RenderPass.h>
#include <vulkan/Swapchain.h>
#include <engine/render/rendergraph/RenderGraphResource.h>

namespace sky {

    enum class SizePolicy : uint8_t {
        CUSTOM,
        INPUT
    };

    class RenderGraphPassData {
    public:
        RenderGraphPassData() = default;
        virtual ~RenderGraphPassData() = default;
    };

    class GraphicPassData : public RenderGraphPassData {
    public:
        GraphicPassData() = default;
        ~GraphicPassData() = default;

        std::vector<RGAttachmentPtr> attachments;
        std::vector<VkClearValue> clears;
        drv::FrameBuffer::Descriptor fbInfo;
        drv::FrameBufferPtr frameBuffer;
    };

    class SwapChainPassData : public RenderGraphPassData {
    public:
        SwapChainPassData() = default;
        ~SwapChainPassData() = default;

        RGImagePtr image;
        drv::SwapChainPtr swapChain;
    };

}
