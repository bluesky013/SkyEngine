//
// Created by Zach Lee on 2023/8/29.
//

#pragma once

#include <core/math/Vector3.h>
#include <core/math/Matrix4.h>
#include <limits>

namespace sky {

    struct AABB {
        using BinarySerializable = void;

        Vector3 min;
        Vector3 max;

        constexpr AABB() : min(VEC3_ZERO), max(VEC3_ZERO) {}
        constexpr AABB(Vector3 min_, Vector3 max_) : min(min_), max(max_) {}

        void Merge(const AABB& other);

        static AABB Transform(const AABB& box, const Matrix4 &matrix);
    };

    static constexpr AABB AABB_MAX = AABB(Vector3(std::numeric_limits<float>::min()), Vector3(std::numeric_limits<float>::max()));
} // namespace sky
