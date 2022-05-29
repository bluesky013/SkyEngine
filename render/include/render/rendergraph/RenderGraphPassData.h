//
// Created by Zach Lee on 2022/1/2.
//

#pragma once
#include <vulkan/FrameBuffer.h>
#include <vulkan/RenderPass.h>
#include <vulkan/Swapchain.h>
#include <vulkan/CommandBuffer.h>
#include <vulkan/GraphicsPipeline.h>
#include <render/rendergraph/RenderGraphResource.h>
#include <memory>

namespace sky {
    class GraphicPassData;

    enum class SizePolicy : uint8_t {
        CUSTOM,
        INPUT
    };

    class RenderGraphPassData {
    public:
        RenderGraphPassData() = default;
        virtual ~RenderGraphPassData() = default;
    };

    class GraphicEncoder {
    public:
        GraphicEncoder() = default;
        virtual ~GraphicEncoder() = default;

        virtual void Execute(drv::CommandBuffer& cmd, GraphicPassData& data) = 0;
    };

    class FullscreenEncoder : public GraphicEncoder {
    public:
        FullscreenEncoder() = default;
        ~FullscreenEncoder() = default;

        virtual void Execute(drv::CommandBuffer& cmd, GraphicPassData& data) override;
    };

    class GraphicPassData : public RenderGraphPassData {
    public:
        GraphicPassData() = default;

        ~GraphicPassData();

        std::vector<RGAttachmentPtr> colors;
        std::vector<RGAttachmentPtr> resolves;
        RGAttachmentPtr depthStencil;
        VkExtent2D extent2D = {1, 1};
        drv::RenderPassPtr pass;
        drv::FrameBufferPtr frameBuffer;
        std::vector<VkClearValue> clears;
        std::unique_ptr<GraphicEncoder> encoder;
    };

    class FullscreenPassData : public GraphicPassData {
    public:
        FullscreenPassData()
        {
            encoder.reset(new FullscreenEncoder());
        }
        ~FullscreenPassData() = default;

        drv::GraphicsPipelinePtr pipeline;
    };

    class GraphicPassExecutor {
    public:
        GraphicPassExecutor(GraphicPassData& dat) : data(dat) {}
        ~GraphicPassExecutor() = default;

        void Execute(drv::CommandBuffer& cmd);

    private:
        GraphicPassData& data;
    };

    void BuildGraphicsPass(GraphicPassData& passData);

    void BuildFrameBuffer(GraphicPassData& passData);
}
