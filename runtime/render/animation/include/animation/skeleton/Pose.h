//
// Created by blues on 2024/8/10.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <core/math/Transform.h>
#include <core/name/Name.h>
#include <unordered_map>

namespace sky {

    struct BoneTransform {
        Vector3 position;
        Vector3 scale;
        Quaternion rotation;
    };

    struct AnimPose : public RefObject {
        std::unordered_map<Name, BoneTransform> bones;
    };
    using AnimPosePtr = CounterPtr<AnimPose>;

} // namespace sky
