//
// Created by Zach Lee on 2023/4/2.
//

#pragma once

#include <memory>
#include <vector>
#include <string>
#include <core/math/Matrix4.h>
#include <core/shapes/Frustum.h>
#include <core/shapes/AABB.h>
#include <core/std/Container.h>
#include <rhi/Buffer.h>
#include <render/resource/Buffer.h>
#include <render/RenderBuiltinLayout.h>

namespace sky {

    class SceneView {
    public:
        ~SceneView() = default;

        void SetMatrix(const Matrix4 &mat, uint32_t index = 0);
        void SetPerspective(float near, float far, float fov, float aspect, uint32_t index = 0);
        void SetOrthogonal(float l, float r, float t, float b, float near, float far, uint32_t index = 0);
        void SetFlipY(bool flip) { flipY = flip; }
        void Update();

        const Matrix4 &GetProject() const { return projects0[0]; }
        const Matrix4 &GetView() const { return viewInfo[0].view; }
        const Matrix4 &GetViewProject() const { return viewInfo[0].viewProject; }

        bool FrustumCulling(const AABB &aabb) const;

        uint32_t GetViewID() const { return viewID; }
        uint32_t GetViewCount() const { return viewCount; }
        uint32_t GetViewMask() const { return viewMask; }

        const RDUniformBufferPtr &GetUBO() const { return viewUbo; }

    private:
        friend class RenderScene;
        explicit SceneView(uint32_t id, uint32_t count = 1, PmrResource *resource = nullptr);

        uint32_t viewID;
        uint32_t viewCount;
        uint32_t viewMask;

        PmrVector<Matrix4> projects0;
        PmrVector<SceneViewInfo> viewInfo;
        PmrVector<Frustum> frustums;
        bool dirty;
        bool flipY = true;

        RDUniformBufferPtr viewUbo;
    };

} // namespace sky
