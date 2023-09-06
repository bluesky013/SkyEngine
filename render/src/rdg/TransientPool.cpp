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

    ImageObject *TransientPool::RequestPersistentImage(const std::string &name, const rdg::GraphImage &desc)
    {
        auto iter = persistentImages.find(name);
        if (iter != persistentImages.end()) {
            const auto &imageDesc = iter->second->image->GetDescriptor();
            if (desc.extent.width == imageDesc.extent.width &&
                desc.extent.height == imageDesc.extent.height &&
                desc.extent.depth == imageDesc.extent.depth &&
                desc.format == imageDesc.format &&
                desc.usage == imageDesc.usage &&
                desc.samples == imageDesc.samples) {
                return iter->second.get();
            }
        }

//        persistentImages[name] = std::make_unique<ImageObject>(CreateImageByDesc(desc));
        return nullptr;
    }

    BufferObject *TransientPool::RequestPersistentBuffer(const std::string &name, const rdg::GraphBuffer &desc)
    {
        auto iter = persistentBuffers.find(name);
        if (iter != persistentBuffers.end()) {
            const auto &bufferDesc = iter->second->buffer->GetBufferDesc();
            if (bufferDesc.size >= desc.size) {
                return iter->second.get();
            }
        }
//        persistentBuffers[name] =std::make_unique<BufferObject>(CreateBufferByDesc(desc));
        return nullptr;
    }

    const rhi::FrameBufferPtr &TransientPool::RequestFrameBuffer(const rhi::FrameBuffer::Descriptor &desc)
    {
        auto iter = frameBuffers.find(desc);
        if (iter != frameBuffers.end()) {
            iter->second.count = 0;
            return iter->second.item;
        }

        CacheItem<rhi::FrameBufferPtr> fbItem = {};
        fbItem.item = RHI::Get()->GetDevice()->CreateFrameBuffer(desc);
        fbItem.count = 0;
        return frameBuffers.emplace(desc, fbItem).first->second.item;
    }

    void TransientPool::ResetPool()
    {
        for (auto iter = frameBuffers.begin(); iter != frameBuffers.end();) {
            iter->second.count++;
            if (iter->second.count >= 60) {
                iter = frameBuffers.erase(iter);
            } else {
                ++iter;
            }
        }
    }

    rhi::ImageViewPtr ImageObject::RequestView(const rhi::ImageViewDesc &desc)
    {
        auto iter = views.find(desc);
        if (iter != views.end()) {
            return iter->second;
        }

        auto view = image->CreateView(desc);
        return views.emplace(desc, view).first->second;
    }
} // namespace sky::rdg