//
// Created on 2026/09/14.
//

#include <D3D12DescriptorEncoder.h>

#include <D3D12Buffer.h>
#include <D3D12Device.h>
#include <D3D12Image.h>
#include <D3D12ResourceGroup.h>
#include <D3D12Sampler.h>
#include <aurora/rhi/Core.h>
#include <core/logger/Logger.h>

#include <algorithm>

namespace sky::aurora {

    D3D12DescriptorEncoder::D3D12DescriptorEncoder(D3D12ResourceGroup &rg) : group(&rg)
    {
    }

    bool D3D12DescriptorEncoder::Fail(const char *message)
    {
        LOG_E("AuroraDX12", "%s", message);
        failed = true;
        return false;
    }

    uint32_t D3D12DescriptorEncoder::FindBinding(uint32_t binding) const
    {
        for (uint32_t i = 0; i < group->setResources.size(); ++i)
            if (group->setResources[i].binding == binding)
                return i;
        return INVALID_INDEX;
    }

    bool D3D12DescriptorEncoder::WriteBuffer(uint32_t binding, Buffer *buffer, uint64_t offset, uint64_t range, uint32_t arrayElement)
    {
        if (failed)
            return false;
        auto      *allocator = group->device->GetDescriptorAllocator();
        const auto index     = FindBinding(binding);
        if (!allocator || index == INVALID_INDEX || !buffer || buffer->GetDevice() != group->device)
            return Fail("Buffer binding is missing or belongs to another device");
        const auto &res     = group->setResources[index];
        const bool  dynamic = res.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC || res.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC;
        if (arrayElement >= res.count || offset >= buffer->GetSize())
            return Fail("Buffer descriptor element or offset is invalid");
        if (!range && !dynamic)
            range = buffer->GetSize() - offset;
        if (!range || range > buffer->GetSize() - offset)
            return Fail("Buffer descriptor range is invalid");
        auto      *buf     = static_cast<D3D12Buffer *>(buffer);
        const bool storage = res.type == ShaderResourceType::STORAGE_BUFFER || res.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC;
        if (storage) {
            if (const auto error = buf->CheckStorageRange(res, offset, range))
                return Fail(error->message.c_str());
        } else if ((res.type != ShaderResourceType::UNIFORM_BUFFER && res.type != ShaderResourceType::UNIFORM_BUFFER_DYNAMIC) ||
                   res.access != ShaderResourceAccess::READ_ONLY || !buf->GetDescriptor().usage.TestBit(BufferUsageFlagBit::UNIFORM) ||
                   buf->GetDescriptor().memory == MemoryType::GPU_TO_CPU || offset % 256 || range > 65536 ||
                   ((range + 255) & ~uint64_t{255}) > buf->GetSize() - offset) {
            return Fail("Uniform buffer descriptor type, alignment, usage or range is invalid");
        }
        if (!storage) {
            const auto &blocks = group->GetShader()->GetReflection().blocks;
            const auto  block =
                std::ranges::find_if(blocks, [&](const auto &layout) { return layout.set == group->GetSet() && layout.binding == binding; });
            if (block == blocks.end() || range < block->size)
                return Fail("Uniform buffer logical range is smaller than its target block");
        }
        if (dynamic) {
            for (auto &dynamic : group->dynamicBindings) {
                if (dynamic.binding == res.binding) {
                    dynamic.buffer     = CounterPtr<D3D12Buffer>(buf);
                    dynamic.baseOffset = offset;
                    dynamic.range      = range;
                    return true;
                }
            }
            return Fail("Dynamic buffer is absent from its root plan");
        }
        const auto handle = allocator->GetCbvSrvUavCpuHandle(group->allocation.cbvSrvUavFirst + group->cbvSrvUavOffsets[index] + arrayElement);
        if (storage) {
            if (const auto error = buf->CreateStorageView(handle, res, offset, range))
                return Fail(error->message.c_str());
        } else {
            buf->CreateCBV(handle, offset, range);
        }
        group->pins[{binding, arrayElement}] = CounterPtr<RefObject>(buffer);
        group->mDirty                        = true;
        return true;
    }

