//
// Created by Zach on 2023/2/1.
//

#pragma once

#include <rhi/GraphicsPipeline.h>
#include <gles/DevObject.h>
#include <gles/PipelineLayout.h>
#include <gles/Core.h>
#include <vector>
#include <string>

namespace sky::gles {

    struct GLAttribute {
        std::string name;
        GLint location;
    };

    class GraphicsPipeline : public rhi::GraphicsPipeline, public DevObject {
    public:
        GraphicsPipeline(Device &dev) : DevObject(dev) {}
        ~GraphicsPipeline() = default;

        bool Init(const Descriptor &desc);
        GLuint GetProgram() const { return program; }
        const PipelineLayoutPtr &GetPipelineLayout() const { return pipelineLayout; }
        const std::vector<uint32_t> &GetDescriptorOffsets() const { return descriptorOffsets; }
        const std::vector<GLDescriptorIndex> &GetDescriptorIndices() const { return descriptorIndices; }
        const GLState &GetGLState() { return state; }

    private:
        bool InitProgram(const Descriptor &desc);
        void InitVertexInput();
        void InitDescriptorIndices(const Descriptor &desc);
        void InitGLState(const Descriptor &desc);

        GLState state;
        std::vector<GLAttribute> attributes;
        std::vector<GLDescriptorIndex> descriptorIndices; // binding or location
        std::vector<uint32_t> descriptorOffsets;          // set offset to descriptors [set, offset]
        PipelineLayoutPtr pipelineLayout;
        GLuint program = 0;
    };
    using GraphicsPipelinePtr = std::shared_ptr<GraphicsPipeline>;

}