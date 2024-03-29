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
        void SetProjective(float near, float far, float fov, float aspect, uint32_t index = 0);
        void Update();

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

        PmrVector<SceneViewInfo> viewInfo;
        PmrVector<Frustum> frustums;
        bool dirty;

        RDUniformBufferPtr viewUbo;
    };

} // namespace sky
