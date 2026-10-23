//
// Aurora Metal ResourceGroup.
//

#include <MetalResourceGroup.h>

#include "MetalDescriptorEncoder.h"
#include "MetalMSLSlots.h"
#include <MetalBuffer.h>
#include <MetalDevice.h>
#include <MetalImage.h>
#include <MetalSampler.h>
#include <MetalShader.h>
#include <core/logger/Logger.h>

#include <algorithm>

#import <Metal/Metal.h>

static const char *TAG = "AuroraMetal";

namespace sky::aurora {

    MetalResourceGroup::MetalResourceGroup(MetalDevice &dev) : device(dev)
    {
    }

    MetalResourceGroup::~MetalResourceGroup() = default;

    bool MetalResourceGroup::Init(const Descriptor &desc)
    {
        if (desc.shader == nullptr || desc.shader->GetDevice() != &device) {
            LOG_E(TAG, "ResourceGroup requires a shader from its device");
            return false;
        }
        if (!InitParameterLayout(desc))
            return false;
        shader   = static_cast<MetalShader *>(desc.shader);
        setIndex = desc.set;

        const auto *plan = shader->GetResourceSetPlan(setIndex);
        if (plan == nullptr) {
            LOG_E(TAG, "ResourceGroup has no linked logical resource set");
            return false;
        }
        stageSlots      = plan->stages;
        dynamicBindings = plan->dynamicBindings;
        return true;
    }

    const Device *MetalResourceGroup::GetDevice() const noexcept
    {
        return &device;
    }

    std::unique_ptr<DescriptorEncoder> MetalResourceGroup::CreateEncoder()
    {
        if (IsParameterSnapshot()) {
            return nullptr;
        }
        return std::make_unique<MetalDescriptorEncoder>(*this);
    }

