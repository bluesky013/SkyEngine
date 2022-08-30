//
// Created by Zach Lee on 2021/12/22.
//

#include <render/RenderMeshPrimtive.h>
#include <render/RenderView.h>

namespace sky {

    void RenderView::Update()
    {
        viewInfo.worldToClipMatrix = viewInfo.viewToClipMatrix * viewInfo.worldToViewMatrix;
        dirty                      = true;
    }

    void RenderView::SetTransform(const Matrix4 &transform)
    {
        viewInfo.viewToWorldMatrix = transform;
        viewInfo.worldToViewMatrix = glm::inverse(transform);
        viewInfo.position.x        = transform[3][0];
        viewInfo.position.y        = transform[3][1];
        viewInfo.position.z        = transform[3][2];
        Update();
    }

    void RenderView::SetProjectMatrix(const Matrix4 &projectMatrix)
    {
        viewInfo.viewToClipMatrix = projectMatrix;
        Update();
    }

    const ViewInfo &RenderView::GetViewInfo() const
    {
        return viewInfo;
    }

    void RenderView::SetViewTag(uint32_t tag)
    {
        viewTag = tag;
    }

    void RenderView::AddRenderPrimitive(RenderPrimitive *primitive)
    {
        if (primitive == nullptr || (viewTag & primitive->GetViewMask()) == 0) {
            return;
        }
        primitives.emplace_back(primitive);
    }

    const std::vector<RenderPrimitive *> &RenderView::GetPrimitives() const
    {
        return primitives;
    }

    void RenderView::Reset()
    {
        primitives.clear();
        dirty = false;
    }

    bool RenderView::IsDirty() const
    {
        return dirty;
    }
} // namespace sky