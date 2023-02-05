//
// Created by Zach on 2023/2/1.
//

#pragma once

#include <rhi/GraphicsPipeline.h>
#include <gles/DevObject.h>

namespace sky::gles {

    class GraphicsPipeline : public rhi::GraphicsPipeline, public DevObject {
    public:
        GraphicsPipeline(Device &dev) : DevObject(dev) {}
        ~GraphicsPipeline() = default;

        bool Init(const Descriptor &desc);
        GLuint GetProgram() const { return program; }
        const rhi::PipelineState &GetPipelineState() const { return state; }

    private:
        bool InitProgram(const Descriptor &desc);

        rhi::PipelineState state;
        GLuint program = 0;
    };
    using GraphicsPipelinePtr = std::shared_ptr<GraphicsPipeline>;

}