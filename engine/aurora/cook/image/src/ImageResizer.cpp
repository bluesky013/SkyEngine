//
// Downscale-to-limit resize (see ImageResizer.h).
//

#include <aurora/cook/image/ImageResizer.h>
#include <aurora/cook/image/ImageFilter.h>

#include <core/platform/Platform.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace sky::aurora::cook {

    namespace {

        void ResampleImage(const ImageObject &image, const ImageMipData &srcMip, ImageMipData &dstMip, MipGenType filterType)
        {
            const uint32_t components = image.components;

            auto filter = MakeFilter(filterType);
            if (!filter) {
                return;
            }

            PolyphaseKernel<float> kernelX(*filter, srcMip.width, dstMip.width, 32);
            PolyphaseKernel<float> kernelY(*filter, srcMip.height, dstMip.height, 32);

            const PixelType type = GetPixelType(image.format);

            // Two-pass separable filter: X into a float temp, then Y.
            ImageMipData tmpData = ImageMipData::Create(dstMip.width, srcMip.height, srcMip.depth, components * sizeof(float));

            for (uint32_t z = 0; z < srcMip.depth; ++z) {
                for (uint32_t y = 0; y < srcMip.height; ++y) {
                    float *row = reinterpret_cast<float *>(tmpData.data.get()) + (z * srcMip.height + y) * dstMip.width * components;
                    kernelX.ApplyX(srcMip, type, y, z, row, components);
                }
            }

            std::vector<float> tmpCol(dstMip.height * components, 0.f);
            for (uint32_t z = 0; z < srcMip.depth; ++z) {
                for (uint32_t x = 0; x < dstMip.width; ++x) {
                    kernelY.ApplyY(tmpData, PixelType::Float, x, z, tmpCol.data(), components);

                    for (uint32_t y = 0; y < dstMip.height; ++y) {
                        uint8_t     *dst = dstMip.data.get() + ((z * dstMip.height + y) * dstMip.width + x) * image.pixelSize;
                        const float *src = tmpCol.data() + y * components;

                        Color color = {};
                        for (uint32_t c = 0; c < components; ++c) {
                            color.v[c] = src[c];
                        }
                        SetImageColor(type, components, dst, color);
                    }
                }
            }
        }

    } // namespace

    void ImageResizer::DoWork()
    {
        auto &image = payload.image;
        SKY_ASSERT(image && !image->mips.empty());

        const uint32_t srcW = image->width;
        const uint32_t srcH = image->height;

        if (srcW <= payload.maxWidth && srcH <= payload.maxHeight) {
            return;
        }

        const float scaleW = static_cast<float>(payload.maxWidth) / static_cast<float>(srcW);
        const float scaleH = static_cast<float>(payload.maxHeight) / static_cast<float>(srcH);
        const float scale  = std::min(scaleW, scaleH);

        uint32_t dstW = std::max(1u, static_cast<uint32_t>(std::floor(srcW * scale)));
        uint32_t dstH = std::max(1u, static_cast<uint32_t>(std::floor(srcH * scale)));
        dstW          = std::min(dstW, payload.maxWidth);
        dstH          = std::min(dstH, payload.maxHeight);

        ImageMipData &srcMip = image->mips[0];
        ImageMipData  dstMip = ImageMipData::Create(dstW, dstH, srcMip.depth, image->pixelSize);

        ResampleImage(*image, srcMip, dstMip, payload.filterType);

        image->width  = dstW;
        image->height = dstH;
        image->mips.clear();
        image->mips.emplace_back(std::move(dstMip));
    }

} // namespace sky::aurora::cook
