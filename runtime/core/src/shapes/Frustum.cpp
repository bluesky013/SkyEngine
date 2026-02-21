//
// Created by Zach Lee on 2023/8/13.
//

#include <core/shapes/Frustum.h>

namespace sky {

    static Plane GetFrustumPlane(const Vector3 &v, float d)
    {
        const float invSqrt = 1.f / sqrt(v.Dot(v));
        return Plane{v * (-invSqrt), d * invSqrt};
    }

    Frustum CreateFrustumByViewProjectMatrix(const Matrix4 &mtx)
    {
        Frustum frustum = {};
        // left
        frustum.planes[0] = GetFrustumPlane(Vector3(
                                                mtx[0][3] + mtx[0][0],
                                                mtx[1][3] + mtx[1][0],
                                                mtx[2][3] + mtx[2][0]), mtx[3][3] + mtx[3][0]);
        // right
        frustum.planes[1] = GetFrustumPlane(Vector3(
                                                mtx[0][3] - mtx[0][0],
                                                mtx[1][3] - mtx[1][0],
                                                mtx[2][3] - mtx[2][0]), mtx[3][3] - mtx[3][0]);
        // bottom
        frustum.planes[2] = GetFrustumPlane(Vector3(
                                                mtx[0][3] + mtx[0][1],
                                                mtx[1][3] + mtx[1][1],
                                                mtx[2][3] + mtx[2][1]), mtx[3][3] + mtx[3][1]);
        // top
        frustum.planes[3] = GetFrustumPlane(Vector3(
                                                mtx[0][3] - mtx[0][1],
                                                mtx[1][3] - mtx[1][1],
                                                mtx[2][3] - mtx[2][1]), mtx[3][3] - mtx[3][1]);
        // near
        frustum.planes[4] = GetFrustumPlane(Vector3(
                                                mtx[0][3] + mtx[0][2],
                                                mtx[1][3] + mtx[1][2],
                                                mtx[2][3] + mtx[2][2]), mtx[3][3] + mtx[3][2]);
        // far
        frustum.planes[5] = GetFrustumPlane(Vector3(
                                                mtx[0][3] - mtx[0][2],
                                                mtx[1][3] - mtx[1][2],
                                                mtx[2][3] - mtx[2][2]), mtx[3][3] - mtx[3][2]);
        return frustum;
    }

} // namespace sky
