//
// Created by blues on 2025/2/1.
//

#include <builder/render/image/ImageResizer.h>
#include <builder/render/image/ImageFilter.h>
#include <core/platform/Platform.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace sky::builder {

    // ---------- Polyphase 1-D kernel (same algorithm as ImageMipGen) ----------
    namespace {

        struct PolyphaseKernel1D {
            PolyphaseKernel1D(Filter<float> &filter,
                              uint32_t srcLen,
                              uint32_t dstLen,
                              uint32_t samples = 32)
                : length(dstLen)
            {
                scale = static_cast<float>(dstLen) / static_cast<float>(srcLen);
                width = filter.GetWidth() / scale;

                windowSize = static_cast<uint32_t>(std::ceil(width * 2.f)) + 1;
                weights.resize(windowSize, 1.f);

                float sum = 0.f;
                for (uint32_t j = 0; j < windowSize; ++j) {
                    float s = filter.Sample(static_cast<float>(j) - windowSize / 2.f,
                                            scale, static_cast<int>(samples));
                    weights[j] = s;
                    sum += s;
                }
                for (auto &w : weights) {
                    w /= sum;
                }
            }

            // Resample a row (X direction) of the source mip into float output.
            void ApplyX(const ImageMipData &src, PixelType type,
                        uint32_t y, uint32_t z,
                        float *out, uint32_t components) const
            {
                uint32_t baseIndex = (z * src.height + y) * src.width;
                for (uint32_t i = 0; i < length; ++i) {
                    float center = (static_cast<float>(i) + 0.5f) / scale;
                    auto left = static_cast<int32_t>(std::floor(center - width));

                    for (uint32_t c = 0; c < components; ++c) {
                        float acc = 0.f;
                        for (uint32_t k = 0; k < windowSize; ++k) {
                            int32_t tx = left + static_cast<int32_t>(k);
                            auto sx = static_cast<uint32_t>(
                                std::clamp(tx, 0, static_cast<int32_t>(src.width) - 1));
                            uint32_t idx = baseIndex + sx;

                            float val = 0.f;
                            switch (type) {
                            case PixelType::U8:
                                val = U8ToF32(src.data.get()[idx * components + c]);
                                break;
                            case PixelType::Float:
                                val = reinterpret_cast<const float *>(
                                    src.data.get())[idx * components + c];
                                break;
                            default:
                                break;
                            }
                            acc += weights[k] * val;
                        }
                        out[i * components + c] = acc;
                    }
                }
            }

            // Resample a column (Y direction) of a float-typed tmp buffer.
            void ApplyY(const ImageMipData &src, PixelType type,
                        uint32_t x, uint32_t z,
                        float *out, uint32_t components) const
            {
                for (uint32_t i = 0; i < length; ++i) {
                    float center = (static_cast<float>(i) + 0.5f) / scale;
                    auto left = static_cast<int32_t>(std::floor(center - width));

                    for (uint32_t c = 0; c < components; ++c) {
                        float acc = 0.f;
                        for (uint32_t k = 0; k < windowSize; ++k) {
                            int32_t ty = left + static_cast<int32_t>(k);
                            auto sy = static_cast<uint32_t>(
                                std::clamp(ty, 0, static_cast<int32_t>(src.height) - 1));
                            uint32_t idx = (z * src.height + sy) * src.width + x;

                            float val = 0.f;
                            switch (type) {
                            case PixelType::U8:
                                val = U8ToF32(src.data.get()[idx * components + c]);
                                break;
                            case PixelType::Float:
                                val = reinterpret_cast<const float *>(
                                    src.data.get())[idx * components + c];
                                break;
                            default:
                                break;
                            }
                            acc += weights[k] * val;
                        }
                        out[i * components + c] = acc;
                    }
                }
            }

            float    scale;
            float    width;
            uint32_t length;
            uint32_t windowSize;
            std::vector<float> weights;
        };

        void ResampleImage(const ImageObject &image,
                           const ImageMipData &srcMip,
                           ImageMipData &dstMip,
                           MipGenType filterType)
        {
            uint32_t components = image.components;

            std::unique_ptr<Filter<float>> filter;
            switch (filterType) {
            case MipGenType::Box:
                filter = std::make_unique<BoxFilter<float>>();
                break;
            case MipGenType::Kaiser:
                filter = std::make_unique<KaiserFilter<float>>(4.f, 7.f);
                break;
            default:
                SKY_ASSERT(false && "unsupported filter type");
                return;
            }

            PolyphaseKernel1D kernelX(*filter, srcMip.width,  dstMip.width);
            PolyphaseKernel1D kernelY(*filter, srcMip.height, dstMip.height);

            PixelType type = GetPixelType(image.format);

            // Two-pass separable filter: X first into a float temp, then Y.
            // Temp buffer: dstWidth ¡Á srcHeight, float components.
            ImageMipData tmpData = ImageMipData::Create(
                dstMip.width, srcMip.height, srcMip.depth,
                components * static_cast<uint32_t>(sizeof(float)));

            // Pass 1 ¡ª horizontal
            for (uint32_t z = 0; z < srcMip.depth; ++z) {
                for (uint32_t y = 0; y < srcMip.height; ++y) {
                    float *row = reinterpret_cast<float *>(tmpData.data.get())
                        + (z * srcMip.height + y) * dstMip.width * components;
                    kernelX.ApplyX(srcMip, type, y, z, row, components);
                }
            }

            // Pass 2 ¡ª vertical
            std::vector<float> tmpCol(dstMip.height * components, 0.f);
            for (uint32_t z = 0; z < srcMip.depth; ++z) {
                for (uint32_t x = 0; x < dstMip.width; ++x) {
                    kernelY.ApplyY(tmpData, PixelType::Float, x, z, tmpCol.data(), components);

                    for (uint32_t y = 0; y < dstMip.height; ++y) {
                        uint8_t *dst = dstMip.data.get()
                            + ((z * dstMip.height + y) * dstMip.width + x) * image.pixelSize;
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

    } // anonymous namespace

    void ImageResizer::DoWork()
    {
        auto &image = payload.image;
        SKY_ASSERT(image && !image->mips.empty());

        uint32_t srcW = image->width;
        uint32_t srcH = image->height;

        // Nothing to do if already within limits.
        if (srcW <= payload.maxWidth && srcH <= payload.maxHeight) {
            return;
        }

        // Compute target size preserving aspect ratio.
        float scaleW = static_cast<float>(payload.maxWidth)  / static_cast<float>(srcW);
        float scaleH = static_cast<float>(payload.maxHeight) / static_cast<float>(srcH);
        float scale  = std::min(scaleW, scaleH);

        uint32_t dstW = std::max(1u, static_cast<uint32_t>(std::floor(srcW * scale)));
        uint32_t dstH = std::max(1u, static_cast<uint32_t>(std::floor(srcH * scale)));

        // Clamp to exact limits (floating-point floor may overshoot by 1).
        dstW = std::min(dstW, payload.maxWidth);
        dstH = std::min(dstH, payload.maxHeight);

        // Only resample mip 0; discard any existing mip chain.
        ImageMipData &srcMip = image->mips[0];
        ImageMipData dstMip = ImageMipData::Create(dstW, dstH, srcMip.depth, image->pixelSize);

        ResampleImage(*image, srcMip, dstMip, payload.filterType);

        // Replace the image contents.
        image->width  = dstW;
        image->height = dstH;
        image->mips.clear();
        image->mips.emplace_back(std::move(dstMip));
    }

} // namespace sky::builder
