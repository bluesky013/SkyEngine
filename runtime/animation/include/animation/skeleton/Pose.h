//
// Created by blues on 2024/8/10.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <core/math/Transform.h>
#include <core/name/Name.h>
#include <unordered_map>

namespace sky {

    class Skeleton;

    struct Pose : public RefObject {
        std::vector<Transform> transforms;
        Skeleton* skeleton = nullptr; // weak reference
    };
    using PosePtr = CounterPtr<Pose>;

} // namespace sky
