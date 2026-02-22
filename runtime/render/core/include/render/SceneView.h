//
// Created by Zach Lee on 2023/4/2.
//

#pragma once

#include <core/math/Matrix4.h>
#include <core/shapes/Frustum.h>
#include <core/shapes/AABB.h>
#include <core/shapes/Bounds.h>
#include <core/std/Container.h>
#include <render/resource/Buffer.h>
#include <render/RenderBuiltinLayout.h>
#include <vector>

namespace sky {

    class SceneView {
    public:
        ~SceneView() = default;

        void SetMatrix(const Matrix4 &mat, uint32_t index = 0);
        void SetPerspective(float near, float far, float fov, float aspect, uint32_t index = 0);
        void SetOrthogonal(float l, float r, float t, float b, float near, float far, uint32_t index = 0);
        void SetFlipY(bool flip) { flipY = flip; }
        void Update();

        const Matrix4 &GetWorld() const { return viewInfo[0].world; }
        const Matrix4 &GetProject() const { return projects[0]; }
        const Matrix4 &GetView() const { return viewInfo[0].view; }
        const Matrix4 &GetViewProject() const { return viewInfo[0].viewProject; }
        Vector3 GetViewOrigin() const { return Vector3(viewInfo[0].world[3][0], viewInfo[0].world[3][1], viewInfo[0].world[3][2]); }

        bool FrustumCulling(const AABB &aabb) const;
        bool FrustumCulling(const BoundingBoxSphere &bounds) const;

        uint32_t GetViewCount() const { return viewCount; }
        uint32_t GetViewMask() const { return viewMask; }

        float GetNearPlane() const { return near; }
        float GetFarPlane() const { return far; }

        const RDUniformBufferPtr &GetUBO() const { return viewUbo; }
    private:
        friend class RenderScene;
        explicit SceneView(uint32_t count = 1, PmrResource *resource = nullptr);

        uint32_t viewCount;
        uint32_t viewMask;

        float near = 0.f;
        float far = 1.f;

        PmrVector<Matrix4> projects;
        PmrVector<SceneViewInfo> viewInfo;
        PmrVector<SceneViewInfo> lastViewInfo;
        PmrVector<Frustum> frustums;

        bool dirty;
        bool flipY = true;

        RDUniformBufferPtr viewUbo;
    };

} // namespace sky
