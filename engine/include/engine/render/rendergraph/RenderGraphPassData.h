//
// Created by Zach Lee on 2022/1/2.
//

#pragma once
#include <vulkan/FrameBuffer.h>
#include <engine/render/rendergraph/RenderGraphResource.h>

namespace sky {

    class RenderGraphPassData {
    public:
        RenderGraphPassData() = default;
        virtual ~RenderGraphPassData() = default;
    };

    class GraphicPassData : public RenderGraphPassData {
    public:
        GraphicPassData() = default;
        ~GraphicPassData() = default;



    protected:
        std::vector<GraphAttachment*> colors;
        std::vector<GraphAttachment*> resolves;
        std::vector<GraphAttachment*> inputs;
        GraphAttachment* depth;
        drv::FrameBufferPtr frameBuffer;
    };

}
