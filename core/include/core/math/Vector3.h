//
// Created by Zach Lee on 2021/11/12.
//


#pragma once

#include "core/math/Math.h"

namespace sky {

    struct Vector3 {
        union {
            float v[3];
            struct {
                float x;
                float y;
                float z;
            };
        };

        Vector3();
        Vector3(float v);
        Vector3(float x_, float y_, float z_);

        inline Vector3 operator+(const Vector3& rhs) const;

        inline Vector3 operator-() const;
        inline Vector3 operator-(const Vector3& rhs) const;

        inline Vector3 operator*(const Vector3& rhs) const;
        inline Vector3 operator*(float m) const;

        inline Vector3 operator/(const Vector3& rhs) const;
        inline Vector3 operator/(float d) const;

        inline Vector3& operator+=(const Vector3& rhs);
        inline Vector3& operator-=(const Vector3& rhs);
        inline Vector3& operator*=(const Vector3& rhs);
        inline Vector3& operator/=(const Vector3& rhs);
        inline Vector3& operator*=(float m);
        inline Vector3& operator/=(float d);

        inline float &operator[](uint32_t i);
        inline float operator[](uint32_t i) const;
    };
}

#include "core/math/Vector3.inl"