    MetalDescriptorEncoder::MetalDescriptorEncoder(MetalResourceGroup &resourceGroup)
        : group(&resourceGroup), buffers(resourceGroup.buffers), textures(resourceGroup.textures), samplers(resourceGroup.samplers)
    {
    }
    bool MetalDescriptorEncoder::Fail(const char *message)
    {
        if (error.empty()) {
            error = message;
            LOG_E(TAG, "%s", message);
        }
        return false;
    }
    const ShaderResource *MetalDescriptorEncoder::Find(uint32_t binding, uint32_t arrayElement)
    {
        if (ended || !error.empty()) {
            Fail("Descriptor transaction is failed or ended");
            return nullptr;
        }
        for (const auto &resource : group->shader->GetReflection().resources) {
            if (resource.set == group->setIndex && resource.binding == binding) {
                if (arrayElement || resource.descriptorArray || resource.count != 1) {
                    Fail("Unsupported Metal DirectArguments descriptor array");
                    return nullptr;
                }
                return &resource;
            }
        }
        Fail("Unknown logical Metal descriptor binding");
        return nullptr;
    }
    bool MetalDescriptorEncoder::WriteBuffer(uint32_t binding, Buffer *buffer, uint64_t offset, uint64_t range, uint32_t arrayElement)
    {
        const auto *resource = Find(binding, arrayElement);
        if (resource == nullptr) {
            return false;
        }
        const bool uniform = resource->type == ShaderResourceType::UNIFORM_BUFFER || resource->type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
        const bool storage = resource->type == ShaderResourceType::STORAGE_BUFFER || resource->type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC;
        const bool dynamic =
            resource->type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC || resource->type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC;
        if ((!uniform && !storage) || buffer == nullptr || buffer->GetDevice() != &group->device || offset >= buffer->GetSize() ||
            (dynamic && !range) || (range && range > buffer->GetSize() - offset)) {
            return Fail("Invalid Metal buffer descriptor type, owner or range");
        }
        const auto *native = static_cast<MetalBuffer *>(buffer);
        if (!native->GetNativeHandle() ||
            !native->GetDescriptor().usage.TestBit(uniform ? BufferUsageFlagBit::UNIFORM : BufferUsageFlagBit::STORAGE)) {
            return Fail("Metal buffer descriptor has invalid native storage or usage");
        }
        const auto size = range ? range : buffer->GetSize() - offset;
        if (uniform) {
            const auto alignment = std::max(1u, group->device.GetCapability().minUniformBufferOffsetAlignment);
            if (offset % alignment) {
                return Fail("Metal uniform buffer offset is misaligned");
            }
            for (const auto &block : group->shader->GetReflection().blocks) {
                if (block.set == group->setIndex && block.binding == binding && size < block.size) {
                    return Fail("Metal uniform buffer range is smaller than target block");
                }
            }
        } else {
            const auto stride = resource->bufferKind == ShaderBufferKind::RAW ? 4 : resource->elementStride;
            if (!stride || offset % stride || size % stride) {
                return Fail("Metal storage buffer range does not match reflected stride");
            }
        }
        buffers[binding] = {CounterPtr<MetalBuffer>(static_cast<MetalBuffer *>(buffer)), offset, size};
        return true;
    }
    bool MetalDescriptorEncoder::WriteImage(uint32_t binding, Image *image, ImageLayout layout, uint32_t arrayElement)
    {
        const auto *resource = Find(binding, arrayElement);
        if (resource == nullptr) {
            return false;
        }
        const bool storage = resource->type == ShaderResourceType::STORAGE_IMAGE;
        if ((!storage && resource->type != ShaderResourceType::SAMPLED_IMAGE && resource->type != ShaderResourceType::INPUT_ATTACHMENT) ||
            image == nullptr || image->GetDevice() != &group->device) {
            return Fail("Invalid Metal image descriptor type or owner");
        }
        auto       *native     = static_cast<MetalImage *>(image);
        const auto &descriptor = native->GetDescriptor();
        const auto  usage      = storage                                                  ? ImageUsageFlagBit::STORAGE
                                 : resource->type == ShaderResourceType::INPUT_ATTACHMENT ? ImageUsageFlagBit::INPUT_ATTACHMENT
                                                                                          : ImageUsageFlagBit::SAMPLED;
        if (!descriptor.usage.TestBit(usage) || (storage && layout != ImageLayout::GENERAL) ||
            (!storage && layout != ImageLayout::SHADER_READ_ONLY && layout != ImageLayout::DEPTH_STENCIL_READ_ONLY &&
             layout != ImageLayout::GENERAL)) {
            return Fail("Invalid Metal image descriptor usage or layout");
        }
        id<MTLTexture> texture = (__bridge id<MTLTexture>)native->GetNativeHandle();
        if (!texture) {
            return Fail("Metal image descriptor has no native texture");
        }
        ShaderTextureDimension dimension = ShaderTextureDimension::UNKNOWN;
        bool                   arrayed = false, multisampled = false;
        switch (texture.textureType) {
        case MTLTextureType1D: dimension = ShaderTextureDimension::D1; break;
        case MTLTextureType1DArray:
            dimension = ShaderTextureDimension::D1;
            arrayed   = true;
            break;
        case MTLTextureType2D: dimension = ShaderTextureDimension::D2; break;
        case MTLTextureType2DArray:
            dimension = ShaderTextureDimension::D2;
            arrayed   = true;
            break;
        case MTLTextureType2DMultisample:
            dimension    = ShaderTextureDimension::D2;
            multisampled = true;
            break;
        case MTLTextureType2DMultisampleArray:
            dimension    = ShaderTextureDimension::D2;
            arrayed      = true;
            multisampled = true;
            break;
        case MTLTextureTypeCube: dimension = ShaderTextureDimension::CUBE; break;
        case MTLTextureTypeCubeArray:
            dimension = ShaderTextureDimension::CUBE;
            arrayed   = true;
            break;
        case MTLTextureType3D: dimension = ShaderTextureDimension::D3; break;
        default: break;
        }
        const auto format         = native->GetPixelFormat();
        const bool unsignedFormat = format == PixelFormat::R8_UINT || (format >= PixelFormat::R32_UINT && format <= PixelFormat::RGBA32_UINT);
        if (dimension != resource->texture.dimension || arrayed != resource->texture.arrayed || multisampled != resource->texture.multisampled ||
            resource->texture.scalarType != (unsignedFormat ? ShaderScalarType::UINT : ShaderScalarType::FLOAT)) {
            return Fail("Metal texture shape or scalar type differs from target reflection");
        }
        textures[binding] = native;
        return true;
    }
    bool MetalDescriptorEncoder::WriteSampler(uint32_t binding, Sampler *sampler, uint32_t arrayElement)
    {
        const auto *resource = Find(binding, arrayElement);
        if (resource == nullptr) {
            return false;
        }
        if (resource->type != ShaderResourceType::SAMPLER || resource->sampler != ShaderSamplerType::REGULAR || sampler == nullptr ||
            sampler->GetDevice() != &group->device) {
            return Fail("Invalid Metal sampler type, owner or comparison mode");
        }
        auto *native = static_cast<MetalSampler *>(sampler);
        if (!native->GetNativeHandle()) {
            return Fail("Metal sampler has no native state");
        }
        samplers[binding] = native;
        return true;
    }
    bool MetalDescriptorEncoder::Ready()
    {
        if (ended || !error.empty()) {
            return false;
        }
        for (const auto &resource : group->shader->GetReflection().resources) {
            if (resource.set != group->setIndex) {
                continue;
            }
            if ((IsMetalBufferResource(resource.type) && !buffers.count(resource.binding)) ||
                (IsMetalTextureResource(resource.type) && !textures.count(resource.binding)) ||
                (resource.type == ShaderResourceType::SAMPLER && !samplers.count(resource.binding))) {
                return Fail("Metal descriptor transaction has an unassigned active binding");
            }
        }
        return true;
    }
    void MetalDescriptorEncoder::Commit()
    {
        group->buffers  = std::move(buffers);
        group->textures = std::move(textures);
        group->samplers = std::move(samplers);
        ended           = true;
    }
    bool MetalDescriptorEncoder::End()
    {
        if (!Ready()) {
            return false;
        }
        Commit();
        return true;
    }

