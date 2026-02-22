//
// Created by blues on 2025/1/30.
//

#include <builder/render/image/ImageConverter.h>
#include <rhi/Decode.h>

namespace sky::builder {

    static void Gamma(Color& color, float gamma)
    {
        if (gamma == 1.f) {
            return;
        }

        color.r = std::powf(std::max(0.0f, color.r), gamma);
        color.g = std::powf(std::max(0.0f, color.g), gamma);
        color.b = std::powf(std::max(0.0f, color.b), gamma);
    }

    void ImageConverter::DoWork()
    {
        SKY_ASSERT(payload.src->width == payload.dst->width);
        SKY_ASSERT(payload.src->height == payload.dst->height);
        SKY_ASSERT(payload.src->mips.size() == payload.dst->mips.size());

        for (uint32_t mip = 0; mip < payload.src->mips.size(); ++mip) {

            auto &srcMip = payload.src->mips[mip];
            auto &dstMip = payload.dst->mips[mip];

            uint8_t* srcData = srcMip.data.get();
            uint8_t* dstData = dstMip.data.get();

            uint32_t pixelCount = srcMip.width * srcMip.height * srcMip.depth;

            for (uint32_t i = 0; i < pixelCount; ++i, srcData += payload.src->pixelSize, dstData += payload.dst->pixelSize)
            {
                Color color = {0.f, 0.f, 0.f, 1.f};
                GetImageColor(GetPixelType(payload.src->format), payload.src->components, srcData, color);

                Gamma(color, payload.gamma);

                SetImageColor(GetPixelType(payload.dst->format), payload.dst->components, dstData, color);
            }
        }
    }

} // sky::builder