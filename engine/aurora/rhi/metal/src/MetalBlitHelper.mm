//
// Metal blit/resolve helper.
//

#import <Metal/Metal.h>

#include "MetalBlitHelper.h"
#include "MetalDevice.h"
#include "MetalImage.h"
#include "MetalUtils.h"
#include <core/logger/Logger.h>

static const char *TAG = "AuroraMetal";

namespace sky::aurora {

    namespace {
        // fullscreen triangle; rect uniforms in texel space, normalized in-shader
        const char *BLIT_SHADER_SOURCE = R"(
#include <metal_stdlib>
using namespace metal;

struct BlitRects {
    float4 srcRect; // texel space: xy offset, zw size
    float4 dstRect;
    float2 srcSize;
    float2 dstSize;
};

struct VSOut {
    float4 pos [[position]];
    float2 uv;
};

vertex VSOut blitVS(uint vid [[vertex_id]], constant BlitRects &r [[buffer(0)]]) {
    float2 corner = float2((vid << 1) & 2, vid & 2); // (0,0) (2,0) (0,2)
    VSOut o;
    float2 dstPx = r.dstRect.xy + corner * 0.5 * r.dstRect.zw;
    o.pos = float4(dstPx / r.dstSize * 2.0 - 1.0, 0.0, 1.0);
    o.uv  = (r.srcRect.xy + corner * 0.5 * r.srcRect.zw) / r.srcSize;
    return o;
}

fragment float4 blitFS(VSOut in [[stage_in]],
                       texture2d<float> tex [[texture(0)]],
                       sampler smp [[sampler(0)]]) {
    return tex.sample(smp, in.uv);
}
)";

