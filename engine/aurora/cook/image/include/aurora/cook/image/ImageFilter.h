//
// Resampling filters and the separable polyphase kernel used by mip generation
// and resize. Ported from the legacy render builder image pipeline.
//

#pragma once

#include <aurora/cook/image/ImageProcess.h>
#include <core/math/Math.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace sky::aurora::cook {

    namespace filter {

        template <typename T>
        inline T Square(T x)
        {
            return x * x;
        }

        template <typename T>
        inline T Sinc(const T x)
        {
            if (std::fabs(x) < T(0.0001)) {
                return T(1.0) + x * x * (T(-1.0 / 6.0) + x * x * T(1.0 / 120.0));
            }
            return std::sin(x) / x;
        }

        template <typename T>
        T Bessel_i0(T x)
        {
            static constexpr T EPSILON = T(1e-6);

            const T c = Square(x) / T(4.0);
            T sum     = T(1.0);
            T t       = c;

            for (int i = 2; t > EPSILON; ++i) {
                sum += t;
                t *= c / (i * i);
            }
            return sum;
        }

    } // namespace filter

    template <typename T>
    class Filter {
    public:
        explicit Filter(T w)
            : width(w)
        {
        }
        virtual ~Filter() = default;

        virtual T Eval(T pos) = 0;

        T Sample(T pos, T scale, int samples)
        {
            double sum = 0;
            T      inv = T(1.0) / T(samples);

            for (int i = 0; i < samples; ++i) {
                T p     = (pos + (T(i) + T(0.5)) * inv) * scale;
                T value = Eval(p);
                sum += value;
            }
            return T(sum * inv);
        }

        T GetWidth() const { return width; }

    protected:
        T width = T(1.0);
    };

    template <typename T>
    class KaiserFilter : public Filter<T> {
    public:
        KaiserFilter(T piA, T w)
            : Filter<T>(w)
            , piA(piA)
            , i0_piA(T(1.0) / filter::Bessel_i0(piA))
        {
        }

        ~KaiserFilter() override = default;

    private:
        T Eval(T pos) override
        {
            T t  = pos / this->width;
            T t2 = t * t;

            if (t2 < T(1.0)) {
                return filter::Sinc(T(PI) * pos) * filter::Bessel_i0(piA * std::sqrt(T(1.0) - t2)) * i0_piA;
            }
            return T(0.0);
        }

        T piA;
        T i0_piA;
    };

    template <typename T>
    class BoxFilter : public Filter<T> {
    public:
        BoxFilter()
            : Filter<T>(T(0.5))
        {
        }

        T Eval(T pos) override
        {
            if (std::fabs(pos) <= this->width) {
                return T(1.0);
            }
            return T(0.0);
        }
    };

    // Lanczos windowed sinc: L(x) = sinc(x) * sinc(x / a), support |x| < a.
    // a = 3 (Lanczos3) is the common quality/perf default.
    template <typename T>
    class LanczosFilter : public Filter<T> {
    public:
        explicit LanczosFilter(T a)
            : Filter<T>(a)
        {
        }

        ~LanczosFilter() override = default;

    private:
        T Eval(T pos) override
        {
            if (std::fabs(pos) >= this->width) {
                return T(0.0);
            }
            return filter::Sinc(T(PI) * pos) * filter::Sinc(T(PI) * pos / this->width);
        }
    };

    inline std::unique_ptr<Filter<float>> MakeFilter(MipGenType type)
    {
        switch (type) {
        case MipGenType::Box:
            return std::make_unique<BoxFilter<float>>();
        case MipGenType::Kaiser:
            return std::make_unique<KaiserFilter<float>>(4.f, 7.f);
        case MipGenType::Lanczos3:
            return std::make_unique<LanczosFilter<float>>(3.f);
        }
        return nullptr;
    }

    // Separable polyphase resampler. Weights are built per destination sample
    // from the true source-space tap distance (tap - center) and normalized,
    // so the kernel is correctly centred. A shared window anchored at `left`
    // (the legacy layout) shifted every tap by up to half a pixel.
    template <typename T>
    struct PolyphaseKernel {
        PolyphaseKernel(Filter<T> &filter, uint32_t srcLength, uint32_t dstLength, uint32_t samples)
            : scale(static_cast<T>(dstLength) / static_cast<T>(srcLength))
            , width(filter.GetWidth() / scale)
            , sampleCount(samples)
        {
            windowSize = static_cast<uint32_t>(std::ceil(width * T(2.0))) + 2;

            offsets.resize(dstLength);
            weights.resize(dstLength);

            for (uint32_t i = 0; i < dstLength; ++i) {
                const T    center = (static_cast<T>(i) + T(0.5)) / scale;
                const auto left   = static_cast<int32_t>(std::floor(center - width));
                offsets[i]        = left;

                auto &row = weights[i];
                row.resize(windowSize);
                T sum = T(0.0);
                for (uint32_t k = 0; k < windowSize; ++k) {
                    const T    distance = (static_cast<T>(left) + static_cast<T>(k)) - center;
                    const auto value    = filter.Sample(distance, scale, static_cast<int>(sampleCount));
                    row[k]              = value;
                    sum += value;
                }
                if (sum != T(0.0)) {
                    for (auto &value : row) {
                        value /= sum;
                    }
                }
            }
        }

        static float Fetch(const ImageMipData &mipData, PixelType type, uint32_t index, uint32_t component, uint32_t components)
        {
            const uint8_t *base = mipData.data.get();
            const uint32_t stride = index * components + component;
            switch (type) {
            case PixelType::U8:
                return U8ToF32(base[stride]);
            case PixelType::HALF:
                return HalfToFloat(reinterpret_cast<const uint16_t *>(base)[stride]);
            case PixelType::Float:
                return reinterpret_cast<const float *>(base)[stride];
            default:
                break;
            }
            return 0.f;
        }

        void ApplyX(const ImageMipData &mipData, PixelType type, uint32_t y, uint32_t z, float *out, uint32_t components) const
        {
            const uint32_t base = (z * mipData.height + y) * mipData.width;

            for (uint32_t i = 0; i < offsets.size(); ++i) {
                const auto    &row  = weights[i];
                const int32_t  left = offsets[i];

                for (uint32_t j = 0; j < components; ++j) {
                    float sum = 0.f;
                    for (uint32_t k = 0; k < windowSize; ++k) {
                        int32_t tx = left + static_cast<int32_t>(k);
                        auto    x  = static_cast<uint32_t>(FilterClamp(tx, static_cast<int32_t>(mipData.width)));
                        sum += row[k] * Fetch(mipData, type, base + x, j, components);
                    }
                    out[i * components + j] = sum;
                }
            }
        }

        void ApplyY(const ImageMipData &mipData, PixelType type, uint32_t x, uint32_t z, float *out, uint32_t components) const
        {
            for (uint32_t i = 0; i < offsets.size(); ++i) {
                const auto    &row  = weights[i];
                const int32_t  left = offsets[i];

                for (uint32_t j = 0; j < components; ++j) {
                    float sum = 0.f;
                    for (uint32_t k = 0; k < windowSize; ++k) {
                        int32_t ty = left + static_cast<int32_t>(k);
                        auto    y  = static_cast<uint32_t>(FilterClamp(ty, static_cast<int32_t>(mipData.height)));
                        sum += row[k] * Fetch(mipData, type, (z * mipData.height + y) * mipData.width + x, j, components);
                    }
                    out[i * components + j] = sum;
                }
            }
        }

    private:
        static int32_t FilterClamp(int32_t x, int32_t w)
        {
            return std::clamp(x, 0, w - 1);
        }

        T scale       = T(1.0);
        T width       = T(1.0);
        uint32_t sampleCount = 0;
        uint32_t windowSize  = 0;

        std::vector<int32_t>        offsets;
        std::vector<std::vector<T>> weights;
    };

} // namespace sky::aurora::cook
