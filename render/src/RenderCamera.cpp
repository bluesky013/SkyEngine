//
// Created by Zach Lee on 2022/7/28.
//

#include <render/RenderCamera.h>
#include <core/math/MathUtil.h>

namespace sky {

    void RenderCamera::Init()
    {
        renderView = std::make_shared<RenderView>();
    }

    void RenderCamera::SetTransform(const Matrix4& transform)
    {
        renderView->SetTransform(transform);
    }

    void RenderCamera::SetFov(float value)
    {
        fov = value;
        dirty = true;
    }

    void RenderCamera::SetAspect(float value)
    {
        aspect = value;
        dirty = true;
    }

    void RenderCamera::SetNearFar(float n, float f)
    {
        near = n;
        far = f;
        dirty = true;
    }

    bool RenderCamera::AspectFromViewport() const
    {
        return autoAspect;
    }

    void RenderCamera::UpdateProjection()
    {
        if (!dirty) {
            return;
        }

        if (projectType == ProjectType::PERSPECTIVE) {
            renderView->SetProjectMatrix(glm::perspective(ToRadian(fov), aspect, near, far));
        }
        dirty = false;
    }

}