        struct BlitRects {
            float srcRect[4];
            float dstRect[4];
            float srcSize[2];
            float dstSize[2];
        };
    } // namespace

    MetalBlitHelper::MetalBlitHelper(MetalDevice &dev) : device(dev)
    {
    }

    MetalBlitHelper::~MetalBlitHelper()
    {
        for (auto &[key, pso] : pipelines) {
            [(id<MTLRenderPipelineState>)pso release];
        }
        pipelines.clear();
        if (linearSampler != nullptr) {
            [(id<MTLSamplerState>)linearSampler release];
        }
        if (nearestSampler != nullptr) {
            [(id<MTLSamplerState>)nearestSampler release];
        }
    }

    void *MetalBlitHelper::GetSampler(bool linear)
    {
        void *&slot = linear ? linearSampler : nearestSampler;
        if (slot == nullptr) {
            auto *desc        = [[MTLSamplerDescriptor alloc] init];
            desc.minFilter    = linear ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
            desc.magFilter    = desc.minFilter;
            desc.sAddressMode = MTLSamplerAddressModeClampToEdge;
            desc.tAddressMode = MTLSamplerAddressModeClampToEdge;
            slot              = (__bridge_retained void *)[(id<MTLDevice>)device.GetNativeDevice() newSamplerStateWithDescriptor:desc];
            [desc release];
        }
        return slot;
    }

    void *MetalBlitHelper::GetBlitPipeline(uint32_t mtlFormat, uint32_t sampleCount)
    {
        const uint64_t key = (uint64_t(mtlFormat) << 8) | sampleCount;
        auto           it  = pipelines.find(key);
        if (it != pipelines.end()) {
            return it->second;
        }

        auto          *mtlDevice = (id<MTLDevice>)device.GetNativeDevice();
        NSError       *error     = nil;
        NSString      *source    = [NSString stringWithUTF8String:BLIT_SHADER_SOURCE];
        id<MTLLibrary> library   = [mtlDevice newLibraryWithSource:source options:nil error:&error];
        if (library == nil) {
            LOG_E(TAG, "blit shader compile failed: %s", error != nil ? [[error localizedDescription] UTF8String] : "unknown");
            return nullptr;
        }

        auto *desc                           = [[MTLRenderPipelineDescriptor alloc] init];
        desc.vertexFunction                  = [library newFunctionWithName:@"blitVS"];
        desc.fragmentFunction                = [library newFunctionWithName:@"blitFS"];
        desc.colorAttachments[0].pixelFormat = static_cast<MTLPixelFormat>(mtlFormat);
        desc.rasterSampleCount               = sampleCount;
        id<MTLRenderPipelineState> pso       = [mtlDevice newRenderPipelineStateWithDescriptor:desc error:&error];
        [desc release];
        [library release];
        if (pso == nil) {
            LOG_E(TAG, "blit pipeline creation failed: %s", error != nil ? [[error localizedDescription] UTF8String] : "unknown");
            return nullptr;
        }

        void *handle = (__bridge_retained void *)pso;
        pipelines.emplace(key, handle);
        return handle;
    }

    bool MetalBlitHelper::Blit(void *commandBuffer, MetalImage *src, MetalImage *dst, const std::vector<BlitInfo> &regions, Filter filter)
    {
        auto               *cb      = (__bridge id<MTLCommandBuffer>)commandBuffer;
        auto               *srcTex  = (__bridge id<MTLTexture>)src->GetNativeHandle();
        auto               *dstTex  = (__bridge id<MTLTexture>)dst->GetNativeHandle();
        id<MTLSamplerState> sampler = (__bridge id<MTLSamplerState>)GetSampler(filter == Filter::LINEAR);

        for (const auto &region : regions) {
            const uint32_t layers = std::max(region.dstRange.layers, 1u);
            for (uint32_t i = 0; i < layers; ++i) {
                const uint32_t dstLevel = region.dstRange.level;
                const uint32_t dstSlice = region.dstRange.baseLayer + i;
                const uint32_t srcLevel = region.srcRange.level;
                const uint32_t srcSlice = region.srcRange.baseLayer + (region.srcRange.layers > 1 ? i : 0);

                auto *pso = (__bridge id<MTLRenderPipelineState>)GetBlitPipeline(static_cast<uint32_t>(dstTex.pixelFormat),
                                                                                 static_cast<uint32_t>(dstTex.sampleCount));
                if (pso == nil) {
                    return false;
                }

                // mip-level dimensions for the rect normalization
                const float srcW = float(std::max(srcTex.width >> srcLevel, 1ul));
                const float srcH = float(std::max(srcTex.height >> srcLevel, 1ul));
                const float dstW = float(std::max(dstTex.width >> dstLevel, 1ul));
                const float dstH = float(std::max(dstTex.height >> dstLevel, 1ul));

                BlitRects rects  = {};
                rects.srcRect[0] = float(region.srcOffsets[0].x);
                rects.srcRect[1] = float(region.srcOffsets[0].y);
                rects.srcRect[2] = float(region.srcOffsets[1].x - region.srcOffsets[0].x);
                rects.srcRect[3] = float(region.srcOffsets[1].y - region.srcOffsets[0].y);
                rects.dstRect[0] = float(region.dstOffsets[0].x);
                rects.dstRect[1] = float(region.dstOffsets[0].y);
                rects.dstRect[2] = float(region.dstOffsets[1].x - region.dstOffsets[0].x);
                rects.dstRect[3] = float(region.dstOffsets[1].y - region.dstOffsets[0].y);
                rects.srcSize[0] = srcW;
                rects.srcSize[1] = srcH;
                rects.dstSize[0] = dstW;
                rects.dstSize[1] = dstH;

                auto *rpDesc                           = [MTLRenderPassDescriptor renderPassDescriptor];
                rpDesc.colorAttachments[0].texture     = dstTex;
                rpDesc.colorAttachments[0].level       = dstLevel;
                rpDesc.colorAttachments[0].slice       = dstSlice;
                rpDesc.colorAttachments[0].loadAction  = MTLLoadActionDontCare;
                rpDesc.colorAttachments[0].storeAction = MTLStoreActionStore;

                // sample through a single-level/single-slice view so the
                // shader always reads mip 0 of the view
                id<MTLTexture>              srcView = [srcTex newTextureViewWithPixelFormat:srcTex.pixelFormat
                                                                   textureType:MTLTextureType2D
                                                                        levels:NSMakeRange(srcLevel, 1)
                                                                        slices:NSMakeRange(srcSlice, 1)];
                id<MTLRenderCommandEncoder> enc     = [cb renderCommandEncoderWithDescriptor:rpDesc];
                if (enc == nil) {
                    [srcView release];
                    LOG_E(TAG, "blit: failed to create render encoder (dst not renderable?)");
                    return false;
                }
                [enc setRenderPipelineState:pso];
                [enc setVertexBytes:&rects length:sizeof(rects) atIndex:0];
                [enc setFragmentTexture:srcView atIndex:0];
                [enc setFragmentSamplerState:sampler atIndex:0];
                MTLViewport vp = {rects.dstRect[0], rects.dstRect[1], rects.dstRect[2], rects.dstRect[3], 0.0, 1.0};
                [enc setViewport:vp];
                MTLScissorRect sc = {uint32_t(region.dstOffsets[0].x), uint32_t(region.dstOffsets[0].y), uint32_t(rects.dstRect[2]),
                                     uint32_t(rects.dstRect[3])};
                [enc setScissorRect:sc];
                [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
                [enc endEncoding];
                [srcView release];
            }
        }
        return true;
    }

    void MetalBlitHelper::Resolve(void *commandBuffer, MetalImage *src, MetalImage *dst, const std::vector<ResolveInfo> &regions)
    {
        auto *cb     = (__bridge id<MTLCommandBuffer>)commandBuffer;
        auto *srcTex = (__bridge id<MTLTexture>)src->GetNativeHandle();
        auto *dstTex = (__bridge id<MTLTexture>)dst->GetNativeHandle();

        for (const auto &region : regions) {
            const uint32_t layers = std::max(region.dstRange.layers, 1u);
            for (uint32_t i = 0; i < layers; ++i) {
                // an empty pass with a multisample-resolve store action still
                // performs the resolve at endEncoding
                auto *rpDesc                              = [MTLRenderPassDescriptor renderPassDescriptor];
                rpDesc.colorAttachments[0].texture        = srcTex;
                rpDesc.colorAttachments[0].level          = region.srcRange.level;
                rpDesc.colorAttachments[0].slice          = region.srcRange.baseLayer + i;
                rpDesc.colorAttachments[0].loadAction     = MTLLoadActionLoad;
                rpDesc.colorAttachments[0].resolveTexture = dstTex;
                rpDesc.colorAttachments[0].resolveLevel   = region.dstRange.level;
                rpDesc.colorAttachments[0].resolveSlice   = region.dstRange.baseLayer + i;
                rpDesc.colorAttachments[0].storeAction    = MTLStoreActionMultisampleResolve;

                id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:rpDesc];
                [enc endEncoding];
            }
        }
    }

} // namespace sky::aurora
