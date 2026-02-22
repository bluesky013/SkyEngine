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
            struct {
                Vector4 m[4];
            };
        };

        FORCEINLINE Matrix4();
        FORCEINLINE Matrix4(const Vector4 &r0, const Vector4 &r1, const Vector4 &r2, const Vector4 &r3);

        FORCEINLINE static const Matrix4 &Identity();

        FORCEINLINE void Translate(const Vector3 &rhs);
        FORCEINLINE Matrix4 Inverse() const;
        FORCEINLINE Matrix4 InverseTranspose() const;
        FORCEINLINE float Determinant() const;

        FORCEINLINE Matrix4 operator+(const Matrix4& rhs) const;
        FORCEINLINE Matrix4 operator-(const Matrix4& rhs) const;
        FORCEINLINE Matrix4 operator*(const Matrix4& rhs) const;
        FORCEINLINE Matrix4 operator*(float multiplier) const;
        FORCEINLINE Matrix4 operator/(float divisor) const;
        FORCEINLINE Matrix4 operator-() const;

        FORCEINLINE Matrix4& operator+=(const Matrix4& rhs);
        FORCEINLINE Matrix4& operator-=(const Matrix4& rhs);
        FORCEINLINE Matrix4& operator*=(const Matrix4& rhs);
        FORCEINLINE Matrix4& operator*=(float multiplier);
        FORCEINLINE Matrix4& operator/=(float divisor);

        FORCEINLINE Vector4 operator*(const Vector4& rhs) const;

        FORCEINLINE Vector4 &operator[](uint32_t i);
        FORCEINLINE Vector4 operator[](uint32_t i) const;
    };
}

#include "core/math/Matrix4.inl"