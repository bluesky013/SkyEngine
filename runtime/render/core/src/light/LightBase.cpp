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