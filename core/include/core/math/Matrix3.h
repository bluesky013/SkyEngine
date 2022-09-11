//
// Created by Zach Lee on 2021/11/12.
//


#pragma once

#include "core/math/Math.h"
#include "core/math/Vector3.h"
#include "core/math/Euler.h"

namespace sky {

    struct Matrix3 {
        union {
            float v[9];
            struct {
                Vector3 m[3];
            };
        };

        inline Matrix3();
        inline Matrix3(const Vector3 &r0, const Vector3 &r1, const Vector3 &r2);

        inline static Matrix3 FromEulerYXZ(const Euler &euler);
        inline static const Matrix3 &Identity();

        inline Matrix3 Inverse() const;

        inline Matrix3 operator+(const Matrix3& rhs) const;
        inline Matrix3 operator-(const Matrix3& rhs) const;
        inline Matrix3 operator*(const Matrix3& rhs) const;
        inline Matrix3 operator*(float multiplier) const;
        inline Matrix3 operator/(float divisor) const;
        inline Matrix3 operator-() const;

        inline Matrix3& operator+=(const Matrix3& rhs);
        inline Matrix3& operator-=(const Matrix3& rhs);
        inline Matrix3& operator*=(const Matrix3& rhs);
        inline Matrix3& operator*=(float multiplier);
        inline Matrix3& operator/=(float divisor);

        inline Vector3 operator*(const Vector3& rhs) const;

        inline Vector3 &operator[](uint32_t i);
        inline Vector3 operator[](uint32_t i) const;
    };
}

#include "core/math/Matrix3.inl"