    bool MetalResourceGroup::ValidateBindings(uint32_t numDynamicOffsets, const uint32_t *dynamicOffsets) const
    {
        if (numDynamicOffsets != dynamicBindings.size() || (numDynamicOffsets && dynamicOffsets == nullptr)) {
            return false;
        }
        for (const auto &resource : shader->GetReflection().resources) {
            if (resource.set != setIndex) {
                continue;
            }
            if ((IsMetalBufferResource(resource.type) && !buffers.count(resource.binding)) ||
                (IsMetalTextureResource(resource.type) && !textures.count(resource.binding)) ||
                (resource.type == ShaderResourceType::SAMPLER && !samplers.count(resource.binding))) {
                return false;
            }
        }
        for (size_t i = 0; i < dynamicBindings.size(); ++i) {
            const auto found = buffers.find(dynamicBindings[i]);
            if (found == buffers.end()) {
                return false;
            }
            const auto &binding   = found->second;
            const auto  size      = binding.buffer->GetSize();
            const auto  offset    = dynamicOffsets[i];
            uint32_t    alignment = 4;
            for (const auto &resource : shader->GetReflection().resources) {
                if (resource.set == setIndex && resource.binding == dynamicBindings[i]) {
                    alignment = resource.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC
                                    ? std::max(1u, device.GetCapability().minUniformBufferOffsetAlignment)
                                : resource.bufferKind == ShaderBufferKind::RAW ? 4
                                                                               : resource.elementStride;
                    break;
                }
            }
            if (!alignment || offset % alignment || binding.offset > size || offset > size - binding.offset ||
                binding.range > size - binding.offset - offset) {
                return false;
            }
        }
        return true;
    }

