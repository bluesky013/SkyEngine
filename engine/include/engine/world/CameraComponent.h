//
// Created by Zach Lee on 2021/11/13.
//

#pragma once

#include <core/math/Matrix4.h>
#include <engine/base/Component.h>

namespace sky {

    enum class ProjectType : uint32_t { ORTHOGONAL, PROJECTIVE };

    class CameraComponent : public Component {
    public:
        CameraComponent()  = default;
        ~CameraComponent() = default;

        TYPE_RTTI_WITH_VT(CameraComponent, "473D65D7-2D8D-48E3-86FE-0C20016A387D")

        static void Reflect();

        void Perspective(float near, float far, float fov, float aspect);

        void Otho(float left, float right, float top, float bottom, float near, float far);

        void UpdateProjection();

        void OnTick(float time) override;

        void OnActive() override;

        void OnDestroy() override;

    private:
        float near   = 0.1f;
        float far    = 100.f;
        float fov    = 60;
        float aspect = 1.f;

        float left   = -1.f;
        float right  = 1.f;
        float top    = 1.f;
        float bottom = -1.f;

        ProjectType type;
        Matrix4     projection;
    };

} // namespace sky