//
// Aurora Metal ResourceGroup.
//

#include <MetalResourceGroup.h>
#include <MetalDescriptorEncoder.h>
#include <MetalBuffer.h>
#include <MetalImage.h>
#include <MetalSampler.h>
#include <MetalShader.h>
#include <core/logger/Logger.h>

#import <Metal/Metal.h>

static const char *TAG = "AuroraMetal";

namespace sky::aurora {

    MetalResourceGroup::MetalResourceGroup(MetalDevice &dev)
        : device(dev)
    {
    }

    bool MetalResourceGroup::Init(const Descriptor &desc)
    {
        if (desc.shader == nullptr) {
            LOG_E(TAG, "ResourceGroup requires a non-null shader");
            return false;
        }
        shader   = static_cast<MetalShader *>(desc.shader);
        setIndex = desc.set;
        return true;
    }

    std::unique_ptr<DescriptorEncoder> MetalResourceGroup::CreateEncoder()
    {
        return std::make_unique<MetalDescriptorEncoder>(*this);
    }

    void MetalResourceGroup::WriteBuffer(uint32_t binding, MetalBuffer *buffer, uint64_t offset)
    {
        auto &slot  = buffers[binding];
        slot.buffer = buffer;
        slot.offset = offset;
    }

    void MetalResourceGroup::WriteImage(uint32_t binding, MetalImage *image)
    {
        textures[binding] = image;
    }

    void MetalResourceGroup::WriteSampler(uint32_t binding, MetalSampler *sampler)
    {
        samplers[binding] = sampler;
    }

    void MetalResourceGroup::BindGraphics(void *encoder) const
    {
        auto *enc = (__bridge id<MTLRenderCommandEncoder>)encoder;
        for (const auto &[binding, slot] : buffers) {
            auto *buf = (__bridge id<MTLBuffer>)slot.buffer->GetNativeHandle();
            [enc setVertexBuffer:buf offset:(NSUInteger)slot.offset atIndex:binding];
            [enc setFragmentBuffer:buf offset:(NSUInteger)slot.offset atIndex:binding];
        }
        for (const auto &[binding, image] : textures) {
            auto *tex = (__bridge id<MTLTexture>)image->GetNativeHandle();
            [enc setVertexTexture:tex atIndex:binding];
            [enc setFragmentTexture:tex atIndex:binding];
        }
        for (const auto &[binding, sampler] : samplers) {
            auto *smp = (__bridge id<MTLSamplerState>)sampler->GetNativeHandle();
            [enc setVertexSamplerState:smp atIndex:binding];
            [enc setFragmentSamplerState:smp atIndex:binding];
        }
    }

    void MetalResourceGroup::BindCompute(void *encoder) const
    {
        auto *enc = (__bridge id<MTLComputeCommandEncoder>)encoder;
        for (const auto &[binding, slot] : buffers) {
            auto *buf = (__bridge id<MTLBuffer>)slot.buffer->GetNativeHandle();
            [enc setBuffer:buf offset:(NSUInteger)slot.offset atIndex:binding];
        }
        for (const auto &[binding, image] : textures) {
            [enc setTexture:(__bridge id<MTLTexture>)image->GetNativeHandle() atIndex:binding];
        }
        for (const auto &[binding, sampler] : samplers) {
            [enc setSamplerState:(__bridge id<MTLSamplerState>)sampler->GetNativeHandle() atIndex:binding];
        }
    }

} // namespace sky::aurora
