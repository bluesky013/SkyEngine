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
#if SKY_SIMD_ENABLED
        SFloat4 a(simd);
        SFloat4 b(rhs.simd);
        simd = (a += b).value;
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
#if SKY_SIMD_ENABLED
        SFloat4 a(simd);
        SFloat4 b(rhs.simd);
        simd = (a -= b).value;
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
#if SKY_SIMD_ENABLED
        SFloat4 a(simd);
        SFloat4 b(rhs.simd);
        simd = (a *= b).value;
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
#if SKY_SIMD_ENABLED
        SFloat4 a(simd);
        SFloat4 b(rhs.simd);
        simd = (a /= b).value;
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
#if SKY_SIMD_ENABLED
        SFloat4 a(simd);
        SFloat4 b = SFloat4::Splat(m);
        simd = (a *= b).value;
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
#if SKY_SIMD_ENABLED
        SFloat4 a(simd);
        SFloat4 b = SFloat4::Splat(d);
        simd = (a /= b).value;
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
#if SKY_SIMD_ENABLED
        SFloat4 a(simd);
        SFloat4 b(rhs.simd);
        return SFloat4::HorizontalSum(a * b);
#else
        Vector4 ret = (*this) * rhs;
        return (ret.x + ret.y) + (ret.z + ret.w);
#endif
    }

    inline void Vector4::Normalize()
    {
#if SKY_SIMD_ENABLED
        SFloat4 a(simd);
        SFloat4 dotSplat = SFloat4::HorizontalSumSplat(a * a);
        SFloat4 inv = SFloat4::InvSqrt(dotSplat);
        simd = (a * inv).value;
#else
        float inverseSqrt = 1 / sqrt(Dot(*this));
        Vector4::operator*=(inverseSqrt);
#endif
    }
}
