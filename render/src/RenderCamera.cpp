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
        renderView->SetTransform(transform);
    }

    void RenderCamera::SetProjectMatrix(const Matrix4& projectMatrix)
    {
        renderView->SetProjectMatrix(projectMatrix);
    }

}