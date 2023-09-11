//
// Created by blues on 2023/9/11.
//

#pragma once

#include "rhi/Core.h"
#include <cstdint>

namespace sky::builder {

    enum class Quality : uint32_t {
        ULTRA_FAST,
        VERY_FAST,
        FAST,
        BASIC,
        SLOW
    };

    struct CompressOption {
        Quality quality = Quality::SLOW;
        rhi::PixelFormat targetFormat = rhi::PixelFormat::ASTC_4x4_UNORM_BLOCK;
    };

    struct BufferImageInfo {
        uint32_t offset  = 0;
        uint32_t stride  = 0;
        uint32_t width   = 0;
        uint32_t height  = 0;
        uint32_t layer   = 1;
        bool genMip = true;
    };

    void InitializeCompressor();
    void CompressImage(uint8_t *ptr, const BufferImageInfo &copy, uint8_t *&out, const CompressOption &options);
} // namespace sky::builder