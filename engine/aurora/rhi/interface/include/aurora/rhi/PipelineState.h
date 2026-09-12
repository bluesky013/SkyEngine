//
// Created by Zach Lee on 2026/3/30.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <aurora/rhi/Core.h>

namespace sky::aurora {
    class Shader;

    struct AttachmentFormat {
        PixelFormat colors[MAX_COLOR_ATTACHMENTS] = {};
        uint32_t    numColors                     = 0;
        PixelFormat depthStencil                  = PixelFormat::UNDEFINED;
        SampleCount sampleCount                   = SampleCount::X1;
        uint32_t    viewMask                      = 0;
    };

    class GraphicsPipeline : public RefObject {
    public:
        struct Descriptor {
            PipelineState*   state    = nullptr;
            Shader*          shader   = nullptr;
            AttachmentFormat format;
        };

        GraphicsPipeline() = default;
        ~GraphicsPipeline() override = default;
    };

    class ComputePipeline : public RefObject {
    public:
        struct Descriptor {
            Shader* cs = nullptr;
        };

        ComputePipeline() = default;
        ~ComputePipeline() override = default;
    };

    class RayTracingPipeline : public RefObject {
    public:
        struct Descriptor {
        };

        RayTracingPipeline() = default;
        ~RayTracingPipeline() override = default;
    };

} // namespace sky::aurora