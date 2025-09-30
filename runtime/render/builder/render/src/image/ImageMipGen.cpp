//
// Created by blues on 2025/1/30.
//

#include <builder/render/image/ImageMipGen.h>
#include <builder/render/image/ImageFilter.h>
#include <rhi/Decode.h>
#include <cmath>
#include <memory>

namespace sky::builder {

    // from nvidia texture tools
    inline int32_t WrapClamp(int32_t x, int32_t w)
    {
        return std::clamp(x, 0, w - 1);
    }

    inline int32_t WrapRepeat(int32_t x, int32_t w)
    {
        if (x >= 0) {
            return x % w;
        }
        return (x + 1) % w + w - 1;
    }

    inline int32_t WrapMirror(int32_t x, int32_t w)
    {
        if (w == 1) {
            x = 0;
        }

        x = abs(x);
        while (x >= w) {
            x = abs(w + w - x - 2);
        }
        return x;
    }

    inline int32_t FilterWrap(int32_t x, int32_t w)
    {
        return WrapRepeat(x, w);
    }

    inline int32_t FilterClamp(int32_t x, int32_t w)
    {
        return std::clamp(x, 0, w - 1);
    }

    template <typename T>
    struct PolyphaseKernel {

        PolyphaseKernel(Filter<T>& filter, uint32_t srcLength, uint32_t dstLength, uint32_t samples)
            : length(dstLength)
        {
            scale = static_cast<T>(dstLength) / static_cast<T>(srcLength);
            width = filter.GetWidth() / scale;

            windowSize = static_cast<uint32_t>(std::ceil(width * static_cast<T>(2.0))) + 1;
            weights.resize(windowSize, static_cast<T>(1.0));

            T sum = static_cast<T>(0.0);
            for (uint32_t j = 0; j < windowSize; ++j) {
                auto sample = filter.Sample(static_cast<T>(j) - windowSize / T(2.0), scale, samples);
                weights[j] = sample;
                sum += sample;
            }

            for (uint32_t j = 0; j < windowSize; ++j) {
                weights[j] /= sum;
            }
        }

        void ApplyX(const ImageMipData &mipData, PixelType type, uint32_t y, uint32_t z, float *out, uint32_t components)
        {
            uint32_t baseIndex = (z * mipData.height + y) * mipData.width;
            for (uint32_t i = 0; i < length; i++)
            {
                T center = (static_cast<T>(i) + static_cast<T>(0.5)) / scale;
                auto left = static_cast<int32_t>(std::floor(center - width));
                auto right = static_cast<int32_t>(std::ceil(center + width));
                SKY_ASSERT(right - left <= static_cast<int32_t>(windowSize))

                for (uint32_t j = 0; j < components; ++j) {

                    float sum = 0;
                    for (uint32_t k = 0; k < windowSize; ++k) {
                        auto weight = weights[k];

                        int32_t tx = left + static_cast<int32_t>(k);
                        auto x = static_cast<uint32_t>(FilterClamp(tx, static_cast<int32_t>(mipData.width)));
                        uint32_t index = baseIndex + x;

                        float val = 0.f;
                        switch (type) {
                        case PixelType::U8:
                            val = U8ToF32(mipData.data.get()[index * components + j]);
                            break;
                        case PixelType::Float:
                            val = reinterpret_cast<const float *>(mipData.data.get())[index * components + j];
                            break;
                        default:
                            break;
                        }
                        sum += weight * val;
                    }

                    out[i * components + j] = sum;
                }
            }
        }

        void ApplyY(const ImageMipData &mipData, PixelType type, uint32_t x, uint32_t z, float *out, uint32_t components)
        {
            for (uint32_t i = 0; i < length; i++)
            {
                T center = (static_cast<T>(i) + static_cast<T>(0.5)) / scale;
                auto left = static_cast<int32_t>(std::floor(center - width));
                auto right = static_cast<int32_t>(std::ceil(center + width));
                SKY_ASSERT(right - left <= static_cast<int32_t>(windowSize))

                for (uint32_t j = 0; j < components; ++j) {

                    float sum = 0;
                    for (uint32_t k = 0; k < windowSize; ++k) {
                        auto weight = weights[k];

                        int32_t ty = left + static_cast<int32_t>(k);
                        auto y = static_cast<uint32_t>(FilterClamp(ty, static_cast<int32_t>(mipData.height)));
                        uint32_t index = (z * mipData.height + y) * mipData.width + x;

                        float val = 0.f;
                        switch (type) {
                        case PixelType::U8:
                            val = U8ToF32(mipData.data.get()[index * components + j]);
                            break;
                        case PixelType::Float:
                            val = reinterpret_cast<const float *>(mipData.data.get())[index * components + j];
                            break;
                        default:
                            break;
                        }

                        sum += weight * val;
                    }

                    out[i * components + j] = sum;
                }
            }
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
        case MipGenType::Box: func = std::make_unique<BoxFilter<float>>(); break;
        case MipGenType::Kaiser: func = std::make_unique<KaiserFilter<float>>(4.f, 7.f); break;
        default:
            SKY_ASSERT(false && "not implemented");
            break;
        }

        if (!func) {
            return;
        }

        PolyphaseKernel kernelX(*func, inData.width, outData.width, 32);
        PolyphaseKernel kernelY(*func, inData.height, outData.height, 32);

        ImageMipData tmpData = ImageMipData::Create(outData.width, inData.height, inData.depth, components * sizeof(float));

        PixelType type = GetPixelType(image.format);

        // filter vertical
        for (uint32_t y = 0; y < inData.height; ++y) {
            float* row = reinterpret_cast<float*>(tmpData.data.get()) + y * outData.width * components;
            kernelX.ApplyX(inData, type, y, 0, row, components);
        }

        std::vector<float> tmpCol(outData.height * components, 0.f);
        // filter horizontal
        for (uint32_t x = 0; x < outData.width; ++x) {
            kernelY.ApplyY(tmpData, PixelType::Float, x, 0, tmpCol.data(), components);

            for (uint32_t y = 0; y < outData.height; ++y) {
                uint8_t* dst = outData.data.get() + (y * outData.width + x) * image.pixelSize;
                const float* src = tmpCol.data() + y * components;

                Color color = {};
                for (uint32_t c = 0; c < components; ++c) {
                    color.v[c] = src[c];
                }
                SetImageColor(type, components, dst, color);
            }
        }
    }

    void ImageMipGen::DoWork()
    {
        SKY_ASSERT(payload.image->type == rhi::ImageType::IMAGE_2D)
        SKY_ASSERT(payload.image->depth == 1)
        SKY_ASSERT(!payload.image->mips.empty())

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