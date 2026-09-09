//
// Aurora scene view implementation.
//

#include <aurora/pipeline/scene/SceneView.h>

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

    bool SceneView::FrustumCulling(const AABB &bounds) const
    {
        // AABB-vs-frustum: outside if fully negative on any plane
        for (const auto &plane : mFrustum.planes) {
            // positive vertex (p-vertex) of the AABB against the plane normal
            const Vector3 pVertex{
                plane.normal.x >= 0.f ? bounds.max.x : bounds.min.x,
                plane.normal.y >= 0.f ? bounds.max.y : bounds.min.y,
                plane.normal.z >= 0.f ? bounds.max.z : bounds.min.z,
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
