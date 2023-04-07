//
// Created by Zach Lee on 2023/4/7.
//

#include <scene_render/Camera.h>
#include <core/math/MathUtil.h>

namespace sky::rhi {

    void Camera::SetTransform(const Matrix4 &matrix)
    {
        transform  = matrix;
        view       = transform.Inverse();
        data.position.x = transform[3][0];
        data.position.y = transform[3][1];
        data.position.z = transform[3][2];
    }

    void Camera::MakeProjective(float fov, float aspect, float near, float far)
    {
        project = MakePerspective(fov, aspect, near, far);
    }

    void Camera::Update()
    {
        data.vp = project * view;
    }

}