//
// Created by Zach Lee on 2021/12/1.
//

#pragma once

#include <core/math/Matrix4.h>
#include <core/math/Vector3.h>
#include <memory>
#include <render/resources/Shader.h>
#include <string>
#include <vector>

namespace sky {
    class RenderPrimitive;

    struct ViewInfo {
        Matrix4 viewToWorldMatrix = Matrix4::Identity();
        Matrix4 worldToViewMatrix = Matrix4::Identity();
        Matrix4 viewToClipMatrix  = Matrix4::Identity();
        Matrix4 worldToClipMatrix = Matrix4::Identity();
        Vector3 position;
    };

    class RenderView {
    public:
        RenderView()  = default;
        ~RenderView() = default;

        void SetTransform(const Matrix4 &transform);

        void SetProjectMatrix(const Matrix4 &projectMatrix);

        const ViewInfo &GetViewInfo() const;

        void SetViewTag(uint32_t tag);

        void AddRenderPrimitive(RenderPrimitive *primitive);

        const std::vector<RenderPrimitive *> &GetPrimitives() const;

        void Reset();

        bool IsDirty() const;

    private:
        void Update();

        ViewInfo                       viewInfo;
        uint32_t                       viewTag = 0;
        bool                           dirty   = true;
        std::vector<RenderPrimitive *> primitives;
    };
    using RDViewPtr = std::shared_ptr<RenderView>;

} // namespace sky