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

        Vector2();
        Vector2(float v);
        Vector2(float x_, float y_);

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
}

#include "core/math/Vector2.inl"