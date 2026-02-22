//
// Created by Zach Lee on 2022/9/10.
//

namespace sky {

    FORCEINLINE constexpr Vector4::Vector4() : Vector4(0, 0, 0, 0)
    {
    }

    FORCEINLINE constexpr Vector4::Vector4(float v) : Vector4(v, v, v, v)
    {
    }

    FORCEINLINE constexpr Vector4::Vector4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_)
    {
    }

#if SKY_SIMD_SSE
    // SSE implementation for addition
    FORCEINLINE Vector4 Vector4::operator+(const Vector4& rhs) const
    {
        Vector4 result;
        result.simd = _mm_add_ps(simd, rhs.simd);
        return result;
    }
#else
    FORCEINLINE Vector4 Vector4::operator+(const Vector4& rhs) const
    {
        return Vector4(*this) += rhs;
    }
#endif

    FORCEINLINE Vector4 Vector4::operator-() const
    {
        return Vector4(0) - *this;
    }

#if SKY_SIMD_SSE
    // SSE implementation for subtraction
    FORCEINLINE Vector4 Vector4::operator-(const Vector4& rhs) const
    {
        Vector4 result;
        result.simd = _mm_sub_ps(simd, rhs.simd);
        return result;
    }
#else
    FORCEINLINE Vector4 Vector4::operator-(const Vector4& rhs) const
    {
        return Vector4(*this) -= rhs;
    }
#endif

#if SKY_SIMD_SSE
    // SSE implementation for element-wise multiplication
    FORCEINLINE Vector4 Vector4::operator*(const Vector4& rhs) const
    {
        Vector4 result;
        result.simd = _mm_mul_ps(simd, rhs.simd);
        return result;
    }
#else
    FORCEINLINE Vector4 Vector4::operator*(const Vector4& rhs) const
    {
        return Vector4(*this) *= rhs;
    }
#endif

#if SKY_SIMD_SSE
    // SSE implementation for scalar multiplication
    FORCEINLINE Vector4 Vector4::operator*(float m) const
    {
        Vector4 result;
        __m128 scalar = _mm_set1_ps(m);
        result.simd = _mm_mul_ps(simd, scalar);
        return result;
    }
#else
    FORCEINLINE Vector4 Vector4::operator*(float m) const
    {
        return Vector4(*this) *= m;
    }
#endif

#if SKY_SIMD_SSE
    // SSE implementation for element-wise division
    FORCEINLINE Vector4 Vector4::operator/(const Vector4& rhs) const
    {
        Vector4 result;
        result.simd = _mm_div_ps(simd, rhs.simd);
        return result;
    }
#else
    FORCEINLINE Vector4 Vector4::operator/(const Vector4& rhs) const
    {
        return Vector4(*this) /= rhs;
    }
#endif

#if SKY_SIMD_SSE
    // SSE implementation for scalar division
    FORCEINLINE Vector4 Vector4::operator/(float d) const
    {
        Vector4 result;
        __m128 divisor = _mm_set1_ps(d);
        result.simd = _mm_div_ps(simd, divisor);
        return result;
    }
#else
    FORCEINLINE Vector4 Vector4::operator/(float d) const
    {
        return Vector4(*this) /= d;
    }
#endif

#if SKY_SIMD_SSE
    // SSE implementation for addition assignment
    FORCEINLINE Vector4& Vector4::operator+=(const Vector4& rhs)
    {
        simd = _mm_add_ps(simd, rhs.simd);
        return *this;
    }
#else
    FORCEINLINE Vector4& Vector4::operator+=(const Vector4& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        w += rhs.w;
        return *this;
    }
#endif

#if SKY_SIMD_SSE
    // SSE implementation for subtraction assignment
    FORCEINLINE Vector4& Vector4::operator-=(const Vector4& rhs)
    {
        simd = _mm_sub_ps(simd, rhs.simd);
        return *this;
    }
#else
    FORCEINLINE Vector4& Vector4::operator-=(const Vector4& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        w -= rhs.w;
        return *this;
    }
#endif

