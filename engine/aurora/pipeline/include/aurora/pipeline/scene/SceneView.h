//
// Aurora scene view: frustum + view matrices for culling and sorting.
// v1: CPU-side culling/sorting only; constant upload comes later.
//

#pragma once

#include <core/math/Matrix4.h>
#include <core/shapes/Frustum.h>
#include <core/shapes/AABB.h>

namespace sky::aurora {

    class SceneView {
    public:
        SceneView() = default;
        ~SceneView() = default;

        void SetViewMatrix(const Matrix4 &view);
        void SetProjectionMatrix(const Matrix4 &proj);

        const Matrix4 &GetViewMatrix() const { return mView; }
        const Matrix4 &GetViewProjectMatrix() const { return mViewProject; }

        bool FrustumCulling(const AABB &bounds) const;

        // view-space depth of a world position (for queue sorting)
        float ViewSpaceDepth(const Vector3 &worldPos) const;

    private:
        void UpdateFrustum();

        Matrix4 mView;
        Matrix4 mProj;
        Matrix4 mViewProject;
        Frustum mFrustum;
    };

} // namespace sky::aurora
