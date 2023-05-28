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

    private:
        friend class Device;
        bool Init(const Descriptor &desc);

        id<MTLRenderPipelineState> pso = nil;
        id<MTLDepthStencilState> dsState = nil;
        RasterizerState rasterizerState;
    };

} // namespace sky::mtl
