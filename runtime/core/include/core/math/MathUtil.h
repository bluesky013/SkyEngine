//
// Created by Zach Lee on 2022/7/30.
//

#pragma once
#include <core/math/Math.h>
#include <core/math/Matrix3.h>
#include <core/math/Matrix4.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <core/math/Color.h>
#include <core/math/Quaternion.h>

namespace sky {
#ifndef FLT_EPSILON
    static constexpr float FLT_EPSILON = 1.19209290E-07F; // decimal constant
#endif

    struct MathUtil {
        static void PrintMatrix(const Matrix4 &m);
    };

    inline Vector2 Min(const Vector2 &lhs, const Vector2 &rhs);
    inline Vector3 Min(const Vector3 &lhs, const Vector3 &rhs);
    inline Vector4 Min(const Vector4 &lhs, const Vector4 &rhs);

    inline Vector2 Max(const Vector2 &lhs, const Vector2 &rhs);
    inline Vector3 Max(const Vector3 &lhs, const Vector3 &rhs);
    inline Vector4 Max(const Vector4 &lhs, const Vector4 &rhs);

    template <typename T>
    T Normalize(const T &v)
    {
        return T(v).Normalize();
    }

    inline float ToRadian(float value)
    {
        return value / 180.f * PI;
    }

    inline Vector4 Cast(const Vector3 &vec)
    {
        return {vec.x, vec.y, vec.z, 1.f};
    }

    inline Vector3 Cast(const Vector4 &vec)
    {
        return {vec.x, vec.y, vec.z};
    }

    inline Matrix4 Cast(const Matrix3 &mat)
    {
        Matrix4 ret;
        ret.m[0] = Cast(mat[0]);
        ret.m[1] = Cast(mat[1]);
        ret.m[2] = Cast(mat[2]);
        ret.m[3] = Vector4(0, 0, 0, 1);
        return ret;
    }

    inline Matrix3 Cast(const Matrix4 &mat)
    {
        Matrix3 ret;
        ret.m[0] = Cast(mat[0]);
        ret.m[1] = Cast(mat[1]);
        ret.m[2] = Cast(mat[2]);
        return ret;
    }

    inline Vector4 Cast(const Color& color)
    {
        return {color.r, color.g, color.b, color.a};
    }

    inline Matrix4 MakePerspective(float fovy, float aspect, float near, float far)
    {
        float const inverseHalfTan = cos(0.5f * fovy) / sin(0.5f * fovy);

        Matrix4 ret;
        ret[0][0] = inverseHalfTan / aspect;
        ret[1][1] = inverseHalfTan;
        ret[2][2] = -(far + near) / (far - near);
        ret[2][3] = -1.f;
        ret[3][2] = -(2.f * far * near) / (far - near);
        return ret;
    }

    inline Matrix4 MakeOrthogonal(float left, float right, float top, float bottom, float near, float far)
    {
        Matrix4 ret;
        ret[0][0] = 2.f / (right - left);
        ret[1][1] = 2.f / (top - bottom);
        ret[2][2] = - 2.f / (far - near);
        ret[3][0] = - (right + left) / (right - left);
        ret[3][1] = - (top + bottom) / (top - bottom);
        ret[3][2] = - (far + near) / (far - near);
        ret[3][3] = 1.f;
        return ret;
    }

    inline void Decompose(const Matrix4 &m, Vector3 &trans, Quaternion &quat, Vector3 &scale)
    {
        trans.x = m.v[12];
        trans.y = m.v[13];
        trans.z = m.v[14];

        Vector3 xAxis(m.v[0], m.v[1], m.v[2]);
        float scaleX = xAxis.Length();

        Vector3 yAxis(m.v[4], m.v[5], m.v[6]);
        float scaleY = yAxis.Length();

        Vector3 zAxis(m.v[8], m.v[9], m.v[10]);
        float scaleZ = zAxis.Length();

        float det = m.Determinant();
        if (det < 0) {
            scaleX = -scaleX;
        }
        scale.x = scaleX;
        scale.y = scaleY;
        scale.z = scaleZ;


        float rn;
        rn = 1.0F / scaleX;
        xAxis.x *= rn;
        xAxis.y *= rn;
        xAxis.z *= rn;

        rn = 1.0F / scaleY;
        yAxis.x *= rn;
        yAxis.y *= rn;
        yAxis.z *= rn;

        rn = 1.0F / scaleZ;
        zAxis.x *= rn;
        zAxis.y *= rn;
        zAxis.z *= rn;

        float trace = xAxis.x + yAxis.y + zAxis.z;
        if (trace > 0.0F) {
            float s = 0.5F / std::sqrt(trace + 1.0F);
            quat.w = 0.25F / s;
            quat.x = (yAxis.z - zAxis.y) * s;
            quat.y = (zAxis.x - xAxis.z) * s;
            quat.z = (xAxis.y - yAxis.x) * s;
        } else {
            if (xAxis.x > yAxis.y && xAxis.x > zAxis.z) {
                float s = 0.5F / std::sqrt(1.0F + xAxis.x - yAxis.y - zAxis.z);
                quat.w = (yAxis.z - zAxis.y) * s;
                quat.x = 0.25F / s;
                quat.y = (yAxis.x + xAxis.y) * s;
                quat.z = (zAxis.x + xAxis.z) * s;
            } else if (yAxis.y > zAxis.z) {
                float s = 0.5F / std::sqrt(1.0F + yAxis.y - xAxis.x - zAxis.z);
                quat.w = (zAxis.x - xAxis.z) * s;
                quat.x = (yAxis.x + xAxis.y) * s;
                quat.y = 0.25F / s;
                quat.z = (zAxis.y + yAxis.z) * s;
            } else {
                float s = 0.5F / std::sqrt(1.0F + zAxis.z - xAxis.x - yAxis.y);
                quat.w = (xAxis.y - yAxis.x) * s;
                quat.x = (zAxis.x + xAxis.z) * s;
                quat.y = (zAxis.y + yAxis.z) * s;
                quat.z = 0.25F / s;
            }
        }
    }

