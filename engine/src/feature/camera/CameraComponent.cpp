//
// Created by Zach Lee on 2021/12/1.
//

#include <engine/feature/camera/CameraComponent.h>
#include <engine/world/GameObject.h>
#include <engine/world/World.h>
#include <core/math/MathUtil.h>

namespace sky {

    void CameraComponent::Perspective(float near_, float far_, float fov_, float aspect_)
    {
        near   = near_;
        far    = far_;
        fov    = fov_;
        aspect = aspect_;
        type   = ProjectType::PROJECTIVE;
        UpdateProjection();
    }

    void CameraComponent::Otho(float left_, float right_, float top_, float bottom_, float near_, float far_)
    {
        left   = left_;
        right  = right_;
        top    = top_;
        bottom = bottom_;
        near   = near_;
        far    = far_;
        type   = ProjectType::ORTHOGONAL;
        UpdateProjection();
    }

    void CameraComponent::UpdateProjection()
    {
        if (type == ProjectType::PROJECTIVE) {
            projection = MakePerspective(fov / 180.f * 3.14f, aspect, near, far);
        } else {
            projection = MakeOrthogonal(left, right, top, bottom, near, far);
        }
    }

    void CameraComponent::OnTick(float time)
    {
    }

    void CameraComponent::OnActive()
    {
    }

    void CameraComponent::OnDestroy()
    {
    }
} // namespace sky