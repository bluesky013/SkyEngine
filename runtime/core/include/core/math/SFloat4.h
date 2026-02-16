//
// Created for SIMD math support.
//

#pragma once

#include "core/math/MathSimd.h"
#include <cmath>

namespace sky {

#if SKY_SIMD_SSE

    struct SFloat4 {
        __m128 value;

        SFloat4() = default;
        explicit SFloat4(__m128 v) : value(v) {}

        static inline SFloat4 Load(const float *p) { return SFloat4(_mm_loadu_ps(p)); }
        static inline SFloat4 Set(float x, float y, float z, float w) { return SFloat4(_mm_set_ps(w, z, y, x)); }
        static inline SFloat4 Splat(float v) { return SFloat4(_mm_set1_ps(v)); }

        inline void Store(float *p) const { _mm_storeu_ps(p, value); }

        inline SFloat4 operator+(const SFloat4 &rhs) const { return SFloat4(_mm_add_ps(value, rhs.value)); }
        inline SFloat4 operator-(const SFloat4 &rhs) const { return SFloat4(_mm_sub_ps(value, rhs.value)); }
        inline SFloat4 operator*(const SFloat4 &rhs) const { return SFloat4(_mm_mul_ps(value, rhs.value)); }
        inline SFloat4 operator/(const SFloat4 &rhs) const { return SFloat4(_mm_div_ps(value, rhs.value)); }

        inline SFloat4 &operator+=(const SFloat4 &rhs) { value = _mm_add_ps(value, rhs.value); return *this; }
        inline SFloat4 &operator-=(const SFloat4 &rhs) { value = _mm_sub_ps(value, rhs.value); return *this; }
        inline SFloat4 &operator*=(const SFloat4 &rhs) { value = _mm_mul_ps(value, rhs.value); return *this; }
        inline SFloat4 &operator/=(const SFloat4 &rhs) { value = _mm_div_ps(value, rhs.value); return *this; }

        static inline float HorizontalSum(const SFloat4 &v)
        {
            __m128 shuf1 = _mm_shuffle_ps(v.value, v.value, _MM_SHUFFLE(2, 3, 0, 1));
            __m128 sums1 = _mm_add_ps(v.value, shuf1);
            __m128 shuf2 = _mm_shuffle_ps(sums1, sums1, _MM_SHUFFLE(0, 1, 2, 3));
            __m128 sums2 = _mm_add_ps(sums1, shuf2);
            return _mm_cvtss_f32(sums2);
        }

        static inline SFloat4 HorizontalSumSplat(const SFloat4 &v)
        {
            __m128 shuf1 = _mm_shuffle_ps(v.value, v.value, _MM_SHUFFLE(2, 3, 0, 1));
            __m128 sums1 = _mm_add_ps(v.value, shuf1);
            __m128 shuf2 = _mm_shuffle_ps(sums1, sums1, _MM_SHUFFLE(0, 1, 2, 3));
            return SFloat4(_mm_add_ps(sums1, shuf2));
        }

        static inline SFloat4 InvSqrt(const SFloat4 &v)
        {
            __m128 inv = _mm_rsqrt_ps(v.value);
            // Newton-Raphson refinement: inv = inv * (1.5 - 0.5 * v * inv * inv)
            __m128 half = _mm_set1_ps(0.5f);
            __m128 three_half = _mm_set1_ps(1.5f);
            __m128 muls = _mm_mul_ps(_mm_mul_ps(half, v.value), _mm_mul_ps(inv, inv));
            inv = _mm_mul_ps(inv, _mm_sub_ps(three_half, muls));
            return SFloat4(inv);
        }
    };

#elif SKY_SIMD_NEON

    struct SFloat4 {
        float32x4_t value;

        SFloat4() = default;
        explicit SFloat4(float32x4_t v) : value(v) {}

        static inline SFloat4 Load(const float *p) { return SFloat4(vld1q_f32(p)); }
        static inline SFloat4 Set(float x, float y, float z, float w)
        {
            float data[4] = {x, y, z, w};
            return SFloat4(vld1q_f32(data));
        }
        static inline SFloat4 Splat(float v) { return SFloat4(vdupq_n_f32(v)); }

        inline void Store(float *p) const { vst1q_f32(p, value); }

