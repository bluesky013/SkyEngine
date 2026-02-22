//
// Created by Zach Lee on 2023/4/16.
//

#pragma once

#include <rhi/Buffer.h>
#include <rhi/Image.h>
#include <rhi/ImageView.h>
#include <core/hash/Hash.h>
#include <core/hash/Crc32.h>
#include <render/rdg/RenderGraphTypes.h>

#include <rhi/Core.h>
#include <rhi/Hash.h>

#include <utility>

template<>
struct std::hash<sky::rdg::GraphImage> {
    size_t operator()(const sky::rdg::GraphImage &desc) const {
        uint32_t data = 0;
        sky::HashCombine32(data, sky::Crc32::Cal(desc.viewType));
        sky::HashCombine32(data, sky::Crc32::Cal(desc.format));
        sky::HashCombine32(data, sky::Crc32::Cal(desc.extent));
        sky::HashCombine32(data, sky::Crc32::Cal(desc.mipLevels));
        sky::HashCombine32(data, sky::Crc32::Cal(desc.arrayLayers));
        sky::HashCombine32(data, sky::Crc32::Cal(desc.samples));
        sky::HashCombine32(data, sky::Crc32::Cal(desc.usage));
        return data;
    }
};

template<>
struct std::equal_to<sky::rdg::GraphImage> {
    constexpr bool operator()(const sky::rdg::GraphImage& lhs, const sky::rdg::GraphImage& rhs) const
    {
        return lhs.viewType == rhs.viewType &&
               lhs.format == rhs.format &&
               lhs.extent.width == rhs.extent.width &&
               lhs.extent.height == rhs.extent.height &&
               lhs.extent.depth == rhs.extent.depth &&
               lhs.mipLevels == rhs.mipLevels &&
               lhs.arrayLayers == rhs.arrayLayers &&
               lhs.samples == rhs.samples &&
               lhs.usage == rhs.usage &&
               lhs.residency == rhs.residency;
    }
};

template<>
struct std::hash<sky::rdg::GraphBuffer> {
    size_t operator()(const sky::rdg::GraphBuffer &desc) const {
        uint32_t data = 0;
        sky::HashCombine32(data, sky::Crc32::Cal(desc.size));
        sky::HashCombine32(data, sky::Crc32::Cal(desc.usage));
        return data;
    }
};

template<>
struct std::equal_to<sky::rdg::GraphBuffer> {
    constexpr bool operator()(const sky::rdg::GraphBuffer& lhs, const sky::rdg::GraphBuffer& rhs) const
    {
        return lhs.size == rhs.size && lhs.usage == rhs.usage && lhs.residency == rhs.residency;
    };
};

namespace sky::rdg {

    struct ImageObject {
        explicit ImageObject(rhi::ImagePtr img) : image(std::move(img)) {}
        rhi::ImageViewPtr RequestView(const rhi::ImageViewDesc &desc);

        rhi::ImagePtr image;
        std::unordered_map<rhi::ImageViewDesc, rhi::ImageViewPtr> views;
    };

    struct BufferObject {
        explicit BufferObject(rhi::BufferPtr buf) : buffer(std::move(buf)) {}

        rhi::BufferPtr buffer;
    };

    class TransientPool {
    public:
        TransientPool() = default;
        virtual ~TransientPool() = default;

        void Init();
        virtual void ResetPool();

        virtual ImageObject *RequestImage(const rdg::GraphImage &desc) = 0;
        virtual BufferObject *RequestBuffer(const rdg::GraphBuffer &desc) = 0;

        virtual void RecycleImage(rhi::ImagePtr &image, const rdg::GraphImage &desc) = 0;
        virtual void RecycleBuffer(rhi::BufferPtr &buffer, const rdg::GraphBuffer &desc) = 0;

        ImageObject *RequestPersistentImage(const std::string &name, const rdg::GraphImage &desc);
        BufferObject *RequestPersistentBuffer(const std::string &name, const rdg::GraphBuffer &desc);

        const rhi::FrameBufferPtr &RequestFrameBuffer(const rhi::FrameBuffer::Descriptor &desc);
        ResourceGroup *RequestResourceGroup(uint64_t id, const RDResourceLayoutPtr &layout);

        rhi::ImageViewPtr RequestImageView(const Name& view, const rhi::ImagePtr &image, const rhi::ImageViewDesc& viewDesc);

        template <typename T>
        struct CacheItem {
            T item;
            uint32_t count = 0;
            bool allocated = false;
        };

    protected:
        rhi::ImagePtr CreateImageByDesc(const rdg::GraphImage &desc);
        rhi::BufferPtr CreateBufferByDesc(const rdg::GraphBuffer &desc);

        rhi::DescriptorSetPoolPtr setPool;

        std::unordered_map<rhi::FrameBuffer::Descriptor, CacheItem<rhi::FrameBufferPtr>> frameBuffers;
        std::unordered_map<uint64_t, CacheItem<std::unique_ptr<ResourceGroup>>> resourceGroups;

        std::unordered_map<std::string, std::unique_ptr<ImageObject>> persistentImages;
        std::unordered_map<std::string, std::unique_ptr<BufferObject>> persistentBuffers;

        std::unordered_map<Name, CacheItem<std::pair<rhi::ImagePtr, rhi::ImageViewPtr>>> viewCache;
    };

} // namespace sky::rdg
