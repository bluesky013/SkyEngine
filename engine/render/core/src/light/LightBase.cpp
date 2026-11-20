//
// Created by blues on 2024/6/10.
//

#include <render/light/LightBase.h>
#include <render/SceneView.h>
#include <render/RHI.h>

namespace sky {

    void PointLight::Collect(LightInfo &info)
    {
        info.color = color;

    }

    void SpotLight::Collect(LightInfo &info)
    {
        info.color = color;
        info.position  = ToVec4(position);
        info.direction = ToVec4(direction);
    }

    void SpotLight::BuildShadowMatrix(SceneView &view) const
    {
        Matrix4 p = Matrix4::Identity();
        p[1][1] = RHI::Get()->GetDevice()->GetConstants().flipY ? -1.f : 1.f;

        // Build a look-at world matrix for the spot light.
        // SceneView::SetMatrix expects a world (TRS) matrix – the engine computes
        // the view matrix as its inverse internally.
        Vector3 fwd = direction;
        fwd.Normalize();

        const float kDotY = fwd.y; // fwd · (0,1,0)
        Vector3 worldUp = (std::abs(kDotY) > 0.99f)
                          ? Vector3(1.f, 0.f, 0.f)
                          : Vector3(0.f, 1.f, 0.f);

        Vector3 right = fwd.Cross(worldUp);
        right.Normalize();
        Vector3 up = right.Cross(fwd);
        up.Normalize();

        // Row-major world matrix (translation in row 3):
        //   row0 = right,   row1 = up,   row2 = fwd,   row3 = position
        Matrix4 worldMat = Matrix4::Identity();
        worldMat[0][0] = right.x; worldMat[0][1] = right.y; worldMat[0][2] = right.z;
        worldMat[1][0] = up.x;    worldMat[1][1] = up.y;    worldMat[1][2] = up.z;
        worldMat[2][0] = fwd.x;   worldMat[2][1] = fwd.y;   worldMat[2][2] = fwd.z;
        worldMat[3][0] = position.x;
        worldMat[3][1] = position.y;
        worldMat[3][2] = position.z;
        worldMat[3][3] = 1.f;

        view.SetMatrix(worldMat);
        view.SetPerspective(0.1f, range, angle * 2.f, 1.f);
    }

    void DirectLight::Collect(LightInfo &info)
    {
        info.color = color;
        info.direction = ToVec4(direction);
    }

    void MainDirectLight::BuildMatrix(SceneView& view)
    {
        Matrix4 p = Matrix4::Identity();
        p[1][1] = RHI::Get()->GetDevice()->GetConstants().flipY ? -1.f : 1.f;

        view.SetMatrix(worldMatrix);
        view.SetOrthogonal(-110.f, 110.f, 110.f, -110.f, 1.f, 500.f);

        viewProject = p * view.GetProject() * view.GetView();
    }

} // namespace sky