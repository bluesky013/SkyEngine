//
// Mip chain generation (see ImageMipGen.h).
//

#include <aurora/cook/image/ImageMipGen.h>
#include <aurora/cook/image/ImageFilter.h>

#include <core/platform/Platform.h>

#include <algorithm>
#include <vector>

namespace sky::aurora::cook {

    namespace {

        void FilterImage(ImageObject &image, uint32_t srcMip, uint32_t dstMip, MipGenType filterType)
        {
            ImageMipData       &inData     = image.mips[srcMip];
            ImageMipData       &outData    = image.mips[dstMip];
            const uint32_t      components = image.components;

            auto func = MakeFilter(filterType);
            if (!func) {
                return;
            }

            PolyphaseKernel<float> kernelX(*func, inData.width, outData.width, 32);
            PolyphaseKernel<float> kernelY(*func, inData.height, outData.height, 32);

            ImageMipData tmpData = ImageMipData::Create(outData.width, inData.height, inData.depth, components * sizeof(float));

            const PixelType type = GetPixelType(image.format);

            // Pass 1: horizontal, source rows -> float temp rows.
            for (uint32_t y = 0; y < inData.height; ++y) {
                float *row = reinterpret_cast<float *>(tmpData.data.get()) + y * outData.width * components;
                kernelX.ApplyX(inData, type, y, 0, row, components);
            }

            // Pass 2: vertical, float temp rows -> destination.
            std::vector<float> tmpCol(outData.height * components, 0.f);
            for (uint32_t x = 0; x < outData.width; ++x) {
                kernelY.ApplyY(tmpData, PixelType::Float, x, 0, tmpCol.data(), components);

                for (uint32_t y = 0; y < outData.height; ++y) {
                    uint8_t     *dst = outData.data.get() + (y * outData.width + x) * image.pixelSize;
                    const float *src = tmpCol.data() + y * components;

                    Color color = {};
                    for (uint32_t c = 0; c < components; ++c) {
                        color.v[c] = src[c];
                    }
                    SetImageColor(type, components, dst, color);
                }
            }
        }

    } // namespace

    void ImageMipGen::DoWork()
    {
        SKY_ASSERT(payload.image->type == ImageType::IMAGE_2D);
        SKY_ASSERT(payload.image->depth == 1);
        SKY_ASSERT(!payload.image->mips.empty());

        const uint32_t mipLevel = GetMipLevel(payload.image->width, payload.image->height);
        SKY_ASSERT(mipLevel >= 1);

        auto &image = *payload.image;
        image.mips.resize(mipLevel);
        for (uint32_t i = 1; i < mipLevel; ++i) {
            const auto &srcMip = image.mips[i - 1];
            auto       &dstMip = image.mips[i];

            const uint32_t w = std::max(1u, srcMip.width / 2);
            const uint32_t h = std::max(1u, srcMip.height / 2);
            const uint32_t d = std::max(1u, srcMip.depth / 2);
            dstMip           = ImageMipData::Create(w, h, d, image.pixelSize);

            FilterImage(image, i - 1, i, payload.type);
        }
    }

} // namespace sky::aurora::cook
