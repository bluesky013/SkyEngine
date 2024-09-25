//
// Created by Zach Lee on 2023/9/6.
//

#pragma once

#include <rhi/Core.h>
#include <core/hash/Hash.h>
#include <core/hash/Crc32.h>

namespace std {

    template <>
    struct hash<sky::rhi::ImageViewDesc> {
        size_t operator()(const sky::rhi::ImageViewDesc &desc) const noexcept
        {
            uint32_t res = 0;
            sky::HashCombine32(res, sky::Crc32::Cal(desc));
            return res;
        }
    };

    template <>
    struct equal_to<sky::rhi::ImageViewDesc> {
        bool operator()(const sky::rhi::ImageViewDesc &x, const sky::rhi::ImageViewDesc &y) const noexcept
        {
            return x.viewType == y.viewType &&
                x.subRange.baseLevel == y.subRange.baseLevel &&
                x.subRange.levels == y.subRange.levels &&
                x.subRange.baseLayer == y.subRange.baseLayer &&
                x.subRange.layers == y.subRange.layers &&
                x.subRange.aspectMask == y.subRange.aspectMask;
        }
    };

    template <>
    struct hash<sky::rhi::FrameBuffer::Descriptor> {
        size_t operator()(const sky::rhi::FrameBuffer::Descriptor &desc) const noexcept
        {
            uint32_t res = 0;
            sky::HashCombine32(res, sky::Crc32::Cal(desc.extent));
            for (const auto &view : desc.views) {
                sky::HashCombine32(res, sky::Crc32::Cal(view.get()));
            }
            return res;
        }
    };

    template <>
    struct equal_to<sky::rhi::FrameBuffer::Descriptor> {
        bool operator()(const sky::rhi::FrameBuffer::Descriptor &x, const sky::rhi::FrameBuffer::Descriptor &y) const noexcept
        {
            if (x.views.size() != y.views.size()) {
                return false;
            }

            for (uint32_t i = 0; i < x.views.size(); ++i) {
                if (x.views[i].get() != y.views[i].get()) {
                    return false;
                }
            }

            return x.extent.width == y.extent.width &&
                   x.extent.height == y.extent.height;
        }
    };

} // namespace std