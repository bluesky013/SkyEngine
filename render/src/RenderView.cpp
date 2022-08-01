//
// Created by Zach Lee on 2021/12/22.
//

#include <render/RenderView.h>
#include <render/RenderPrimtive.h>

namespace sky {

    void RenderView::SetTransform(const Matrix4& transform)
    {
        viewInfo.viewToWorldMatrix = transform;
        viewInfo.worldToViewMatrix = glm::inverse(transform);
        viewInfo.position.x = transform[3][0];
        viewInfo.position.y = transform[3][1];
        viewInfo.position.z = transform[3][2];
    }

    void RenderView::SetProjectMatrix(const Matrix4& projectMatrix)
    {
        viewInfo.viewToClipMatrix = projectMatrix;
        viewInfo.worldToClipMatrix = viewInfo.viewToClipMatrix * viewInfo.worldToViewMatrix;
    }

    const ViewInfo& RenderView::GetViewInfo() const
    {
        return viewInfo;
    }

    void RenderView::SetViewTag(uint32_t tag)
    {
        viewTag = tag;
    }

    void RenderView::AddRenderPrimitive(RenderPrimitive* primitive)
    {
        if (primitive == nullptr || (viewTag & primitive->GetViewMask()) == 0) {
            return;
        }
        primitives.emplace_back(primitive);
    }

    const std::vector<RenderPrimitive*>& RenderView::GetPrimitives() const
    {
        return primitives;
    }

    void RenderView::Reset()
    {
        primitives.clear();
    }
}