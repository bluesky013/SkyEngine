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

    namespace {
        MTLStencilDescriptor *ToMetalStencilDescriptor(const StencilState &state)
        {
            auto *desc = [[MTLStencilDescriptor alloc] init];
            desc.stencilCompareFunction      = ToMetalCompare(state.compareOp);
            desc.stencilFailureOperation     = ToMetalStencilOp(state.failOp);
            desc.depthFailureOperation       = ToMetalStencilOp(state.depthFailOp);
            desc.depthStencilPassOperation   = ToMetalStencilOp(state.passOp);
            desc.readMask                    = state.compareMask;
            desc.writeMask                   = state.writeMask;
            return desc;
        }
    } // namespace

    MetalGraphicsPipeline::MetalGraphicsPipeline(MetalDevice &dev)
        : device(dev)
    {
    }

    MetalGraphicsPipeline::~MetalGraphicsPipeline()
    {
        if (depthStencilState != nullptr) {
            [(id<MTLDepthStencilState>)depthStencilState release];
            depthStencilState = nullptr;
        }
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
        if (desc.state != nullptr && desc.state->multiSample.alphaToCoverage) {
            pipelineDesc.alphaToCoverageEnabled = YES;
        }

        NSError *error = nil;
        auto *nativePipeline = [metalDevice newRenderPipelineStateWithDescriptor:pipelineDesc error:&error];
        [pipelineDesc release];
        if (nativePipeline == nil) {
            const char *message = error != nil ? [[error localizedDescription] UTF8String] : "unknown";
            LOG_E(TAG, "newRenderPipelineStateWithDescriptor failed: %s", message);
            return false;
        }

        // depth/stencil + rasterizer state are encoder-time state on Metal;
        // bake them into objects/values applied by BindPipeline
        if (desc.state != nullptr) {
            const auto &ds = desc.state->depthStencil;
            auto *dsDesc = [[MTLDepthStencilDescriptor alloc] init];
            dsDesc.depthCompareFunction = ds.depthTest ? ToMetalCompare(ds.compareOp) : MTLCompareFunctionAlways;
            dsDesc.depthWriteEnabled    = ds.depthWrite;
            if (ds.stencilTest) {
                dsDesc.frontFaceStencil = ToMetalStencilDescriptor(ds.front);
                dsDesc.backFaceStencil  = ToMetalStencilDescriptor(ds.back);
                // MTLDepthStencilDescriptor copies the stencil descriptors
                [dsDesc.frontFaceStencil release];
                [dsDesc.backFaceStencil release];
            }
            auto *dsState = [metalDevice newDepthStencilStateWithDescriptor:dsDesc];
            [dsDesc release];
            if (dsState == nil) {
                LOG_E(TAG, "newDepthStencilStateWithDescriptor failed");
                [nativePipeline release];
                return false;
            }
            depthStencilState = (__bridge_retained void *)dsState;
            stencilRefFront   = ds.front.reference;
            stencilRefBack    = ds.back.reference;

            const auto &rs = desc.state->rasterState;
            cullMode                 = rs.cullMode;
            frontFace                = rs.frontFace;
            polygonMode              = rs.polygonMode;
            depthClampEnable         = rs.depthClampEnable;
            depthBiasEnable          = rs.depthBiasEnable;
            depthBiasConstantFactor  = rs.depthBiasConstantFactor;
            depthBiasClamp           = rs.depthBiasClamp;
            depthBiasSlopeFactor     = rs.depthBiasSlopeFactor;
            topology                 = desc.state->inputAssembly.topology;
        }

        shader   = static_cast<MetalShader *>(desc.shader);
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

        // MSL binaries do not carry numthreads; the shader compiler captures it
        // from slang entry-point reflection
        const auto &refl = shader->GetReflection();
        if (refl.threadGroupSize[0] > 0) {
            threadGroupSize[0] = refl.threadGroupSize[0];
            threadGroupSize[1] = refl.threadGroupSize[1];
            threadGroupSize[2] = refl.threadGroupSize[2];
        } else {
            LOG_W(TAG, "compute shader reflection has no thread group size, fallback to (1,1,1)");
        }

        this->shader = shader;
        pipeline     = nativePipeline;
        return true;
    }

} // namespace sky::aurora