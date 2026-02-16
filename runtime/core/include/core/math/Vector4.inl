//
// Created by Zach Lee on 2022/9/10.
//

namespace sky {

    inline constexpr Vector4::Vector4() : Vector4(0, 0, 0, 0)
    {
    }

    inline constexpr Vector4::Vector4(float v) : Vector4(v, v, v, v)
    {
    }

    inline constexpr Vector4::Vector4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_)
    {
    }

    inline Vector4 Vector4::operator+(const Vector4& rhs) const
    {
        return Vector4(*this) += rhs;
    }

    inline Vector4 Vector4::operator-() const
    {
        return Vector4(0) - *this;
    }

    inline Vector4 Vector4::operator-(const Vector4& rhs) const
    {
        return Vector4(*this) -= rhs;
    }

    inline Vector4 Vector4::operator*(const Vector4& rhs) const
    {
        return Vector4(*this) *= rhs;
    }

    inline Vector4 Vector4::operator*(float m) const
    {
        return Vector4(*this) *= m;
    }

    inline Vector4 Vector4::operator/(const Vector4& rhs) const
    {
        return Vector4(*this) /= rhs;
    }

    inline Vector4 Vector4::operator/(float d) const
    {
        return Vector4(*this) /= d;
    }

    inline Vector4& Vector4::operator+=(const Vector4& rhs)
    {
#if SKY_SIMD_SSE
        simd = _mm_add_ps(simd, rhs.simd);
#elif SKY_SIMD_NEON
        simd = vaddq_f32(simd, rhs.simd);
#else
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        w += rhs.w;
#endif
        return *this;
    }

    inline Vector4& Vector4::operator-=(const Vector4& rhs)
    {
#if SKY_SIMD_SSE
        simd = _mm_sub_ps(simd, rhs.simd);
#elif SKY_SIMD_NEON
        simd = vsubq_f32(simd, rhs.simd);
#else
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        w -= rhs.w;
#endif
        return *this;
    }

    inline Vector4& Vector4::operator*=(const Vector4& rhs)
    {
#if SKY_SIMD_SSE
        simd = _mm_mul_ps(simd, rhs.simd);
#elif SKY_SIMD_NEON
        simd = vmulq_f32(simd, rhs.simd);
#else
        x *= rhs.x;
        y *= rhs.y;
        z *= rhs.z;
        w *= rhs.w;
#endif
        return *this;
    }

    inline Vector4& Vector4::operator/=(const Vector4& rhs)
    {
#if SKY_SIMD_SSE
        simd = _mm_div_ps(simd, rhs.simd);
#elif SKY_SIMD_NEON
        simd = vdivq_f32(simd, rhs.simd);
#else
        x /= rhs.x;
        y /= rhs.y;
        z /= rhs.z;
        w /= rhs.w;
#endif
        return *this;
    }

    inline Vector4& Vector4::operator*=(float m)
    {
#if SKY_SIMD_SSE
        simd = _mm_mul_ps(simd, _mm_set1_ps(m));
#elif SKY_SIMD_NEON
        simd = vmulq_n_f32(simd, m);
#else
        x *= m;
        y *= m;
        z *= m;
        w *= m;
#endif
        return *this;
    }

    inline Vector4& Vector4::operator/=(float d)
    {
#if SKY_SIMD_SSE
        simd = _mm_div_ps(simd, _mm_set1_ps(d));
#elif SKY_SIMD_NEON
        float32x4_t dv = vdupq_n_f32(d);
        simd = vdivq_f32(simd, dv);
#else
        x /= d;
        y /= d;
        z /= d;
        w /= d;
#endif
        return *this;
    }

    inline float &Vector4::operator[](uint32_t i)
    {
        return v[i];
    }

    inline float Vector4::operator[](uint32_t i) const
    {
        return v[i];
    }

    inline float Vector4::Dot(const Vector4 &rhs) const
    {
#if SKY_SIMD_SSE
        __m128 mul = _mm_mul_ps(simd, rhs.simd);
        __m128 shuf1 = _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(2, 3, 0, 1));
        __m128 sums1 = _mm_add_ps(mul, shuf1);
        __m128 shuf2 = _mm_shuffle_ps(sums1, sums1, _MM_SHUFFLE(0, 1, 2, 3));
        __m128 sums2 = _mm_add_ps(sums1, shuf2);
        return _mm_cvtss_f32(sums2);
#elif SKY_SIMD_NEON
        float32x4_t mul = vmulq_f32(simd, rhs.simd);
        float32x2_t sum = vadd_f32(vget_low_f32(mul), vget_high_f32(mul));
        sum = vpadd_f32(sum, sum);
        return vget_lane_f32(sum, 0);
#else
        Vector4 ret = (*this) * rhs;
        return (ret.x + ret.y) + (ret.z + ret.w);
#endif
    }

    inline void Vector4::Normalize()
    {
#if SKY_SIMD_SSE
        __m128 dot = _mm_mul_ps(simd, simd);
        __m128 shuf1 = _mm_shuffle_ps(dot, dot, _MM_SHUFFLE(2, 3, 0, 1));
        __m128 sums1 = _mm_add_ps(dot, shuf1);
        __m128 shuf2 = _mm_shuffle_ps(sums1, sums1, _MM_SHUFFLE(0, 1, 2, 3));
        __m128 sums2 = _mm_add_ps(sums1, shuf2);
        __m128 inv = _mm_rsqrt_ps(sums2);
        simd = _mm_mul_ps(simd, inv);
#elif SKY_SIMD_NEON
        float32x4_t mul = vmulq_f32(simd, simd);
        float32x2_t sum = vadd_f32(vget_low_f32(mul), vget_high_f32(mul));
        sum = vpadd_f32(sum, sum);
        float32x4_t dotVal = vdupq_lane_f32(sum, 0);
        float32x4_t inv = vrsqrteq_f32(dotVal);
        simd = vmulq_f32(simd, inv);
#else
        float inverseSqrt = 1 / sqrt(Dot(*this));
        Vector4::operator*=(inverseSqrt);
#endif
    }
}
