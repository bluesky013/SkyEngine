//
// Created by Zach Lee on 2021/11/13.
//

#pragma once

#include <core/math/Math.h>
#include <core/math/Vector3.h>

namespace sky {

    struct Quaternion {
        union {
            float v[4];
            struct {
                float x;
                float y;
                float z;
                float w;
            };
        };

        inline Quaternion();
        inline Quaternion(float w, float x, float y, float z);
        inline Quaternion(float angle, const Vector3 &axis);

        inline void Normalize();
        inline Quaternion Conjugate() const;

        inline Quaternion operator*(const Quaternion &rhs) const;
        inline Vector3 operator*(const Vector3 &rhs) const;

        inline Quaternion& operator*=(float m);
        inline Quaternion& operator/=(float d);

        void FromEulerYZX(Vector3 euler)
        {
            float halfToRad = 0.5F * sky::PI / 180.F;
            euler.x *= halfToRad;
            euler.y *= halfToRad;
            euler.z *= halfToRad;
            float sx = std::sin(euler.x);
            float cx = std::cos(euler.x);
            float sy = std::sin(euler.y);
            float cy = std::cos(y);
            float sz = std::sin(z);
            float cz = std::cos(z);

            x = sx * cy * cz + cx * sy * sz;
            y = cx * sy * cz + sx * cy * sz;
            z = cx * cy * sz - sx * sy * cz;
            w = cx * cy * cz - sx * sy * sz;
        }

        Vector3 ToEulerYZX() const
        {
            float bank{0};
            float heading{0};
            float attitude{0};
            float test = x * y + z * w;
            float r2d = 180.F / sky::PI;
            if (test > 0.499999) {
                bank = 0;
                heading = 2 * atan2(x, w) * r2d;
                attitude = 90;
            } else if (test < -0.499999) {
                bank = 0;
                heading = -2 * atan2(x, w) * r2d;
                attitude = -90;
            } else {
                float sqx = x * x;
                float sqy = y * y;
                float sqz = z * z;
                bank = atan2(2 * x * w - 2 * y * z, 1 - 2 * sqx - 2 * sqz) * r2d;
                heading = atan2(2 * y * w - 2 * x * z, 1 - 2 * sqy - 2 * sqz) * r2d;
                attitude = asin(2 * test) * r2d;
            }
            return {bank, heading, attitude};
        }
    };
}

#include "core/math/Quaternion.inl"