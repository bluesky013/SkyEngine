//
// Created by Zach Lee on 2023/5/28.
//

#include <mtl/GraphicsPipeline.h>
#include <mtl/VertexInput.h>
#include <mtl/RenderPass.h>
#include <mtl/Conversion.h>
#include <mtl/Shader.h>
#include <mtl/Device.h>

namespace sky::mtl {

    static const rhi::BlendState EMPTY_BLEND = {};

    GraphicsPipeline::~GraphicsPipeline()
    {
        if (pso) {
            [pso release];
        }

        if (dsState) {
            [dsState release];
        }
    }

    bool GraphicsPipeline::Init(const Descriptor &desc)
    {
        {
            auto *pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];

            // shader stages
            pipelineDesc.vertexFunction = std::static_pointer_cast<Shader>(desc.vs)->GetNativeHandle();
            pipelineDesc.fragmentFunction = std::static_pointer_cast<Shader>(desc.fs)->GetNativeHandle();

            // vertexDesc
            pipelineDesc.vertexDescriptor = std::static_pointer_cast<VertexInput>(desc.vertexInput)->GetNativeDescriptor();

            // rendering pipelineState
            auto renderPass = std::static_pointer_cast<RenderPass>(desc.renderPass);
            const auto &colors = renderPass->GetColorAttachments();
            pipelineDesc.sampleCount = renderPass->GetSamplerCount();
            for (uint32_t i = 0; i < colors.size(); ++i) {
                const auto &blendState = i < desc.state.blendStates.size() ? desc.state.blendStates[i] : EMPTY_BLEND;
                pipelineDesc.colorAttachments[i].pixelFormat = colors[i].format;
                pipelineDesc.colorAttachments[i].writeMask = FromRHI(blendState.writeMask);

                pipelineDesc.colorAttachments[i].blendingEnabled = blendState.blendEn;
                pipelineDesc.colorAttachments[i].alphaBlendOperation = FromRHI(blendState.alphaBlendOp);
                pipelineDesc.colorAttachments[i].rgbBlendOperation = FromRHI(blendState.colorBlendOp);

                pipelineDesc.colorAttachments[i].destinationAlphaBlendFactor = FromRHI(blendState.dstAlpha);
                pipelineDesc.colorAttachments[i].destinationRGBBlendFactor = FromRHI(blendState.dstColor);
                pipelineDesc.colorAttachments[i].sourceAlphaBlendFactor = FromRHI(blendState.srcAlpha);
                pipelineDesc.colorAttachments[i].sourceRGBBlendFactor = FromRHI(blendState.srcColor);
            }

            if (renderPass->HasDepth()) {
                pipelineDesc.depthAttachmentPixelFormat = renderPass->GetDepthAttachment().format;
            }
            if (renderPass->HasStencil()) {
                pipelineDesc.stencilAttachmentPixelFormat = renderPass->GetStencilAttachment().format;
            }

            // input primitive topology
            pipelineDesc.inputPrimitiveTopology = FromRHI(desc.state.inputAssembly.topology);

            // rasterizer
            auto &rs = desc.state.rasterState;
            rasterizerState.frontFace = FromRHI(rs.frontFace);
            rasterizerState.cullMode = FromRHI(rs.cullMode);
            rasterizerState.fillMode = FromRHI(rs.polygonMode);
            rasterizerState.depthClipMode = rs.depthClampEnable ? MTLDepthClipModeClamp : MTLDepthClipModeClip;
            rasterizerState.depthBias = rs.depthBiasEnable ? rs.depthBiasConstantFactor : 0.f;
            rasterizerState.depthBiasClamp = rs.depthBiasEnable ? rs.depthBiasClamp : 0.f;
            rasterizerState.depthSlopeScale = rs.depthBiasEnable ? rs.depthBiasSlopeFactor : 0.f;
            [pipelineDesc release];
        }

        // depth stencil
        {
            auto *dsDesc                = [[MTLDepthStencilDescriptor alloc] init];
            dsDesc.depthWriteEnabled    = desc.state.depthStencil.depthWrite;
            dsDesc.depthCompareFunction = desc.state.depthStencil.depthTest ?
                FromRHI(desc.state.depthStencil.compareOp) : MTLCompareFunctionAlways;

            auto *front = [[MTLStencilDescriptor alloc] init];
            front.stencilCompareFunction = desc.state.depthStencil.stencilTest ?
                FromRHI(desc.state.depthStencil.front.compareOp) : MTLCompareFunctionAlways;
            front.stencilFailureOperation = FromRHI(desc.state.depthStencil.front.failOp);
            front.depthStencilPassOperation = FromRHI(desc.state.depthStencil.front.passOp);
            front.depthFailureOperation = FromRHI(desc.state.depthStencil.front.depthFailOp);
            front.writeMask = desc.state.depthStencil.front.writeMask;
            front.readMask = desc.state.depthStencil.front.compareMask;

            auto *back = [[MTLStencilDescriptor alloc] init];
            back.stencilCompareFunction = desc.state.depthStencil.stencilTest ?
                FromRHI(desc.state.depthStencil.back.compareOp) : MTLCompareFunctionAlways;
            back.stencilFailureOperation = FromRHI(desc.state.depthStencil.back.failOp);
            back.depthStencilPassOperation = FromRHI(desc.state.depthStencil.back.passOp);
            back.depthFailureOperation = FromRHI(desc.state.depthStencil.back.depthFailOp);
            back.writeMask = desc.state.depthStencil.back.writeMask;
            back.readMask = desc.state.depthStencil.back.compareMask;

            dsDesc.frontFaceStencil = front;
            dsDesc.backFaceStencil = back;

            dsState = [device.GetMetalDevice() newDepthStencilStateWithDescriptor: dsDesc];

            [front release];
            [back release];
            [dsDesc release];
        }
        return pso != nil && dsState != nil;
    }

} // namespace sky::mtl
