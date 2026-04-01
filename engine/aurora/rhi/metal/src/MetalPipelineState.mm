//
// Created on 2026/04/02.
//

#include <MetalPipelineState.h>
#include <MetalDevice.h>
#include <MetalShader.h>
#include <MetalUtils.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraMetal";

namespace sky::aurora {

    MetalGraphicsPipeline::MetalGraphicsPipeline(MetalDevice &dev)
        : device(dev)
    {
    }

    MetalGraphicsPipeline::~MetalGraphicsPipeline()
    {
        if (pipeline != nullptr) {
            [(id<MTLRenderPipelineState>)pipeline release];
            pipeline = nullptr;
        }
    }

    bool MetalGraphicsPipeline::Init(const Descriptor &desc)
    {
        auto *metalDevice = (id<MTLDevice>)device.GetNativeDevice();
        auto *shader = desc.shader != nullptr ? static_cast<MetalShader *>(desc.shader) : nullptr;
        if (metalDevice == nil || shader == nullptr || shader->GetVertexFunction() == nullptr) {
            LOG_E(TAG, "graphics pipeline requires a valid Metal device and vertex shader");
            return false;
        }

        auto *pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineDesc.vertexFunction = (id<MTLFunction>)shader->GetVertexFunction()->GetNativeHandle();
        pipelineDesc.fragmentFunction = shader->GetFragmentFunction() != nullptr ?
            (id<MTLFunction>)shader->GetFragmentFunction()->GetNativeHandle() : nil;
        pipelineDesc.inputPrimitiveTopology = ToMetalPrimitiveTopology(desc.state != nullptr ?
            desc.state->inputAssembly.topology : PrimitiveTopology::TRIANGLE_LIST);

        for (uint32_t i = 0; i < desc.format.numColors; ++i) {
            auto format = ToMetalPixelFormat(desc.format.colors[i]);
            pipelineDesc.colorAttachments[i].pixelFormat = format;

            if (desc.state != nullptr && i < desc.state->blendStates.size()) {
                const auto &blend = desc.state->blendStates[i];
                pipelineDesc.colorAttachments[i].blendingEnabled = blend.blendEn;
                pipelineDesc.colorAttachments[i].sourceRGBBlendFactor = ToMetalBlendFactor(blend.srcColor);
                pipelineDesc.colorAttachments[i].destinationRGBBlendFactor = ToMetalBlendFactor(blend.dstColor);
                pipelineDesc.colorAttachments[i].rgbBlendOperation = ToMetalBlendOp(blend.colorBlendOp);
                pipelineDesc.colorAttachments[i].sourceAlphaBlendFactor = ToMetalBlendFactor(blend.srcAlpha);
                pipelineDesc.colorAttachments[i].destinationAlphaBlendFactor = ToMetalBlendFactor(blend.dstAlpha);
                pipelineDesc.colorAttachments[i].alphaBlendOperation = ToMetalBlendOp(blend.alphaBlendOp);
                pipelineDesc.colorAttachments[i].writeMask = ToMetalWriteMask(blend.writeMask);
            }
        }

        const auto depthStencilFormat = ToMetalPixelFormat(desc.format.depthStencil);
        if (depthStencilFormat == MTLPixelFormatDepth24Unorm_Stencil8 || depthStencilFormat == MTLPixelFormatDepth32Float_Stencil8) {
            pipelineDesc.depthAttachmentPixelFormat = depthStencilFormat;
            pipelineDesc.stencilAttachmentPixelFormat = depthStencilFormat;
        } else if (depthStencilFormat != MTLPixelFormatInvalid) {
            pipelineDesc.depthAttachmentPixelFormat = depthStencilFormat;
        }
        pipelineDesc.rasterSampleCount = ToMetalSampleCount(desc.format.sampleCount);

        NSError *error = nil;
        auto *nativePipeline = [metalDevice newRenderPipelineStateWithDescriptor:pipelineDesc error:&error];
        [pipelineDesc release];
        if (nativePipeline == nil) {
            const char *message = error != nil ? [[error localizedDescription] UTF8String] : "unknown";
            LOG_E(TAG, "newRenderPipelineStateWithDescriptor failed: %s", message);
            return false;
        }

        topology = desc.state != nullptr ? desc.state->inputAssembly.topology : PrimitiveTopology::TRIANGLE_LIST;
        pipeline = nativePipeline;
        return true;
    }

    MetalComputePipeline::MetalComputePipeline(MetalDevice &dev)
        : device(dev)
    {
    }

    MetalComputePipeline::~MetalComputePipeline()
    {
        if (pipeline != nullptr) {
            [(id<MTLComputePipelineState>)pipeline release];
            pipeline = nullptr;
        }
    }

    bool MetalComputePipeline::Init(const Descriptor &desc)
    {
        auto *metalDevice = (id<MTLDevice>)device.GetNativeDevice();
        auto *shader = desc.cs != nullptr ? static_cast<MetalShader *>(desc.cs) : nullptr;
        if (metalDevice == nil || shader == nullptr || shader->GetComputeFunction() == nullptr) {
            LOG_E(TAG, "compute pipeline requires a valid compute shader");
            return false;
        }

        NSError *error = nil;
        auto *nativePipeline = [metalDevice newComputePipelineStateWithFunction:
            (id<MTLFunction>)shader->GetComputeFunction()->GetNativeHandle() error:&error];
        if (nativePipeline == nil) {
            const char *message = error != nil ? [[error localizedDescription] UTF8String] : "unknown";
            LOG_E(TAG, "newComputePipelineStateWithFunction failed: %s", message);
            return false;
        }

        pipeline = nativePipeline;
        return true;
    }

} // namespace sky::aurora