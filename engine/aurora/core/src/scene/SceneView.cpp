//
// Aurora scene view implementation.
//

#include <aurora/scene/SceneView.h>

namespace sky::aurora {

    void SceneView::SetViewMatrix(const Matrix4 &view)
    {
        mView = view;
        UpdateFrustum();
    }

    void SceneView::SetProjectionMatrix(const Matrix4 &proj)
    {
        mProj = proj;
        UpdateFrustum();
    }

    void SceneView::UpdateFrustum()
    {
        mViewProject = mProj * mView;
        mFrustum     = CreateFrustumByViewProjectMatrix(mViewProject);
    }

    bool SceneView::FrustumCulling(const BoundingBoxSphere &bounds) const
    {
        // box-vs-frustum: outside if fully negative on any plane
        const Vector3 min = bounds.Min();
        const Vector3 max = bounds.Max();
        for (const auto &plane : mFrustum.planes) {
            // positive vertex (p-vertex) of the box against the plane normal
            const Vector3 pVertex{
                plane.normal.x >= 0.f ? max.x : min.x,
                plane.normal.y >= 0.f ? max.y : min.y,
                plane.normal.z >= 0.f ? max.z : min.z,
            };
            if (plane.normal.Dot(pVertex) + plane.distance < 0.f) {
                return false;
            }
        }
        return true;
    }

    float SceneView::ViewSpaceDepth(const Vector3 &worldPos) const
    {
        const Vector4 viewPos = mView * Vector4(worldPos.x, worldPos.y, worldPos.z, 1.f);
        return viewPos.z;
    }

} // namespace sky::aurora