#if SKY_SIMD_SSE
    // SSE implementation for element-wise multiplication assignment
    FORCEINLINE Vector4& Vector4::operator*=(const Vector4& rhs)
    {
        simd = _mm_mul_ps(simd, rhs.simd);
        return *this;
    }
#else
    FORCEINLINE Vector4& Vector4::operator*=(const Vector4& rhs)
    {
        x *= rhs.x;
        y *= rhs.y;
        z *= rhs.z;
        w *= rhs.w;
        return *this;
    }
#endif

#if SKY_SIMD_SSE
    // SSE implementation for element-wise division assignment
    FORCEINLINE Vector4& Vector4::operator/=(const Vector4& rhs)
    {
        simd = _mm_div_ps(simd, rhs.simd);
        return *this;
    }
#else
    FORCEINLINE Vector4& Vector4::operator/=(const Vector4& rhs)
    {
        x /= rhs.x;
        y /= rhs.y;
        z /= rhs.z;
        w /= rhs.w;
        return *this;
    }
#endif

#if SKY_SIMD_SSE
    // SSE implementation for scalar multiplication assignment
    FORCEINLINE Vector4& Vector4::operator*=(float m)
    {
        __m128 scalar = _mm_set1_ps(m);
        simd = _mm_mul_ps(simd, scalar);
        return *this;
    }
#else
    FORCEINLINE Vector4& Vector4::operator*=(float m)
    {
        x *= m;
        y *= m;
        z *= m;
        w *= m;
        return *this;
    }
#endif

#if SKY_SIMD_SSE
    // SSE implementation for scalar division assignment
    FORCEINLINE Vector4& Vector4::operator/=(float d)
    {
        __m128 divisor = _mm_set1_ps(d);
        simd = _mm_div_ps(simd, divisor);
        return *this;
    }
#else
    FORCEINLINE Vector4& Vector4::operator/=(float d)
    {
        x /= d;
        y /= d;
        z /= d;
        w /= d;
        return *this;
    }
#endif

    FORCEINLINE float &Vector4::operator[](uint32_t i)
    {
        return v[i];
    }

    FORCEINLINE float Vector4::operator[](uint32_t i) const
    {
        return v[i];
    }

#if SKY_SIMD_SSE
    // SSE implementation for dot product
    FORCEINLINE float Vector4::Dot(const Vector4 &rhs) const
    {
        // Multiply corresponding elements
        __m128 mul = _mm_mul_ps(simd, rhs.simd);

        // Horizontal sum using shuffle and add
        // Shuffle: [z, w, x, y] from [x, y, z, w]
        __m128 shuf = _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(2, 3, 0, 1));
        // Add paired elements
        __m128 sum1 = _mm_add_ps(mul, shuf);

        // Shuffle again: [y, x, w, z] from [x+z, y+w, z+x, w+y]
        shuf = _mm_shuffle_ps(sum1, sum1, _MM_SHUFFLE(1, 0, 3, 2));
        // Final add to get sum of all elements
        __m128 sum2 = _mm_add_ps(sum1, shuf);

        // Extract scalar result
        return _mm_cvtss_f32(sum2);
    }
#else
    FORCEINLINE float Vector4::Dot(const Vector4 &rhs) const
    {
        Vector4 ret = (*this) * rhs;
        return (ret.x + ret.y) + (ret.z + ret.w);
    }
#endif

#if SKY_SIMD_SSE
    // SSE implementation for normalization
    FORCEINLINE void Vector4::Normalize()
    {
        // Calculate dot product (magnitude squared)
        float magSq = Dot(*this);
        float inverseSqrt = 1.0f / sqrt(magSq);

        // Multiply by inverse square root
        __m128 invSqrtVec = _mm_set1_ps(inverseSqrt);
        simd = _mm_mul_ps(simd, invSqrtVec);
    }
#else
    FORCEINLINE void Vector4::Normalize()
    {
        float inverseSqrt = 1 / sqrt(Dot(*this));
        Vector4::operator*=(inverseSqrt);
    }
#endif
}
