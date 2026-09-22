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

#include <algorithm>

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

        for (const auto &res : shader->GetReflection().resources) {
            if (res.set != setIndex) {
                continue;
            }
            if (res.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC ||
                res.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC) {
                dynamicBindings.push_back(res.binding);
            }
        }
        std::sort(dynamicBindings.begin(), dynamicBindings.end());
        return true;
    }

    std::unique_ptr<DescriptorEncoder> MetalResourceGroup::CreateEncoder()
    {
        return std::make_unique<MetalDescriptorEncoder>(*this);
    }

    void MetalResourceGroup::WriteBuffer(uint32_t binding, MetalBuffer *buffer, uint64_t offset, uint32_t arrayElement)
    {
        auto &slot  = buffers[binding + arrayElement];
        slot.buffer = buffer;
        slot.offset = offset;
    }

    void MetalResourceGroup::WriteImage(uint32_t binding, MetalImage *image, uint32_t arrayElement)
    {
        textures[binding + arrayElement] = image;
    }

    void MetalResourceGroup::WriteSampler(uint32_t binding, MetalSampler *sampler, uint32_t arrayElement)
    {
        samplers[binding + arrayElement] = sampler;
    }

    // dynamic bindings are rebound per draw with the caller-provided offsets
    // (stable descriptor + per-draw offset, same contract as the other backends)
    uint64_t MetalResourceGroup::ResolveOffset(uint32_t binding, uint64_t recordedOffset,
                                               uint32_t numDynamicOffsets, const uint32_t *dynamicOffsets,
                                               uint32_t &dynamicCursor) const
    {
        if (dynamicCursor < dynamicBindings.size() && dynamicBindings[dynamicCursor] == binding) {
            ++dynamicCursor;
            const uint32_t idx = dynamicCursor - 1;
            if (idx < numDynamicOffsets && dynamicOffsets != nullptr) {
                return recordedOffset + dynamicOffsets[idx];
            }
            LOG_W(TAG, "missing dynamic offset for binding %u", binding);
        }
        return recordedOffset;
    }

    void MetalResourceGroup::BindGraphics(void *encoder, uint32_t numDynamicOffsets, const uint32_t *dynamicOffsets) const
    {
        auto *enc = (__bridge id<MTLRenderCommandEncoder>)encoder;
        uint32_t dynamicCursor = 0;
        // buffers must be visited in ascending binding order so dynamic
        // offsets line up with the sorted dynamicBindings table
        std::vector<std::pair<uint32_t, const BufferBinding *>> ordered;
        ordered.reserve(buffers.size());
        for (const auto &[binding, slot] : buffers) {
            ordered.emplace_back(binding, &slot);
        }
        std::sort(ordered.begin(), ordered.end(),
                  [](const auto &a, const auto &b) { return a.first < b.first; });
        for (const auto &[binding, slot] : ordered) {
            auto *buf = (__bridge id<MTLBuffer>)slot->buffer->GetNativeHandle();
            const auto offset = (NSUInteger)ResolveOffset(binding, slot->offset,
                                                          numDynamicOffsets, dynamicOffsets, dynamicCursor);
            [enc setVertexBuffer:buf offset:offset atIndex:binding];
            [enc setFragmentBuffer:buf offset:offset atIndex:binding];
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

    void MetalResourceGroup::BindCompute(void *encoder, uint32_t numDynamicOffsets, const uint32_t *dynamicOffsets) const
    {
        auto *enc = (__bridge id<MTLComputeCommandEncoder>)encoder;
        uint32_t dynamicCursor = 0;
        std::vector<std::pair<uint32_t, const BufferBinding *>> ordered;
        ordered.reserve(buffers.size());
        for (const auto &[binding, slot] : buffers) {
            ordered.emplace_back(binding, &slot);
        }
        std::sort(ordered.begin(), ordered.end(),
                  [](const auto &a, const auto &b) { return a.first < b.first; });
        for (const auto &[binding, slot] : ordered) {
            auto *buf = (__bridge id<MTLBuffer>)slot->buffer->GetNativeHandle();
            const auto offset = (NSUInteger)ResolveOffset(binding, slot->offset,
                                                          numDynamicOffsets, dynamicOffsets, dynamicCursor);
            [enc setBuffer:buf offset:offset atIndex:binding];
        }
        for (const auto &[binding, image] : textures) {
            [enc setTexture:(__bridge id<MTLTexture>)image->GetNativeHandle() atIndex:binding];
        }
        for (const auto &[binding, sampler] : samplers) {
            [enc setSamplerState:(__bridge id<MTLSamplerState>)sampler->GetNativeHandle() atIndex:binding];
        }
    }

} // namespace sky::aurora
