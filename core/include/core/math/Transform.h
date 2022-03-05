//
// Created by Zach Lee on 2021/11/13.
//


#pragma once

#include <core/math/Math.h>
#include <core/math/Vector.h>
#include <core/math/Matrix.h>
#include <core/math/Quaternion.h>

namespace sky {

    struct Transform {
        Vector3 translation = {0, 0, 0};
        Vector3 scale = {1, 1, 1};
        Quaternion rotation = {1, 0, 0, 0};

        inline Vector3 Translate(const Vector3& rhs) const
        {
            return glm::rotate(rotation, scale * rhs) + translation;
        }

        inline Transform GetInverse() const
        {
            Transform result;
            result.rotation = glm::conjugate(rotation);
            result.scale = 1.f / scale;
            result.translation = -result.scale * glm::rotate(result.rotation, translation);
            return result;
        }

        inline Transform operator*(const Transform& rhs) const
        {
            Transform result;
            result.rotation = rotation * rhs.rotation;
            result.scale = scale * rhs.scale;
            result.translation = Translate(rhs.translation);
            return result;
        }

        static const Transform& GetIdentity()
        {
            static Transform transform;
            return transform;
        }
    };

}