//
// Created by Zach Lee on 2021/12/1.
//


#include <engine/render/camera/CameraComponent.h>
#include <engine/world/TransformComponent.h>
#include <engine/world/GameObject.h>
#include <glm/gtc/matrix_transform.hpp>

namespace sky {

    void CameraComponent::Perspective(float near_, float far_, float fov_, float aspect_)
    {
        near = near_;
        far = far_;
        fov = fov_;
        aspect = aspect_;
        type = ProjectType::PROJECTIVE;
        UpdateProjection();
    }

    void CameraComponent::Otho(float left_, float right_, float top_, float bottom_, float near_, float far_)
    {
        left = left_;
        right = right_;
        top = top_;
        bottom = bottom_;
        near = near_;
        far = far_;
        type = ProjectType::ORTHOGONAL;
        UpdateProjection();
    }

    void CameraComponent::UpdateProjection()
    {
        if (type == ProjectType::PROJECTIVE) {
            projection = glm::perspective(fov / 180.f * 3.14f, aspect, near, far);
        } else {
            projection = glm::orthoRH_NO(left, right, bottom, top, near, far);
        }
    }

    void CameraComponent::OnTick(float time)
    {
        auto trans = object->GetComponent<TransformComponent>();
    }

    void CameraComponent::OnInit()
    {
    }

    void CameraComponent::OnDestroy()
    {

    }
}