    bool D3D12DescriptorEncoder::WriteImage(uint32_t binding, Image *image, ImageLayout layout, uint32_t arrayElement)
    {
        if (failed)
            return false;
        auto      *allocator = group->device->GetDescriptorAllocator();
        const auto index     = FindBinding(binding);
        if (!allocator || index == INVALID_INDEX || !image || image->GetDevice() != group->device)
            return Fail("Image binding is missing or belongs to another device");
        const auto &res = group->setResources[index];
        if (arrayElement >= res.count || (res.type != ShaderResourceType::SAMPLED_IMAGE && res.type != ShaderResourceType::STORAGE_IMAGE))
            return Fail("Image descriptor element or resource type is invalid");
        auto       *img            = static_cast<D3D12Image *>(image);
        const auto &desc           = img->GetDescriptor();
        const auto  dimension      = desc.imageType == ImageType::IMAGE_1D   ? ShaderTextureDimension::D1
                                     : desc.imageType == ImageType::IMAGE_3D ? ShaderTextureDimension::D3
                                                                             : ShaderTextureDimension::D2;
        const auto  format         = desc.format;
        const bool  unsignedFormat = format == PixelFormat::R8_UINT || (format >= PixelFormat::R32_UINT && format <= PixelFormat::RGBA32_UINT);
        if (res.texture.dimension != dimension || res.texture.arrayed != (desc.arrayLayers > 1) ||
            res.texture.multisampled != (desc.samples != SampleCount::X1) ||
            res.texture.scalarType != (unsignedFormat ? ShaderScalarType::UINT : ShaderScalarType::FLOAT))
            return Fail("Image descriptor shape or scalar type disagrees with its reflected type");
        const bool storage = res.type == ShaderResourceType::STORAGE_IMAGE;
        if (storage ? !desc.usage.TestBit(ImageUsageFlagBit::STORAGE) || layout != ImageLayout::GENERAL || desc.samples != SampleCount::X1 ||
                          desc.arrayLayers != 1
                    : !desc.usage.TestBit(ImageUsageFlagBit::SAMPLED) || (layout != ImageLayout::SHADER_READ_ONLY && layout != ImageLayout::GENERAL))
            return Fail("Image usage, layout or native view is unsupported");
        const auto handle = allocator->GetCbvSrvUavCpuHandle(group->allocation.cbvSrvUavFirst + group->cbvSrvUavOffsets[index] + arrayElement);
        if (storage)
            img->CreateUAV(handle);
        else
            img->CreateSRV(handle);
        group->pins[{binding, arrayElement}] = CounterPtr<RefObject>(image);
        group->mDirty                        = true;
        return true;
    }

    bool D3D12DescriptorEncoder::WriteSampler(uint32_t binding, Sampler *sampler, uint32_t arrayElement)
    {
        if (failed)
            return false;
        auto      *allocator = group->device->GetDescriptorAllocator();
        const auto index     = FindBinding(binding);
        if (!allocator || index == INVALID_INDEX || !sampler || sampler->GetDevice() != group->device)
            return Fail("Sampler binding is missing or belongs to another device");
        const auto &res = group->setResources[index];
        if (arrayElement >= res.count || res.type != ShaderResourceType::SAMPLER || res.sampler != ShaderSamplerType::REGULAR)
            return Fail("Sampler descriptor element or type is unsupported");
        auto      *smp    = static_cast<D3D12Sampler *>(sampler);
        const auto handle = allocator->GetSamplerCpuHandle(group->allocation.samplerFirst + group->samplerOffsets[index] + arrayElement);
        group->device->GetNativeHandle()->CreateSampler(&smp->GetNativeDesc(), handle);
        group->pins[{binding, arrayElement}] = CounterPtr<RefObject>(sampler);
        group->mDirty                        = true;
        return true;
    }

} // namespace sky::aurora
