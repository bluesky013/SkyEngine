//
// Created by Zach Lee on 2023/5/28.
//

#pragma once

#include <rhi/GraphicsPipeline.h>
#include <mtl/DevObject.h>
#import <Metal/MTLRenderPipeline.h>
#import <Metal/MTLDepthStencil.h>

namespace sky::mtl {
    class Device;

    struct RasterizerState {
        MTLWinding frontFace;
        MTLCullMode cullMode;
        MTLTriangleFillMode fillMode;
        MTLDepthClipMode depthClipMode;
        float depthBias = 0.f;
        float depthBiasClamp = 0.f;
        float depthSlopeScale = 0.f;
    };

    class GraphicsPipeline : public rhi::GraphicsPipeline, public DevObject {
    public:
        GraphicsPipeline(Device &dev) : DevObject(dev) {}
        ~GraphicsPipeline();

        id<MTLRenderPipelineState> GetRenderPipelineState() const { return pso; }
        id<MTLDepthStencilState> GetDepthStencilState() const { return dsState; }
        const RasterizerState &GetRasterizerState() const { return rasterizerState; }

        uint32_t GetFrontReference() const { return frontReference; }
        uint32_t GetBackReference() const { return backReference; }
        MTLPrimitiveType GetPrimitiveType() const { return primitiveType; }

    private:
        friend class Device;
        bool Init(const Descriptor &desc);

        id<MTLRenderPipelineState> pso = nil;
        id<MTLDepthStencilState> dsState = nil;
        RasterizerState rasterizerState;
        uint32_t frontReference = 0;
        uint32_t backReference = 0;
        MTLPrimitiveType primitiveType = MTLPrimitiveTypeTriangle;
    };

} // namespace sky::mtl