        inline SFloat4 operator+(const SFloat4 &rhs) const { return SFloat4(vaddq_f32(value, rhs.value)); }
        inline SFloat4 operator-(const SFloat4 &rhs) const { return SFloat4(vsubq_f32(value, rhs.value)); }
        inline SFloat4 operator*(const SFloat4 &rhs) const { return SFloat4(vmulq_f32(value, rhs.value)); }
        inline SFloat4 operator/(const SFloat4 &rhs) const { return SFloat4(vdivq_f32(value, rhs.value)); }

        inline SFloat4 &operator+=(const SFloat4 &rhs) { value = vaddq_f32(value, rhs.value); return *this; }
        inline SFloat4 &operator-=(const SFloat4 &rhs) { value = vsubq_f32(value, rhs.value); return *this; }
        inline SFloat4 &operator*=(const SFloat4 &rhs) { value = vmulq_f32(value, rhs.value); return *this; }
        inline SFloat4 &operator/=(const SFloat4 &rhs) { value = vdivq_f32(value, rhs.value); return *this; }

        static inline float HorizontalSum(const SFloat4 &v)
        {
            float32x2_t sum = vadd_f32(vget_low_f32(v.value), vget_high_f32(v.value));
            sum = vpadd_f32(sum, sum);
            return vget_lane_f32(sum, 0);
        }

        static inline SFloat4 HorizontalSumSplat(const SFloat4 &v)
        {
            float32x2_t sum = vadd_f32(vget_low_f32(v.value), vget_high_f32(v.value));
            sum = vpadd_f32(sum, sum);
            return SFloat4(vdupq_lane_f32(sum, 0));
        }

        static inline SFloat4 InvSqrt(const SFloat4 &v)
        {
            float32x4_t inv = vrsqrteq_f32(v.value);
            // Newton-Raphson refinement
            inv = vmulq_f32(inv, vrsqrtsq_f32(vmulq_f32(v.value, inv), inv));
            return SFloat4(inv);
        }
    };

#else

    struct SFloat4 {
        float value[4];

        SFloat4() = default;

        static inline SFloat4 Load(const float *p) { SFloat4 r; r.value[0] = p[0]; r.value[1] = p[1]; r.value[2] = p[2]; r.value[3] = p[3]; return r; }
        static inline SFloat4 Set(float x, float y, float z, float w) { SFloat4 r; r.value[0] = x; r.value[1] = y; r.value[2] = z; r.value[3] = w; return r; }
        static inline SFloat4 Splat(float v) { SFloat4 r; r.value[0] = r.value[1] = r.value[2] = r.value[3] = v; return r; }

        inline void Store(float *p) const { p[0] = value[0]; p[1] = value[1]; p[2] = value[2]; p[3] = value[3]; }

        inline SFloat4 operator+(const SFloat4 &rhs) const { return SFloat4(*this) += rhs; }
        inline SFloat4 operator-(const SFloat4 &rhs) const { return SFloat4(*this) -= rhs; }
        inline SFloat4 operator*(const SFloat4 &rhs) const { return SFloat4(*this) *= rhs; }
        inline SFloat4 operator/(const SFloat4 &rhs) const { return SFloat4(*this) /= rhs; }

        inline SFloat4 &operator+=(const SFloat4 &rhs) { for (int i = 0; i < 4; ++i) value[i] += rhs.value[i]; return *this; }
        inline SFloat4 &operator-=(const SFloat4 &rhs) { for (int i = 0; i < 4; ++i) value[i] -= rhs.value[i]; return *this; }
        inline SFloat4 &operator*=(const SFloat4 &rhs) { for (int i = 0; i < 4; ++i) value[i] *= rhs.value[i]; return *this; }
        inline SFloat4 &operator/=(const SFloat4 &rhs) { for (int i = 0; i < 4; ++i) value[i] /= rhs.value[i]; return *this; }

        static inline float HorizontalSum(const SFloat4 &v)
        {
            return (v.value[0] + v.value[1]) + (v.value[2] + v.value[3]);
        }

        static inline SFloat4 HorizontalSumSplat(const SFloat4 &v)
        {
            return Splat(HorizontalSum(v));
        }

        static inline SFloat4 InvSqrt(const SFloat4 &v)
        {
            SFloat4 r;
            for (int i = 0; i < 4; ++i) {
                r.value[i] = 1.0f / std::sqrt(v.value[i]);
            }
            return r;
        }
    };

#endif

} // namespace sky
