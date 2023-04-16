//
// Created by Zach Lee on 2023/4/16.
//

#pragma once

#include <rhi/Buffer.h>
#include <rhi/BufferView.h>
#include <rhi/Image.h>
#include <rhi/ImageView.h>
#include <core/hash/Hash.h>
#include <core/hash/Crc32.h>
#include <render/rdg/RenderGraphTypes.h>

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
        sky::HashCombine32(data, sky::Crc32::Cal(desc.mask));
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
               lhs.mask == rhs.mask;
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
        return lhs.size == rhs.size && lhs.usage == rhs.usage;
    };
};

namespace sky::rdg {

    class TransientPool {
    public:
        TransientPool() = default;
        virtual ~TransientPool() = default;

        virtual rhi::ImageViewPtr requestImage(const rdg::GraphImage &desc) = 0;
        virtual rhi::BufferViewPtr requestBuffer(const rdg::GraphBuffer &desc) = 0;
    };

} // namespace sky::rdg