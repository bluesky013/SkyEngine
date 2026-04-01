//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/PipelineState.h>

namespace sky::aurora {

    class MetalDevice;

    class MetalGraphicsPipeline : public GraphicsPipeline {
    public:
        explicit MetalGraphicsPipeline(MetalDevice &dev);
        ~MetalGraphicsPipeline() override;

        bool Init(const Descriptor &desc);

        void *GetNativeHandle() const { return pipeline; }

    private:
        MetalDevice       &device;
        void              *pipeline = nullptr;
        PrimitiveTopology  topology = PrimitiveTopology::TRIANGLE_LIST;
    };

    class MetalComputePipeline : public ComputePipeline {
    public:
        explicit MetalComputePipeline(MetalDevice &dev);
        ~MetalComputePipeline() override;

        bool Init(const Descriptor &desc);

        void *GetNativeHandle() const { return pipeline; }

    private:
        MetalDevice &device;
        void        *pipeline = nullptr;
    };

} // namespace sky::aurora