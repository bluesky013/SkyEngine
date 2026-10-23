#include <VulkanDescriptorEncoder.h>

#include <VulkanBuffer.h>
#include <VulkanConversion.h>
#include <VulkanDevice.h>
#include <VulkanImage.h>
#include <VulkanSampler.h>
#include <core/logger/Logger.h>

namespace sky::aurora {
    VulkanDescriptorEncoder::VulkanDescriptorEncoder(VulkanResourceGroup &resourceGroup)
        : group(&resourceGroup)
        , infos(resourceGroup.mWriteInfos)
        , assigned(resourceGroup.mAssigned)
        , buffers(resourceGroup.mBuffers)
        , images(resourceGroup.mImages)
        , samplers(resourceGroup.mSamplers)
    {
    }
    bool VulkanDescriptorEncoder::Fail(const char *message)
    {
        if (error.empty()) {
            error = message;
            LOG_E("AuroraVulkan", "%s", message);
        }
        return false;
    }
    const VulkanPlannedBinding *VulkanDescriptorEncoder::Find(uint32_t binding, uint32_t element)
    {
        if (!error.empty() || ended) {
            Fail("Descriptor transaction is failed or already ended");
            return nullptr;
        }
        for (const auto &candidate : group->mBindings) {
            if (candidate.logicalBinding == binding && element < candidate.count) {
                return &candidate;
            }
        }
        Fail("Unknown logical descriptor binding or array element");
        return nullptr;
    }
    bool VulkanDescriptorEncoder::WriteBuffer(uint32_t binding, Buffer *buffer, uint64_t offset, uint64_t range, uint32_t element)
    {
        const auto *plan = Find(binding, element);
        if (plan == nullptr) {
            return false;
        }
        const bool uniform = plan->type == ShaderResourceType::UNIFORM_BUFFER || plan->type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
        const bool storage = plan->type == ShaderResourceType::STORAGE_BUFFER || plan->type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC;
        const bool dynamic = plan->type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC || plan->type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC;
        if ((!uniform && !storage) || buffer == nullptr || buffer->GetDevice() != group->device || offset >= buffer->GetSize() ||
            (dynamic && !range) || (range && range > buffer->GetSize() - offset)) {
            return Fail("Invalid buffer descriptor type, owner, offset or range");
        }
        const auto *native    = static_cast<VulkanBuffer *>(buffer);
        const auto &limits    = group->device->GetLimits();
        const auto  alignment = uniform ? limits.minUniformBufferOffsetAlignment : limits.minStorageBufferOffsetAlignment;
        const auto  maximum   = uniform ? limits.maxUniformBufferRange : limits.maxStorageBufferRange;
        const auto  size      = range ? range : buffer->GetSize() - offset;
        if ((alignment && offset % alignment) || size > maximum ||
            !native->GetDescriptor().usage.TestBit(uniform ? BufferUsageFlagBit::UNIFORM : BufferUsageFlagBit::STORAGE)) {
            return Fail("Buffer descriptor usage, alignment or range exceeds target limits");
        }
        for (const auto &resource : group->shader->GetReflection().resources) {
            if (resource.set != group->mSetIndex || resource.binding != binding) {
                continue;
            }
            if (storage) {
                const auto stride = resource.bufferKind == ShaderBufferKind::RAW ? 4 : resource.elementStride;
                if (!stride || offset % stride || size % stride) {
                    return Fail("Vulkan storage buffer range differs from reflected stride");
                }
            } else {
                for (const auto &block : group->shader->GetReflection().blocks) {
                    if (block.set == group->mSetIndex && block.binding == binding && size < block.size) {
                        return Fail("Vulkan uniform buffer range is smaller than target block");
                    }
                }
            }
            break;
        }
        const auto slot    = plan->slotBase + element;
        infos[slot].buffer = {native->GetNativeHandle(), offset, size};
        assigned[slot]     = true;
        buffers[slot]      = buffer;
        return true;
    }
    bool VulkanDescriptorEncoder::WriteImage(uint32_t binding, Image *image, ImageLayout layout, uint32_t element)
    {
        const auto *plan = Find(binding, element);
        if (plan == nullptr) {
            return false;
        }
        if ((plan->type != ShaderResourceType::SAMPLED_IMAGE && plan->type != ShaderResourceType::STORAGE_IMAGE &&
             plan->type != ShaderResourceType::INPUT_ATTACHMENT) ||
            image == nullptr || image->GetDevice() != group->device) {
            return Fail("Invalid image descriptor type or owner");
        }
        const auto *native = static_cast<VulkanImage *>(image);
        const auto  usage  = plan->type == ShaderResourceType::SAMPLED_IMAGE   ? ImageUsageFlagBit::SAMPLED
                             : plan->type == ShaderResourceType::STORAGE_IMAGE ? ImageUsageFlagBit::STORAGE
                                                                               : ImageUsageFlagBit::INPUT_ATTACHMENT;
        if (native->GetDefaultView() == VK_NULL_HANDLE || !native->GetDescriptor().usage.TestBit(usage) ||
            (plan->type == ShaderResourceType::STORAGE_IMAGE && layout != ImageLayout::GENERAL) ||
            (layout != ImageLayout::GENERAL && layout != ImageLayout::SHADER_READ_ONLY && layout != ImageLayout::DEPTH_STENCIL_READ_ONLY)) {
            return Fail("Invalid image descriptor usage, view or layout");
        }
        const auto &descriptor = native->GetDescriptor();
        const bool  cube = descriptor.imageType == ImageType::IMAGE_2D && descriptor.viewUsage.TestBit(ImageViewUsageFlagBit::CUBE_MAP_COMPATIBLE);
        const auto  dimension      = cube                                          ? ShaderTextureDimension::CUBE
                                     : descriptor.imageType == ImageType::IMAGE_1D ? ShaderTextureDimension::D1
                                     : descriptor.imageType == ImageType::IMAGE_2D ? ShaderTextureDimension::D2
                                                                                   : ShaderTextureDimension::D3;
        const bool  arrayed        = cube ? descriptor.arrayLayers > 6 : descriptor.imageType != ImageType::IMAGE_3D && descriptor.arrayLayers > 1;
        const auto  format         = descriptor.format;
        const bool  unsignedFormat = format == PixelFormat::R8_UINT || (format >= PixelFormat::R32_UINT && format <= PixelFormat::RGBA32_UINT);
        for (const auto &resource : group->shader->GetReflection().resources) {
            if (resource.set != group->mSetIndex || resource.binding != binding) {
                continue;
            }
            if (resource.texture.dimension != dimension || resource.texture.arrayed != arrayed ||
                resource.texture.multisampled != (descriptor.samples != SampleCount::X1) ||
                resource.texture.scalarType != (unsignedFormat ? ShaderScalarType::UINT : ShaderScalarType::FLOAT)) {
                return Fail("Vulkan texture shape or scalar type differs from target reflection");
            }
            break;
        }
        const auto slot   = plan->slotBase + element;
        infos[slot].image = {VK_NULL_HANDLE, native->GetDefaultView(), FromImageLayout(layout)};
        assigned[slot]    = true;
        images[slot]      = image;
        return true;
    }
    bool VulkanDescriptorEncoder::WriteSampler(uint32_t binding, Sampler *sampler, uint32_t element)
    {
        const auto *plan = Find(binding, element);
        if (plan == nullptr) {
            return false;
        }
        if (plan->type != ShaderResourceType::SAMPLER || sampler == nullptr || sampler->GetDevice() != group->device) {
            return Fail("Invalid sampler descriptor type or owner");
        }
        for (const auto &resource : group->shader->GetReflection().resources) {
            if (resource.set == group->mSetIndex && resource.binding == binding && resource.sampler != ShaderSamplerType::REGULAR) {
                return Fail("Vulkan comparison sampler state is unsupported by this sampler contract");
            }
        }
        const auto slot   = plan->slotBase + element;
        infos[slot].image = {static_cast<VulkanSampler *>(sampler)->GetNativeHandle(), VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};
        assigned[slot]    = true;
        samplers[slot]    = sampler;
        return true;
    }
    bool VulkanDescriptorEncoder::Ready()
    {
        if (ended || !error.empty()) {
            return false;
        }
        for (const auto value : assigned) {
            if (!value) {
                return Fail("Descriptor transaction has unassigned active elements");
            }
        }
        return true;
    }
    void VulkanDescriptorEncoder::AppendWrites(std::vector<VkWriteDescriptorSet> &writes)
    {
        for (const auto &set : group->mSets) {
            for (const auto &binding : group->shader->GetBindingPlan().sets.at(set.index).bindings) {
                VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                write.dstSet          = set.handle;
                write.dstBinding      = binding.physicalBinding;
                write.descriptorCount = binding.count;
                write.descriptorType  = FromShaderResourceType(binding.type);
                const bool isBuffer =
                    binding.type == ShaderResourceType::UNIFORM_BUFFER || binding.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC ||
                    binding.type == ShaderResourceType::STORAGE_BUFFER || binding.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC;
                if (isBuffer) {
                    bufferWrites.emplace_back();
                    auto &data = bufferWrites.back();
                    for (uint32_t i = 0; i < binding.count; ++i) {
                        data.push_back(infos[binding.slotBase + i].buffer);
                    }
                    write.pBufferInfo = data.data();
                } else {
                    imageWrites.emplace_back();
                    auto &data = imageWrites.back();
                    for (uint32_t i = 0; i < binding.count; ++i) {
                        data.push_back(infos[binding.slotBase + i].image);
                    }
                    write.pImageInfo = data.data();
                }
                writes.push_back(write);
            }
        }
    }
    void VulkanDescriptorEncoder::Commit()
    {
        group->mWriteInfos = std::move(infos);
        group->mAssigned   = std::move(assigned);
        group->mBuffers    = std::move(buffers);
        group->mImages     = std::move(images);
        group->mSamplers   = std::move(samplers);
        ended              = true;
    }
    bool VulkanDescriptorEncoder::End()
    {
        if (!Ready()) {
            return false;
        }
        const auto                       &fn = group->device->GetDeviceFn();
        std::vector<VkWriteDescriptorSet> fallback;
        for (const auto &set : group->mSets) {
            const auto update = group->shader->GetDescriptorUpdateTemplate(set.index);
            if (update == VK_NULL_HANDLE) {
                AppendWrites(fallback);
                break;
            }
        }
        if (!fallback.empty()) {
            fn.vkUpdateDescriptorSets(group->device->GetNativeHandle(), static_cast<uint32_t>(fallback.size()), fallback.data(), 0, nullptr);
        } else {
            for (const auto &set : group->mSets) {
                fn.vkUpdateDescriptorSetWithTemplate(group->device->GetNativeHandle(), set.handle,
                                                     group->shader->GetDescriptorUpdateTemplate(set.index), infos.data());
            }
        }
        Commit();
        return true;
    }
} // namespace sky::aurora
