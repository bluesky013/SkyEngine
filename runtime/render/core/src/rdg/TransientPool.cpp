//
// Created by Zach Lee on 2023/6/12.
//

#include <render/rdg/TransientPool.h>
#include <render/RHI.h>
#include <render/Renderer.h>
#include <rhi/Decode.h>

namespace sky::rdg {
    static constexpr uint32_t MAX_SET_PER_POOL = 128;
    static std::vector<rhi::DescriptorSetPool::PoolSize> SIZES = {
        {rhi::DescriptorType::SAMPLED_IMAGE,          MAX_SET_PER_POOL * 2},
        {rhi::DescriptorType::SAMPLER,                MAX_SET_PER_POOL},
        {rhi::DescriptorType::UNIFORM_BUFFER,         MAX_SET_PER_POOL},
        {rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC, MAX_SET_PER_POOL},
        {rhi::DescriptorType::STORAGE_BUFFER,         MAX_SET_PER_POOL},
        {rhi::DescriptorType::STORAGE_IMAGE,          MAX_SET_PER_POOL},
    };

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

        persistentImages[name] = std::make_unique<ImageObject>(CreateImageByDesc(desc));
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
        persistentBuffers[name] = std::make_unique<BufferObject>(CreateBufferByDesc(desc));
        return nullptr;
    }

    ResourceGroup *TransientPool::RequestResourceGroup(uint64_t id, const RDResourceLayoutPtr &layout)
    {
        if (!layout) {
            return Renderer::Get()->GetDefaultResource().emptySet.Get();
        }

        auto iter = resourceGroups.find(id);
        if (iter != resourceGroups.end()) {
            if (iter->second.item->GetLayout()->GetRHILayout()->GetHash() == layout->GetRHILayout()->GetHash()) {
                iter->second.count = 0;
                return iter->second.item.get();
            }
        }

        resourceGroups[id].item = std::make_unique<ResourceGroup>();
        resourceGroups[id].item->Init(layout, *setPool);
        resourceGroups[id].count = 0;
        return resourceGroups[id].item.get();
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

    rhi::ImageViewPtr TransientPool::RequestImageView(const Name& view, const rhi::ImagePtr &image, const rhi::ImageViewDesc& viewDesc)
    {
        auto iter = viewCache.find(view);
        if (iter != viewCache.end() && iter->second.item.first.get() == image.get()) {
            iter->second.count = 0;
            return iter->second.item.second;
        }
        auto imageView = image->CreateView(viewDesc);
        viewCache[view] = CacheItem<std::pair<rhi::ImagePtr, rhi::ImageViewPtr>>{{image, imageView}, 0};
        return imageView;
    }

    void TransientPool::Init()
    {
        rhi::DescriptorSetPool::Descriptor desc = {};
        desc.maxSets = MAX_SET_PER_POOL;
        desc.sizeData = SIZES.data();
        desc.sizeCount = static_cast<uint32_t>(SIZES.size());
        setPool = RHI::Get()->GetDevice()->CreateDescriptorSetPool(desc);
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

        for (auto iter = resourceGroups.begin(); iter != resourceGroups.end();) {
            iter->second.count++;
            if (iter->second.count >= 60) {
                iter = resourceGroups.erase(iter);
            } else {
                ++iter;
            }
        }

        for (auto iter = viewCache.begin(); iter != viewCache.end();) {
            iter->second.count++;
            if (iter->second.count >= 60) {
                iter = viewCache.erase(iter);
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
