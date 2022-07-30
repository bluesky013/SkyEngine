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
        viewInfo.viewToWorldMatrix = transform;
        viewInfo.worldToViewMatrix = glm::inverse(transform);
        viewInfo.position.x = transform[3][0];
        viewInfo.position.y = transform[3][1];
        viewInfo.position.z = transform[3][2];
    }

    void RenderCamera::SetProjectMatrix(const Matrix4& projectMatrix)
    {
        viewInfo.viewToClipMatrix = projectMatrix;
        viewInfo.worldToClipMatrix = viewInfo.viewToClipMatrix * viewInfo.worldToViewMatrix;
    }

}