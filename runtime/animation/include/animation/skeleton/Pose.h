//
// Created by blues on 2024/8/10.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <core/math/Transform.h>
#include <core/name/Name.h>
#include <unordered_map>
#include <memory>

namespace sky {

#define BLEND_WEIGHT_THRESHOLD 0.00001f

    class Skeleton;

    enum class PoseBlendMode : uint32_t {
        OVERRIDE,
        ADDITIVE
    };

    struct Pose {
        std::vector<Transform> transforms;
        Skeleton* skeleton = nullptr; // weak reference

        void ResetRefPose();

        static void BlendTransform(const Transform& src, Transform& dst, float weight);
        static void BlendTransformAdditive(const Transform& delta, Transform& dst, float weight);

        static void BlendPose(const Pose& src, Pose& dst, float weight, PoseBlendMode mode);
    };

    using PoseSharedPtr = std::shared_ptr<Pose>;

} // namespace sky
