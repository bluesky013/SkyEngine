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

namespace sky {

    struct SceneViewInfo {
        Matrix4 worldMatrix;
        Matrix4 viewMatrix;
        Matrix4 projectMatrix;
        Matrix4 viewProjectMatrix;
    };

    class SceneView {
    public:
        explicit SceneView(uint32_t view = 1, PmrResource *resource = nullptr);
        ~SceneView() = default;

        void SetMatrix(const Matrix4 &mat, uint32_t viewID = 0);
        void SetProjective(float near, float far, float fov, float aspect, uint32_t viewID = 0);
        void Update();

        bool FrustumCulling(const AABB &aabb) const;

    private:
        uint32_t viewCount;

        PmrVector<SceneViewInfo> viewInfo;
        PmrVector<Frustum> frustums;
        bool dirty;

        RDUniformBufferPtr viewUbo;
    };

} // namespace sky
