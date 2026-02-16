//
// Created by Zach Lee on 2021/11/12.
//


#pragma once

#include "core/math/Math.h"
#include "core/math/SFloat4.h"

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
            float32x4_t simd;
#endif
        };

        inline constexpr Vector4();
        inline constexpr Vector4(float v); // NOLINT
        inline constexpr Vector4(float x_, float y_, float z_, float w_);

        inline void Normalize();
        inline float Dot(const Vector4 &rhs) const;

        inline Vector4 operator+(const Vector4& rhs) const;

        inline Vector4 operator-() const;
        inline Vector4 operator-(const Vector4& rhs) const;

        inline Vector4 operator*(const Vector4& rhs) const;
        inline Vector4 operator*(float m) const;

        inline Vector4 operator/(const Vector4& rhs) const;
        inline Vector4 operator/(float d) const;

        inline Vector4& operator+=(const Vector4& rhs);
        inline Vector4& operator-=(const Vector4& rhs);
        inline Vector4& operator*=(const Vector4& rhs);
        inline Vector4& operator/=(const Vector4& rhs);
        inline Vector4& operator*=(float m);
        inline Vector4& operator/=(float d);

        inline float &operator[](uint32_t i);
        inline float operator[](uint32_t i) const;
    };
}

#include "core/math/Vector4.inl"

namespace sky {
    static constexpr Vector4 VEC4_ZERO = Vector4();
    static constexpr Vector4 VEC4_ONE = Vector4(1, 1, 1, 1);
} // namespace sky
