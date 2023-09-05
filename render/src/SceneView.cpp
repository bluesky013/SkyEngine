//
// Created by Zach Lee on 2023/4/2.
//

#include <render/SceneView.h>
#include <core/math/MathUtil.h>
#include <core/shapes/Shapes.h>
#include <render/RHI.h>

namespace sky {

    SceneView::SceneView(uint32_t view, PmrResource *resource)
        : viewCount(view)
        , viewInfo(resource)
        , frustums(resource)
        , dirty(true)
    {
        viewInfo.resize(viewCount);
        frustums.resize(viewCount);

        viewUbo = std::make_shared<UniformBuffer>();
        viewUbo->Init(sizeof(SceneViewInfo));
    }

    void SceneView::SetMatrix(const Matrix4 &mat, uint32_t viewID)
    {
        viewInfo[viewID].worldMatrix = mat;
        viewInfo[viewID].viewMatrix = mat.Inverse();
        dirty = true;
    }

    void SceneView::SetProjective(float near, float far, float fov, float aspect, uint32_t viewID)
    {
        viewInfo[viewID].projectMatrix = MakePerspective(fov, aspect, near, far);
        dirty = true;
    }

    void SceneView::Update()
    {
        if (!dirty) {
            return;
        }

        for (uint32_t i = 0; i < viewCount; ++i) {
            viewInfo[i].viewProjectMatrix = viewInfo[i].projectMatrix * viewInfo[i].viewMatrix;
            frustums[i] = CreateFrustumByViewProjectMatrix(viewInfo[i].viewProjectMatrix);

            viewUbo->Write(i * sizeof(SceneViewInfo), viewInfo[i]);
        }
        dirty = false;
    }

    bool SceneView::FrustumCulling(const AABB &aabb) const
    {
        for (auto &frustum : frustums) {
            if (Intersection(aabb, frustum)) {
                return true;
            }
        }
        return false;
    }

} // namespace sky
