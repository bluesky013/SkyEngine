//
// Created by Zach Lee on 2021/12/1.
//


#pragma once

#include <render/resources/Shader.h>
#include <core/math/Matrix.h>
#include <core/math/Vector.h>
#include <string>
#include <memory>
#include <vector>

namespace sky {
    class RenderPrimitive;

    struct ViewInfo {
        Matrix4 viewToWorldMatrix = glm::identity<Matrix4>();
        Matrix4 worldToViewMatrix = glm::identity<Matrix4>();
        Matrix4 viewToClipMatrix = glm::identity<Matrix4>();
        Matrix4 worldToClipMatrix = glm::identity<Matrix4>();
        Vector3 position;
    };

    class RenderView {
    public:
        RenderView() = default;
        ~RenderView() = default;

        void SetTransform(const Matrix4& transform);

        void SetProjectMatrix(const Matrix4& projectMatrix);

        const ViewInfo& GetViewInfo() const;

        void SetViewTag(uint32_t tag);

        void AddRenderPrimitive(RenderPrimitive* primitive);

        const std::vector<RenderPrimitive*>& GetPrimitives() const;

        void Reset();

        bool IsDirty() const;

    private:
        void Update();

        ViewInfo viewInfo;
        uint32_t viewTag = 0;
        bool dirty = true;
        std::vector<RenderPrimitive*> primitives;
    };
    using RDViewPtr = std::shared_ptr<RenderView>;

}