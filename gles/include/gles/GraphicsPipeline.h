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

    private:
        bool InitProgram(const Descriptor &desc);

        rhi::PipelineState state;
        GLuint program = 0;
    };

}