    inline Vector2 Min(const Vector2 &lhs, const Vector2 &rhs)
    {
        Vector2 ret;
        ret.x = std::fmin(lhs.x, rhs.x);
        ret.y = std::fmin(lhs.y, rhs.y);
        return ret;
    }

    inline Vector3 Min(const Vector3 &lhs, const Vector3 &rhs)
    {
        Vector3 ret;
        ret.x = std::fmin(lhs.x, rhs.x);
        ret.y = std::fmin(lhs.y, rhs.y);
        ret.z = std::fmin(lhs.z, rhs.z);
        return ret;
    }

    inline Vector4 Min(const Vector4 &lhs, const Vector4 &rhs)
    {
        Vector4 ret;
        ret.x = std::fmin(lhs.x, rhs.x);
        ret.y = std::fmin(lhs.y, rhs.y);
        ret.z = std::fmin(lhs.z, rhs.z);
        ret.w = std::fmin(lhs.w, rhs.w);
        return ret;
    }

    inline Vector2 Max(const Vector2 &lhs, const Vector2 &rhs)
    {
        Vector2 ret;
        ret.x = std::fmax(lhs.x, rhs.x);
        ret.y = std::fmax(lhs.y, rhs.y);
        return ret;
    }

    inline Vector3 Max(const Vector3 &lhs, const Vector3 &rhs)
    {
        Vector3 ret;
        ret.x = std::fmax(lhs.x, rhs.x);
        ret.y = std::fmax(lhs.y, rhs.y);
        ret.z = std::fmax(lhs.z, rhs.z);
        return ret;
    }

    inline Vector4 Max(const Vector4 &lhs, const Vector4 &rhs)
    {
        Vector4 ret;
        ret.x = std::fmax(lhs.x, rhs.x);
        ret.y = std::fmax(lhs.y, rhs.y);
        ret.z = std::fmax(lhs.z, rhs.z);
        ret.w = std::fmax(lhs.w, rhs.w);
        return ret;
    }

    inline Vector2 Floor(const Vector2 &v)
    {
        return Vector2 {
            std::floor(v.x), std::floor(v.y),
        };
    }

    inline Vector3 Floor(const Vector3 &v)
    {
        return Vector3 {
            std::floor(v.x), std::floor(v.y), std::floor(v.z)
        };
    }

    inline Vector2 Fract(const Vector2 &v)
    {
        float tmp;
        return Vector2 {
            std::modf(v.x, &tmp), std::modf(v.y, &tmp)
        };
    }

    inline Vector3 Fract(const Vector3 &v)
    {
        float tmp;
        return Vector3 {
            std::modf(v.x, &tmp), std::modf(v.y, &tmp), std::modf(v.z, &tmp)
        };
    }

    constexpr inline uint32_t Ceil(uint32_t v0, uint32_t v1) {
        return (v0 + v1 - 1) / v1;
    }

    constexpr inline float FloatSelect(float comp, float ge, float lt)
    {
        return comp >= 0.f ? ge : lt;
    }

    constexpr inline Vector3 Lerp(const Vector3 &v1, const Vector3 &v2, float t)
    {
        return Vector3 {
          v1.x + (v2.x - v1.x) * t,
          v1.y + (v2.y - v1.y) * t,
          v1.z + (v2.z - v1.z) * t
        };
    }

} // namespace sky
