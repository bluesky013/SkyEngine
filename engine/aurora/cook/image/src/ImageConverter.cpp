//
// Per-pixel color transform (see ImageConverter.h).
//

#include <aurora/cook/image/ImageConverter.h>

#include <core/platform/Platform.h>

#include <algorithm>
#include <cmath>

namespace sky::aurora::cook {

    namespace {

        void Gamma(Color &color, float gamma)
        {
            if (gamma == 1.f) {
                return;
            }
            color.r = std::pow(std::max(0.0f, color.r), gamma);
            color.g = std::pow(std::max(0.0f, color.g), gamma);
            color.b = std::pow(std::max(0.0f, color.b), gamma);
        }

    } // namespace

    void ImageConverter::DoWork()
    {
        SKY_ASSERT(payload.src->width == payload.dst->width);
        SKY_ASSERT(payload.src->height == payload.dst->height);
        SKY_ASSERT(payload.src->mips.size() == payload.dst->mips.size());

        for (uint32_t mip = 0; mip < payload.src->mips.size(); ++mip) {
            auto &srcMip = payload.src->mips[mip];
            auto &dstMip = payload.dst->mips[mip];

            uint8_t       *srcData    = srcMip.data.get();
            uint8_t       *dstData    = dstMip.data.get();
            const uint32_t pixelCount = srcMip.width * srcMip.height * srcMip.depth;

            for (uint32_t i = 0; i < pixelCount; ++i, srcData += payload.src->pixelSize, dstData += payload.dst->pixelSize) {
                Color color = {0.f, 0.f, 0.f, 1.f};
                GetImageColor(GetPixelType(payload.src->format), payload.src->components, srcData, color);

                Gamma(color, payload.gamma);

                SetImageColor(GetPixelType(payload.dst->format), payload.dst->components, dstData, color);
            }
        }
    }

} // namespace sky::aurora::cook
