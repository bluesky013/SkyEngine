//
// Created by Zach Lee on 2021/11/12.
//


#pragma once

#include "core/math/Math.h"

namespace sky {

    struct alignas(16) Vector4 {
        union {
            float v[4];
            struct {
                float x;
                float y;
                float z;
                float w;
            };
#if SKY_SIMD_SSE
            __m128 simd;
#elif SKY_SIMD_NEON
            float32x4_t  simd;
#endif
        };

        using BaseType = float;

        FORCEINLINE constexpr Vector4();
        FORCEINLINE constexpr Vector4(float v); // NOLINT
        FORCEINLINE constexpr Vector4(float x_, float y_, float z_, float w_);

        FORCEINLINE void Normalize();
        FORCEINLINE float Dot(const Vector4 &rhs) const;

        FORCEINLINE Vector4 operator+(const Vector4& rhs) const;

        FORCEINLINE Vector4 operator-() const;
        FORCEINLINE Vector4 operator-(const Vector4& rhs) const;

        FORCEINLINE Vector4 operator*(const Vector4& rhs) const;
        FORCEINLINE Vector4 operator*(float m) const;

        FORCEINLINE Vector4 operator/(const Vector4& rhs) const;
        FORCEINLINE Vector4 operator/(float d) const;

        FORCEINLINE Vector4& operator+=(const Vector4& rhs);
        FORCEINLINE Vector4& operator-=(const Vector4& rhs);
        FORCEINLINE Vector4& operator*=(const Vector4& rhs);
        FORCEINLINE Vector4& operator/=(const Vector4& rhs);
        FORCEINLINE Vector4& operator*=(float m);
        FORCEINLINE Vector4& operator/=(float d);

        FORCEINLINE float &operator[](uint32_t i);
        FORCEINLINE float operator[](uint32_t i) const;
    };

    template <>
    struct VectorTraits<Vector4> {
        using BaseType = Vector4::BaseType;
        static constexpr size_t Size = 4;

        static BaseType Visit(const Vector4& inVal, size_t index) { return inVal.v[index]; }
    };
}

#include "core/math/Vector4.inl"

namespace sky {
    static constexpr Vector4 VEC4_ZERO = Vector4();
    static constexpr Vector4 VEC4_ONE = Vector4(1, 1, 1, 1);
} // namespace sky
