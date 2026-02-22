//
// Created by Zach Lee on 2021/11/12.
//


#pragma once

#include "core/math/Math.h"

namespace sky {

    struct Vector2 {
        union {
            float v[2];
            struct {
                float x;
                float y;
            };
        };

        using BaseType = float;

        inline constexpr Vector2();
        inline constexpr explicit Vector2(float v);
        inline constexpr Vector2(float x_, float y_);

        inline float Dot(const Vector2 &rhs) const;
        inline float Length() const;
        inline void Normalize();

        inline Vector2 operator+(const Vector2& rhs) const;

        inline Vector2 operator-() const;
        inline Vector2 operator-(const Vector2& rhs) const;

        inline Vector2 operator*(const Vector2& rhs) const;
        inline Vector2 operator*(float m) const;

        inline Vector2 operator/(const Vector2& rhs) const;
        inline Vector2 operator/(float d) const;

        inline Vector2& operator+=(const Vector2& rhs);
        inline Vector2& operator-=(const Vector2& rhs);
        inline Vector2& operator*=(const Vector2& rhs);
        inline Vector2& operator/=(const Vector2& rhs);
        inline Vector2& operator*=(float m);
        inline Vector2& operator/=(float d);

        inline float &operator[](uint32_t i);
        inline float operator[](uint32_t i) const;
    };

    template <>
    struct VectorTraits<Vector2> {
        using BaseType = Vector2::BaseType;
        static constexpr size_t Size = 2;

        static BaseType Visit(const Vector2& inVal, size_t index) { return inVal.v[index]; }
    };

} // namespace sky

#include "core/math/Vector2.inl"

namespace sky {
    static constexpr Vector2 VEC2_ZERO = Vector2();
    static constexpr Vector2 VEC2_ONE = Vector2(1, 1);
} // namespace sky