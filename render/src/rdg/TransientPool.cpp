//
// Created by Zach Lee on 2023/6/12.
//

#include <render/rdg/TransientPool.h>
#include <render/RHI.h>
#include <rhi/Decode.h>

namespace sky::rdg {

    rhi::ImagePtr TransientPool::CreateImageByDesc(const rdg::GraphImage &desc)
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

        return device->CreateImage(imageDesc);
    }

    rhi::BufferPtr TransientPool::CreateBufferByDesc(const rdg::GraphBuffer &desc)
    {
        rhi::Device *device = RHI::Get()->GetDevice();

        rhi::Buffer::Descriptor bufferDesc = {};
        bufferDesc.size = desc.size;
        bufferDesc.usage = desc.usage;
        bufferDesc.memory = rhi::MemoryType::GPU_ONLY;

        return device->CreateBuffer(bufferDesc);
    }

    rhi::ImagePtr TransientPool::RequestPersistentImage(const std::string &name, const rdg::GraphImage &desc)
    {
        auto iter = persistentImages.find(name);
        if (iter != persistentImages.end()) {
            const auto &imageDesc = iter->second->GetDescriptor();
            if (desc.extent.width == imageDesc.extent.width &&
                desc.extent.height == imageDesc.extent.height &&
                desc.extent.depth == imageDesc.extent.depth &&
                desc.format == imageDesc.format) {
                return iter->second;
            }
        }
        auto image = CreateImageByDesc(desc);
        persistentImages[name] = image;
        return image;
    }

    rhi::BufferPtr TransientPool::RequestPersistentBuffer(const std::string &name, const rdg::GraphBuffer &desc)
    {
        auto iter = persistentBuffers.find(name);
        if (iter != persistentBuffers.end()) {
            const auto &bufferDesc = iter->second->GetBufferDesc();
            if (bufferDesc.size >= desc.size) {
                return iter->second;
            }
        }
        auto buffer = CreateBufferByDesc(desc);
        persistentBuffers[name] = buffer;
        return buffer;
    }
} // namespace sky::rdg