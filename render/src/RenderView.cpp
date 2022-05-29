//
// Created by Zach Lee on 2021/12/22.
//

#include <render/RenderView.h>

namespace sky {

    void RenderView::SetTransform(const Matrix4& transform)
    {
        viewToWorldMatrix = transform;
        worldToViewMatrix = glm::inverse(transform);
        position.x = transform[3][0];
        position.y = transform[3][1];
        position.z = transform[3][2];
    }

    void RenderView::SetProjectMatrix(const Matrix4& projectMatrix)
    {
        viewToClipMatrix = projectMatrix;
        worldToClipMatrix = viewToClipMatrix * worldToViewMatrix;
    }

    void RenderView::UpdateData()
    {

    }
}