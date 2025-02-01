//
// Created by blues on 2025/1/29.
//

#pragma once

namespace sky::builder {

    namespace filter {

        template <typename T>
        inline T Square(T x)
        {
            return x * x;
        }

        template <typename T>
        T Bessel_i0(T x)
        {
            static constexpr T EPSILON = static_cast<T>(1e-6);

            const T c = filter::Square(x) / static_cast<T>(4.0);
            T sum = static_cast<T>(1.0);
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
            T inv = static_cast<T>(1.0) / static_cast<T>(samples);

            for(int i = 0; i < samples; ++i)
            {
                T p = (pos + (static_cast<T>(i) + static_cast<T>(0.5)) * inv) * scale;
                T value = Eval(p);
                sum += value;
            }

            return static_cast<T>(sum * inv);
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
            , i0_piA(static_cast<T>(1.0) / filter::Bessel_i0(piA_))
        {
        }

        ~KaiserFilter() override = default;

    private:
        T Eval(T pos) override
        {
            auto v1 = static_cast<T>(piA * sqrt(1.0 - filter::Square(pos / this->width)));
            return i0_piA * filter::Bessel_i0(v1);
        }

        T piA;
        T i0_piA;
    };
} // namespace sky::builder
