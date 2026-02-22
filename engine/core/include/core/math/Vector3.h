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

        using BaseType = float;

        inline constexpr Vector3();
        inline constexpr explicit Vector3(float v);
        inline constexpr Vector3(float x_, float y_, float z_);

        inline Vector3 Cross(const Vector3 &rhs) const;
        inline float Dot(const Vector3 &rhs) const;
        inline float Length() const;
        inline void Normalize();

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

        inline bool operator==(const Vector3& rhs) const;
    };

    template <>
    struct VectorTraits<Vector3> {
        using BaseType = Vector3::BaseType;
        static constexpr size_t Size = 3;

        static BaseType Visit(const Vector3& inVal, size_t index) { return inVal.v[index]; }
    };
} // namespace sky

#include "core/math/Vector3.inl"

namespace sky {
    static constexpr Vector3 VEC3_ZERO = Vector3();
    static constexpr Vector3 VEC3_ONE = Vector3(1, 1, 1);
    static constexpr Vector3 VEC3_X = Vector3(1, 0, 0);
    static constexpr Vector3 VEC3_Y = Vector3(0, 1, 0);
    static constexpr Vector3 VEC3_Z = Vector3(0, 0, 1);
    static constexpr Vector3 VEC3_NZ = Vector3(0, 0, -1);
} // namespace sky