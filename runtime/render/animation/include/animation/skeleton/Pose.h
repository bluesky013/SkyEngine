//
// Created by blues on 2024/8/10.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <core/math/Transform.h>
#include <vector>

namespace sky {

    class PoseBase : public RefObject {
    public:
        explicit PoseBase(std::vector<Transform> transforms) : bones(std::move(transforms)) {}
        ~PoseBase() override = default;

    private:
        std::vector<Transform> bones;
    };
    using PosePtr = CounterPtr<PoseBase>;

} // namespace sky
