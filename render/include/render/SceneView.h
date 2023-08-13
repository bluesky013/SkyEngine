//
// Created by Zach Lee on 2023/4/2.
//

#pragma once

#include <memory>
#include <vector>
#include <string>
#include <core/math/Matrix4.h>
#include <core/shapes/Frustum.h>
#include <core/std/Container.h>

namespace sky {

    class SceneView {
    public:
        explicit SceneView(uint32_t view = 1, PmrResource *resource = nullptr);
        ~SceneView() = default;

        void SetViewTag(const std::string &id) { viewTag = id; }
        const PmrString &GetViewTag() const { return viewTag; }

        void SetViewMatrix(const Matrix4 &mat, uint32_t viewID = 0);
        void SetProjective(float near, float far, float fov, float aspect, uint32_t viewID = 0);
        void Update();

    private:
        uint32_t viewCount;
        PmrString viewTag;
        PmrVector<Matrix4> matView;
        PmrVector<Matrix4> matProj;
        PmrVector<Matrix4> matProjInv;
        PmrVector<Matrix4> matViewProj;
        PmrVector<Matrix4> matViewProjInv;
        PmrVector<Frustum> frustums;
        PmrVector<bool>    dirty;
    };
    using ViewPtr = std::shared_ptr<SceneView>;

} // namespace sky