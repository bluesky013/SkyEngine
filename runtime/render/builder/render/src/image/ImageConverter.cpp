//
// Created by blues on 2025/1/30.
//

#include <builder/render/image/ImageConverter.h>
#include <rhi/Decode.h>
#include <core/math/Color.h>

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

    void GetColor(PixelType type, uint32_t components, const uint8_t *src, Color &color)
    {
        for (uint32_t i = 0; i < components; ++i) {
            switch (type) {
            case PixelType::U8:
                color.v[i] = U8ToF32(src[i]);
                break;
            case PixelType::Float:
            {
                const float *ptr = reinterpret_cast<const float *>(src);
                color.v[i] = ptr[i];
            }
                break;
            default: SKY_ASSERT("not implement") break;
            }
        }
    }

    void SetColor(PixelType type, uint32_t components, uint8_t *dst, const Color &color)
    {
        for (uint32_t i = 0; i < components; ++i) {
            switch (type) {
            case PixelType::U8:
                dst[i] = F32ToU8(color.v[i]);
                break;
            case PixelType::Float:
            {
                float *ptr = reinterpret_cast<float *>(dst);
                ptr[i] = color.v[i];
            }
            default:
                SKY_ASSERT("not implement")
                break;
            }
        }
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
                GetColor(payload.src->pixelType, payload.src->components, srcData, color);

                Gamma(color, payload.gamma);

                SetColor(payload.dst->pixelType, payload.dst->components, dstData, color);
            }
        }
    }

} // sky::builder