//
// Created by Zach Lee on 2022/7/28.
//

#include <render/RenderCamera.h>

namespace sky {

    void RenderCamera::Init()
    {
        renderView = std::make_shared<RenderView>();
    }

    void RenderCamera::SetTransform(const Matrix4& transform)
    {
        viewToWorldMatrix = transform;
        worldToViewMatrix = glm::inverse(transform);
        position.x = transform[3][0];
        position.y = transform[3][1];
        position.z = transform[3][2];
    }

    void RenderCamera::SetProjectMatrix(const Matrix4& projectMatrix)
    {
        viewToClipMatrix = projectMatrix;
        worldToClipMatrix = viewToClipMatrix * worldToViewMatrix;
    }

}