    // dynamic bindings are rebound per draw with the caller-provided offsets
    // (stable descriptor + per-draw offset, same contract as the other backends)
    uint64_t MetalResourceGroup::ResolveOffset(
        uint32_t binding, uint64_t recordedOffset, uint32_t numDynamicOffsets, const uint32_t *dynamicOffsets, uint32_t &dynamicCursor) const
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
        if (!ValidateBindings(numDynamicOffsets, dynamicOffsets)) {
            LOG_E(TAG, "Invalid Metal resource group dynamic offsets");
            return;
        }
        auto    *enc           = (__bridge id<MTLRenderCommandEncoder>)encoder;
        uint32_t dynamicCursor = 0;
        // buffers must be visited in ascending binding order so dynamic
        // offsets line up with the sorted dynamicBindings table
        std::vector<std::pair<uint32_t, const BufferBinding *>> ordered;
        ordered.reserve(buffers.size());
        for (const auto &[binding, slot] : buffers) {
            ordered.emplace_back(binding, &slot);
        }
        std::sort(ordered.begin(), ordered.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
        for (const auto &[binding, slot] : ordered) {
            auto      *buf    = (__bridge id<MTLBuffer>)slot->buffer->GetNativeHandle();
            const auto offset = (NSUInteger)ResolveOffset(binding, slot->offset, numDynamicOffsets, dynamicOffsets, dynamicCursor);
            if (const auto found = stageSlots[0].buffer.find(binding); found != stageSlots[0].buffer.end())
                [enc setVertexBuffer:buf offset:offset atIndex:found->second];
            if (const auto found = stageSlots[1].buffer.find(binding); found != stageSlots[1].buffer.end())
                [enc setFragmentBuffer:buf offset:offset atIndex:found->second];
        }
        for (const auto &[binding, image] : textures) {
            auto *tex = (__bridge id<MTLTexture>)image->GetNativeHandle();
            if (const auto found = stageSlots[0].texture.find(binding); found != stageSlots[0].texture.end())
                [enc setVertexTexture:tex atIndex:found->second];
            if (const auto found = stageSlots[1].texture.find(binding); found != stageSlots[1].texture.end())
                [enc setFragmentTexture:tex atIndex:found->second];
        }
        for (const auto &[binding, sampler] : samplers) {
            auto *smp = (__bridge id<MTLSamplerState>)sampler->GetNativeHandle();
            if (const auto found = stageSlots[0].sampler.find(binding); found != stageSlots[0].sampler.end())
                [enc setVertexSamplerState:smp atIndex:found->second];
            if (const auto found = stageSlots[1].sampler.find(binding); found != stageSlots[1].sampler.end())
                [enc setFragmentSamplerState:smp atIndex:found->second];
        }
    }

    void MetalResourceGroup::BindCompute(void *encoder, uint32_t numDynamicOffsets, const uint32_t *dynamicOffsets) const
    {
        if (!ValidateBindings(numDynamicOffsets, dynamicOffsets)) {
            LOG_E(TAG, "Invalid Metal resource group dynamic offsets");
            return;
        }
        auto                                                   *enc           = (__bridge id<MTLComputeCommandEncoder>)encoder;
        uint32_t                                                dynamicCursor = 0;
        std::vector<std::pair<uint32_t, const BufferBinding *>> ordered;
        ordered.reserve(buffers.size());
        for (const auto &[binding, slot] : buffers) {
            ordered.emplace_back(binding, &slot);
        }
        std::sort(ordered.begin(), ordered.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
        for (const auto &[binding, slot] : ordered) {
            auto      *buf    = (__bridge id<MTLBuffer>)slot->buffer->GetNativeHandle();
            const auto offset = (NSUInteger)ResolveOffset(binding, slot->offset, numDynamicOffsets, dynamicOffsets, dynamicCursor);
            if (const auto found = stageSlots[2].buffer.find(binding); found != stageSlots[2].buffer.end())
                [enc setBuffer:buf offset:offset atIndex:found->second];
        }
        for (const auto &[binding, image] : textures) {
            if (const auto found = stageSlots[2].texture.find(binding); found != stageSlots[2].texture.end())
                [enc setTexture:(__bridge id<MTLTexture>)image->GetNativeHandle() atIndex:found->second];
        }
        for (const auto &[binding, sampler] : samplers) {
            if (const auto found = stageSlots[2].sampler.find(binding); found != stageSlots[2].sampler.end())
                [enc setSamplerState:(__bridge id<MTLSamplerState>)sampler->GetNativeHandle() atIndex:found->second];
        }
    }

} // namespace sky::aurora
