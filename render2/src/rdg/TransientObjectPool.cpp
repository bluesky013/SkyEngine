//
// Created by Zach Lee on 2023/4/16.
//

#include <render/rdg/TransientObjectPool.h>
#include <render/RHI.h>

namespace sky::rdg {
    rhi::ImageViewPtr TransientObjectPool::requestImage(const rdg::GraphImage &desc)
    {
        auto iter = images.find(desc);
        if (iter != images.end()) {
            return iter->second;
        }

        rhi::Device *device = RHI::Get()->GetDevice();

        rhi::Image::Descriptor imageDesc = {};
        imageDesc.imageType   = desc.viewType == rhi::ImageViewType::VIEW_3D ? rhi::ImageType::IMAGE_3D : rhi::ImageType::IMAGE_2D;
        imageDesc.format      = desc.format;
        imageDesc.extent      = desc.extent;
        imageDesc.mipLevels   = desc.mipLevels;
        imageDesc.arrayLayers = desc.arrayLayers;
        imageDesc.samples     = desc.samples;
        imageDesc.usage       = desc.usage;
        imageDesc.memory      = rhi::MemoryType::GPU_ONLY;

        auto image = device->CreateImage(imageDesc);
        rhi::ImageViewDesc viewDesc = {};
        viewDesc.viewType = desc.viewType;
        viewDesc.subRange = {0, desc.mipLevels, 0, desc.arrayLayers};
        viewDesc.mask = desc.mask;

        return images.emplace(desc, image->CreateView(viewDesc)).first->second;
    }

    rhi::BufferViewPtr TransientObjectPool::requestBuffer(const rdg::GraphBuffer &desc)
    {
        auto iter = buffers.find(desc);
        if (iter != buffers.end()) {
            return iter->second;
        }

        rhi::Device *device = RHI::Get()->GetDevice();

        rhi::Buffer::Descriptor bufferDesc = {};
        bufferDesc.size     = desc.size;
        bufferDesc.usage       = desc.usage;
        bufferDesc.memory      = rhi::MemoryType::GPU_ONLY;

        auto buffer = device->CreateBuffer(bufferDesc);
        rhi::BufferViewDesc viewDesc = {};
        viewDesc.offset = 0;
        viewDesc.range = desc.size;

        return buffers.emplace(desc, buffer->CreateView(viewDesc)).first->second;
    }
} // namespace sky::rdg