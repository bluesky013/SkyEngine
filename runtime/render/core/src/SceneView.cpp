//
// Created by Zach Lee on 2023/4/2.
//

#include <render/SceneView.h>
#include <core/math/MathUtil.h>
#include <core/shapes/Shapes.h>
#include <render/RHI.h>
#include <algorithm>

namespace sky {

    SceneView::SceneView(uint32_t id, uint32_t count, PmrResource *resource)
        : viewID(id)
        , viewCount(count)
        , viewMask(0xFF)
        , viewInfo(resource)
        , lastViewInfo(resource)
        , frustums(resource)
        , dirty(true)
    {
        projects.resize(viewCount);
        viewInfo.resize(viewCount);
        frustums.resize(viewCount);

        viewUbo = new UniformBuffer();
        viewUbo->Init(sizeof(SceneViewInfo));
    }

    void SceneView::SetMatrix(const Matrix4 &mat, uint32_t index)
    {
        viewInfo[index].world = mat;
        viewInfo[index].view = mat.Inverse();
        dirty = true;
    }

    void SceneView::SetPerspective(float near_, float far_, float fov, float aspect, uint32_t index)
    {
        near = near_;
        far = far_;
        projects[index] = MakePerspective(fov, aspect, near, far);
//        viewInfo[index].zParam.x = 1 - far / near;
//        viewInfo[index].zParam.y = far / near;
//        viewInfo[index].zParam.z = viewInfo[index].zParam.x / far;
//        viewInfo[index].zParam.w = viewInfo[index].zParam.y / far;
        dirty = true;
    }

    void SceneView::SetOrthogonal(float l, float r, float t, float b, float near, float far, uint32_t index)
    {
        projects[index] = MakeOrthogonal(l, r, t, b, near, far);
        dirty = true;
    }

    void SceneView::Update()
    {
        if (!dirty) {
            return;
        }



        for (uint32_t i = 0; i < viewCount; ++i) {
            viewInfo[i].lastViewProject = viewInfo[i].viewProject;

            Matrix4 p = Matrix4::Identity();
            p[1][1] = RHI::Get()->GetDevice()->GetConstants().flipY && flipY ? -1.f : 1.f;

            viewInfo[i].viewProject = p * projects[i] * viewInfo[i].view;
            frustums[i] = CreateFrustumByViewProjectMatrix(viewInfo[i].viewProject);

            viewUbo->WriteT(i * sizeof(SceneViewInfo), viewInfo[i]);
        }
        dirty = false;
    }

    bool SceneView::FrustumCulling(const AABB &aabb) const
    {
        return std::any_of(frustums.begin(), frustums.end(), [&aabb](const auto &frustum) {
            return Intersection(aabb, frustum);
        });
    }

} // namespace sky
