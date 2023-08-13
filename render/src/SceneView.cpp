//
// Created by Zach Lee on 2023/4/2.
//

#include <render/SceneView.h>
#include <core/math/MathUtil.h>

namespace sky {

    SceneView::SceneView(uint32_t view, PmrResource *resource)
        : viewCount(view)
        , viewTag(resource)
        , matView(resource)
        , matProj(resource)
        , matProjInv(resource)
        , matViewProj(resource)
        , matViewProjInv(resource)
        , frustums(resource)
        , dirty(resource)
    {
        matView.resize(viewCount);
        matProj.resize(viewCount);
        matProjInv.resize(viewCount);
        matViewProj.resize(viewCount);
        matViewProjInv.resize(viewCount);
        dirty.resize(viewCount);
        frustums.resize(viewCount);
    }

    void SceneView::SetViewMatrix(const Matrix4 &mat, uint32_t viewID)
    {
        matView[viewID] = mat;
        dirty[viewID] = true;
    }

    void SceneView::SetProjective(float near, float far, float fov, float aspect, uint32_t viewID)
    {
        matProj[viewID] = MakePerspective(fov, aspect, near, far);
        matProjInv[viewID] = matProj[viewID].Inverse();
        dirty[viewID] = true;
    }

    void SceneView::Update()
    {
        for (uint32_t i = 0; i < viewCount; ++i) {
            if (dirty[i]) {
                matViewProj[i] = matProj[i] * matView[i];
                matViewProjInv[i] = matViewProj[i].Inverse();
                dirty[i] = false;
            }
        }
    }

} // namespace sky