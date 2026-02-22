//
// Created by Zach Lee on 2021/11/13.
//

#pragma once

#include <core/math/Math.h>
#include <core/math/Matrix3.h>
#include <core/math/Matrix4.h>
#include <core/math/Vector3.h>
#include <core/math/Quaternion.h>

namespace sky {

    struct Transform {
        Vector3    translation = {0, 0, 0};
        Vector3    scale       = {1, 1, 1};
        Quaternion rotation    = {1, 0, 0, 0};

        inline Vector3 Translate(const Vector3 &rhs) const
        {
            return rotation * (scale * rhs) + translation;
        }

        inline Transform GetInverse() const
        {
            Transform result;
            result.rotation    = rotation.Conjugate();
            result.scale       = Vector3(1.f) / scale;
            result.translation = -result.scale * (result.rotation * translation);
            return result;
        }

        inline Transform operator*(const Transform &rhs) const
        {
            Transform result;
            result.rotation    = rotation * rhs.rotation;
            result.scale       = scale * rhs.scale;
            result.translation = Translate(rhs.translation);
            return result;
        }

        inline Vector3 GetRotationEuler() const { return rotation.ToEulerYZX(); }
        inline void SetRotationFromEuler(const Vector3 &euler) { rotation.FromEulerYZX(euler); }

        inline Matrix4 ToMatrix() const
        {
            auto matR = rotation.ToMatrix();
            matR[0] *= scale.x;
            matR[1] *= scale.y;
            matR[2] *= scale.z;

            matR[3][0] = translation.x;
            matR[3][1] = translation.y;
            matR[3][2] = translation.z;
            return matR;
        }

        static const Transform &GetIdentity()
        {
            static Transform transform;
            return transform;
        }
    };

} // namespace sky
