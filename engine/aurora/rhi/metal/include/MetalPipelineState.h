//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/PipelineState.h>

namespace sky::aurora {

    class MetalDevice;
    class MetalShader;

    class MetalGraphicsPipeline : public GraphicsPipeline {
    public:
        explicit MetalGraphicsPipeline(MetalDevice &dev);
        ~MetalGraphicsPipeline() override;

        bool Init(const Descriptor &desc);

        void *GetNativeHandle() const { return pipeline; }

        MetalShader      *GetShader()           const { return shader.Get(); }
        PrimitiveTopology GetTopology()         const { return topology; }
        void             *GetDepthStencilState() const { return depthStencilState; }

        // rasterizer state applied by the encoder at BindPipeline time
        CullingModeFlags GetCullMode()    const { return cullMode; }
        FrontFace        GetFrontFace()   const { return frontFace; }
        PolygonMode      GetPolygonMode() const { return polygonMode; }
        bool             GetDepthClamp()  const { return depthClampEnable; }
        bool             GetDepthBias(float &constant, float &clamp, float &slope) const
        {
            constant = depthBiasConstantFactor;
            clamp    = depthBiasClamp;
            slope    = depthBiasSlopeFactor;
            return depthBiasEnable;
        }
        void GetStencilReference(uint32_t &front, uint32_t &back) const
        {
            front = stencilRefFront;
            back  = stencilRefBack;
        }

    private:
        MetalDevice          &device;
        CounterPtr<MetalShader> shader;
        void                *pipeline          = nullptr; // id<MTLRenderPipelineState>
        void                *depthStencilState = nullptr; // id<MTLDepthStencilState>
        PrimitiveTopology    topology              = PrimitiveTopology::TRIANGLE_LIST;
        CullingModeFlags     cullMode              = CullModeFlagBits::NONE;
        FrontFace            frontFace             = FrontFace::CCW;
        PolygonMode          polygonMode           = PolygonMode::FILL;
        bool                 depthClampEnable      = false;
        bool                 depthBiasEnable       = false;
        float                depthBiasConstantFactor = 0.f;
        float                depthBiasClamp          = 0.f;
        float                depthBiasSlopeFactor    = 0.f;
        uint32_t             stencilRefFront       = 0;
        uint32_t             stencilRefBack        = 0;
    };

    class MetalComputePipeline : public ComputePipeline {
    public:
        explicit MetalComputePipeline(MetalDevice &dev);
        ~MetalComputePipeline() override;

        bool Init(const Descriptor &desc);

        void *GetNativeHandle() const { return pipeline; }

        MetalShader *GetShader() const { return shader.Get(); }
        // [numthreads] from slang reflection; MSL binaries do not carry it
        void GetThreadGroupSize(uint32_t (&out)[3]) const
        {
            out[0] = threadGroupSize[0];
            out[1] = threadGroupSize[1];
            out[2] = threadGroupSize[2];
        }

    private:
        MetalDevice          &device;
        CounterPtr<MetalShader> shader;
        void                *pipeline = nullptr; // id<MTLComputePipelineState>
        uint32_t            threadGroupSize[3] = {1, 1, 1};
    };

} // namespace sky::aurora