//
// Created by blues on 2024/8/10.
//

#pragma once

#include <animation/core/AnimationTypes.h>

#include <core/template/ReferenceObject.h>
#include <core/math/Transform.h>
#include <memory>

namespace sky {

    class Skeleton;

    enum class PoseBlendMode : uint32_t {
        OVERRIDE,
        ADDITIVE
    };

    struct AnimPose {
        std::vector<Transform> transforms;
        Skeleton* skeleton = nullptr; // weak reference

        void ResetRefPose();
        void NormalizeRotation();

        static void BlendTransform(const Transform& src, Transform& dst, float weight);
        static void BlendTransformAdditive(const Transform& delta, Transform& dst, float weight);

        static void BlendPose(const AnimPose & src, AnimPose & dst, float weight, PoseBlendMode mode);
    };

    using PoseSharedPtr = std::shared_ptr<AnimPose>;

} // namespace sky
