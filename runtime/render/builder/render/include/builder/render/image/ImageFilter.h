//
// Created by blues on 2025/1/29.
//

#pragma once

#include <core/math/Math.h>

namespace sky::builder {

    namespace filter {

        template <typename T>
        inline T Square(T x)
        {
            return x * x;
        }

        template <typename T>
        inline float Sinc(const T x)
        {
            if (fabs(x) < T(0.0001)) {
                return T(1.0) + x * x * (T(-1.0 / 6.0) + x * x * T(1.0 / 120.0));
            }

            return sin(x) / x;
        }

        template <typename T>
        T Bessel_i0(T x)
        {
            static constexpr T EPSILON = T(1e-6);

            const T c = filter::Square(x) / T(4.0);
            T sum = T(1.0);
            T t = c;

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
        explicit Filter(T w) : width(w) {}
        virtual ~Filter() = default;

        virtual T Eval(T pos) = 0;

        T Sample(T pos, T scale, int samples)
        {
            double sum = 0;
            T inv = T(1.0) / T(samples);

            for(int i = 0; i < samples; ++i)
            {
                T p = (pos + (T(i) + T(0.5)) * inv) * scale;
                T value = Eval(p);
                sum += value;
            }

            return T(sum * inv);
        }

        T GetWidth() const { return width; }
    protected:
        T width = 1.f;
    };

    template <typename T>
    class KaiserFilter : public Filter<T> {
    public:
        explicit KaiserFilter(T piA_, T w)
            : Filter<T>(w)
            , piA(piA_)
            , i0_piA(T(1.0) / filter::Bessel_i0(piA_))
        {
        }

        ~KaiserFilter() override = default;

    private:
        T Eval(T pos) override
        {
            float t  = pos / this->width;
            float t2 = t * t;

            if (t2 < 1.0f) {
                return filter::Sinc(PI * pos) * filter::Bessel_i0(piA * sqrtf(1.0f - t2)) * i0_piA;
            }
            return 0.0f;

//            const float sinc = filter::Sinc(PI * pos);
//            auto v1 = T(piA * sqrt(1.0 - filter::Square(pos / this->width)));
//            return i0_piA * filter::Bessel_i0(sinc * v1);
        }

        T piA;
        T i0_piA;
    };

    template <typename T>
    class BoxFilter : public Filter<T> {
    public:
        BoxFilter() : Filter<T>(T(0.5)) {}

        T Eval(T pos) override
        {
            if (fabsf(pos) <= this->width) {
                return 1.0f;
            }
            return 0.0f;
        }
    };
} // namespace sky::builder
