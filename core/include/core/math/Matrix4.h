//
// Created by Zach Lee on 2021/11/12.
//


#pragma once

#include "core/math/Math.h"
#include "core/math/Vector3.h"
#include "core/math/Vector4.h"

namespace sky {

    struct Matrix4 {
        union {
            float v[16];
            float m[4][4];
            struct {
                Vector4 cols[4];
            };
        };

        Matrix4();
        Matrix4(const Vector4 &r0, const Vector4 &r1, const Vector4 &r2, const Vector4 &r3);

        inline Matrix4 operator+(const Matrix4& rhs) const;
        inline Matrix4 operator-(const Matrix4& rhs) const;
        inline Matrix4 operator*(const Matrix4& rhs) const;
        inline Matrix4 operator*(float multiplier) const;
        inline Matrix4 operator/(float divisor) const;
        inline Matrix4 operator-() const;

        inline Matrix4& operator+=(const Matrix4& rhs);
        inline Matrix4& operator-=(const Matrix4& rhs);
        inline Matrix4& operator*=(const Matrix4& rhs);
        inline Matrix4& operator*=(float multiplier);
        inline Matrix4& operator/=(float divisor);

        inline Vector4 operator*(const Vector4& rhs) const;

        inline Vector4 &operator[](uint32_t i);
        inline Vector4 operator[](uint32_t i) const;
    };
}

#include "core/math/Matrix4.inl"