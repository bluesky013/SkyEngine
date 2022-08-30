//
// Created by Zach Lee on 2021/11/13.
//

#pragma once

#include <core/math/Math.h>
#include <core/math/Matrix.h>
#include <core/math/Quaternion.h>
#include <core/math/Vector.h>

namespace sky {

    struct Transform {
        Vector3    translation = {0, 0, 0};
        Vector3    scale       = {1, 1, 1};
        Quaternion rotation    = {1, 0, 0, 0};

        inline Vector3 Translate(const Vector3 &rhs) const
        {
            return glm::rotate(rotation, scale * rhs) + translation;
        }

        inline Transform GetInverse() const
        {
            Transform result;
            result.rotation    = glm::conjugate(rotation);
            result.scale       = 1.f / scale;
            result.translation = -result.scale * glm::rotate(result.rotation, translation);
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

        inline Matrix4 ToMatrix() const
        {
            auto matR = glm::mat4_cast(rotation);
            matR[0] *= scale.x;
            matR[1] *= scale.y;
            matR[2] *= scale.z;

            matR[3][0] = translation.x;
            matR[3][1] = translation.y;
            matR[3][2] = translation.z;
            return matR;
        }

        static Transform FromMatrix(const Matrix4 &matrix)
        {
            Transform result;
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(matrix, result.scale, result.rotation, result.translation, skew, perspective);
            return result;
        }

        static const Transform &GetIdentity()
        {
            static Transform transform;
            return transform;
        }
    };

} // namespace sky