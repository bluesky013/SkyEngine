//
// Created by blues on 2025/1/29.
//

#include <builder/render/image/ImageProcess.h>

namespace sky::builder {

    uint32_t GetMipLevel(uint32_t width, uint32_t height)
    {
        uint32_t size = std::max(width, height);
        uint32_t level = 0;
        while (size != 0) {
            size >>= 1;
            ++level;
        }
        return level;
    }

    uint32_t GetBytePerComp(PixelType type)
    {
        if (type == PixelType::U8) return 1;
        return 4;
    }
} // namespace sky::builder