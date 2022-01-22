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

        std::vector<RGAttachmentPtr> colors;
        std::vector<RGAttachmentPtr> resolves;
        RGAttachmentPtr depthStencil;
        VkExtent2D extent2D = {1, 1};
        drv::RenderPassPtr pass;
        drv::FrameBufferPtr frameBuffer;
        std::vector<VkClearValue> clears;
    };

    void BuildGraphicsPass(GraphicPassData& passData);

    void BuildFrameBuffer(GraphicPassData& passData);
}
