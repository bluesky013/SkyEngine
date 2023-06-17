//
// Created by Zach Lee on 2023/6/12.
//

#include <render/rdg/TransientPool.h>
#include <render/RHI.h>

namespace sky::rdg {

    rhi::ImageViewPtr TransientPool::CreateImageByDesc(const rdg::GraphImage &desc)
    {
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
        viewDesc.subRange = {0, imageDesc.mipLevels, 0, imageDesc.arrayLayers};
        viewDesc.mask = desc.mask;
        viewDesc.viewType = desc.viewType;

        return image->CreateView(viewDesc);
    }

    rhi::BufferViewPtr TransientPool::CreateBufferByDesc(const rdg::GraphBuffer &desc)
    {
        rhi::Device *device = RHI::Get()->GetDevice();

        rhi::Buffer::Descriptor bufferDesc = {};
        bufferDesc.size = desc.size;
        bufferDesc.usage = desc.usage;
        bufferDesc.memory = rhi::MemoryType::GPU_ONLY;

        auto buffer = device->CreateBuffer(bufferDesc);

        rhi::BufferViewDesc viewDesc = {};
        viewDesc.offset = 0;
        viewDesc.range = bufferDesc.size;
        return buffer->CreateView(viewDesc);
    }

    rhi::ImageViewPtr TransientPool::RequestPersistentImage(const std::string &name, const rdg::GraphImage &desc)
    {
        auto iter = persistentImages.find(name);
        if (iter != persistentImages.end()) {
            auto &ext = iter->second->GetExtent();
            auto format = iter->second->GetFormat();
            if (desc.extent.width == ext.width &&
                desc.extent.height == ext.height &&
                desc.extent.depth == ext.depth &&
                desc.format == format) {
                return iter->second;
            }
        }
        auto image = CreateImageByDesc(desc);
        persistentImages[name] = image;
        return image;
    }

    rhi::BufferViewPtr TransientPool::RequestPersistentBuffer(const std::string &name, const rdg::GraphBuffer &desc)
    {
        auto iter = persistentBuffers.find(name);
        if (iter != persistentBuffers.end()) {
            auto &vd = iter->second->GetViewDesc();

            if (vd.range >= desc.size) {
                return iter->second;
            }
        }
        auto buffer = CreateBufferByDesc(desc);
        persistentBuffers[name] = buffer;
        return buffer;
    }
} // namespace sky::rdg