//
// Created by blues on 2025/1/30.
//

#include <builder/render/image/ImageMipGen.h>
#include <builder/render/image/ImageFilter.h>
#include <rhi/Decode.h>
#include <cmath>

namespace sky::builder {

    // from nvidia texture tools
    inline int32_t WrapClamp(int32_t x, int32_t w)
    {
        return std::clamp(x, 0, w - 1);
    }

    inline int32_t WrapRepeat(int32_t x, int32_t w)
    {
        if (x >= 0) return x % w;
        else return (x + 1) % w + w - 1;
    }

    inline int32_t WrapMirror(int32_t x, int32_t w)
    {
        if (w == 1) x = 0;

        x = abs(x);
        while (x >= w) {
            x = abs(w + w - x - 2);
        }
        return x;
    }

    inline int32_t Wrap(int32_t x, int32_t w)
    {
        return WrapRepeat(x, w);
    }

    template <typename T>
    struct PolyphaseKernel {

        PolyphaseKernel(Filter<T>& filter, uint32_t srcLength, uint32_t dstLength, uint32_t samples)
            : length(dstLength)
        {
            scale = static_cast<T>(dstLength) / static_cast<T>(srcLength);
            width = filter.GetWidth() / scale;

            windowSize = static_cast<uint32_t>(std::ceil(width * static_cast<T>(2.0))) + 1;
            weights.resize(windowSize * dstLength, static_cast<T>(1.0));

            for (uint32_t i = 0; i < dstLength; ++i) {
                T center = (static_cast<T>(i) + static_cast<T>(0.5)) / scale;

                int32_t left = static_cast<int32_t>(std::floor(center - width));
                int32_t right = static_cast<int32_t>(std::ceil(center + width));
                SKY_ASSERT(right - left <= static_cast<int32_t>(windowSize))

                T sum = static_cast<T>(0.0);
                for (uint32_t j = 0; j < windowSize; ++j) {
                    auto sample = filter.Sample(left + static_cast<T>(j) - center, scale, samples);

                    weights[i * windowSize + j] = sample;
                    sum += sample;
                }

                for (uint32_t j = 0; j < windowSize; ++j) {
                    weights[i * windowSize + j] /= sum;
                }
            }
        }

        void ApplyX(const ImageMipData &mipData, uint32_t y, uint32_t z, float *out, uint32_t components)
        {
            uint32_t baseIndex = (z * mipData.height + y) * mipData.width;
            for (uint32_t i = 0; i < length; i++)
            {
                T center = (static_cast<T>(i) + static_cast<T>(0.5)) / scale;
                int32_t left = static_cast<int32_t>(std::floor(center - width));
                int32_t right = static_cast<int32_t>(std::ceil(center + width));
                SKY_ASSERT(right - left <= static_cast<int32_t>(windowSize))

                for (uint32_t j = 0; j < components; ++j) {

                    float sum = 0;
                    for (uint32_t k = 0; k < windowSize; ++k) {
                        auto weight = Weight(i, k);

                        int32_t tx = left + static_cast<int32_t>(k);
                        uint32_t x = static_cast<uint32_t>(Wrap(tx, mipData.width));
                        uint32_t index = baseIndex + x;

                        const float* ptr = reinterpret_cast<const float*>(mipData.data.get());
                        sum += weight * ptr[index * components + j];
                    }

                    out[i * components + j] = sum;
                }
            }
        }

        void ApplyY(const ImageMipData &mipData, uint32_t x, uint32_t z, float *out, uint32_t components)
        {
            for (uint32_t i = 0; i < length; i++)
            {
                T center = (static_cast<T>(i) + static_cast<T>(0.5)) / scale;
                int32_t left = static_cast<int32_t>(std::floor(center - width));
                int32_t right = static_cast<int32_t>(std::ceil(center + width));
                SKY_ASSERT(right - left <= static_cast<int32_t>(windowSize))

                for (uint32_t j = 0; j < components; ++j) {

                    float sum = 0;
                    for (uint32_t k = 0; k < windowSize; ++k) {
                        auto weight = Weight(i, k);

                        int32_t ty = left + static_cast<int32_t>(k);
                        uint32_t y = Wrap(ty, mipData.height);
                        uint32_t index = (z * mipData.height + y) * mipData.width + x;

                        const float* ptr = reinterpret_cast<const float*>(mipData.data.get());
                        sum += weight * ptr[index * components + j];
                    }

                    out[i * components + j] = sum;
                }
            }
        }

        float Weight(uint32_t column, uint32_t x) const
        {
            SKY_ASSERT(column < length);
            SKY_ASSERT(x < windowSize);
            return weights[column * windowSize + x];
        }

        T scale;
        T width;
        uint32_t length;
        uint32_t windowSize;
        std::vector<T> weights;
    };

    static void FilterImage(ImageObject &image, uint32_t srcMip, uint32_t dstMip, MipGenType filter)
    {
        ImageMipData &inData = image.mips[srcMip];
        ImageMipData &outData = image.mips[dstMip];
        uint32_t components = image.components;

        std::unique_ptr<Filter<float>> func;
        switch (filter) {
        case MipGenType::Kaiser: func.reset(new KaiserFilter<float>(4.f, 3)); break;
        default:
            SKY_ASSERT(false && "not implemented");
            break;
        }

        if (!func) {
            return;
        }

        PolyphaseKernel kernelX(*func, inData.width, outData.width, 32);
        PolyphaseKernel kernelY(*func, inData.height, outData.height, 32);

        ImageMipData tmpData = ImageMipData::Create(outData.width, inData.height, inData.depth, image.pixelSize);
        outData = ImageMipData::Create(outData.width, outData.height, inData.depth, image.pixelSize);

        // filter vertical
        for (uint32_t y = 0; y < inData.height; ++y) {
            float* row = reinterpret_cast<float*>(tmpData.data.get()) + y * outData.width * components;
            kernelX.ApplyX(inData, y, 0, row, components);
        }

        std::vector<float> tmpCol(outData.height * components, 0.f);
        // filter horizontal
        for (uint32_t x = 0; x < outData.width; ++x) {
            kernelY.ApplyY(tmpData, x, 0, tmpCol.data(), components);

            for (uint32_t y = 0; y < outData.height; ++y) {
                float* dst = reinterpret_cast<float*>(outData.data.get()) + (y * outData.width + x) * components;
                float* src = tmpCol.data() + y * components;
                for (uint32_t c = 0; c < components; ++c) {
                    dst[c] = src[c];
                }
            }
        }
    }

    void ImageMipGen::DoWork()
    {
        SKY_ASSERT(payload.image->type == rhi::ImageType::IMAGE_2D)
        SKY_ASSERT(payload.image->depth == 1)
        SKY_ASSERT(payload.image->mips.size() >= 1)

        auto mipLevel = GetMipLevel(payload.image->width, payload.image->height);
        SKY_ASSERT(mipLevel >= 1);

        payload.image->mips.resize(mipLevel);
        auto &image = *payload.image;
        for (uint32_t i = 1; i < mipLevel; ++i) {
            const auto &srcMip = payload.image->mips[i - 1];
            auto &dstMip = payload.image->mips[i];

            uint32_t w = std::max(1u, srcMip.width / 2);
            uint32_t h = std::max(1u, srcMip.height / 2);
            uint32_t d = std::max(1u, srcMip.depth / 2);
            dstMip = ImageMipData::Create(w, h, d, image.pixelSize);

            FilterImage(*payload.image, i - 1, i, MipGenType::Kaiser);
        }
    }

} // namespace sky::builder