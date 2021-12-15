//
// Created by Zach Lee on 2021/11/13.
//


#pragma once

#include <engine/world/Component.h>
#include <core/math/Matrix.h>
#include <core/math/Vector.h>
#include <cstdint>

namespace sky {

    enum class ProjectType : uint32_t {
        ORTHOGONAL,
        PROJECTIVE
    };

    class CameraComponent : public Component {
    public:
        CameraComponent() = default;
        ~CameraComponent() = default;

    private:
        float near;
        float far;

        Matrix4 projection;
    };


}